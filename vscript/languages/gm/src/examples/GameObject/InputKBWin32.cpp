//
// InputKBWin32.cpp
//

#include <windows.h>
#include "InputKBWin32.h"

// Init statics and constants
InputKBWin32 InputKBWin32::s_staticInstance;



InputKBWin32::InputKBWin32()
{
  m_keyDownBufferIndex = 0;

  Init();
}



InputKBWin32::~InputKBWin32()
{
}


void InputKBWin32::Init()
{
  for(int kIndex=0; kIndex<MAX_KEYS; ++kIndex)
  {
    m_keyDownBuffer[0][kIndex] = 0;
    m_keyDownBuffer[1][kIndex] = 0;
    m_keyStatus[kIndex] = KEY_STATUS_UP;
  }
}


void InputKBWin32::Update()
{
//#define HAS_WINDOW_UPDATE // Define this if we have a window and a message pump

  int lastBuffIndex = !m_keyDownBufferIndex;
  int curBuffIndex = m_keyDownBufferIndex;
  
  m_keyDownBufferIndex = lastBuffIndex; // Flip buffers

  char* bufferCurrent = &m_keyDownBuffer[curBuffIndex][0];
  char* bufferLast = &m_keyDownBuffer[lastBuffIndex][0];

  // Get the button states from Win32
#ifdef HAS_WINDOW_UPDATE
  // We have a window and message pump
  BYTE win32KeyBuffer[256];
  GetKeyboardState(win32KeyBuffer);
#else // HAS_WINDOW_UPDATE
  short win32KeyBuffer[256];
  for(int vkIndex=0; vkIndex < 256; ++vkIndex)
  {
    win32KeyBuffer[vkIndex] = GetAsyncKeyState(vkIndex);
  }
#endif //HAS_WINDOW_UPDATE

  // Find state changes
  for(int kIndex=0; kIndex < MAX_KEYS; ++kIndex)
  {
    int status;
  
    // Convert win32 keystate to true / false
#ifdef HAS_WINDOW_UPDATE
    if(win32KeyBuffer[kIndex] & (1<<7))
#else // HAS_WINDOW_UPDATE
    if(win32KeyBuffer[kIndex] & (1<<15))
#endif // HAS_WINDOW_UPDATE
    {
      bufferCurrent[kIndex] = true;
    }
    else
    {
      bufferCurrent[kIndex] = false;
    }

    status = 0;
    if(bufferCurrent[kIndex])
    {
      status |= KEY_STATUS_DOWN;
      if(!bufferLast[kIndex])
      {
        status |= KEY_STATUS_PRESSED;
      }
    }
    else
    {
      status |= KEY_STATUS_UP;
      if(bufferLast[kIndex])
      {
        status |= KEY_STATUS_RELEASED;
      }
    }
  
    m_keyStatus[kIndex] = status;
  }
}
