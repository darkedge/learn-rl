#include "imgui.h"

#include <stdio.h>

enum class Disc
{
  White,
  Yellow,
  Red
};

Disc s_CurrentPlayer = Disc::Yellow;
static constexpr auto NUM_COLUMNS = 7;
static constexpr auto NUM_ROWS    = 6;

static Disc s_PlayingField[NUM_COLUMNS][NUM_ROWS] = {};

static ImU32
GetColor(Disc disc)
{
  switch (disc)
  {
  case Disc::Yellow:
    return ImColor(0xFF00FFFF);
  case Disc::Red:
    return ImColor(0xFF0000FF);
  case Disc::White:
  default:
    return ImColor(0x33FFFFFF);
  }
}

static void NextTurn()
{
  switch (s_CurrentPlayer)
  {
  case Disc::Yellow:
    s_CurrentPlayer = Disc::Red;
    break;
  case Disc::Red:
  case Disc::White:
  default:
    s_CurrentPlayer = Disc::Yellow;
  }
}

static bool PushColumn(int id)
{
  Disc* rows   = s_PlayingField[id];
  bool success = false;
  for (int i = NUM_ROWS - 1; i >= 0; --i)
  {
    if (rows[i] == Disc::White)
    {
      rows[i] = s_CurrentPlayer;
      success = true;
      break;
    }
  }

  return success;
}

void ConnectFourWindowInit()
{
}

void ConnectFourWindow(bool* show)
{
  ImGui::Begin("ConnectFour", show);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (ImGui::Button("2 player"))
  {
    memset(s_PlayingField, 0, sizeof(s_PlayingField));
    s_CurrentPlayer = Disc::Yellow;
  }

  ImGui::SameLine();
  if (ImGui::Button("1p (yellow)"))
  {
    memset(s_PlayingField, 0, sizeof(s_PlayingField));
  }

  ImGui::SameLine();
  if (ImGui::Button("1p (red)"))
  {
    memset(s_PlayingField, 0, sizeof(s_PlayingField));
	  // TODO
  }

  const float radius = 28.0f;
  ImVec2 p           = ImGui::GetCursorScreenPos();
  p.x += radius;
  p.y += radius;
  for (int i = 0; i < NUM_ROWS; i++)
  {
    for (int j = 0; j < NUM_COLUMNS; j++)
    {
      draw_list->AddCircleFilled(p, radius, GetColor(s_PlayingField[j][i]), 20);
      p.x += 2.0f * radius + 1;
    }
    p.x = ImGui::GetCursorScreenPos().x + radius;
    p.y += 2.0f * radius + 1;
  }
  ImGui::Dummy(ImVec2(0.0f, (2 * NUM_ROWS) * radius + NUM_ROWS));
  for (int i = 0; i < NUM_COLUMNS; i++)
  {
    ImGui::PushID(i);
    char label[10] = {};
    sprintf(label, "Play %d", i);
    if (ImGui::Button(label))
    {
      if (PushColumn(i))
      {
        NextTurn();
      }
    }
    ImGui::SameLine();
    ImGui::PopID();
  }

  ImGui::End();
}

void ConnectFourWindowDestroy()
{
}