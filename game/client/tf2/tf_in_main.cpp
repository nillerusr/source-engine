//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2 specific input handling
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "kbutton.h"
#include "input.h"

//-----------------------------------------------------------------------------
// Purpose: TF Input interface
//-----------------------------------------------------------------------------
class CTFInput : public CInput
{
public:
	int		GetButtonBits( int );
};

static CTFInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;


//-----------------------------------------------------------------------------
// Purpose: Remove Jump from the input bits
//-----------------------------------------------------------------------------
int CTFInput::GetButtonBits( int bResetState )
{
	int iBits = CInput::GetButtonBits( bResetState );

	//iBits &= ~IN_JUMP;

	return iBits;
}
