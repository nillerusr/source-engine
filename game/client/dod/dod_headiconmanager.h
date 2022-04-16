//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef DOD_HEADICONMANAGER_H
#define DOD_HEADICONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "bitvec.h"

class IMaterial;

class CHeadIconManager : CAutoGameSystem
{
public:
	CHeadIconManager();
	~CHeadIconManager();

	virtual bool Init();
	virtual void Shutdown();

public:
	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void	DrawHeadIcons();

	// Call from player render calls to indicate a head icon should be drawn for this player this frame
	void	PlayerDrawn( C_BasePlayer *pPlayer );

private:
	IMaterial	*m_pAlliesIconMaterial;		// For labels above players' heads.
	IMaterial	*m_pAxisIconMaterial;		// For labels above players' heads.
	CBitVec<MAX_PLAYERS> m_PlayerDrawn;		// Was the player drawn this frame?
};


// Get the (global) head icon manager. 
CHeadIconManager* HeadIconManager();


#endif // DOD_HEADICONMANAGER_H
