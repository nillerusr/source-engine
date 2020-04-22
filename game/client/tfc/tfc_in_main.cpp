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
class CTFCInput : public CInput
{
public:
};

static CTFCInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;

