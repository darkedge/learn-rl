#include "imgui.h"
#include "IconsKenney.h"

enum class Symbol
{
  Empty,
  Cross,
  Nought
};

// 0 1 2
// 3 4 5
// 6 7 8
static Symbol s_field[9];
static Symbol s_currentPlayer = Symbol::Cross;

static Symbol FindWinningSymbol(Symbol pField[9])
{
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
        if (pField[i + j] != s)
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
    s_field[slot] = s_currentPlayer;

    switch (FindWinningSymbol(s_field))
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

void TicTacToeWindowInit()
{
}

void TicTacToeWindow(bool* show)
{
  ImGui::Begin("TicTacToe", show);

  if (ImGui::Button("New Game"))
  {
    s_gameover = false;
    memset(&s_field, 0, sizeof(s_field));
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
      switch (s_field[i * 3 + j])
      {
      case Symbol::Cross:
        ImGui::Text(ICON_KI_BUTTON_X);
        break;
      case Symbol::Nought:
        ImGui::Text(ICON_KI_BUTTON_CIRCLE);
        break;
      case Symbol::Empty:
        if (ImGui::Button("Play"))
        {
          Play(i * 3 + j);
        }
        break;
      }
      ImGui::NextColumn();
      ImGui::PopID();
    }
  }
  ImGui::Columns(1);

  ImGui::End();
}

void TicTacToeWindowDestroy()
{
}