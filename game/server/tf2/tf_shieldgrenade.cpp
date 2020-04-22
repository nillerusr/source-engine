//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "BaseAnimating.h"
#include "tf_shieldgrenade.h"
#include "tf_shieldshared.h"
#include "tf_shield_flat.h"
#include "tf_player.h"
#include "engine/IEngineSound.h"
#include "Sprite.h"

#define SHIELD_GRENADE_FUSE_TIME 2.25f

//-----------------------------------------------------------------------------
//
// The shield grenade class
//
//-----------------------------------------------------------------------------

class CShieldGrenade : public CBaseAnimating
{
	DECLARE_CLASS( CShieldGrenade, CBaseAnimating );
public:
	DECLARE_DATADESC();

	CShieldGrenade();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	void			SetLifetime( float timer );

	int GetDamageType() const { return DMG_ENERGYBEAM; }

	void			StickyTouch( CBaseEntity *pOther );
	void			BeepThink( void );
	void			ShieldActiveThink( void );
	void			DeathThink( void );

	virtual bool CanTakeEMPDamage() { return true; }
	virtual bool TakeEMPDamage( float duration );

private:
	// Check when we're done with EMP
	void	CheckEMPDamageFinish( );
	void	CreateShield( );
	void	ComputeActiveThinkTime( );
	void	BounceSound( );

private:
	// Make sure all grenades explode
	Vector	m_LastCollision;

	// Time when EMP runs out
	float	m_flEMPDamageEndTime;
	float	m_ShieldLifetime;
	float	m_flDetonateTime;

	// Are we EMPed?
	bool	m_IsEMPed;
	bool	m_IsDeployed;

	// The deployed shield
	CHandle<CShieldFlat>	m_hDeployedShield;

	CSprite	*m_pLiveSprite;
};

//-----------------------------------------------------------------------------
// Data table
//-----------------------------------------------------------------------------

// Global Savedata for friction modifier
BEGIN_DATADESC( CShieldGrenade )

	// Function Pointers
	DEFINE_FUNCTION( StickyTouch ),
	DEFINE_FUNCTION( BeepThink ),
	DEFINE_FUNCTION( ShieldActiveThink ),
	DEFINE_FUNCTION( DeathThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_shield, CShieldGrenade );
PRECACHE_REGISTER( grenade_shield );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CShieldGrenade::CShieldGrenade()
{
	UseClientSideAnimation();
	m_hDeployedShield.Set(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldGrenade::Precache( void )
{
	PrecacheModel( "models/weapons/w_grenade.mdl" );

	PrecacheScriptSound( "ShieldGrenade.Bounce" );
	PrecacheScriptSound( "ShieldGrenade.StickBeep" );

	PrecacheModel( "sprites/redglow1.vmt" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldGrenade::Spawn( void )
{
	BaseClass::Spawn();
	m_LastCollision.Init( 0, 0, 0 );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetGravity( 1.0 );
	SetFriction( 0.9 );
	SetModel( "models/weapons/w_grenade.mdl");
	UTIL_SetSize(this, Vector( -4, -4, -4), Vector(4, 4, 4));
	m_IsEMPed = false;
	m_IsDeployed = false;

	m_flEMPDamageEndTime = 0.0f;

	SetTouch( StickyTouch );
	SetCollisionGroup( TFCOLLISION_GROUP_GRENADE );

	// Create a green light
	m_pLiveSprite = CSprite::SpriteCreate( "sprites/redglow1.vmt", GetLocalOrigin() + Vector(0,0,1), false );
	m_pLiveSprite->SetTransparency( kRenderGlow, 0, 0, 255, 128, kRenderFxNoDissipation );
	m_pLiveSprite->SetScale( 1 );
	m_pLiveSprite->SetAttachment( this, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldGrenade::UpdateOnRemove( void )
{
	if ( m_pLiveSprite )
	{
		UTIL_Remove( m_pLiveSprite );
		m_pLiveSprite = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldGrenade::SetLifetime( float timer )
{
	m_ShieldLifetime = timer;
}


//-----------------------------------------------------------------------------
// EMP Related methods
//-----------------------------------------------------------------------------
bool CShieldGrenade::TakeEMPDamage( float duration )
{
	m_flEMPDamageEndTime = gpGlobals->curtime + duration;
	m_IsEMPed = true;
	if (m_hDeployedShield)
	{
		m_hDeployedShield->SetEMPed(true);

		// Recompute the next think time
		ComputeActiveThinkTime();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: See if EMP impairment time has elapsed
//-----------------------------------------------------------------------------
void CShieldGrenade::CheckEMPDamageFinish( void )
{
	if ( !m_flEMPDamageEndTime || gpGlobals->curtime < m_flEMPDamageEndTime )
		return;

	m_flEMPDamageEndTime = 0.0f;
	m_IsEMPed = false;
	if (m_hDeployedShield)
	{
		m_hDeployedShield->SetEMPed(false);

		// Recompute the next think time
		ComputeActiveThinkTime();
	}
}


//-----------------------------------------------------------------------------
// Plays a random bounce sound
//-----------------------------------------------------------------------------
void CShieldGrenade ::BounceSound( void )
{
	EmitSound( "ShieldGrenade.Bounce" );
}

//-----------------------------------------------------------------------------
// Purpose: Make the grenade stick to whatever it touches
//-----------------------------------------------------------------------------
void CShieldGrenade::StickyTouch( CBaseEntity *pOther )
{
	if (m_IsDeployed)
		return;

	// The touch can get called multiple times - create an ignore case if we
	// have already stuck.
	if ( m_LastCollision == GetLocalOrigin() )
		return;

	// Only stick to floors...
	Vector up( 0, 0, 1 );
	if ( DotProduct( GetTouchTrace().plane.normal, up ) < 0.5f )
		return;
	
	// Only stick to BSP models
	if ( pOther->IsBSPModel() == false )
		return;

	BounceSound();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );

	// Beep
	EmitSound( "ShieldGrenade.StickBeep" );

	// Start ticking...
	SetThink( BeepThink );
	m_IsDeployed = true;
	SetNextThink( gpGlobals->curtime + 0.01f );
	m_flDetonateTime = gpGlobals->curtime + SHIELD_GRENADE_FUSE_TIME;

	m_LastCollision = GetLocalOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: Play beeping sounds until the charge explode
//-----------------------------------------------------------------------------
void CShieldGrenade::CreateShield( void )
{
	/*
	// Set the orientation of the shield based on 
	// the closest teammate's relative position...
	float mindist = FLT_MAX;
	Vector dir;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
		if ( pPlayer )
		{
			if (pPlayer->GetTeamNumber() == GetTeamNumber())
			{
				Vector tempdir;
				VectorSubtract( pPlayer->Center(), Center(), tempdir );
				float dist = VectorNormalize( tempdir );
				if (dist < mindist)
				{
					 mindist = dist;
					 VectorCopy( tempdir, dir );
				}
			}
		}
	}

	if( mindist == FLT_MAX )
	{
		AngleVectors( GetAngles(), &dir );
	}

	// Never pitch the shield
	dir.z = 0.0f;
	VectorNormalize(dir);

	QAngle relAngles;
	VMatrix parentMatrix;
	VMatrix worldShieldMatrix;
	VMatrix relativeMatrix;
	VMatrix parentInvMatrix;

	// Construct a transform from shield to grenade (parent)
	MatrixFromAngles( GetAngles(), parentMatrix );

#ifdef _DEBUG
	bool ok = 
#endif
		MatrixInverseGeneral( parentMatrix, parentInvMatrix );
	Assert( ok );

	Vector up( 0, 0, 1 );
	Vector left;
	CrossProduct( up, dir, left );
	MatrixSetIdentity( worldShieldMatrix );
	worldShieldMatrix.SetUp( up );
	worldShieldMatrix.SetLeft( left );
	worldShieldMatrix.SetForward( dir );

	MatrixMultiply( parentInvMatrix, worldShieldMatrix, relativeMatrix );
	MatrixToAngles( relativeMatrix, relAngles );
	*/
	
	Vector offset( 0, 0, SHIELD_GRENADE_HEIGHT * 0.5f );
	m_hDeployedShield = CreateFlatShield( this, SHIELD_GRENADE_WIDTH, 
		SHIELD_GRENADE_HEIGHT, offset, vec3_angle );

	// Notify it about EMP state
	if (m_IsEMPed)
		m_hDeployedShield->SetEMPed(true);

	// Play a sound
//	WeaponSound( SPECIAL1 );
}


//-----------------------------------------------------------------------------
// Purpose: Play beeping sounds until the charge explode
//-----------------------------------------------------------------------------
void CShieldGrenade::BeepThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if (m_flDetonateTime <= gpGlobals->curtime)
	{
		// Here we must project the shield
		CreateShield();
		SetThink( ShieldActiveThink );
		m_flDetonateTime = gpGlobals->curtime + m_ShieldLifetime;

		// Get the EMP state correct
		CheckEMPDamageFinish();
		ComputeActiveThinkTime();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
}


//-----------------------------------------------------------------------------
// Compute next think time while active
//-----------------------------------------------------------------------------
void CShieldGrenade::ComputeActiveThinkTime( void )
{
	// Next think should be when we detonate, unless we un-EMP before then
	SetNextThink( gpGlobals->curtime + m_flDetonateTime );
	if (m_IsEMPed)
	{
		Assert( m_flEMPDamageEndTime != 0.0f );
		if ( m_flEMPDamageEndTime < GetNextThink() )
			SetNextThink( gpGlobals->curtime + m_flEMPDamageEndTime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Here's where the grenade "detonates"
//-----------------------------------------------------------------------------
void CShieldGrenade::ShieldActiveThink( void )
{
	if (m_flDetonateTime > gpGlobals->curtime)
	{
		// If it's not time to die, check EMP state
		CheckEMPDamageFinish();
	}
	else
	{
		if (m_hDeployedShield)
		{
			m_hDeployedShield->Activate( false );
		}
		SetNextThink( gpGlobals->curtime + SHIELD_FLAT_SHUTDOWN_TIME + 0.2f );
		SetThink( DeathThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CShieldGrenade::DeathThink( void )
{
	// kill the grenade
	if (m_hDeployedShield)
	{
		UTIL_Remove( m_hDeployedShield );
		m_hDeployedShield.Set(0);
	}
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Creates a shield grenade
//-----------------------------------------------------------------------------
CBaseEntity *CreateShieldGrenade( const Vector &position, const QAngle &angles,	const Vector &velocity, const QAngle &angVelocity, CBaseEntity *pOwner, float timer )
{
	CShieldGrenade *pGrenade = (CShieldGrenade *)CBaseEntity::Create( "grenade_shield", position, angles, pOwner );
	pGrenade->SetLifetime( timer );
	pGrenade->SetAbsVelocity( velocity );

	if (pOwner)
	{
		pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	}

	return pGrenade;
}