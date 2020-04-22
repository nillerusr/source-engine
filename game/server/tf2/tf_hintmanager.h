//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HINTMANAGER_H
#define TF_HINTMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class CBaseTFPlayer;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFHintManager : public CBaseEntity
{
	DECLARE_CLASS( CTFHintManager, CBaseEntity );

public:

	DECLARE_SERVERCLASS();

						CTFHintManager( void );

	virtual void		Spawn( void );

	virtual void		Think( void );

	virtual int			UpdateTransmitState();

private:

	void				AddHint( CBaseTFPlayer *player, int hintID, int priority, int entityIndex = 0 );
};

#endif // TF_HINTMANAGER_H
