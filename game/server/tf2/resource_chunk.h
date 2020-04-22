//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RESOURCE_CHUNK_H
#define RESOURCE_CHUNK_H
#ifdef _WIN32
#pragma once
#endif


#include "props.h"

class CResourceZone;

extern ConVar	resource_chunk_value;
extern ConVar	resource_chunk_processed_value;

//-----------------------------------------------------------------------------
// Purpose: A resource chunk that's harvestable by a player
//-----------------------------------------------------------------------------
class CResourceChunk : public CBaseProp
{
	DECLARE_CLASS( CResourceChunk, CBaseProp );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );

	void			ChunkTouch( CBaseEntity *pOther );
	void			ChunkRemove( void );

	virtual bool	IsStandable( const CBaseEntity *pStander ) { return true; } // can pStander stand on this entity?
	virtual bool	IsProcessed( void ) { return m_bIsProcessed; };

	float			GetResourceValue( void );

	static CResourceChunk *Create( bool bProcessed, const Vector &vecOrigin, const Vector &vecVelocity );

public:
	CHandle<CResourceZone>	m_hZone;
	bool				m_bIsProcessed;
	bool				m_bBeingCollected;
};

#endif // RESOURCE_CHUNK_H
