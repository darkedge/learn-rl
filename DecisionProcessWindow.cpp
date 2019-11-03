#include "imgui.h"
#include "Random.h"
#include "IconsKenney.h"
#include <stdint.h>
#include <algorithm>
#include <vector>

// Game

static constexpr auto NUM_COLUMNS           = 4;
static constexpr auto NUM_ROWS              = 3;
static constexpr float CHANCE_FORWARD       = 0.8f;
static constexpr float CHANCE_DEVIATE_LEFT  = 0.1f;
static constexpr float CHANCE_DEVIATE_RIGHT = 0.1f;
static constexpr auto MAX_LOOP              = 10000;

static const float s_rewards[NUM_COLUMNS][NUM_ROWS] = {
    {-0.04f, -0.04f, -0.04f},
    {-0.04f, 0.0f, -0.04f},
    {-0.04f, -0.04f, -0.04f},
    {1.0f, -1.0f, -0.04f}};

static float s_adpRewards[NUM_COLUMNS][NUM_ROWS] = {
    {-0.04f, -0.04f, -0.04f},
    {-0.04f, 0.0f, -0.04f},
    {-0.04f, -0.04f, -0.04f},
    {1.0f, -1.0f, -0.04f}};

static bool s_state[NUM_COLUMNS][NUM_ROWS] = {
    {true, true, true},
    {true, false, true},
    {true, true, true},
    {true, true, true}};

static bool s_terminal[NUM_COLUMNS][NUM_ROWS] = {
    {false, false, false},
    {false, false, false},
    {false, false, false},
    {true, true, false}};

struct UtilityAverage
{
  float sumOfSamples;
  int numSamples;
};

static float s_utilities[NUM_COLUMNS][NUM_ROWS];
static UtilityAverage s_utilityAverage[NUM_COLUMNS][NUM_ROWS];

static float s_sumOfRewards;

struct Pos
{
  int x;
  int y;
};

enum class EDir
{
  Up,
  Down,
  Left,
  Right
};

static EDir s_policy[NUM_COLUMNS][NUM_ROWS];
static Pos s_currentPos = {0, 2};

static EDir DeviateLeft(EDir dir)
{
  switch (dir)
  {
  case EDir::Up:
    return EDir::Left;
  case EDir::Down:
    return EDir::Right;
  case EDir::Left:
    return EDir::Down;
  case EDir::Right:
    return EDir::Up;
  default:
    return EDir::Up;
  }
}

static EDir DeviateRight(EDir dir)
{
  switch (dir)
  {
  case EDir::Up:
    return EDir::Right;
  case EDir::Down:
    return EDir::Left;
  case EDir::Left:
    return EDir::Up;
  case EDir::Right:
    return EDir::Down;
  default:
    return EDir::Up;
  }
}

static const char* GetDirText(EDir dir)
{
  switch (dir)
  {
  case EDir::Up:
    return ICON_KI_ARROW_TOP;
  case EDir::Down:
    return ICON_KI_ARROW_BOTTOM;
  case EDir::Left:
    return ICON_KI_ARROW_LEFT;
  case EDir::Right:
    return ICON_KI_ARROW_RIGHT;
  default:
    return "";
  }
}

static float s_roll;

static Pos GetMove(Pos pos, EDir dir)
{
  Pos d = {};

  switch (dir)
  {
  case EDir::Up:
    d.y = -1;
    break;
  case EDir::Down:
    d.y = 1;
    break;
  case EDir::Left:
    d.x = -1;
    break;
  case EDir::Right:
    d.x = 1;
    break;
  default:
    break;
  }

  Pos newPos;
  newPos.x = pos.x + d.x;
  newPos.y = pos.y + d.y;

  // Clamp
  newPos.x = std::min(NUM_COLUMNS - 1, std::max(0, newPos.x));
  newPos.y = std::min(NUM_ROWS - 1, std::max(0, newPos.y));

  // Obstacles
  if (!s_state[newPos.x][newPos.y])
  {
    memcpy(&newPos, &pos, sizeof(newPos));
  }

  return newPos;
}

static float CalculateUtility(const float u[NUM_COLUMNS][NUM_ROWS], Pos cur, EDir* out_dir = nullptr)
{
  // Define utility using Bellman equation
  Pos posUp    = GetMove(cur, EDir::Up);
  Pos posDown  = GetMove(cur, EDir::Down);
  Pos posLeft  = GetMove(cur, EDir::Left);
  Pos posRight = GetMove(cur, EDir::Right);

  float up    = CHANCE_FORWARD * u[posUp.x][posUp.y] + CHANCE_DEVIATE_LEFT * u[posLeft.x][posLeft.y] + CHANCE_DEVIATE_RIGHT * u[posRight.x][posRight.y];
  float down  = CHANCE_FORWARD * u[posDown.x][posDown.y] + CHANCE_DEVIATE_LEFT * u[posRight.x][posRight.y] + CHANCE_DEVIATE_RIGHT * u[posLeft.x][posLeft.y];
  float left  = CHANCE_FORWARD * u[posLeft.x][posLeft.y] + CHANCE_DEVIATE_LEFT * u[posDown.x][posDown.y] + CHANCE_DEVIATE_RIGHT * u[posUp.x][posUp.y];
  float right = CHANCE_FORWARD * u[posRight.x][posRight.y] + CHANCE_DEVIATE_LEFT * u[posUp.x][posUp.y] + CHANCE_DEVIATE_RIGHT * u[posDown.x][posDown.y];

  //return std::max(max, std::max(up, std::max(down, std::max(left, right))));

  float r = right;
  if (out_dir) *out_dir = EDir::Right;
  if (left > r)
  {
    r = left;
    if (out_dir) *out_dir = EDir::Left;
  }
  if (down > r)
  {
    r = down;
    if (out_dir) *out_dir = EDir::Down;
  }
  if (up > r)
  {
    r = up;
    if (out_dir) *out_dir = EDir::Up;
  }

  return r;
}

static float CalculateUtilityPolicy(float u[NUM_COLUMNS][NUM_ROWS], Pos cur, EDir dir)
{
  // Define utility using Bellman equation
  Pos posForward      = GetMove(cur, dir);
  Pos posDeviateLeft  = GetMove(cur, DeviateLeft(dir));
  Pos posDeviateRight = GetMove(cur, DeviateRight(dir));

  float utility = CHANCE_FORWARD * u[posForward.x][posForward.y] + CHANCE_DEVIATE_LEFT * u[posDeviateLeft.x][posDeviateLeft.y] + CHANCE_DEVIATE_RIGHT * u[posDeviateRight.x][posDeviateRight.y];

  return utility;
}

// Evaluate the utilities for the given policy
static void EvaluatePolicy(EDir policy[NUM_COLUMNS][NUM_ROWS], float utilities[NUM_COLUMNS][NUM_ROWS])
{
  for (int i = 0; i < NUM_ROWS; i++)
  {
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      if (s_state[j][i] && !s_terminal[j][i])
      {
        // Do Bellman update
        utilities[j][i] = CalculateUtilityPolicy(utilities, Pos{j, i}, policy[j][i]);
      }
    }
  }
}

// Randomize policy vector
static void RandomizePolicy()
{
  for (int i = 0; i < NUM_ROWS; i++)
  {
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      if (s_state[j][i] && !s_terminal[j][i])
        switch (rng::Int() % 4)
        {
        case 0:
          s_policy[j][i] = EDir::Up;
          break;
        case 1:
          s_policy[j][i] = EDir::Down;
          break;
        case 2:
          s_policy[j][i] = EDir::Left;
          break;
        case 3:
          s_policy[j][i] = EDir::Right;
          break;
        default:
          break;
        }
    }
  }
}

static void PolicyIteration()
{
  float u[NUM_COLUMNS][NUM_ROWS] = {};
  const float gamma              = 1.0f;
  int iterations                 = 0;

  u[3][0] = 1.0f;
  u[3][1] = -1.0f;

  RandomizePolicy();

  bool changed;
  do
  {
    ++iterations;
    EvaluatePolicy(s_policy, u); // Evaluate utilities using current policy
    changed = false;

    for (int i = 0; i < NUM_ROWS; i++)
    {
      for (int j = 0; j < NUM_COLUMNS; j++)
      {
        if (s_state[j][i] && !s_terminal[j][i])
        {
          EDir dir      = EDir::Up;
          float utility = s_rewards[j][i] + gamma * CalculateUtility(u, Pos{j, i}, &dir);
          if (utility > u[j][i])
          {
            s_policy[j][i] = dir;
            changed        = true;
          }
        }
      }
    }
  } while (changed);

  printf("%d iterations\n", iterations);

  // pi is now the optimal policy
  memcpy(s_utilities, u, sizeof(s_utilities));
}

static void ValueIteration()
{
  float delta                          = 0.0f;
  float u[NUM_COLUMNS][NUM_ROWS]       = {};
  float u_prime[NUM_COLUMNS][NUM_ROWS] = {};
  const float gamma                    = 1.0f;
  const float error                    = 0.001f;
  EDir pi[NUM_COLUMNS][NUM_ROWS]       = {};
  int iterations                       = 0;

  u_prime[3][0] = 1.0f;
  u_prime[3][1] = -1.0f;

  do
  {
    ++iterations;
    memcpy(u, u_prime, sizeof(u));
    delta = 0.0f;
    for (int i = 0; i < NUM_ROWS; i++)
    {
      for (int j = 0; j < NUM_COLUMNS; j++)
      {
        if (s_state[j][i] && !s_terminal[j][i])
        {
          // Do Bellman update
          EDir dir;
          u_prime[j][i] = s_rewards[j][i] + gamma * CalculateUtility(u, Pos{j, i}, &dir);
          pi[j][i]      = dir;

          // Select max
          float abs = std::abs(u_prime[j][i] - u[j][i]);
          if (abs > delta)
          {
            delta = abs;
          }
        }
      }
    }
  } while (delta > 0.0f && delta >= error * (1.0f - gamma) / gamma);

  printf("%d iterations\n", iterations);

  memcpy(s_policy, pi, sizeof(s_policy));
  memcpy(s_utilities, u, sizeof(u));
}

static Pos TryMove(Pos current, EDir dir)
{
  Pos d;
  s_roll           = rng::Float();
  bool deviate     = (s_roll > CHANCE_FORWARD);
  bool deviateLeft = (s_roll > CHANCE_FORWARD + CHANCE_DEVIATE_LEFT);
  switch (dir)
  {
  case EDir::Up:
    d.x = deviate ? deviateLeft ? -1 : 1 : 0;
    d.y = deviate ? 0 : -1;
    break;
  case EDir::Down:
    d.x = deviate ? deviateLeft ? 1 : -1 : 0;
    d.y = deviate ? 0 : 1;
    break;
  case EDir::Left:
    d.x = deviate ? 0 : -1;
    d.y = deviate ? deviateLeft ? 1 : -1 : 0;
    break;
  case EDir::Right:
    d.x = deviate ? 0 : 1;
    d.y = deviate ? deviateLeft ? -1 : 1 : 0;
    break;
  default:
    d = {};
    break;
  }

  // Move
  Pos newPos;
  newPos.x = current.x + d.x;
  newPos.y = current.y + d.y;

  // Clamp
  newPos.x = std::min(NUM_COLUMNS - 1, std::max(0, newPos.x));
  newPos.y = std::min(NUM_ROWS - 1, std::max(0, newPos.y));

  // Obstacles
  if (!s_state[newPos.x][newPos.y])
  {
    memcpy(&newPos, &current, sizeof(newPos));
  }

  s_sumOfRewards += s_rewards[newPos.x][newPos.y];

  return newPos;
}

static void ResetAgent()
{
  s_sumOfRewards = 0;
  s_currentPos   = {0, 2};
}

static void StepTrial()
{
  s_currentPos = TryMove(s_currentPos, s_policy[s_currentPos.x][s_currentPos.y]);
}

// With a given policy, do a trial run
static void DoTrial()
{
  s_currentPos = {0, 2};
  int loop     = 0;
  while (++loop < MAX_LOOP && !((s_currentPos.x == 3 && s_currentPos.y == 0) || (s_currentPos.x == 3 && s_currentPos.y == 1)))
  {
    StepTrial();
  }
  printf("%d iterations\n", loop);
}

static void DirectUtilityEstimation()
{
  s_currentPos = {0, 2};

  std::vector<Pos> nodes;
  nodes.push_back(s_currentPos);

  int loop = 0;
  while (++loop < MAX_LOOP && !((s_currentPos.x == 3 && s_currentPos.y == 0) || (s_currentPos.x == 3 && s_currentPos.y == 1)))
  {
    StepTrial();
    nodes.push_back(s_currentPos);
  }
  printf("%d iterations\n", loop);

  if (loop <= MAX_LOOP)
  {
    // Create utility sample
    for (int i = 0; i < nodes.size(); i++)
    {
      const auto& n = nodes[i];
      float sample  = s_rewards[n.x][n.y];
      for (int j = i + 1; j < nodes.size(); j++)
      {
        const auto& o = nodes[j];
        sample += s_rewards[o.x][o.y];
      }
      s_utilityAverage[n.x][n.y].sumOfSamples += sample;
      s_utilityAverage[n.x][n.y].numSamples++;
    }
  }
}

static void ResetEstimation()
{
  memset(s_utilityAverage, 0, sizeof(s_utilityAverage));
}

// Passive Adaptive Dynamic Programming
static int s_stateActionTuples[NUM_COLUMNS][NUM_ROWS][4];
static int s_stateActionStateTuples[NUM_COLUMNS][NUM_ROWS][4][NUM_COLUMNS][NUM_ROWS];    // Oh fuck
static float s_transitionProbabilities[NUM_COLUMNS][NUM_ROWS][4][NUM_COLUMNS][NUM_ROWS]; // Oh fuck
static const Pos* s_previousState;
static EDir s_previousAction; // = EDir::Null;

static EDir PassiveAdpAgent(const Pos* currentState, float rewardSignal)
{
  if (true) // s prime is new
  {
    s_utilities[currentState->x][currentState->y]  = rewardSignal;
    s_adpRewards[currentState->x][currentState->y] = rewardSignal;
  }

  if (s_previousState)
  {
    int& n_sa   = s_stateActionTuples[s_previousState->x][s_previousState->y][(int)s_previousAction];
    auto& n_sas = s_stateActionStateTuples[s_previousState->x][s_previousState->y][(int)s_previousAction];
    n_sa++;
    n_sas[currentState->x][currentState->y]++;
    for (int i = 0; i < NUM_ROWS; i++)
    {
      for (int j = 0; j < NUM_COLUMNS; j++)
      {
        int freq = n_sas[j][i];
        if (freq != 0)
        {
          s_transitionProbabilities[s_previousState->x][s_previousState->y][(int)s_previousAction][j][i] = (float)freq / (float)n_sa;
        }
      }
    }
  }

  EvaluatePolicy(s_policy, s_utilities); // Evaluate utilities using current policy

  if (s_terminal[currentState->x][currentState->y])
  {
    s_previousState = nullptr;
    //s_previousAction = EDir::Null;
  }
  else
  {
    s_previousState  = currentState;
    s_previousAction = s_policy[currentState->x][currentState->y];
  }

  return s_previousAction;
}

void DecisionProcessWindowInit()
{
  
}

void DecisionProcessWindow(bool* show)
{
  ImGui::Begin("DecisionProcess", show);

  if (ImGui::Button("Value Iteration"))
  {
    ValueIteration();
  }
  ImGui::SameLine();
  if (ImGui::Button("Policy Iteration"))
  {
    PolicyIteration();
  }
  ImGui::SameLine();
  if (ImGui::Button("Randomize Policy"))
  {
    RandomizePolicy();
  }

  if (ImGui::Button("Do Trial"))
  {
    ResetAgent();
    DoTrial();
  }
  ImGui::SameLine();
  if (ImGui::Button("Step Trial"))
  {
    StepTrial();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset Agent"))
  {
    ResetAgent();
  }

  if (ImGui::Button("Direct Utility Estimation"))
  {
    ResetAgent();
    DirectUtilityEstimation();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset Estimation"))
  {
    ResetEstimation();
  }

  if (ImGui::Button("PassiveAdpAgent"))
  {
    PassiveAdpAgent(&s_currentPos, s_rewards[s_currentPos.x][s_currentPos.y]);
  }

  static int overlay = 2;
  ImGui::RadioButton("Rewards", &overlay, 0);
  ImGui::RadioButton("Utility", &overlay, 1);
  ImGui::RadioButton("Policy", &overlay, 2);
  ImGui::RadioButton("Estimation", &overlay, 3);

  ImGui::Columns(5);
  for (int i = 0; i < NUM_ROWS; i++)
  {
    ImGui::Text("%d", NUM_ROWS - i);
    ImGui::NextColumn();
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      if (overlay == 0 && s_state[j][i])
      {
        ImGui::Text("%.2f", s_rewards[j][i]);
      }
      else if (overlay == 1 && s_state[j][i])
      {
        ImGui::Text("%.3f", s_utilities[j][i]);
      }
      else if (overlay == 2 && s_state[j][i] && !s_terminal[j][i])
      {
        ImGui::Text(GetDirText(s_policy[j][i]));
      }
      else if (overlay == 3 && s_state[j][i] && !s_terminal[j][i])
      {
        ImGui::Text("%.3f", s_utilityAverage[j][i].numSamples == 0 ? 0.0f : s_utilityAverage[j][i].sumOfSamples / s_utilityAverage[j][i].numSamples);
        ImGui::Text("%d samples", s_utilityAverage[j][i].numSamples);
      }

      if (s_currentPos.x == j && s_currentPos.y == i)
      {
        ImGui::Text(ICON_KI_FIGURE);
      }
      else if (!s_state[j][i])
      {
        ImGui::Text("<wall>");
      }
      else
      {
        ImGui::Text("");
      }
      ImGui::NextColumn();
    }
    ImGui::Separator();
  }
  ImGui::NextColumn();
  for (int i = 0; i < NUM_COLUMNS; i++)
  {
    ImGui::Text("%d", i + 1);
    ImGui::NextColumn();
  }
  ImGui::Columns(1);

  // Movement
  if (ImGui::Button(ICON_KI_ARROW_LEFT))
  {
    s_currentPos = TryMove(s_currentPos, EDir::Left);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_KI_ARROW_BOTTOM))
  {
    s_currentPos = TryMove(s_currentPos, EDir::Down);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_KI_ARROW_TOP))
  {
    s_currentPos = TryMove(s_currentPos, EDir::Up);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_KI_ARROW_RIGHT))
  {
    s_currentPos = TryMove(s_currentPos, EDir::Right);
  }

  ImGui::InputFloat("Last RNG Roll", &s_roll);
  ImGui::InputFloat("Total reward", &s_sumOfRewards);

  ImGui::End();
}

void DecisionProcessWindowDestroy()
{
}