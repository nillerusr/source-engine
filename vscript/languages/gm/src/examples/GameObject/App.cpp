#include <windows.h>  
#include <mmsystem.h> // multimedia timer (may need winmm.lib)
#include "App.h"
#include "ScriptSys.h"
#include "GameObj.h"
#include "ScriptObj.h"
#include "gmCall.h"
#include "InputKBWin32.h"

App* App::s_instancePtr = NULL;


App::App()
{
  s_instancePtr = this;
}



App::~App()
{
  s_instancePtr = NULL;
}


bool App::Init()
{
  InputKBWin32::Get().Init();

  // Init console
  InitConsole();

  // Init script system for game objects
  ScriptSys::Init();

  // Register app bindings
  RegisterScriptBindings();

  // Init timer
  m_deltaTime = 0;
  m_lastTime = timeGetTime();

  // Compile and run script
  ScriptSys::Get()->ExecuteFile("TestGameObj.gm");

//TEST REMOVE
//  ClearScreen();
//  SetColor(COLOR_YELLOW, COLOR_RED);
//  SetCursor(10,10);
//  Print("Hello");
//  PrintAt(10,10,"Hello");


  return true;
}


void App::Destroy()
{
  ScriptSys::Destroy();
}


bool App::Update()
{
  int curTime = timeGetTime();
  m_deltaTime = curTime - m_lastTime;
  m_lastTime = curTime;

  // Update input
  InputKBWin32::Get().Update();
      
  // Execute some script
  ScriptSys::Get()->Execute(m_deltaTime);

  if(InputKBWin32::Get().IsKeyPressed('A'))
  {
    TestA();
  }
  else if(InputKBWin32::Get().IsKeyPressed('B'))
  {
    TestB();
  }
  else if(InputKBWin32::Get().IsKeyPressed('C'))
  {
    TestC();
  }

  if(InputKBWin32::Get().IsKeyPressed(VK_ESCAPE))
  {
    return false;
  }

  return true;
}


void App::InitConsole()
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  m_console = GetStdHandle(STD_OUTPUT_HANDLE);
  
  GetConsoleScreenBufferInfo(m_console, &csbi);

  DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

  m_screenSizeX = csbi.dwSize.X;
  m_screenSizeY = csbi.dwSize.Y;
}


void App::TestA()
{
  // Test create a game object and set member in script
  GameObj* newObj = new GameObj;

  printf("new GameObj = %x\n", newObj);
  printf("GameObj.m_scriptObj = %x\n", newObj->GetScriptObj());

  newObj->GetScriptObj()->SetMemberString("m_name", "MangoBoy");
  newObj->GetScriptObj()->ExecuteGlobalFunctionOnThis("WhatsMyName");

  delete newObj;

  gmCall call;
  if(call.BeginGlobalFunction(ScriptSys::Get()->GetMachine(), "RunGlobalObject"))
  {
    call.End();
  }
}


void App::TestB()
{
  // Test call script function that keeps running for a bit
  gmCall call;
  if(call.BeginGlobalFunction(ScriptSys::Get()->GetMachine(), "ThreadYieldTest"))
  {
    call.End();
  }
}


void App::TestC()
{
  // Quick test of collect garbage and cpp owned objects

  GameObj* newObj = new GameObj;

  printf("new GameObj = %x\n", newObj);
  printf("GameObj.m_scriptObj = %x\n", newObj->GetScriptObj());

  newObj->GetScriptObj()->SetMemberString("m_name", "PotatoHead");
  //newObj->GetScriptObj()->ExecuteGlobalFunctionOnThis("WhatsMyName");

  ScriptSys::Get()->GetMachine()->CollectGarbage(true);

  delete newObj;
}


void App::SetCursor(int a_x, int a_y)
{
  ClipScreenCoordsi(a_x, a_y);
  
  COORD point;

  point.X = (short) a_x; 
  point.Y = (short) a_y;
  
  SetConsoleCursorPosition(m_console, point);
}


void App::ClearScreen()
{
  COORD coordScreen = { 0, 0 };
  DWORD cCharsWritten;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD dwConSize;
  GetConsoleScreenBufferInfo(m_console, &csbi);
  dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
  FillConsoleOutputCharacter(m_console, TEXT(' '), dwConSize, coordScreen, &cCharsWritten);
  GetConsoleScreenBufferInfo(m_console, &csbi);
  FillConsoleOutputAttribute(m_console, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
  SetConsoleCursorPosition(m_console, coordScreen);
}


void App::Print(const char* a_string)
{
  printf("%s", a_string);
}


void App::PrintAt(int a_x, int a_y, const char* a_string)
{
  SetCursor(a_x, a_y);
  Print(a_string);
}


int App::GetAttribFromColIndex(int a_colorIndex, bool a_isForeground)
{
  // WARNING These struct must match the color enums
  static int foreColors[COLOR_MAX]=
  {
    0, //COLOR_BLACK
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, //COLOR_WHITE,
    FOREGROUND_RED, //COLOR_RED
    FOREGROUND_GREEN, //COLOR_GREEN,
    FOREGROUND_BLUE, //COLOR_BLUE,
    FOREGROUND_RED | FOREGROUND_BLUE, //COLOR_MAGENTA,
    FOREGROUND_GREEN | FOREGROUND_BLUE, //COLOR_CYAN,
    FOREGROUND_RED | FOREGROUND_GREEN, //COLOR_YELLOW,
  };
  
  static int backColors[COLOR_MAX]=
  {
    0, //COLOR_BLACK
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE, //COLOR_WHITE,
    BACKGROUND_RED, //COLOR_RED
    BACKGROUND_GREEN, //COLOR_GREEN,
    BACKGROUND_BLUE, //COLOR_BLUE,
    BACKGROUND_RED | BACKGROUND_BLUE, //COLOR_MAGENTA,
    BACKGROUND_GREEN | BACKGROUND_BLUE, //COLOR_CYAN,
    BACKGROUND_RED | BACKGROUND_GREEN, //COLOR_YELLOW,
  };


  if( (a_colorIndex < COLOR_MIN) && (a_colorIndex >= COLOR_MAX) )
  {
    if(a_isForeground)
    {
      return foreColors[COLOR_WHITE];
    }
    else
    {
      return backColors[COLOR_WHITE];
    }
  }

  if(a_isForeground)
  {
    return foreColors[a_colorIndex];
  }
  else
  {
    return backColors[a_colorIndex];
  }
}


void App::SetColor(int a_foreColorIndex, int a_backColorIndex)
{
  int foreCol = GetAttribFromColIndex(a_foreColorIndex, true);
  int backCol = GetAttribFromColIndex(a_backColorIndex, false);

  int param = foreCol | backCol;

  SetConsoleTextAttribute(m_console, (short) param);  
}


bool App::ClipScreenCoordsf(float& a_posX, float& a_posY)
{
  bool wasClipped = false;

  if(a_posX < 0.0f)
  {
    a_posX = 0.0f;
    wasClipped = true;
  }
  else if(a_posX >= (float)App::Get()->GetScreenSizeX())
  {
    a_posX = (float)(App::Get()->GetScreenSizeX() - 1);
    wasClipped = true;
  }

  if(a_posY < 0.0f)
  {
    a_posY = 0.0f;
    wasClipped = true;
  }
  else if(a_posY >= (float)App::Get()->GetScreenSizeY())
  {
    a_posY = (float)(App::Get()->GetScreenSizeY() - 1);
    wasClipped = true;
  }

  return wasClipped;
}


bool App::ClipScreenCoordsi(int& a_posX, int& a_posY)
{
  bool wasClipped = false;

  if(a_posX < 0)
  {
    a_posX = 0;
    wasClipped = true;
  }
  else if(a_posX >= App::Get()->GetScreenSizeX())
  {
    a_posX = App::Get()->GetScreenSizeX() - 1;
    wasClipped = true;
  }

  if(a_posY < 0)
  {
    a_posY = 0;
    wasClipped = true;
  }
  else if(a_posY >= App::Get()->GetScreenSizeY())
  {
    a_posY = App::Get()->GetScreenSizeY() - 1;
    wasClipped = true;
  }

  return wasClipped;
}


//////////////////////////////////////////////////
// Script bindings
//////////////////////////////////////////////////


int GM_CDECL App::Console_Print(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(a_string, 0);

  App::Get()->Print(a_string);

  return GM_OK;
}


int GM_CDECL App::Console_SetCursor(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(a_curX, 0);
  GM_CHECK_INT_PARAM(a_curY, 1);

  App::Get()->ClipScreenCoordsi(a_curX, a_curY);
  App::Get()->SetCursor(a_curX, a_curY);

  return GM_OK;
}


int GM_CDECL App::Console_SetColor(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(a_foreCol, 0);
  GM_CHECK_INT_PARAM(a_backCol, 1);

  App::Get()->SetColor(a_foreCol, a_backCol);

  return GM_OK;
}


int GM_CDECL App::Input_KeyPressed(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(a_vKey, 0);

  if(InputKBWin32::Get().IsKeyPressed(a_vKey))
  {
    a_thread->PushInt(1);
  }
  else
  {
    a_thread->PushInt(0);
  }

  return GM_OK;
}


int GM_CDECL App::Input_KeyDown(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(a_vKey, 0);

  if(InputKBWin32::Get().IsKeyDown(a_vKey))
  {
    a_thread->PushInt(1);
  }
  else
  {
    a_thread->PushInt(0);
  }

  return GM_OK;
}


void App::RegisterScriptBindings()
{
  static gmFunctionEntry ConsoleLib[] = 
  { 
    /*gm
      \lib Console
      \brief Console Library
    */
    /*gm
      \function Print
      \brief Print string at current cursor position
      \param string a_string
    */
    {"Print", Console_Print},
    /*gm
      \function SetCursor
      \brief Set cursor position
      \param int a_curX Cursor x position
      \param int a_curY Cursor y position
    */
    {"SetCursor", Console_SetCursor},

    /*gm
      \function SetColor
      \brief Set text color
      \param int a_foreCol Foreground color
      \param int a_backCol Background color
    */
    {"SetColor", Console_SetColor},
  };


  static gmFunctionEntry InputLib[] = 
  { 
    /*gm
      \lib Console
      \brief Console Library
    */
    /*gm
      \function KeyPressed
      \brief Was this key pressed
      \param int a_vKey windows virtual key code (Most match ascii uppercase)
      \return true if key was pressed this frame
    */
    {"KeyPressed", Input_KeyPressed},

    /*gm
      \function KeyDown
      \brief Is this key down
      \param int a_vKey windows virtual key code (Most match ascii uppercase)
      \return true if is down
    */
    {"KeyDown", Input_KeyDown},

  };

  gmMachine* machine = ScriptSys::Get()->GetMachine();

  machine->RegisterLibrary(ConsoleLib, sizeof(ConsoleLib) / sizeof(ConsoleLib[0]), "Console");
  machine->RegisterLibrary(InputLib, sizeof(InputLib) / sizeof(InputLib[0]), "Input");

  // Make some global constants
  machine->GetGlobals()->Set(machine, "COLOR_BLACK", gmVariable(GM_INT, COLOR_BLACK));
  machine->GetGlobals()->Set(machine, "COLOR_WHITE", gmVariable(GM_INT, COLOR_WHITE));
  machine->GetGlobals()->Set(machine, "COLOR_RED", gmVariable(GM_INT, COLOR_RED));
  machine->GetGlobals()->Set(machine, "COLOR_GREEN", gmVariable(GM_INT, COLOR_GREEN));
  machine->GetGlobals()->Set(machine, "COLOR_BLUE", gmVariable(GM_INT, COLOR_BLUE));
  machine->GetGlobals()->Set(machine, "COLOR_MAGENTA", gmVariable(GM_INT, COLOR_MAGENTA));
  machine->GetGlobals()->Set(machine, "COLOR_CYAN", gmVariable(GM_INT, COLOR_CYAN));
  machine->GetGlobals()->Set(machine, "COLOR_YELLOW", gmVariable(GM_INT, COLOR_YELLOW));
}
