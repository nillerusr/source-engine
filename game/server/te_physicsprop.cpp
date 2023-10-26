//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetempentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: create clientside physics prop, as breaks model if needed
//-----------------------------------------------------------------------------
class CTEPhysicsProp : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPhysicsProp, CBaseTempEntity );

					CTEPhysicsProp( const char *name );
	virtual			~CTEPhysicsProp( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVector( m_vecOrigin );
	CNetworkQAngle( m_angRotation );
	CNetworkVector( m_vecVelocity );
	CNetworkVar( int, m_nModelIndex );
	CNetworkVar( int, m_nSkin );
	CNetworkVar( int, m_nFlags );
	CNetworkVar( int, m_nEffects );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPhysicsProp::CTEPhysicsProp( const char *name ) :
	CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nSkin				= 0;
	m_nFlags			= 0;
	m_nEffects			= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPhysicsProp::~CTEPhysicsProp( void )
{
}

IMPLEMENT_SERVERCLASS_ST(CTEPhysicsProp, DT_TEPhysicsProp)
	SendPropVector( SENDINFO(m_vecOrigin), -1, SPROP_COORD),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 0), 13 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 1), 13 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 2), 13 ),
	SendPropVector( SENDINFO(m_vecVelocity), -1, SPROP_COORD),
	SendPropModelIndex( SENDINFO(m_nModelIndex) ),
	SendPropInt( SENDINFO(m_nSkin), ANIMATION_SKIN_BITS),
	SendPropInt( SENDINFO(m_nFlags), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nEffects), EF_MAX_BITS, SPROP_UNSIGNED),
END_SEND_TABLE()

// Singleton to fire TEBreakModel objects
static CTEPhysicsProp s_TEPhysicsProp( "physicsprop" );

void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects )
{
	s_TEPhysicsProp.m_vecOrigin		= pos;
	s_TEPhysicsProp.m_angRotation	= angles;
	s_TEPhysicsProp.m_vecVelocity	= vel;
	s_TEPhysicsProp.m_nModelIndex	= modelindex;	
	s_TEPhysicsProp.m_nSkin			= skin;
	s_TEPhysicsProp.m_nFlags		= flags;
	s_TEPhysicsProp.m_nEffects		= effects;

	// Send it over the wire
	s_TEPhysicsProp.Create( filter, delay );
}
