//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmekeyboardinput.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

#include "vgui/iinput.h"
#include "vgui/keycode.h"
#include "tier3/tier3.h"

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// global list of all keys supported
//-----------------------------------------------------------------------------
struct KeyInfo
{
	vgui::KeyCode code;
	const char *str;
};

const uint g_nKeys = 48;
const KeyInfo g_keyInfo[ g_nKeys ] =
{
	{ KEY_0, "0" },
	{ KEY_1, "1" },
	{ KEY_2, "2" },
	{ KEY_3, "3" },
	{ KEY_4, "4" },
	{ KEY_5, "5" },
	{ KEY_6, "6" },
	{ KEY_7, "7" },
	{ KEY_8, "8" },
	{ KEY_9, "9" },
	{ KEY_A, "A" },
	{ KEY_B, "B" },
	{ KEY_C, "C" },
	{ KEY_D, "D" },
	{ KEY_E, "E" },
	{ KEY_F, "F" },
	{ KEY_G, "G" },
	{ KEY_H, "H" },
	{ KEY_I, "I" },
	{ KEY_J, "J" },
	{ KEY_K, "K" },
	{ KEY_L, "L" },
	{ KEY_M, "M" },
	{ KEY_N, "N" },
	{ KEY_O, "O" },
	{ KEY_P, "P" },
	{ KEY_Q, "Q" },
	{ KEY_R, "R" },
	{ KEY_S, "S" },
	{ KEY_T, "T" },
	{ KEY_U, "U" },
	{ KEY_V, "V" },
	{ KEY_W, "W" },
	{ KEY_X, "X" },
	{ KEY_Y, "Y" },
	{ KEY_Z, "Z" },
	{ KEY_F1, "F1" },
	{ KEY_F2, "F2" },
	{ KEY_F3, "F3" },
	{ KEY_F4, "F4" },
	{ KEY_F5, "F5" },
	{ KEY_F6, "F6" },
	{ KEY_F7, "F7" },
	{ KEY_F8, "F8" },
	{ KEY_F9, "F9" },
	{ KEY_F10, "F10" },
	{ KEY_F11, "F11" },
	{ KEY_F12, "F12" },
};


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeKeyboardInput, CDmeKeyboardInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeKeyboardInput::OnConstruction()
{
	m_keys = new CDmaVar< bool >[ g_nKeys ];

	for ( uint ki = 0; ki < g_nKeys; ++ki )
	{
		m_keys[ ki ].Init( this, g_keyInfo[ ki ].str );
	}
}

void CDmeKeyboardInput::OnDestruction()
{
	delete[] m_keys;
}

bool CDmeKeyboardInput::IsDirty()
{
	for ( uint ki = 0; ki < g_nKeys; ++ki )
	{
		if ( m_keys[ ki ].Get() != GetKeyStatus( ki ) )
			return true;
	}
	return false;
}

void CDmeKeyboardInput::Operate()
{
	for ( uint ki = 0; ki < g_nKeys; ++ki )
	{
		m_keys[ ki ].Set( GetKeyStatus( ki ) );
	}
}

void CDmeKeyboardInput::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
}

void CDmeKeyboardInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( uint ki = 0; ki < g_nKeys; ++ki )
	{
		attrs.AddToTail( m_keys[ ki ].GetAttribute() );
	}
}

bool CDmeKeyboardInput::GetKeyStatus( uint ki )
{
	return g_pVGuiInput->IsKeyDown( g_keyInfo[ ki ].code );
}
