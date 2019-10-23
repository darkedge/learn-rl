#include "Menu.h"
#include "imgui.h"
#include "TicTacToeWindow.h"

static bool s_TicTacToeWindow = true;

void MenuInit()
{
  TicTacToeWindowInit();
}

void Menu()
{
  ImGui::BeginMainMenuBar();
  if (ImGui::BeginMenu("Main"))
  {
    if (ImGui::MenuItem("TicTacToe")) s_TicTacToeWindow = true;
    ImGui::EndMenu();
  }
  ImGui::EndMainMenuBar();

  if (s_TicTacToeWindow) TicTacToeWindow(&s_TicTacToeWindow);
}

void MenuDestroy()
{
  TicTacToeWindowDestroy();
}