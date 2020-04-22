//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef KEYBOARD_H
#define KEYBOARD_H
#ifdef _WIN32
#pragma once
#endif


#define KEY_MOD_SHIFT				0x0001
#define KEY_MOD_CONTROL				0x0002
#define KEY_MOD_ALT					0x0004


//
// Defines the maximum number of physical keys. These physical keys correspond to
// the windows virtual key codes shown below. Missing key codes may be up for grabs,
// but it is probably safer to add to the end of the list.
//
#define MAX_PHYSICAL_KEYS			256

//
// VK_LBUTTON        0x01
// VK_RBUTTON        0x02
// VK_CANCEL         0x03
// VK_MBUTTON        0x04
// ?				 0x04
// ?				 0x05
// ?				 0x06
// ?				 0x07
// VK_BACK           0x08
// VK_TAB            0x09
// ?				 0x0A
// ?				 0x0B
// VK_CLEAR          0x0C
// VK_RETURN         0x0D
// ?				 0x0E
// ?				 0x0F
// VK_SHIFT          0x10
// VK_CONTROL        0x11
// VK_MENU           0x12
// VK_PAUSE          0x13
// VK_CAPITAL        0x14
// VK_KANA           0x15
// VK_HANGUL         0x15
// VK_JUNJA          0x17
// VK_FINAL          0x18
// VK_KANJI          0x19
// ?				 0x1A
// VK_ESCAPE         0x1B
// VK_CONVERT        0x1C
// VK_NONCONVERT     0x1D
// VK_ACCEPT         0x1E
// VK_MODECHANGE     0x1F
// VK_SPACE          0x20
// VK_PRIOR          0x21
// VK_NEXT           0x22
// VK_END            0x23
// VK_HOME           0x24
// VK_LEFT           0x25
// VK_UP             0x26
// VK_RIGHT          0x27
// VK_DOWN           0x28
// VK_SELECT         0x29
// VK_PRINT          0x2A
// VK_EXECUTE        0x2B
// VK_SNAPSHOT       0x2C
// VK_INSERT         0x2D
// VK_DELETE         0x2E
// VK_HELP           0x2F
//
// VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39)
//
// VK_0			0x30
// VK_1			0x31
// VK_2			0x32
// VK_3			0x33
// VK_4			0x34
// VK_5			0x35
// VK_6			0x36
// VK_7			0x37
// VK_8			0x38
// VK_9			0x39
// ?			0x40
//
// VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A)
//
// VK_A			0X41
// VK_B			0X42
// VK_C			0X43
// VK_D			0X44
// VK_E			0X45
// VK_F			0X46
// VK_G			0X47
// VK_H			0X48
// VK_I			0X49
// VK_J			0X4A
// VK_K			0X4B
// VK_L			0X4C
// VK_M			0X4D
// VK_N			0X4E
// VK_O			0X4F
// VK_P			0X50
// VK_Q			0X51
// VK_R			0X52
// VK_S			0X53
// VK_T			0X54
// VK_U			0X55
// VK_V			0X56
// VK_W			0X57
// VK_X			0X58
// VK_Y			0X59
// VK_Z			0X5A
// VK_LWIN      0x5B
// VK_RWIN      0x5C
// VK_APPS      0x5D
// ?			0x5E
// ?			0x5F
// VK_NUMPAD0   0x60
// VK_NUMPAD1   0x61
// VK_NUMPAD2   0x62
// VK_NUMPAD3   0x63
// VK_NUMPAD4   0x64
// VK_NUMPAD5   0x65
// VK_NUMPAD6   0x66
// VK_NUMPAD7   0x67
// VK_NUMPAD8   0x68
// VK_NUMPAD9   0x69
// VK_MULTIPLY  0x6A
// VK_ADD       0x6B
// VK_SEPARATOR 0x6C
// VK_SUBTRACT  0x6D
// VK_DECIMAL   0x6E
// VK_DIVIDE    0x6F
//
// Function keys
//
// VK_F1        0x70
// VK_F2        0x71
// VK_F3        0x72
// VK_F4        0x73
// VK_F5        0x74
// VK_F6        0x75
// VK_F7        0x76
// VK_F8        0x77
// VK_F9        0x78
// VK_F10       0x79
// VK_F11       0x7A
// VK_F12       0x7B
// VK_F13       0x7C
// VK_F14       0x7D
// VK_F15       0x7E
// VK_F16       0x7F
// VK_F17       0x80
// VK_F18       0x81
// VK_F19       0x82
// VK_F20       0x83
// VK_F21       0x84
// VK_F22       0x85
// VK_F23       0x86
// VK_F24       0x87
// ?			0x88
// ?			0x89
// ?			0x8A
// ?			0x8B
// ?			0x8C
// ?			0x8D
// ?			0x8E
// ?			0x8F
// VK_NUMLOCK   0x90
// VK_SCROLL    0x91
// ?			0x92
// ?			0x93
// ?			0x94
// ?			0x95
// ?			0x96
// ?			0x97
// ?			0x98
// ?			0x99
// ?			0x9A
// ?			0x9B
// ?			0x9C
// ?			0x9D
// ?			0x9E
// ?			0x9F
//
// VK_L* & VK_R* - left and right Alt, Ctrl and Shift virtual keys.
// Used only as parameters to GetAsyncKeyState() and GetKeyState().
// No other API or message will distinguish left and right keys in this way.
//
// VK_LSHIFT    0xA0
// VK_RSHIFT    0xA1
// VK_LCONTROL  0xA2
// VK_RCONTROL  0xA3
// VK_LMENU     0xA4
// VK_RMENU     0xA5
// VK_PROCESSKEY 0xE5
// VK_ATTN      0xF6
// VK_CRSEL     0xF7
// VK_EXSEL     0xF8
// VK_EREOF     0xF9
// VK_PLAY      0xFA
// VK_ZOOM      0xFB
// VK_NONAME    0xFC
// VK_PA1       0xFD
// VK_OEM_CLEAR 0xFE
//


//
// Defines the maximum number of logical keys. Logical keys are application-specific
// values that are associated with physical keys via AddKeyMap.
//
#define MAX_LOGICAL_KEYS			256


//
// Defines the maximum number of unique key bindings.
//
#define MAX_KEYMAPS					256


typedef struct
{
	unsigned int uChar;
	unsigned int uModifierKeys;
	unsigned int uLogicalKey;
} KeyMap_t;


class CKeyboard
{
	public:
			
		CKeyboard(void);
		~CKeyboard(void);

		void AddKeyMap(unsigned int uChar, unsigned int uModifierKeys, unsigned int uLogicalKey);
		void ClearImpulseFlags(void);
		void ClearKeyStates(void);
		float GetKeyScale(unsigned int uLogicalKey);

		void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

		void RemoveAllKeyMaps(void);

	protected:

		bool IsKeyPressed(unsigned int uChar, unsigned int uModifierKeys);
		bool IsModifierKey(unsigned int uChar);

		unsigned int GetModifierKeyBit(unsigned int uChar);

		void UpdateLogicalKeys(unsigned int uChar, bool bPressed);

		unsigned int g_uPhysicalKeyState[MAX_PHYSICAL_KEYS];
		unsigned int g_uLogicalKeyState[MAX_LOGICAL_KEYS];

		KeyMap_t g_uKeyMap[MAX_KEYMAPS];
		unsigned int g_uKeyMaps;
};


#endif // KEYBOARD_H
