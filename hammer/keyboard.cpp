//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Keyboard.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//
// Defines key state bit masks.
//
#define KEYSTATE_DOWN				0x0000FFFF
#define KEYSTATE_IMPULSE_DOWN		0x00010000
#define KEYSTATE_IMPULSE_UP			0x00020000


//
// List of allowed modifier keys and their associated bit masks.
//
static KeyMap_t ModifierKeyTable[] = 
{
	{ VK_SHIFT, KEY_MOD_SHIFT, 0 },
	{ VK_CONTROL, KEY_MOD_CONTROL, 0 },
	{ VK_MENU, KEY_MOD_ALT, 0 }
};


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CKeyboard::CKeyboard(void)
{
	g_uKeyMaps = 0;	
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CKeyboard::~CKeyboard(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Adds a key binding to the 
// Input  : uChar - The virtual keycode of the primary key that must be held down.
//			uModifierKeys - Bitflags specifying which modifier keys must be
//				held down along with the key specified by uChar.
//			uLogicalKey - An application-specific value that indicates which
//				logical function
//-----------------------------------------------------------------------------
void CKeyboard::AddKeyMap(unsigned int uChar, unsigned int uModifierKeys, unsigned int uLogicalKey)
{
	g_uKeyMap[g_uKeyMaps].uChar = uChar;
	g_uKeyMap[g_uKeyMaps].uModifierKeys = uModifierKeys;
	g_uKeyMap[g_uKeyMaps].uLogicalKey = uLogicalKey;
	g_uKeyMaps++;
}


//-----------------------------------------------------------------------------
// Purpose: Clears the KEYSTATE_IMPULSE_UP and KEYSTATE_IMPULSE_DOWN flags from
//			all physical and logical keys.
//-----------------------------------------------------------------------------
void CKeyboard::ClearImpulseFlags(void)
{
	int nKey;

	//
	// Clear the impulse flags for all the physical keys.
	//
	for (nKey = 0; nKey < sizeof(g_uPhysicalKeyState) / sizeof(g_uPhysicalKeyState[0]); nKey++)
	{
		g_uPhysicalKeyState[nKey] &= ~(KEYSTATE_IMPULSE_DOWN | KEYSTATE_IMPULSE_UP);
	}

	//
	// Clear the impulse flags for all the logical keys.
	//
	for (nKey = 0; nKey < sizeof(g_uLogicalKeyState) / sizeof(g_uLogicalKeyState[0]); nKey++)
	{
		g_uLogicalKeyState[nKey] &= ~(KEYSTATE_IMPULSE_DOWN | KEYSTATE_IMPULSE_UP);
	}
}



//-----------------------------------------------------------------------------
// Purpose: Zeros out the key state for all physical and logical keys.
//-----------------------------------------------------------------------------
void CKeyboard::ClearKeyStates(void)
{
	int nKey;

	//
	// Clear the physical key states.
	//
	for (nKey = 0; nKey < sizeof(g_uPhysicalKeyState) / sizeof(g_uPhysicalKeyState[0]); nKey++)
	{
		g_uPhysicalKeyState[nKey] = 0;
	}

	//
	// Clear the logical key states.
	//
	for (nKey = 0; nKey < sizeof(g_uLogicalKeyState) / sizeof(g_uLogicalKeyState[0]); nKey++)
	{
		g_uLogicalKeyState[nKey] = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets a floating point value indicating about how long the logical
//			key has been held down during this sample period.
// Input  : uLogicalKey - Logical key to check.
// Output : Returns one of the following values:
//			0.25 if a key was pressed and released during the sample period,
//			0.5 if it was pressed and held,
//			0 if held then released, and
//			1.0 if held for the entire time.
//-----------------------------------------------------------------------------
float CKeyboard::GetKeyScale(unsigned int uLogicalKey)
{
	if (uLogicalKey >= MAX_LOGICAL_KEYS)
	{
		return(0);
	}

	unsigned int uKeyState = g_uLogicalKeyState[uLogicalKey];

	bool bImpulseDown = (uKeyState & KEYSTATE_IMPULSE_DOWN) != 0;
	bool bImpulseUp = (uKeyState & KEYSTATE_IMPULSE_UP) != 0;
	bool bDown = (uKeyState & KEYSTATE_DOWN) != 0;
	float fValue = 0;
	
	//
	// If we have a leading edge and no trailing edge, the key should be down.
	//
	if (bImpulseDown && !bImpulseUp)
	{
		if (bDown)
		{
			//
			// Pressed and held this frame.
			//
			fValue = 0.5;
		}
	}

	//
	// If we have a trailing edge and no leading edge, the key should be up.
	//
	if (bImpulseUp && !bImpulseDown)
	{
		if (!bDown)
		{
			//
			// Released this frame.
			//
			fValue = 0;
		}
	}

	//
	// If we have neither a leading edge nor a trailing edge, the key was either
	// up the whole frame or down the whole frame.
	//
	if (!bImpulseDown && !bImpulseUp)
	{
		if (bDown)
		{
			//
			// Held the entire frame
			//
			fValue = 1.0;
		}
		else
		{
			//
			// Up the entire frame.
			//
			fValue = 0;
		}
	}

	//
	// If we have both a leading and trailing edge, it was either released and repressed
	// this frame, or pressed and released this frame.
	//
	if (bImpulseDown && bImpulseUp)
	{
		if (bDown)
		{
			//
			// Released and re-pressed this frame.
			//
			fValue = 0.75;
		}
		else
		{
			//
			// Pressed and released this frame.
			//
			fValue = 0.25;
		}
	}
	
	return fValue;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the bit mask associated with the given modifier key.
// Input  : uModifierKey - The virtual key code corresponding to the modifier key.
// Output : The modifier key's bitmask.
//-----------------------------------------------------------------------------
unsigned int CKeyboard::GetModifierKeyBit(unsigned int uChar)
{
	for (int nKey = 0; nKey < sizeof(ModifierKeyTable) / sizeof(ModifierKeyTable[0]); nKey++)
	{
		if (ModifierKeyTable[nKey].uChar == uChar)
		{
			return(ModifierKeyTable[nKey].uModifierKeys);
		}
	}

	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Checks to see if all of the modifier keys specified by bits in uModifierKeys
//			are currently held down.
// Input  : uModifierKeys - Contains bits indicating which modifier keys to check:
//				KEY_MOD_SHIFT
//				KEY_MOD_CONTROL
//				KEY_MOD_ALT
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CKeyboard::IsKeyPressed(unsigned int uChar, unsigned int uModifierKeys)
{
	if (!(g_uPhysicalKeyState[uChar] & KEYSTATE_DOWN))
	{
		return(false);
	}

	bool bKeyPressed = true;

	for (int nKey = 0; nKey < sizeof(ModifierKeyTable) / sizeof(ModifierKeyTable[0]); nKey++)
	{
		if (g_uPhysicalKeyState[ModifierKeyTable[nKey].uChar] & KEYSTATE_DOWN)
		{
			if (!(uModifierKeys & ModifierKeyTable[nKey].uModifierKeys))
			{
				bKeyPressed = false;
			}
		}
		else if (uModifierKeys & ModifierKeyTable[nKey].uModifierKeys)
		{
			bKeyPressed = false;
		}
	}

	return(bKeyPressed);
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether a key is an allowed modifier key, ie, whether it
//			can be used in conjunction with other keys when performing key
//			bindings.
// Input  : uChar - Virtual key to check.
// Output : Returns true if this key is a modifier key, false if not.
//-----------------------------------------------------------------------------
bool CKeyboard::IsModifierKey(unsigned int uChar)
{
	return((uChar == VK_SHIFT) || (uChar == VK_CONTROL) || (uChar == VK_MENU));
}


//-----------------------------------------------------------------------------
// Purpose: Given a key press/release event, updates the status of all logical
//			keys.
// Input  : uChar - The key whose state has changed.
//			bPressed - True if the key was pressed, false if it was released.
//-----------------------------------------------------------------------------
void CKeyboard::UpdateLogicalKeys(unsigned int uChar, bool bPressed)
{
	//
	// Determine whether the key is a modifier key. If so, find its modifier bit.
	//
	bool bIsModifierKey = IsModifierKey(uChar);
	unsigned int uModifierKeyBit = 0;
	if (bIsModifierKey)
	{
		uModifierKeyBit = GetModifierKeyBit(uChar);
	}

	//
	// For every key in the keymap that depends upon this physical key, update
	// the state of the corresponding logical key based on this event.
	//
	for (unsigned int nKey = 0; nKey < g_uKeyMaps; nKey++)
	{
		unsigned int uPhysicalKey = g_uKeyMap[nKey].uChar;
		unsigned int uLogicalKey = g_uKeyMap[nKey].uLogicalKey;
		unsigned int uModifierKeys = g_uKeyMap[nKey].uModifierKeys;

		if ((uPhysicalKey == uChar) || (uModifierKeys & uModifierKeyBit))
		{
			//
			// Check the state of all modifier keys to which this logical key
			// is bound to determine whether the logical key is pressed or not.
			//
			bool bLogicalKeyPressed = IsKeyPressed(g_uKeyMap[nKey].uChar, g_uKeyMap[nKey].uModifierKeys);

			//
			// Update the logical key state.
			//
			if (bPressed)
			{
				if (bLogicalKeyPressed)
				{
					if (!(g_uLogicalKeyState[uLogicalKey] & KEYSTATE_DOWN))
					{
						g_uLogicalKeyState[uLogicalKey] |= KEYSTATE_IMPULSE_DOWN;
					}

					g_uLogicalKeyState[uLogicalKey]++;
				}
			}
			else
			{
				if (g_uLogicalKeyState[uLogicalKey] & KEYSTATE_DOWN)
				{
					g_uLogicalKeyState[uLogicalKey]--;
				}
				
				if (!(g_uLogicalKeyState[uLogicalKey] & KEYSTATE_DOWN))
				{
					g_uLogicalKeyState[uLogicalKey] |= KEYSTATE_IMPULSE_UP;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by the client when a WM_KEYDOWN message is received. 
// Input  : Per CWnd::OnKeyDown.
//-----------------------------------------------------------------------------
void CKeyboard::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if ((!(nFlags & 0x4000)) || (!(g_uPhysicalKeyState[nChar] & KEYSTATE_DOWN)))
	{
		g_uPhysicalKeyState[nChar] |= KEYSTATE_DOWN;
		g_uPhysicalKeyState[nChar] |= KEYSTATE_IMPULSE_DOWN;

		UpdateLogicalKeys(nChar, true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by the client when a WM_KEYUP message is received. 
// Input  : Per CWnd::OnKeyDown.
//-----------------------------------------------------------------------------
void CKeyboard::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (g_uPhysicalKeyState[nChar] & KEYSTATE_DOWN)
	{
		g_uPhysicalKeyState[nChar] &= ~KEYSTATE_DOWN;
	}

	g_uPhysicalKeyState[nChar] |= KEYSTATE_IMPULSE_UP;

	UpdateLogicalKeys(nChar, false);
}



//-----------------------------------------------------------------------------
// Purpose: Deletes all key bindings.
//-----------------------------------------------------------------------------
void CKeyboard::RemoveAllKeyMaps(void)
{
	g_uKeyMaps = 0;
}

