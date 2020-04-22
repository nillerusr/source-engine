//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_SHIELD_FLAT_H
#define TF_SHIELD_FLAT_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_shield.h"
#include "mathlib/vector.h"

//-----------------------------------------------------------------------------
//
// This is the shield projected by the grenade
//
//-----------------------------------------------------------------------------

class CShieldFlat : public CShield
{
	DECLARE_CLASS( CShieldFlat, CShield );
	DECLARE_SERVERCLASS();

public:
	void SetSize( float w, float h );

	virtual void	Spawn( void );

	virtual void	SetEMPed( bool isEmped );

	void Activate( bool active );

	virtual int Width() { return 2; }
	virtual int Height() { return 2; }
	virtual bool IsPanelActive( int x, int y ) { return true; }
	virtual const Vector& GetPoint( int x, int y );
	virtual void ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs );

public:
	// Think methods
	void ShieldMoved();

public:
	
	// networked data
	CNetworkVar( unsigned char, m_ShieldState );
	CNetworkVar( float, m_Width );
	CNetworkVar( float, m_Height );

private:
	QAngle	m_LastAngles;
	Vector	m_LastPosition;
	Vector	m_Pos[4];
};


//-----------------------------------------------------------------------------
// Purpose: Create a mobile version of the shield
//-----------------------------------------------------------------------------

CShieldFlat* CreateFlatShield( CBaseEntity *pOwner, float w, float h, const Vector& relOrigin, const QAngle &relAngles );

#endif TF_SHIELD_FLAT_H