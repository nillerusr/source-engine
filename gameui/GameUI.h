//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUI_H
#define GAMEUI_H
#ifdef _WIN32
#pragma once
#endif

class IGameUI;

//-----------------------------------------------------------------------------
// Purpose: Accessor function to get game ui interface
//-----------------------------------------------------------------------------
inline IGameUI *gameui()
{
	extern IGameUI *g_pGameUI;
	return g_pGameUI;
}

#endif // GAMEUI_H
