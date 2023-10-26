#ifndef INPUTKBWIN32_H
#define INPUTKBWIN32_H

//
// InputKBWin32.h
//

#include "gmThread.h" // For some basic types

/// A simple keyboard input class for Win32.
class InputKBWin32
{
public:

  enum
  {
    MAX_KEYS = 256,                               ///< Max keys on keyboard (for buffer size etc.)
  };

  enum
  {
    KEY_STATUS_UNKNOWN =  0,                      ///< Invalid status
    KEY_STATUS_UP =       (1<<0),                 ///< Button is up
    KEY_STATUS_RELEASED = (1<<1),                 ///< Button released this frame
    KEY_STATUS_DOWN =     (1<<2),                 ///< Button is down
    KEY_STATUS_PRESSED =  (1<<3),                 ///< Button pressed this frame
  };

  /// Access single instance of this class
  static InputKBWin32& Get()                      { return s_staticInstance; }

  /// Destructor
  virtual ~InputKBWin32();

  /// Initialize.  Call before use.
  void Init();

  /// Call each frame.
  void Update();

  /// What is the status of a key.  Returns one of the KEY_STATUS_* enums.
  inline int GetKeyStatus(int a_keyIndex)
  {
    GM_ASSERT((a_keyIndex >=0) && (a_keyIndex < MAX_KEYS));
    return m_keyStatus[a_keyIndex];
  }

  /// Was key pressed this frame? (Non-zero if true)
  inline int IsKeyPressed(int a_keyIndex)
  {
    GM_ASSERT((a_keyIndex >= 0) && (a_keyIndex < MAX_KEYS));
    return (m_keyStatus[a_keyIndex] & KEY_STATUS_PRESSED);
  }

  /// Is key down this frame? (Non-zero if true)
  inline int IsKeyDown(int a_keyIndex)
  {
    GM_ASSERT((a_keyIndex >= 0) && (a_keyIndex < MAX_KEYS));
    return (m_keyStatus[a_keyIndex] & KEY_STATUS_DOWN);
  }

  /// Was key released this frame? (Non-zero if true)
  inline int IsKeyRelesed(int a_keyIndex)
  {
    GM_ASSERT((a_keyIndex >= 0) && (a_keyIndex < MAX_KEYS));
    return (m_keyStatus[a_keyIndex] & KEY_STATUS_RELEASED);
  }

  /// Is key up this frame? (Non-zero if true)
  inline int IsKeyUp(int a_keyIndex)
  {
    GM_ASSERT((a_keyIndex >= 0) && (a_keyIndex < MAX_KEYS));
    return (m_keyStatus[a_keyIndex] & KEY_STATUS_UP);
  }


private:

  /// Constructor, non-public to prevent multiple instances
  InputKBWin32();

  char m_keyDownBuffer[2][MAX_KEYS];              ///< Store current and last frame snapshot
  int m_keyDownBufferIndex;                       ///< Index to swap buffers for current and last frame
  int m_keyStatus[MAX_KEYS];                      ///< Status of keys for this frame, persists until updated.

  static InputKBWin32 s_staticInstance;           ///< Single instance of this class
};


#endif //INPUTKBWIN32_H