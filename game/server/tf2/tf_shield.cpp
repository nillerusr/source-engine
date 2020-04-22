//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Escort's Shield weapon
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_shield.h"
#include "tf_shareddefs.h"
#include "collisionutils.h"
#include <float.h>
#include "sendproxy.h"
#include "mathlib/mathlib.h"

#define PROBE_EFFECT_TIME		0.15f

ConVar	shield_explosive_damage( "shield_explosive_damage","10", FCVAR_REPLICATED, "Shield power damage from explosions" );

// Percentage of total health that the shield's allowed to go below 0
#define SHIELD_MIN_HEALTH_FACTOR		(-0.5)

//-----------------------------------------------------------------------------
// Stores a list of all active shields
//-----------------------------------------------------------------------------
CUtlVector< CShield* >	CShield::s_Shields;


//-----------------------------------------------------------------------------
// Returns true if the entity is a shield
//-----------------------------------------------------------------------------
bool IsShield( CBaseEntity *pEnt )
{
	// This is a faster way...
	return pEnt && (pEnt->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD);
//	return pEnt && FClassnameIs( pEnt, "shield" )
}


//=============================================================================
// Shield effect
//=============================================================================
EXTERN_SEND_TABLE(DT_BaseEntity)

IMPLEMENT_SERVERCLASS_ST(CShield, DT_Shield)
	SendPropInt(SENDINFO(m_nOwningPlayerIndex), MAX_EDICT_BITS, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(m_flPowerLevel), 9, SPROP_ROUNDUP, SHIELD_MIN_HEALTH_FACTOR, 1.0f ),
	SendPropInt(SENDINFO(m_bIsEMPed), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CShield::CShield()
{
	s_Shields.AddToTail(this);
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	SetupRecharge( 0,0,0,0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CShield::~CShield()
{
	int i = s_Shields.Find(this);
	if (i >= 0)
		s_Shields.FastRemove(i);
}

//-----------------------------------------------------------------------------
// Precache !!
//-----------------------------------------------------------------------------
void CShield::Precache()
{
	SetClassname( "shield" );
}

//-----------------------------------------------------------------------------
// Spawn !!
//-----------------------------------------------------------------------------
void CShield::Spawn( void )
{
	m_bIsEMPed = 0;
	SetCollisionGroup( TFCOLLISION_GROUP_SHIELD );

	// Make it translucent
	m_nRenderFX = kRenderTransAlpha;
	SetRenderColorA( 255 );

	m_flNextRechargeTime = gpGlobals->curtime;

	CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );
	SetSolid( SOLID_CUSTOM );

	// Stuff can't come to a rest on shields!
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	UTIL_SetSize( this, vec3_origin, vec3_origin );
}


//-----------------------------------------------------------------------------
// Owner
//-----------------------------------------------------------------------------
void CShield::SetOwnerEntity( CBaseEntity *pOwner )
{
	BaseClass::SetOwnerEntity( pOwner );

	if (pOwner->IsPlayer())
	{
		m_nOwningPlayerIndex = pOwner->entindex();
	}
	else
	{
		m_nOwningPlayerIndex = 0;
	}
	ChangeTeam( pOwner->GetTeamNumber() );
}



//-----------------------------------------------------------------------------
// Does this shield protect from a particular damage type?
//-----------------------------------------------------------------------------
float CShield::ProtectionAmount( int damageType ) const
{
	// As a test, we're trying to make shields impervious to everything
	return 1.0f;
}
		   

//-----------------------------------------------------------------------------
// Activates/deactivates a shield for collision purposes
//-----------------------------------------------------------------------------
void CShield::ActivateCollisions( bool activate )
{
	if ( activate )
	{
		RemoveSolidFlags( FSOLID_NOT_SOLID );
	}
	else
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}
}

//-----------------------------------------------------------------------------
// Activates all shields 
//-----------------------------------------------------------------------------
void CShield::ActivateShields( bool activate, int team )
{
	for (int i = s_Shields.Count(); --i >= 0; )
	{
		// Activate all shields on the same team
		if ( (team == -1) || (team == s_Shields[i]->GetTeamNumber()) )
		{
			s_Shields[i]->ActivateCollisions( activate );
		}
	}
}


//-----------------------------------------------------------------------------
// Checks a ray against shields only
//-----------------------------------------------------------------------------
bool CShield::IsBlockedByShields( const Vector& src, const Vector& end )
{
	trace_t tr;
	Ray_t ray;
	ray.Init( src, end );

	for (int i = s_Shields.Count(); --i >= 0; )
	{
		if (!s_Shields[i]->ShouldCollide( TFCOLLISION_GROUP_WEAPON, MASK_ALL ))
			continue;

		// Coarse bbox test first
		Vector vecAbsMins, vecAbsMaxs;
		s_Shields[i]->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs ); 
		if (!IsBoxIntersectingRay( vecAbsMins, vecAbsMaxs, ray.m_Start, ray.m_Delta ))
			continue;

		if (s_Shields[i]->TestCollision( ray, MASK_ALL, tr ))
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called when we hit something that we deflect...
//-----------------------------------------------------------------------------
void CShield::RegisterDeflection(const Vector& vecDir, int bitsDamageType, trace_t *ptr)
{
	// Probes don't hurt shields
	if (bitsDamageType & DMG_PROBE)
		return;

	Vector normalDir;
	VectorCopy( vecDir, normalDir );
	VectorNormalize( normalDir );

	// Uncomment this line when the client predicts the shield deflections
	//filter.UsePredictionRules();
	EntityMessageBegin( this );
		WRITE_LONG( ptr->hitgroup );
		WRITE_VEC3NORMAL( normalDir );
		WRITE_BYTE( false );	// This was not a partial block
	MessageEnd();

	// If this is a buckshot round, don't pay the power cost to stop it once we've gone over our max buckshot hits this frame
	if ( bitsDamageType & DMG_BUCKSHOT )
	{
		m_iBuckshotHitsThisFrame++;
		if ( m_iBuckshotHitsThisFrame > 4 )
			return;
	}

	// If this is an explosion, it counts for extra
	if ( bitsDamageType & DMG_BLAST )
	{
		SetPower( m_flPower - shield_explosive_damage.GetFloat() );
	}
	else
	{
		// Reduce our power level by a hit
		SetPower( m_flPower - 1 );
	}

	// If we've lost power fully, don't recharge for a bit
	if ( m_flPower <= 0 )
	{
		m_flNextRechargeTime = gpGlobals->curtime + 1.0;
	}
}

//-----------------------------------------------------------------------------
// Called when we hit something that we let through...
//-----------------------------------------------------------------------------
void CShield::RegisterPassThru(const Vector& vecDir, int bitsDamageType, trace_t *ptr)
{
	// Probes don't hurt shields
	if (bitsDamageType & DMG_PROBE)
		return;

	Vector normalDir;
	VectorCopy( vecDir, normalDir );
	VectorNormalize( normalDir );

	EntityMessageBegin( this );
		WRITE_LONG( ptr->hitgroup );
		WRITE_VEC3NORMAL( normalDir );
		WRITE_BYTE( true );	// This was a partial block
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShield::SetPower( float flPower )
{
	m_flPower = MAX( (SHIELD_MIN_HEALTH_FACTOR * m_flMaxPower), flPower );
	if ( m_flPower > m_flMaxPower )
	{
		m_flPower = m_flMaxPower;
	}
	m_flPowerLevel = ( m_flPower / m_flMaxPower );
}

//-----------------------------------------------------------------------------
// Purpose: Setup shield recharging parameters
//-----------------------------------------------------------------------------
void CShield::SetupRecharge( float flPower, float flDelay, float flAmount, float flTickTime )
{ 
	m_flMaxPower = flPower; 
	SetPower( flPower );
	m_flPowerLevel = 1.0;
	m_flRechargeDelay = flDelay;
	m_flRechargeAmount = flAmount;
	m_flRechargeTime = flTickTime;
}

//-----------------------------------------------------------------------------
// Purpose: Recharge the shield if we haven't taken damage for a while
//-----------------------------------------------------------------------------
void CShield::Think( void )
{
	// Clear out buckshot count
	m_iBuckshotHitsThisFrame = 0;

	if ( m_flNextRechargeTime < gpGlobals->curtime )
	{
		if ( m_flPower < m_flMaxPower )
		{
			SetPower( m_flPower + m_flRechargeAmount );
		}

		m_flNextRechargeTime = gpGlobals->curtime + m_flRechargeTime;
	}

	// Let derived objects think if they want to
	if ( m_pfnThink )
	{
		(this->*m_pfnThink)();
	}
}

//-----------------------------------------------------------------------------
// Indicates the shield has been EMPed (or not)
//-----------------------------------------------------------------------------
void CShield::SetEMPed( bool isEmped )
{
	m_bIsEMPed = isEmped;
}

//-----------------------------------------------------------------------------
// Helper method for collision testing
//-----------------------------------------------------------------------------
#pragma warning ( disable : 4701 )

bool CShield::TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace )
{
	// Can't block anything if we're EMPed, or we've got no power left to block
	if ( IsEMPed() )
		return false;
	if ( m_flPower <= 0 )
		return false;

	// Here, we're gonna test for collision.
	// If we don't stop this kind of bullet, we'll generate an effect here
	// but we won't change the trace to indicate a collision.

	// It's just polygon soup...
	int hitgroup;
	bool firstTri;
	int v1[2], v2[2], v3[2];
	float ihit, jhit;
	float mint = FLT_MAX;
	float t;

	int h = Height();
	int w = Width();

	for (int i = 0; i < h - 1; ++i)
	{
		for (int j = 0; j < w - 1; ++j)
		{
			// Don't test if this panel ain't active...
			if (!IsPanelActive( j, i ))
				continue;

			// NOTE: Structure order of points so that our barycentric
			// axes for each triangle are along the (u,v) directions of the mesh
			// The barycentric coords we'll need below 

			// Two triangles per quad...
			t = IntersectRayWithTriangle( ray,
				GetPoint( j, i + 1 ),
				GetPoint( j + 1, i + 1 ),
				GetPoint( j, i ), true );
			if ((t >= 0.0f) && (t < mint))
			{
				mint = t;
				v1[0] = j;		v1[1] = i + 1;
				v2[0] = j + 1;	v2[1] = i + 1;
				v3[0] = j;		v3[1] = i;
				ihit = i; jhit = j;
				firstTri = true;
			}
			
			t = IntersectRayWithTriangle( ray,
				GetPoint( j + 1, i ),
				GetPoint( j, i ),
				GetPoint( j + 1, i + 1 ), true );
			if ((t >= 0.0f) && (t < mint))
			{
				mint = t;
				v1[0] = j + 1;	v1[1] = i;
				v2[0] = j;		v2[1] = i;
				v3[0] = j + 1;	v3[1] = i + 1;
				ihit = i; jhit = j;
				firstTri = false;
			}
		}
	}

	if (mint == FLT_MAX)
		return false;

	// Stuff the barycentric coordinates of the triangle hit into the hit group 
	// For the first triangle, the first edge goes along u, the second edge goes
	// along -v. For the second triangle, the first edge goes along -u,
	// the second edge goes along v.
	const Vector& v1vec = GetPoint(v1[0], v1[1]);
	const Vector& v2vec = GetPoint(v2[0], v2[1]);
	const Vector& v3vec = GetPoint(v3[0], v3[1]);
	float u, v;
	bool ok = ComputeIntersectionBarycentricCoordinates( ray, 
		v1vec, v2vec, v3vec, u, v );
	Assert( ok );
	if ( !ok )
	{
		return false;
	}

	if (firstTri)
		v = 1.0 - v;
	else
		u = 1.0 - u;
	v += ihit; u += jhit;
	v /= (h - 1);
	u /= (w - 1);

	// Compress (u,v) into 1 dot 15, v in top bits
	hitgroup = (((int)(v * (1 << 15))) << 16) + (int)(u * (1 << 15));

	Vector normal;
	float intercept;
	ComputeTrianglePlane( v1vec, v2vec, v3vec, normal, intercept ); 

	UTIL_SetTrace( trace, ray, edict(), mint, hitgroup, CONTENTS_SOLID, normal, intercept );
	VectorAdd( trace.startpos, ray.m_StartOffset, trace.startpos );
	VectorAdd( trace.endpos, ray.m_StartOffset, trace.endpos );

	return true;
}

#pragma warning ( default : 4701 )


