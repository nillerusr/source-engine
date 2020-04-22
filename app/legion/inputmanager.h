//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the input 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "tier2/keybindings.h"
#include "tier1/commandbuffer.h"
#include "bitvec.h"


//-----------------------------------------------------------------------------
// Input management
//-----------------------------------------------------------------------------
class CInputManager : public CGameManager<>
{
public:
	// Inherited from IGameManager
	virtual bool Init();
	virtual void Update( );

	// Add a command into the command queue
	void AddCommand( const char *pCommand );

private:
	// Per-frame update of commands
	void ProcessCommands( );

	// Purpose: 
	void PrintConCommandBaseDescription( const ConCommandBase *pVar );

	CKeyBindings m_KeyBindings;
	CBitVec<BUTTON_CODE_LAST> m_ButtonUpToEngine;
	CCommandBuffer m_CommandBuffer;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CInputManager *g_pInputManager;


#endif // INPUTMANAGER_H

