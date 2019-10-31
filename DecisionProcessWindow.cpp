#include "imgui.h"
#include <stdint.h>
#include <algorithm>
#include <ctime>
#include "IconsKenney.h"

// RNG

static inline uint32_t rotl(const uint32_t x, int k)
{
  return (x << k) | (x >> (32 - k));
}

// state
static uint32_t s[4];

static uint32_t next(void)
{
  const uint32_t result = s[0] + s[3];

  const uint32_t t = s[1] << 9;

  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];

  s[2] ^= t;

  s[3] = rotl(s[3], 11);

  return result;
}

static inline double to_double(uint64_t x)
{
  union {
    uint64_t i;
    double d;
  } u;

  u.i = UINT64_C(0x3FF) << 52 | x >> 12;
  return u.d - 1.0;
}

static inline float to_float(uint32_t x)
{
  union {
    uint32_t i;
    float d;
  } u;

  u.i = UINT32_C(0x7F) << 23 | x >> 9;
  return u.d - 1.0f;
}

// Game

static constexpr auto NUM_COLUMNS           = 4;
static constexpr auto NUM_ROWS              = 3;
static constexpr float CHANCE_FORWARD       = 0.8f;
static constexpr float CHANCE_DEVIATE_LEFT  = 0.1f;
static constexpr float CHANCE_DEVIATE_RIGHT = 0.1f;

static const float s_rewards[NUM_COLUMNS][NUM_ROWS] = {
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

static float s_utilities[NUM_COLUMNS][NUM_ROWS];

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
  float max  = -FLT_MAX;

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
  Pos posForward = GetMove(cur, dir);
  Pos posDeviateLeft = GetMove(cur, DeviateLeft(dir));
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
        utilities[j][i] = CalculateUtilityPolicy(utilities, Pos{ j, i }, policy[j][i]);
      }
    }
  }
}

static void PolicyIteration()
{
  float u[NUM_COLUMNS][NUM_ROWS] = {};
  EDir pi[NUM_COLUMNS][NUM_ROWS] = {};
  const float gamma = 1.0f;
  int iterations = 0;

  u[3][0] = 1.0f;
  u[3][1] = -1.0f;

  // Randomize policy vector
  for (int i = 0; i < NUM_ROWS; i++)
  {
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      if (s_state[j][i] && !s_terminal[j][i])
      switch (next() % 4)
      {
      case 0:
        pi[j][i] = EDir::Up;
        break;
      case 1:
        pi[j][i] = EDir::Down;
        break;
      case 2:
        pi[j][i] = EDir::Left;
        break;
      case 3:
        pi[j][i] = EDir::Right;
        break;
      default:
        break;
      }
    }
  }

  bool changed;
  do
  {
    ++iterations;
    EvaluatePolicy(pi, u); // Evaluate utilities using current policy
    changed = false;

    for (int i = 0; i < NUM_ROWS; i++)
    {
      for (int j = 0; j < NUM_COLUMNS; j++)
      {
        if (s_state[j][i] && !s_terminal[j][i])
        {
          EDir dir = EDir::Up;
          float utility = s_rewards[j][i] + gamma * CalculateUtility(u, Pos{ j, i }, &dir);
          if (utility > u[j][i])
          {
            pi[j][i] = dir;
            changed = true;
          }
        }
      }
    }
  } while (changed);

  printf("%d iterations\n", iterations);

  // pi is now the optimal policy
  memcpy(s_policy, pi, sizeof(s_policy));
  memcpy(s_utilities, u, sizeof(s_utilities));
}

static void ValueIteration()
{
  float delta                          = 0.0f;
  float u[NUM_COLUMNS][NUM_ROWS]       = {};
  float u_prime[NUM_COLUMNS][NUM_ROWS] = {};
  const float gamma                    = 1.0f;
  const float error                    = 0.001f;
  EDir pi[NUM_COLUMNS][NUM_ROWS] = {};
  int iterations = 0;

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
          pi[j][i] = dir;

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
  s_roll           = to_float(next());
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

static Pos s_currentPos = {0, 2};

void DecisionProcessWindowInit()
{
  // Init RNG using splitmix64
  std::srand(std::time(nullptr)); // use current time as seed for random generator
  uint64_t seed = ((uint64_t)std::rand() << 32) | std::rand();

  auto nextgen = [&]() {
    uint64_t z = (seed += 0x9e3779b97f4a7c15);
    z          = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z          = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
  };

  uint64_t* ptr = (uint64_t*)s;
  ptr[0]        = nextgen();
  ptr[1]        = nextgen();

  //ValueIteration();
  //PolicyIteration();
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

  static int overlay;
  ImGui::RadioButton("Rewards", &overlay, 0);
  ImGui::RadioButton("Utility", &overlay, 1);
  ImGui::RadioButton("Policy", &overlay, 2);

  ImGui::Columns(5);
  for (int i = 0; i < NUM_ROWS; i++)
  {
    ImGui::Text("%d", NUM_ROWS - i);
    ImGui::NextColumn();
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      if (overlay == 0 && s_state[j][i])
      {
        char buf[20];
        sprintf(buf, "%.2f", s_rewards[j][i]);
        ImGui::Text(buf);
      }
      else if (overlay == 1 && s_state[j][i])
      {
        char buf[20];
        sprintf(buf, "%.3f", s_utilities[j][i]);
        ImGui::Text(buf);
      }
      else if (overlay == 2 && s_state[j][i] && !s_terminal[j][i])
      {
        ImGui::Text(GetDirText(s_policy[j][i]));
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

  ImGui::InputFloat("Roll", &s_roll);
  ImGui::InputFloat("Total reward", &s_sumOfRewards);

  ImGui::End();
}

void DecisionProcessWindowDestroy()
{
}