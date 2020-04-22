//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "tf_gamerules.h"
#include "weapon_combatshield.h"

#if defined( CLIENT_DLL )

#include "particles_simple.h"
#include "fx.h"
#include "fx_quad.h"
#include "clienteffectprecachesystem.h"

#define CWeaponArcWelder C_WeaponArcWelder
#else
#endif

#include "weapon_repairgun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



// Buff ranges
ConVar weapon_arcwelder_target_range( "weapon_arcwelder_target_range", "90", FCVAR_REPLICATED, "The farthest away you can be for the arcwelder to initially lock onto a target." );
ConVar weapon_arcwelder_stick_range( "weapon_arcwelder_stick_range", "100", FCVAR_REPLICATED, "How far away the arcwelder can stay locked onto someone." );
ConVar weapon_arcwelder_rate( "weapon_arcwelder_rate", "15", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponArcWelder : public CWeaponRepairGun
{
	DECLARE_CLASS( CWeaponArcWelder, CWeaponRepairGun );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponArcWelder( void );

	virtual void Precache();

	virtual float	GetTargetRange( void );
	virtual float	GetStickRange( void );
	virtual float	GetHealRate( void );
	virtual bool	AppliesModifier( void ) { return false; }
	virtual bool	TargetsPlayers( void ) { return false; }
	virtual CBaseEntity *GetTargetToHeal( CBaseEntity *pCurHealing );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual void	ClientThink( void );
	virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void	ViewModelDrawn( C_BaseViewModel *pViewModel );

	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif

private:
	bool		m_bWelding;

#if defined( CLIENT_DLL )
	float		m_flNextEffectTime;
#endif

private:														
	CWeaponArcWelder( const CWeaponArcWelder & );
};

LINK_ENTITY_TO_CLASS( weapon_arcwelder, CWeaponArcWelder );

PRECACHE_WEAPON_REGISTER( weapon_arcwelder );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponArcWelder, DT_WeaponArcWelder )

BEGIN_NETWORK_TABLE( CWeaponArcWelder, DT_WeaponArcWelder )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponArcWelder )
END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponArcWelder::CWeaponArcWelder()
{
	SetPredictionEligible( true );
	m_bWelding = false;
#ifdef CLIENT_DLL
	m_flNextEffectTime = 0;
#endif
}

void CWeaponArcWelder::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "WeaponRepairGun.Healing" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponArcWelder::GetTargetRange( void )
{
	return weapon_arcwelder_target_range.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponArcWelder::GetStickRange( void )
{
	return weapon_arcwelder_target_range.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponArcWelder::GetHealRate( void )
{
	return weapon_arcwelder_rate.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to a healable target
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponArcWelder::GetTargetToHeal( CBaseEntity *pCurHealing )
{
	CBaseEntity *pTarget = BaseClass::GetTargetToHeal(pCurHealing);
	if ( !pTarget )
		return pTarget;

	// Make sure the target is within our field of view
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return NULL;

	Vector vecAiming;
	pOwner->EyeVectors( &vecAiming );

	// Find a player in range of this player, and make sure they're healable.
	Vector vecSrc = pOwner->Weapon_ShootPosition( );
	Vector vecEnd = vecSrc + vecAiming * GetTargetRange();
	trace_t tr;

	// Use WeaponTraceLine so shields are tested...
	TFGameRules()->WeaponTraceLine( vecSrc, vecEnd, (MASK_SHOT & ~CONTENTS_HITBOX), pOwner, DMG_PROBE, &tr );
	if ( tr.fraction != 1.0 && tr.m_pEnt == pTarget )
		return pTarget;

	return NULL;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponArcWelder::ClientThink( void )
{
	CBasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( m_hHealingTarget == NULL )
		return;

	// Don't show it while the player is dead. Ideally, we'd respond to m_bHealing in OnDataChanged,
	// but it stops sending the weapon when it's holstered, and it gets holstered when the player dies.
	C_BasePlayer *pFiringPlayer = dynamic_cast< C_BasePlayer* >( GetOwner() );
	if ( !pFiringPlayer || pFiringPlayer->IsPlayerDead() )
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_NEVER );
		m_bPlayingSound = false;
		StopRepairSound();
		return;
	}

	// Start playing the heal sound, if we're not already
	if ( !m_bPlayingSound )
	{
		m_bPlayingSound = true;
		CLocalPlayerFilter filter;
		EmitSound( filter, entindex(), "WeaponRepairGun.Healing" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponArcWelder::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( !pPlayer )
		return true;

	switch ( event )
	{
	case 7001:
		m_bWelding = true;
		return true;
	case 7002:
		m_bWelding = false;
		return true;
	default:
		break;
	};

	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
}

CLIENTEFFECT_REGISTER_BEGIN( PrecacheArcWelderEffect )
CLIENTEFFECT_MATERIAL( "particle/smoke_arcwelder" )
CLIENTEFFECT_MATERIAL( "effects/spark2" )
CLIENTEFFECT_MATERIAL( "effects/blueflare" )
CLIENTEFFECT_MATERIAL( "effects/blueflare2" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponArcWelder::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
	if ( !m_bWelding || !m_hHealingTarget.Get() )
		return;

	if ( m_flNextEffectTime > gpGlobals->curtime )
		return;
	m_flNextEffectTime = gpGlobals->curtime + 0.1;

	// Get our weldpoint
	Vector attachOrigin;
	QAngle attachAngles; 
	pViewModel->GetAttachment( pViewModel->LookupAttachment("muzzle"), attachOrigin, attachAngles );
	Vector vecEnd = m_hHealingTarget->WorldSpaceCenter();
	trace_t tr;

	// Use WeaponTraceLine so shields are tested...
	TFGameRules()->WeaponTraceLine( attachOrigin, vecEnd, (MASK_SHOT & ~CONTENTS_HITBOX), GetOwner(), DMG_PROBE, &tr );

	// Smoke
	unsigned char color[3];
	int iColOff = random->RandomInt(-16,16);
	color[0] = 120 + iColOff;
	color[1] = 230 + iColOff;
	color[2] = 235 + iColOff;
	/*
	// Pull out from the target a bit
	Vector vecOrigin = vecEnd;
	Vector vecFromTarget = (vecEnd - attachOrigin);
	VectorNormalize( vecFromTarget );
	vecOrigin -= (vecFromTarget * 24);
	*/

	Vector vecOrigin = attachOrigin;
	// Velocity
	Vector vecVelocity = tr.plane.normal;
	vecVelocity.z += random->RandomFloat( 8, 12 );
	// Add it
	CSmartPtr<CSimpleEmitter> pSimple = FX_Smoke( vecOrigin, vecVelocity, 
		random->RandomFloat( 4, 8 ),		// Scale
		1,	
		random->RandomFloat( 0.5, 3.0 ),	// Dietime
		color, 
		random->RandomInt( 64, 200 ),		// Alpha
		"particle/smoke_arcwelder",	
		random->RandomInt(0,255),			// Roll
		0 );								// Rolldelta

	// Sparks
	FX_Sparks( vecOrigin, 1, 4, tr.plane.normal, 2.5, 8, 64, "effects/spark2" );

	// Bright Glow
	SimpleParticle *sParticle;
	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/blueflare" ), vecOrigin );
	if ( sParticle == NULL )
		return;
	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 0.5f;
	sParticle->m_vecVelocity.Init();
	sParticle->m_uchColor[0]	= 255;
	sParticle->m_uchColor[1]	= 255;
	sParticle->m_uchColor[2]	= 255;
	sParticle->m_uchStartSize	= random->RandomInt(5,7);
	sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
	sParticle->m_flRoll			= random->RandomInt(0,360);
	sParticle->m_flRollDelta	= 0;

	// Dull Glow
	sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/blueflare2" ), vecOrigin );
	if ( sParticle == NULL )
		return;
	sParticle->m_flLifetime		= 0.0f;
	sParticle->m_flDieTime		= 0.2f;
	sParticle->m_vecVelocity.Init();
	sParticle->m_uchColor[0]	= 255;
	sParticle->m_uchColor[1]	= 255;
	sParticle->m_uchColor[2]	= 255;
	sParticle->m_uchStartSize	= random->RandomInt(15,20);
	sParticle->m_uchEndSize		= sParticle->m_uchStartSize;
	sParticle->m_flRoll			= random->RandomInt(0,360);
	sParticle->m_flRollDelta	= 0;
}
#endif
