//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SPECTATEGAMES_H
#define SPECTATEGAMES_H
#ifdef _WIN32
#pragma once
#endif

#include "InternetGames.h"

//-----------------------------------------------------------------------------
// Purpose: Spectator games list
//-----------------------------------------------------------------------------
class CSpectateGames : public CInternetGames
{
public:
	CSpectateGames(vgui::Panel *parent);

	// property page handlers
	virtual void OnPageShow();

	virtual bool CheckTagFilter( gameserveritem_t &server );

protected:
	// filters by spectator games
	virtual void GetNewServerList();

private:
	typedef CInternetGames BaseClass;
};


#endif // SPECTATEGAMES_H
