//
// Example application
//
#include <Windows.h>
#include "gmThread.h"

class App
{
public:
   
  enum
  {
    COLOR_MIN = 0,
    
    COLOR_BLACK = COLOR_MIN,
    COLOR_WHITE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_YELLOW,
    
    
    COLOR_MAX
  };

  static App* Get()                               { return s_instancePtr; }

  App();
  ~App();
  bool Init();
  void Destroy();
  bool Update();

  void SetCursor(int a_x, int a_y);
  void ClearScreen();
  void Print(const char* a_string);
  void PrintAt(int a_x, int a_y, const char* a_string);
  void SetColor(int a_foreColorIndex, int a_backColorIndex);
  int GetScreenSizeX()                            { return m_screenSizeX; }
  int GetScreenSizeY()                            { return m_screenSizeY; }

  bool ClipScreenCoordsf(float& a_posX, float& a_posY);
  bool ClipScreenCoordsi(int& a_posX, int& a_posY);

protected:

  int GetAttribFromColIndex(int a_colorIndex, bool a_isForeground);
  void InitConsole();
  void RegisterScriptBindings();

  static int GM_CDECL Console_Print(gmThread* a_thread);
  static int GM_CDECL Console_SetCursor(gmThread* a_thread);
  static int GM_CDECL Console_SetColor(gmThread* a_thread);

  static int GM_CDECL Input_KeyPressed(gmThread* a_thread);
  static int GM_CDECL Input_KeyDown(gmThread* a_thread);

  void TestA();
  void TestB();
  void TestC();

  int m_deltaTime;
  int m_lastTime;

  int m_screenSizeX;
  int m_screenSizeY;
  HANDLE m_console;

  static App* s_instancePtr;                      ///< Ptr to instance of this class when created
};
