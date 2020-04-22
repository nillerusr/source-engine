//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTS_SHARED_H
#define ENTS_SHARED_H

#include <protocol.h>
#include <iserver.h>
#include "clientframe.h"
#include "packed_entity.h"

#ifdef _WIN32
#pragma once
#endif



class CEntityInfo
{
public:

	CEntityInfo() {
		m_nOldEntity = -1;
		m_nNewEntity = -1;
		m_nHeaderBase = -1;
	}
	virtual	~CEntityInfo() {};
	
	bool			m_bAsDelta;
	CClientFrame	*m_pFrom;
	CClientFrame	*m_pTo;

	UpdateType		m_UpdateType;

	int				m_nOldEntity;	// current entity index in m_pFrom
	int				m_nNewEntity;	// current entity index in m_pTo

	int				m_nHeaderBase;
	int				m_nHeaderCount;

	inline void	NextOldEntity( void ) 
	{
		if ( m_pFrom )
		{
			m_nOldEntity = m_pFrom->transmit_entity.FindNextSetBit( m_nOldEntity+1 );

			if ( m_nOldEntity < 0 )
			{
				// Sentinel/end of list....
				m_nOldEntity = ENTITY_SENTINEL;
			}
		}
		else
		{
			m_nOldEntity = ENTITY_SENTINEL;
		}
	}

	inline void	NextNewEntity( void ) 
	{
		m_nNewEntity = m_pTo->transmit_entity.FindNextSetBit( m_nNewEntity+1 );

		if ( m_nNewEntity < 0 )
		{
			// Sentinel/end of list....
			m_nNewEntity = ENTITY_SENTINEL;
		}
	}
};

// PostDataUpdate calls are stored in a list until all ents have been updated.
class CPostDataUpdateCall
{
public:
	int					m_iEnt;
	DataUpdateType_t	m_UpdateType;
};


// Passed around the read functions.
class CEntityReadInfo : public CEntityInfo
{

public:

	CEntityReadInfo() 
	{	m_nPostDataUpdateCalls = 0;
		m_nLocalPlayerBits = 0;
		m_nOtherPlayerBits = 0;
		m_UpdateType = PreserveEnt;
	}

	bf_read			*m_pBuf;
	int				m_UpdateFlags;	// from the subheader
	bool			m_bIsEntity;

	int				m_nBaseline;	// what baseline index do we use (0/1)
	bool			m_bUpdateBaselines; // update baseline while parsing snaphsot
		
	int				m_nLocalPlayerBits; // profiling data
	int				m_nOtherPlayerBits; // profiling data

	CPostDataUpdateCall	m_PostDataUpdateCalls[MAX_EDICTS];
	int					m_nPostDataUpdateCalls;
};

#endif // ENTS_SHARED_H
