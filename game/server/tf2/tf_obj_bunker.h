//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary bunker that players can take cover in.
//
//=============================================================================//

#ifndef TF_OBJ_BUNKER_H
#define TF_OBJ_BUNKER_H
#ifdef _WIN32
#pragma once
#endif

class CObjectBunkerLadder;

// ------------------------------------------------------------------------ //
// Purpose: A stationary bunker that players can take cover in.
// ------------------------------------------------------------------------ //
class CObjectBunker : public CBaseObject
{
	DECLARE_CLASS( CObjectBunker, CBaseObject );

public:
	DECLARE_SERVERCLASS();

	CObjectBunker( void );

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	UpdateOnRemove( void );
	virtual void	FinishedBuilding( void );

private:
	CHandle<CObjectBunkerLadder>		m_hLadder;
};

//-----------------------------------------------------------------------------
// Purpose: Bunker ladder
//-----------------------------------------------------------------------------
class CObjectBunkerLadder : public CBaseAnimating
{
	DECLARE_CLASS( CObjectBunkerLadder, CBaseAnimating );

public:

	DECLARE_SERVERCLASS();

	static CObjectBunkerLadder* Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent );

	CObjectBunkerLadder();

	void	Spawn();
	void	Precache();
	virtual int	OnTakeDamage( const CTakeDamageInfo &info );

public:
	EHANDLE		m_hBunker;
};

#endif // TF_OBJ_BUNKER_H
