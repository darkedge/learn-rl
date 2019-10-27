#include "Menu.h"
#include "imgui.h"
#include "TicTacToeWindow.h"
#include "ConnectFour.h"
#include "DecisionProcessWindow.h"

static bool s_TicTacToeWindow;
static bool s_ConnectFourWindow;
static bool s_DecisionProcessWindow = true;

void MenuInit()
{
  TicTacToeWindowInit();
  ConnectFourWindowInit();
  DecisionProcessWindowInit();
}

void Menu()
{
  ImGui::BeginMainMenuBar();
  if (ImGui::BeginMenu("Main"))
  {
    if (ImGui::MenuItem("TicTacToe")) s_TicTacToeWindow = true;
    if (ImGui::MenuItem("ConnectFour")) s_ConnectFourWindow = true;
    if (ImGui::MenuItem("DecisionProblem")) s_DecisionProcessWindow = true;
    ImGui::EndMenu();
  }
  ImGui::EndMainMenuBar();

  if (s_TicTacToeWindow) TicTacToeWindow(&s_TicTacToeWindow);
  if (s_ConnectFourWindow) ConnectFourWindow(&s_ConnectFourWindow);
  if (s_DecisionProcessWindow) DecisionProcessWindow(&s_DecisionProcessWindow);
}

void MenuDestroy()
{
  TicTacToeWindowDestroy();
  ConnectFourWindowDestroy();
  DecisionProcessWindowDestroy();
}