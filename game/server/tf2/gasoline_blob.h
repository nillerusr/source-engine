//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GASOLINE_BLOB_H
#define GASOLINE_BLOB_H
#ifdef _WIN32
#pragma once
#endif


#include "baseentity.h"


class CGasolineBlob : public CBaseEntity
{
public:
	DECLARE_CLASS( CGasolineBlob, CBaseEntity );
	DECLARE_SERVERCLASS();


	// Create a gasoline blob.
	// flAirLifetime specifies how long it takes to fizzle out in the air.
	static CGasolineBlob*	Create( 
		CBaseEntity *pOwner, 
		const Vector &vOrigin, 
		const Vector &vStartVelocity, 
		bool bUseGravity,
		float flAirLifetime,
		float flLifetime );

	virtual					~CGasolineBlob();

	// A lit blob will always apply at least 25% damage to its "auto burn" blob.
	//
	// This is used when laying down gasoline blobs in a line. Since it's fairly easy to accidentally
	// lay down blobs that won't damage each other because they're too far away, the line of blobs
	// can be linked together using this.
	void			AddAutoBurnBlob( CGasolineBlob *pBlob );


// Overrides.
public:

	virtual int		OnTakeDamage( const CTakeDamageInfo &info );



// Implementation.
public:
	
	bool	IsLit() const;
	bool	IsStopped() const;
	void	SetLit( bool bLit );

	void	Think();

	void	AutoBurn_R( CGasolineBlob *pParent );


private:
	
	typedef CHandle<CGasolineBlob> CGasolineBlobHandle;
	CUtlVector<CGasolineBlobHandle>	m_AutoBurnBlobs;
	
	float			m_flTimeInAir;		// How long we've been in the air.
	float			m_flAirLifetime;	// How long we're allowed to exist in the air.

	CNetworkVar( float, m_flLitStartTime );	// What time did the blob become lit at?
	
	CNetworkVar( int, m_BlobFlags );	// Combination of BLOBFLAG_ defines.
	
	// This is set at the start and is used to know the percentage of lifetime left.
	CNetworkVar( float, m_flMaxLifetime );

	// When the blob was created.
	CNetworkVar( float, m_flCreateTime );

	CNetworkVector( m_vSurfaceNormal );	// This is sent to the client so it can spread the fire out.

	EHANDLE			m_hOwner;
	float			m_HeatLevel;	// This rises when other flames are nearby until we ignite.
};


// Returns true if the entity is a gasoline blob.
bool IsGasolineBlob( CBaseEntity *pEnt );


#endif // GASOLINE_BLOB_H
