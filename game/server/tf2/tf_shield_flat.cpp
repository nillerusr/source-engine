//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_shield_flat.h"
#include "tf_shieldshared.h"


//-----------------------------------------------------------------------------
// Data tables
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( shield_flat, CShieldFlat );

IMPLEMENT_SERVERCLASS_ST(CShieldFlat, DT_Shield_Flat)

	SendPropInt		(SENDINFO(m_ShieldState),	2, SPROP_UNSIGNED ),
	SendPropFloat	(SENDINFO(m_Width), 8, 0, 0, 256 ),
	SendPropFloat	(SENDINFO(m_Height), 8, 0, 0, 256 ),

END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CShieldFlat::Spawn( )
{
	BaseClass::Spawn();
	m_ShieldState = 0;
	ShieldMoved();
}


//-----------------------------------------------------------------------------
// Computes the shield bounding box
//-----------------------------------------------------------------------------
void CShieldFlat::ShieldMoved( void )
{
	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	VectorMA( GetAbsOrigin(), -m_Width * 0.5, right, m_Pos[0] );
	VectorMA( m_Pos[0], -m_Height * 0.5, up, m_Pos[0] );
	VectorMA( m_Pos[0], m_Width, right, m_Pos[1] );
	VectorMA( m_Pos[0], m_Height, up, m_Pos[2] );
	VectorMA( m_Pos[2], m_Width, right, m_Pos[3] );

	m_LastAngles = GetAbsAngles();
	m_LastPosition = GetAbsOrigin();
}


//-----------------------------------------------------------------------------
// Shield size
//-----------------------------------------------------------------------------
void CShieldFlat::SetSize( float w, float h )
{
	m_Width = w;
	m_Height = h;
	Vector mins( -1.0/16.0f, -w * 0.5f, -h * 0.5f );
	Vector maxs(  1.0/16.0f,  w * 0.5f,  h * 0.5f );
	UTIL_SetSize( this, mins, maxs );
	ShieldMoved();
}


//-----------------------------------------------------------------------------
// Compute world axis-aligned bounding box
//-----------------------------------------------------------------------------
void CShieldFlat::ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs )
{
	TransformAABB( CollisionProp()->CollisionToWorldTransform(), 
		CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), *pWorldMins, *pWorldMaxs );
}


//-----------------------------------------------------------------------------
// Shield points
//-----------------------------------------------------------------------------
const Vector& CShieldFlat::GetPoint( int x, int y ) 
{
	if ((m_LastAngles != GetAbsAngles()) || (m_LastPosition != GetAbsOrigin() ))
	{
		ShieldMoved();
	}

	int i = (x >= 1);
	i += (y >= 1) * 2;
	return m_Pos[i];
}


//-----------------------------------------------------------------------------
// Called when the shield is EMPed
//-----------------------------------------------------------------------------
void CShieldFlat::SetEMPed( bool isEmped )
{
	CShield::SetEMPed(isEmped);
	if (IsEMPed())
	{
		m_ShieldState |= SHIELD_FLAT_EMP;
	}
	else
	{
		m_ShieldState &= ~SHIELD_FLAT_EMP;
	}
}


//-----------------------------------------------------------------------------
// Shield is killed here
//-----------------------------------------------------------------------------
void CShieldFlat::Activate( bool active )
{
	if (active)
	{
		m_ShieldState &= ~SHIELD_FLAT_INACTIVE;
	}
	else
	{
		m_ShieldState |= SHIELD_FLAT_INACTIVE;
	}
	ActivateCollisions( active );
}


//-----------------------------------------------------------------------------
// Purpose: Create a mobile version of the shield
//-----------------------------------------------------------------------------
CShieldFlat* CreateFlatShield( CBaseEntity *pOwner, float w, float h, const Vector& relOrigin, const QAngle &relAngles )
{
	CShieldFlat *pShield = (CShieldFlat*)CreateEntityByName("shield_flat");

	pShield->SetParent( pOwner );
	pShield->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pShield, relOrigin );

	// Compute relative angles....
	pShield->SetLocalAngles( relAngles );
	pShield->ChangeTeam( pOwner->GetTeamNumber() );
	pShield->Spawn();
	pShield->SetSize( w , h );

	return pShield;
}

