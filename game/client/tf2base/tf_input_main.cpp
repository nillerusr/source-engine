//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: TF2 specific input handling
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "kbutton.h"
#include "input.h"

//-----------------------------------------------------------------------------
// Purpose: TF Input interface
//-----------------------------------------------------------------------------
class CTFInput : public CInput
{
public:
};

static CTFInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;

