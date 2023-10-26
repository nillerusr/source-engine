//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IGAMECLIENTEXPORTS_H
#define IGAMECLIENTEXPORTS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: Exports a set of functions for the GameUI interface to interact with the game client
//-----------------------------------------------------------------------------
abstract_class IGameClientExports : public IBaseInterface
{
public:
	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
	virtual void MutePlayerGameVoice(int playerIndex) = 0;
	virtual void UnmutePlayerGameVoice(int playerIndex) = 0;

	// notification of gameui state changes
	virtual void OnGameUIActivated() = 0;
	virtual void OnGameUIHidden() = 0;

	// if true, the gameui applies the blur effect
	virtual bool ClientWantsBlurEffect( void ) = 0;
};

#define GAMECLIENTEXPORTS_INTERFACE_VERSION "GameClientExports001"


#endif // IGAMECLIENTEXPORTS_H
