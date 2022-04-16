//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DOD's custom C_PlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_DOD_PLAYERRESOURCE_H
#define C_DOD_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_shareddefs.h"
#include "c_playerresource.h"

class C_DOD_PlayerResource : public C_PlayerResource
{
	DECLARE_CLASS( C_DOD_PlayerResource, C_PlayerResource );
public:
	DECLARE_CLIENTCLASS();

					C_DOD_PlayerResource();
	virtual			~C_DOD_PlayerResource();

	int GetScore( int iIndex );
	int GetPlayerClass( int iIndex );
	
protected:

	int		m_iObjScore[MAX_PLAYERS+1];
	int		m_iPlayerClass[MAX_PLAYERS+1];
};


#endif // C_DOD_PLAYERRESOURCE_H
