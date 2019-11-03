#include <math.h>
#include "imgui.h"
#include "IconsKenney.h"
#include "Random.h"

static constexpr float MATH_E = 2.71828182845904523536028747135266249775724709369995f;

enum class Symbol
{
  Empty, // 0, 0.5f
  Cross, // 1, 1.0f
  Nought // 2, 0.0f
};

struct State
{
  Symbol cells[9];
};

struct Neuron
{
  float activation;
  float weights[9];
};

static float s_input[9];
static Neuron s_hiddenLayerNeurons[9];
static Neuron s_outputNeurons[9];

// 0 1 2
// 3 4 5
// 6 7 8
static State s_field;
static Symbol s_currentPlayer = Symbol::Cross;

static void RandomizeWeights()
{
  for (auto& n : s_hiddenLayerNeurons)
  {
    for (auto& w : n.weights)
    {
      w = rng::Float() * 2.0f - 1.0f;
    }
  }

  for (auto& n : s_outputNeurons)
  {
    for (auto& w : n.weights)
    {
      w = rng::Float() * 2.0f - 1.0f;
    }
  }
}

static float Sigmoid(float x)
{
  //return 1.0f / (1.0f + powf(MATH_E, -1.0f * f)); // Slow but more accurate (use expf?)

  int i = *(int*)&x;
  i &= 0x7FFFFFFF;
  return 0.5f * x / (1.0f + *(float*)&i) + 0.5f;
}

static Symbol FindWinningSymbol(const State* pState)
{
  const Symbol* pField = pState->cells;
  // Horizontal
  for (int i = 0; i < 3; i++)
  {
    Symbol s = pField[i * 3];
    if (s == Symbol::Empty)
    {
      continue;
    }
    else
    {
      bool match = true;
      for (int j = 1; j < 3; j++)
      {
        if (pField[i * 3 + j] != s)
        {
          match = false;
          break;
        }
      }
      if (match)
      {
        return s;
      }
    }
  }

  // Vertical
  for (int i = 0; i < 3; i++)
  {
    Symbol s = pField[i];
    if (s == Symbol::Empty)
    {
      continue;
    }
    else
    {
      bool match = true;
      for (int j = 1; j < 3; j++)
      {
        if (pField[i + 3 * j] != s)
        {
          match = false;
          break;
        }
      }
      if (match)
      {
        return s;
      }
    }
  }

  // Diagonal 0-4-8
  Symbol s = pField[0];
  if ((pField[4] == s) && (pField[8] == s))
  {
    return s;
  }

  // Diagonal 2-4-6
  s = pField[2];
  if ((pField[4] == s) && (pField[6] == s))
  {
    return s;
  }

  return Symbol::Empty;
}

static bool s_gameover;

static void Play(int slot)
{
  if (!s_gameover)
  {
    s_field.cells[slot] = s_currentPlayer;

    switch (FindWinningSymbol(&s_field))
    {
    case Symbol::Cross:
      s_gameover = true;
      break;
    case Symbol::Nought:
      s_gameover = true;
      break;
    default: // Pass turn
      switch (s_currentPlayer)
      {
      case Symbol::Cross:
        s_currentPlayer = Symbol::Nought;
        break;
      case Symbol::Nought:
        s_currentPlayer = Symbol::Cross;
        break;
      default:
        break;
      }
    }
  }
}

static void DrawNeuralNetwork()
{
  ImGui::Begin("Neural Network");

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float radius    = 15.0f;
  ImVec2 p              = ImGui::GetCursorScreenPos();
  p.x += radius;
  p.y += radius;

  ImGui::Columns(3);
  for (const auto& f : s_input)
  {
    draw_list->AddCircleFilled(p, radius, (ImU32)ImColor(f, f, f), 20);
    p.y += 2.0f * radius + 1;
  }
  ImGui::NextColumn();
  p = ImGui::GetCursorScreenPos();
  p.x += radius;
  p.y += radius;
  for (const auto& n : s_hiddenLayerNeurons)
  {
    draw_list->AddCircleFilled(p, radius, (ImU32)ImColor(n.activation, n.activation, n.activation), 20);
    p.y += 2.0f * radius + 1;
  }
  ImGui::NextColumn();
  p = ImGui::GetCursorScreenPos();
  p.x += radius;
  p.y += radius;
  for (const auto& n : s_outputNeurons)
  {
    draw_list->AddCircleFilled(p, radius, (ImU32)ImColor(n.activation, n.activation, n.activation), 20);
    p.y += 2.0f * radius + 1;
  }
  ImGui::Columns(1);

  ImGui::End();
}

static void UpdateInput()
{
  // Convert playfield to input
  for (int i = 0; i < 9; i++)
  {
    const auto& s = s_field.cells[i];
    s_input[i] = (s == Symbol::Cross ? 1.0f : s == Symbol::Nought ? 0.0f : 0.5f);
  }

  // Hidden layer
  for (auto& n : s_hiddenLayerNeurons)
  {
    float f = 0.0f;
    for (int i = 0; i < 9; i++)
    {
      f += s_input[i] * n.weights[i];
    }
    n.activation = Sigmoid(f);
  }

  // Output layer
  for (auto& n : s_outputNeurons)
  {
    float f = 0.0f;
    for (int i = 0; i < 9; i++)
    {
      f += s_hiddenLayerNeurons[i].activation * n.weights[i];
    }
    n.activation = Sigmoid(f);
  }
}

static void EvaluateNeuralNetwork()
{
  UpdateInput();

  // Select argmax
  int s   = -1;
  float f = -FLT_MAX;
  for (int i = 0; i < 9; i++)
  {
    if ((s_field.cells[i] == Symbol::Empty) && (s_outputNeurons[i].activation > f))
    {
      f = s_outputNeurons[i].activation;
      s = i;
    }
  }

  Play(s);
}

void TicTacToeWindowInit()
{
  RandomizeWeights();
  UpdateInput();
}

void TicTacToeWindow(bool* show)
{
  ImGui::Begin("TicTacToe", show);

  if (ImGui::Button("New Game"))
  {
    s_gameover = false;
    memset(&s_field, 0, sizeof(s_field));
    UpdateInput();
    s_currentPlayer = Symbol::Cross;
  }
  ImGui::SameLine();
  ImGui::Text(s_currentPlayer == Symbol::Cross ? "Cross to play" : "Nought to play");

  ImGui::Columns(3);
  ImGui::Separator();
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      ImGui::PushID(i * 3 + j);
      switch (s_field.cells[i * 3 + j])
      {
      case Symbol::Cross:
        ImGui::Text("X");
        break;
      case Symbol::Nought:
        ImGui::Text("O");
        break;
      case Symbol::Empty:
        if (ImGui::Button("Play"))
        {
          Play(i * 3 + j);
          EvaluateNeuralNetwork();
        }
        break;
      }
      ImGui::NextColumn();
      ImGui::PopID();
    }
    ImGui::Separator();
  }
  ImGui::Columns(1);

  ImGui::End();

  DrawNeuralNetwork();
}

void TicTacToeWindowDestroy()
{
}