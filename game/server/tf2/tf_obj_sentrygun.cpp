//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defender's sentrygun objects
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_obj_sentrygun.h"
#include "tf_obj_dragonsteeth.h"
#include "tf_obj_tower.h"
#include "tf_obj_sandbag_bunker.h"
#include "tf_obj_bunker.h"
#include "tf_obj_mapdefined.h"
#include "tf_gamerules.h"
#include "gamerules.h"
#include "ammodef.h"
#include "plasmaprojectile.h"
#include "tf_class_recon.h"
#include "sendproxy.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "grenade_rocket.h"
#include "VGuiScreen.h"

extern short	g_sModelIndexFireball;

#define	MAX_SUPPRESSION_TIME	5.0			// Max amount of time to supress for

// Sentrygun size
#define SENTRYGUN_MINS		Vector(-16, -16, 0)
#define SENTRYGUN_MAXS		Vector( 16,  16, 65)

//=============================================================================
// Link and precache all the sentrygun types
LINK_ENTITY_TO_CLASS(obj_sentrygun_plasma, CObjectSentrygunPlasma);
LINK_ENTITY_TO_CLASS(obj_sentrygun_rocketlauncher, CObjectSentrygunRocketlauncher);
PRECACHE_REGISTER(obj_sentrygun_plasma);
PRECACHE_REGISTER(obj_sentrygun_rocketlauncher);

//=============================================================================
// Data description
BEGIN_DATADESC( CObjectSentrygun )

	DEFINE_THINKFUNC( SentryRotate ),
	DEFINE_THINKFUNC( Attack ),

END_DATADESC()

// Sentrygun team-only vars.
BEGIN_SEND_TABLE_NOBASE( CObjectSentrygun, DT_SentrygunTeamOnlyVars )
	SendPropInt( SENDINFO(m_iAmmo), 9 ),
END_SEND_TABLE()

#define SENTRY_ANIMATION_PARITY_BITS	2

IMPLEMENT_SERVERCLASS_ST(CObjectSentrygun, DT_ObjectSentrygun)
	SendPropInt( SENDINFO( m_iBaseTurnRate ), 3, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hEnemy ) ),
	SendPropDataTable( "teamonly", 0, &REFERENCE_SEND_TABLE( DT_SentrygunTeamOnlyVars ), SendProxy_OnlyToTeam ),	
	SendPropInt( SENDINFO(m_bTurtled), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nAnimationParity ), (1<<SENTRY_ANIMATION_PARITY_BITS), SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nOrientationParity ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE();

ConVar	obj_sentrygun_plasma_health( "obj_sentrygun_plasma_health","200", FCVAR_NONE, "Plasma sentrygun health" );
ConVar	obj_sentrygun_plasma_range( "obj_sentrygun_plasma_range","1500", FCVAR_NONE, "Plasma sentrygun's shot range" );
ConVar	obj_sentrygun_rocketlauncher_health( "obj_sentrygun_rocketlauncher_health","250", FCVAR_NONE, "Rocket Launcher sentrygun health" );
ConVar	obj_sentrygun_range_mid( "obj_sentrygun_range_mid","768", FCVAR_NONE, "Sentrygun's mid targeting range. Targets beyond this need to be in the viewcone to be seen." );
ConVar	obj_sentrygun_range_max( "obj_sentrygun_range_max","1600", FCVAR_NONE, "Sentrygun's max targeting range." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectSentrygun::CObjectSentrygun( void )
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::Spawn( void )
{
	m_bSmarter = false;
	m_bSensors = false;
	m_bSuppressing = false;
	m_bTurtled = false;
	m_bTurtling = false;
	m_flTurtlingFinishedAt = 0;

	SetViewOffset( Vector(0,0,22) );

	// Setup
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	UTIL_SetSize(this, SENTRYGUN_MINS, SENTRYGUN_MAXS);

	BaseClass::Spawn();

	// Start searching for enemies
	m_hEnemy = NULL;
	m_hDesignatedEnemy = NULL;
	SetThink( SentryRotate );
	SetNextThink( gpGlobals->curtime + 0.5f );
	m_flNextLook = gpGlobals->curtime;
	
	SetTechnology( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::Precache()
{
	PrecacheModel( SG_PLASMA_MODEL );
	PrecacheModel( SG_ROCKETLAUNCHER_MODEL );
	PrecacheVGuiScreen( "screen_obj_sentrygun" );

	PrecacheScriptSound( "ObjectSentrygun.ResupplyAmmo" );
	PrecacheScriptSound( "ObjectSentrygun.Idle" );
	PrecacheScriptSound( "ObjectSentrygun.FoundTarget" );
	PrecacheScriptSound( "ObjectSentrygun.Turtle" );
	PrecacheScriptSound( "ObjectSentrygun.UnTurtle" );
	PrecacheScriptSound( "ObjectSentrygun.Fire" );
	PrecacheScriptSound( "ObjectSentrygunRocketlauncher.Fire" );

}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectSentrygun::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_sentrygun";
}

//-----------------------------------------------------------------------------
// Purpose: Hide the base of the gun if it's on an attachment
//-----------------------------------------------------------------------------
void CObjectSentrygun::SetupAttachedVersion( void )
{
	BaseClass::SetupAttachedVersion();

	SetBodygroup( 1, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::SetupUnattachedVersion( void )
{
	BaseClass::SetupUnattachedVersion();

	SetBodygroup( 1, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	// Orient it
	m_vecCurAngles.y = UTIL_AngleMod( GetLocalAngles().y );
	RecomputeOrientation();
}

//-----------------------------------------------------------------------------
// Called when a rotation happens
//-----------------------------------------------------------------------------
void CObjectSentrygun::RecomputeOrientation( )
{
	ResetOrientation();

	m_iRightBound = UTIL_AngleMod( m_vecCurAngles.y - 50);
	m_iLeftBound = UTIL_AngleMod( m_vecCurAngles.y + 50);
	if ( m_iRightBound > m_iLeftBound )
	{
		m_iRightBound = m_iLeftBound;
		m_iLeftBound = UTIL_AngleMod( m_vecCurAngles.y - 50);
	}

	// Start it rotating
	m_vecGoalAngles.y = m_iRightBound;
	m_vecGoalAngles.x = m_vecCurAngles.x = 0;
	m_bTurningRight = true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CObjectSentrygun::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	if ( FStrEq( pCmd, "addammo" ) )
	{
		if ( TakeAmmoFrom( pPlayer ) )
		{
			// We got some ammo, so make a sound
			CPASAttenuationFilter filter( pPlayer, "ObjectSentrygun.ResupplyAmmo" );
			EmitSound( filter, pPlayer->entindex(), "ObjectSentrygun.ResupplyAmmo" );
		}
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the player gave the sentrygun some ammo
//-----------------------------------------------------------------------------
bool CObjectSentrygun::TakeAmmoFrom( CBaseTFPlayer *pPlayer )
{
	// Do I need ammo?
	if ( m_iAmmo >= m_iMaxAmmo )
		return false;

	// Try to fill the sentry up a bit at a time
	int iRoundsToGive = 10;
	iRoundsToGive = MIN( iRoundsToGive, (m_iMaxAmmo - m_iAmmo) );
	iRoundsToGive = MIN( iRoundsToGive, pPlayer->GetAmmoCount( m_iAmmoType ) );
	if ( !iRoundsToGive )
		return false;

	// Give me the ammo
	pPlayer->RemoveAmmo( iRoundsToGive, m_iAmmoType );
	m_iAmmo += iRoundsToGive;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Resupply has taken damage
//-----------------------------------------------------------------------------
int CObjectSentrygun::OnTakeDamage( const CTakeDamageInfo &info )
{
	int iDamage = BaseClass::OnTakeDamage( info );

	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: Object has been blown up
//-----------------------------------------------------------------------------
void CObjectSentrygun::Killed( void )
{
	// Tell the player he's lost this resupply beacon
	if ( GetOwner() )
	{
		GetOwner()->OwnedObjectDestroyed( this );
	}

	BaseClass::Killed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::RestartAnimation( void )
{
	// Increment and mask parity counter
	m_nAnimationParity += 1;
	m_nAnimationParity &= ( (1<<SENTRY_ANIMATION_PARITY_BITS) - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::ResetOrientation()
{
	m_nOrientationParity = !m_nOrientationParity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::SetSentryAnim( TFTURRET_ANIM anim )
{
	if ( GetSequence() != anim )
	{
		switch(anim)
		{
		case TFTURRET_ANIM_FIRE:
		case TFTURRET_ANIM_SPIN:
			if ( GetSequence() != TFTURRET_ANIM_FIRE && GetSequence() != TFTURRET_ANIM_SPIN )
			{
				RestartAnimation();
			}
			break;
		default:
			RestartAnimation();
			break;
		}

		ResetSequence( anim );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle movement of the turret
//-----------------------------------------------------------------------------
bool CObjectSentrygun::MoveTurret(void)
{
	bool bMoved = 0;

	// any x movement?
	if ( m_vecCurAngles.x != m_vecGoalAngles.x )
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1 ;

		m_vecCurAngles.x += 0.1 * (m_iBaseTurnRate * 5) * flDir;

		// if we started below the goal, and now we're past, peg to goal
		if (flDir == 1)
		{
			if (m_vecCurAngles.x > m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		} 
		else
		{
			if (m_vecCurAngles.x < m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		m_fBoneYRotator = m_vecCurAngles.x;

		bMoved = 1;
	}

	if ( m_vecCurAngles.y != m_vecGoalAngles.y )
	{
		float flDir = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1 ;
		float flDist = fabs(m_vecGoalAngles.y - m_vecCurAngles.y);
		bool bReversed = false;
		
		if (flDist > 180)
		{
			flDist = 360 - flDist;
			flDir = -flDir;
			bReversed = true;
		}

		if (m_hEnemy == NULL && !m_bSuppressing)
		{
			if (flDist > 30)
			{
				if (m_fTurnRate < m_iBaseTurnRate * 20)
				{
					m_fTurnRate += m_iBaseTurnRate;
				}
			}
			else
			{
				// Slow down
				if ( m_fTurnRate > (m_iBaseTurnRate * 5) )
					m_fTurnRate -= m_iBaseTurnRate;
			}
		}
		else
		{
			// When tracking enemies, move faster and don't slow
			if (flDist > 30)
			{
				if (m_fTurnRate < m_iBaseTurnRate * 30)
				{
					m_fTurnRate += m_iBaseTurnRate * 3;
				}
			}
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		// if we passed over the goal, peg right to it now
		if (flDir == -1)
		{
			if ( (bReversed == false && m_vecGoalAngles.y > m_vecCurAngles.y) || (bReversed == true && m_vecGoalAngles.y < m_vecCurAngles.y) )
				m_vecCurAngles.y = m_vecGoalAngles.y;
		} 
		else
		{
			if ( (bReversed == false && m_vecGoalAngles.y < m_vecCurAngles.y) || (bReversed == true && m_vecGoalAngles.y > m_vecCurAngles.y) )
				m_vecCurAngles.y = m_vecGoalAngles.y;
		}

		if (m_vecCurAngles.y < 0)
			m_vecCurAngles.y += 360;
		else if (m_vecCurAngles.y >= 360)
			m_vecCurAngles.y -= 360;

		if (flDist < (0.05 * m_iBaseTurnRate))
			m_vecCurAngles.y = m_vecGoalAngles.y;

		m_fBoneXRotator = m_vecCurAngles.y - GetLocalAngles().y;

		bMoved = 1;
	}

	if ( !bMoved || !m_fTurnRate )
	{
		m_fTurnRate = m_iBaseTurnRate;
	}

	return bMoved;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true is the passed ent is in the caller's forward view cone. 
// The dot product is performed in 2d, making the view cone infinitely tall. 
//-----------------------------------------------------------------------------
bool CObjectSentrygun::FInViewCone( CBaseEntity *pEntity )
{
	float		flDot;

	Vector vecFacingDir;
	AngleVectors( m_vecCurAngles, &vecFacingDir );
	Vector vecLOS = ( pEntity->GetAbsOrigin() - GetAbsOrigin() );
	flDot = DotProduct( vecLOS , vecFacingDir );

	if ( flDot > VIEW_FIELD_NARROW )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check the shield's values
//-----------------------------------------------------------------------------
void CObjectSentrygun::CheckShield( void )
{
	if ( m_nRenderFX == kRenderFxNone )
		return;
}

//-----------------------------------------------------------------------------
// Purpose: Rotate and scan for targets
//-----------------------------------------------------------------------------
void CObjectSentrygun::SentryRotate( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	CheckShield();

	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	// If we're turtling, see if we're finished yet
	if ( IsTurtling() )
	{
		if ( m_flTurtlingFinishedAt <= gpGlobals->curtime )
		{
			m_bTurtling = false;
			if ( m_bTurtled )
			{
				AddSolidFlags( FSOLID_NOT_SOLID );
				m_takedamage = DAMAGE_NO;
			}
		}
		return;
	}

	// Turtling sentryguns don't think
	if ( IsTurtled() )
		return;

	// animate
	SetSentryAnim( TFTURRET_ANIM_SPIN );

	// Abort if it's not time to search for enemies
	if ( m_flNextLook < gpGlobals->curtime )
	{
		m_flNextLook = gpGlobals->curtime + 1.0;

		// Look for a target
		m_hEnemy = FindTarget();

		if ( m_hEnemy != NULL )
		{
			FoundTarget();
			return;
		}
	}

	// Rotate
	if ( !MoveTurret() )
	{
		// Play a sound occasionally
		if ( random->RandomFloat(0, 1) < 0.02 )
		{
			EmitSound( "ObjectSentrygun.Idle" );
		}

		// Switch rotation direction
		if (m_bTurningRight)
		{
			m_bTurningRight = false;
			m_vecGoalAngles.y = m_iLeftBound;
		}
		else
		{
			m_bTurningRight = true;
			m_vecGoalAngles.y = m_iRightBound;
		}

		// Randomly look up and down a bit
		if ( random->RandomFloat(0, 1) < 0.3 )
		{
			m_vecGoalAngles.x = (int)random->RandomFloat(-10,10);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if there's a valid target in sight
//-----------------------------------------------------------------------------
CBaseEntity *CObjectSentrygun::FindTarget( void )
{
	CBaseEntity *pHighestPriorityTarget = NULL;
	float fHighestPriority = 0;

	// If I have a designated enemy, and it's valid, assume it will be the target, unless something higher priority shows up.
	if ( m_hDesignatedEnemy.Get() && ValidTarget( m_hDesignatedEnemy) )
	{
		fHighestPriority = GetPriority( m_hDesignatedEnemy );
		pHighestPriorityTarget = m_hDesignatedEnemy;
	}

	// Find a target.
	CBaseEntity		*pList[1024];
	Vector			delta( 2048, 2048, 2048 );
	int count = UTIL_EntitiesInBox( pList, 1024, GetAbsOrigin() - delta, GetAbsOrigin() + delta, FL_CLIENT|FL_NPC|FL_OBJECT );
	
	for ( int i = 0; i < count; i++ )
	{
		if( !pList[i] ) 
			continue;

		if ( pList[i] == this )
			continue;

		float fPriority = GetPriority( pList[i] );

		if( !pHighestPriorityTarget || (fPriority > fHighestPriority) )
		{

			if ( ValidTarget( pList[i] ) )
			{
				pHighestPriorityTarget = pList[i];
				fHighestPriority = fPriority;
			}
		}
	}

	return pHighestPriorityTarget;
}

//-----------------------------------------------------------------------------
// Purpose: Get the priority of the target
//-----------------------------------------------------------------------------
float CObjectSentrygun::GetPriority( CBaseEntity *pTarget )
{
	// Players
	if ( pTarget->IsPlayer() )
	{
		return 20;
	}

	// NPCs
	if ( pTarget->GetFlags() & FL_NPC )
		return 10;

	// Objects
	if ( pTarget->Classify() == CLASS_MILITARY )
	{
		// Sentryguns are highest priority
		CBaseObject *pObject = (CBaseObject *)pTarget;
		if ( pObject->IsSentrygun() )
			return 5;
		return 2;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the sentry targeting range the target is in
//-----------------------------------------------------------------------------
int CObjectSentrygun::Range( CBaseEntity *pTarget )
{
	Vector vecOrg = EyePosition();
	Vector vecTargetOrg = pTarget->EyePosition();

	int iDist = ( vecTargetOrg - vecOrg ).Length();
	
	// Sensors increase targeting range
	if ( m_bSensors )
	{
		iDist *= 0.75;
	}

	if (iDist < obj_sentrygun_range_mid.GetFloat() )
		return RANGE_NEAR;
	if (iDist < obj_sentrygun_range_max.GetFloat() )
		return RANGE_MID;
	return RANGE_FAR;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if a target's valid
//-----------------------------------------------------------------------------
bool CObjectSentrygun::ValidTarget( CBaseEntity *pTarget )
{
	// Make sure we aren't borked:
	if ( !pTarget )
		return false;

	// Don't attack things that have already died
	if ( !pTarget->IsAlive() )
		return false;

	// Don't attack things that cant be hurt
	if ( pTarget->m_takedamage != DAMAGE_YES )
		return false;

	// Don't shoot at objects on the neutral team.
	if( !pTarget->IsInAnyTeam() )
		return false;



	// Ignore camoed players
	if ( pTarget->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)pTarget;
		
		if ( InSameTeam( pPlayer ) )
			return false;
		if ( pPlayer->IsClass( TFCLASS_UNDECIDED ) )
			return false;
		if ( pPlayer->GetCamouflageAmount() >= 30.0f )
			return false;

	}
	else
	{
		// Only attack enemies.
		if ( InSameTeam( pTarget) )
			return false;
	}

	if ( pTarget->GetFlags() & FL_NOTARGET )
		return false;

	if ( !FVisible(pTarget) )	
		return false;

	// Ignore certain enemy infrastructure type objects:
	CBaseObject *pObject = dynamic_cast< CBaseObject* >(pTarget);
	if ( pObject )
	{
		// Make sure it's not placing
		if ( pObject->IsPlacing() )
			return false;

		// Ignore upgrades
		if ( pObject->IsAnUpgrade() )
			return false;

		// Ignore defensive structures
		if ( IsObjectADefensiveBuilding( pObject->GetType() ) )
			return false;

		// Ignore mapdefined objects
		if ( pObject->GetType() == OBJ_MAPDEFINED )
			return false;
	}

	// Make sure there's nothing inbetween us
	Vector vecSrc = EyePosition();

	// Now make sure there isn't something other than team players in the way.
	trace_t tr;
	CTraceFilterSimpleList sentryFilter( COLLISION_GROUP_NONE );
	sentryFilter.AddEntityToIgnore( GetOwner() );
	sentryFilter.AddEntityToIgnore( this );
	sentryFilter.AddEntityToIgnore( GetMoveParent() );
	UTIL_TraceLine( vecSrc, pTarget->WorldSpaceCenter(), MASK_SHOT, &sentryFilter, &tr );
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( (tr.fraction < 1.0) && ( pEntity != pTarget ) )
		return false;

	int iRange = Range(pTarget);
	if ( iRange == RANGE_FAR )
		return false;

	// Better sensors allow them to track irrespective of facing
	if ( iRange == RANGE_MID && (!FInViewCone(pTarget) && !m_bSensors) )
		return false;

	// Don't shoot at turtled sentry guns.
	CObjectSentrygun *pSentry = dynamic_cast< CObjectSentrygun* >( pTarget );
	if ( pSentry  && pSentry->IsTurtled() )
		return false;

	// Don't shoot at targets blocked by enemy shields
	bool bBlocked = TFGameRules()->IsBlockedByEnemyShields( GetAbsOrigin(), pTarget->GetAbsOrigin(), GetTeamNumber() );
	if( bBlocked )
		return false;

		
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the sentry has some ammo to fire
//-----------------------------------------------------------------------------
bool CObjectSentrygun::HasAmmo( void ) 
{
	return (m_iAmmo > 0);
}

//-----------------------------------------------------------------------------
// Purpose: We've found a valid target
//-----------------------------------------------------------------------------
void CObjectSentrygun::FoundTarget()
{
	if ( HasAmmo() )
	{
		EmitSound( "ObjectSentrygun.FoundTarget" );
	}

	SetThink( Attack );
	SetNextThink( gpGlobals->curtime + 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: Make sure our target is still valid, and if so, fire at it
//-----------------------------------------------------------------------------
void CObjectSentrygun::Attack( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	CheckShield();

	// Turtling sentryguns don't attack
	if ( IsTurtled() )
	{
		SetThink( SentryRotate );
		return;
	}

	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	// Make sure our target is still valid and that we've still got ammo
	if ( m_bSuppressing && HasAmmo() )
	{
		if ( gpGlobals->curtime > (m_flStartedSuppressing + MAX_SUPPRESSION_TIME) )
		{
			m_bSuppressing = false;
			SetThink( SentryRotate );
			return;
		}

		// Check to see if we can find a valid target to switch to
		m_hEnemy = FindTarget();
		if ( m_hEnemy )
		{
			// Stop supressing and target this new enemy
			m_bSuppressing = false;
			m_flStartedSuppressing = 0;
			FoundTarget();
		}
	}
	else if ( !ValidTarget(m_hEnemy) || HasAmmo() == false )
	{
		m_hEnemy = NULL;

		// Smarter sentryguns will suppression fire for a few seconds
		if ( m_bSmarter && WillSuppress() && HasAmmo() )
		{
			m_bSuppressing = true;
			m_flStartedSuppressing = gpGlobals->curtime;
		}
		else
		{
			RecomputeOrientation();
			SetThink( SentryRotate );
			return;
		}
	}

	// If I have a designated enemy, and it's valid, target it instead of my current enemy
	if ( m_hDesignatedEnemy.Get() && (m_hEnemy.Get() != m_hDesignatedEnemy.Get()) )
	{
		if ( ValidTarget( m_hDesignatedEnemy ) )
		{
			m_hEnemy = m_hDesignatedEnemy;
			FoundTarget();
		}
	}

	// Figure out where we're firing at
	Vector vecMid = EyePosition();
	if ( m_bSuppressing )
	{
		// Suppression fire should just shoot at it's last known position
		m_vecFireTarget = m_vecLastKnownPosition;
	}
	else
	{
		m_vecFireTarget = m_hEnemy->BodyTarget( vecMid );
		m_vecLastKnownPosition = m_vecFireTarget;
	}
	Vector vecDirToEnemy = m_vecFireTarget - vecMid;
	QAngle angToTarget;
	VectorAngles(vecDirToEnemy, angToTarget);
	
	angToTarget.y = UTIL_AngleMod( angToTarget.y );
	if (angToTarget.x < -180)
		angToTarget.x += 360;
	if (angToTarget.x > 180)
		angToTarget.x -= 360;

	// now all numbers should be in [1...360]
	// pin to turret limitations to [-50...50]
	if (angToTarget.x > 50)
		angToTarget.x = 50;
	else if (angToTarget.x < -50)
		angToTarget.x = -50;
	m_vecGoalAngles.y = angToTarget.y;
	m_vecGoalAngles.x = angToTarget.x;
	
	MoveTurret();

	// Fire on the target if it's within 10 units of being aimed right at it
	if ( m_flNextAttack <= gpGlobals->curtime && (m_vecGoalAngles - m_vecCurAngles).Length() <= 15 )
	{
		// Suppressing turrets fire randomly
		if ( m_bSuppressing )
		{
			if ( random->RandomInt( 0,1 ) != 0 )
			{
				m_flNextAttack = gpGlobals->curtime + 0.5;
				return;
			}
		}

		// See if the object or its owner is taking emp damage, if so, don't fire
		if ( ShouldBeActive() )
		{
			Fire();
		}
	}
	else
	{
		SetSentryAnim( TFTURRET_ANIM_SPIN );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a rotation happens
//-----------------------------------------------------------------------------
void CObjectSentrygun::ObjectMoved( void )
{
	m_vecCurAngles.y = UTIL_AngleMod( GetLocalAngles().y );
	RecomputeOrientation();
	m_fBoneXRotator = 0;
	BaseClass::ObjectMoved();
}

//-----------------------------------------------------------------------------
// Purpose: Fire at our target
//-----------------------------------------------------------------------------
bool CObjectSentrygun::Fire( void )
{
	// Base sentry doesn't know how to fire
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Tell this sentrygun to attack the following target, if it can
//-----------------------------------------------------------------------------
void CObjectSentrygun::DesignateTarget( CBaseEntity *pTarget )
{
	m_hDesignatedEnemy = pTarget;

	if ( m_hEnemy.Get() != m_hDesignatedEnemy.Get() )
	{
		m_hEnemy = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CObjectSentrygun::IsTurtled( void )
{
	return m_bTurtled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CObjectSentrygun::IsTurtling( void )
{
	return m_bTurtling;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::ToggleTurtle( void )
{
	// Don't turtle while building
	if ( IsPlacing() || IsBuilding() || IsTurtling() )
		return;

	// Don't turtle if I'm built on anything
	if ( GetMoveParent() )
		return;

	// Swap turtle state
	if ( IsTurtled() )
	{
		UnTurtle();
	}
	else
	{
		Turtle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::Turtle( void )
{
	m_bTurtled = true;
	m_bTurtling = true;

	m_flTurtlingFinishedAt = gpGlobals->curtime + SENTRY_TURTLE_TIME;
	EmitSound( "ObjectSentrygun.Turtle" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSentrygun::UnTurtle( void )
{
	// Make sure there's enough room to unturtle
	// NJS: this seems a bit hacky and returns false positives sometimes, for now we're just assuming that if it can turtle, it can also unturtle.
	//trace_t tr;
	//UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin(), WorldAlignMins() - Vector( 4,4,4 ), WorldAlignMaxs() + Vector( 4,4,4 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	//if ( tr.startsolid || tr.allsolid )
	//	return;

	m_bTurtled = false;
	m_bTurtling = true;
	m_flTurtlingFinishedAt = gpGlobals->curtime + SENTRY_TURTLE_TIME;
	EmitSound( "ObjectSentrygun.UnTurtle" );

	RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_YES;
}

//-----------------------------------------------------------------------------
// Purpose: Set the technology levels of the sentrygun
//-----------------------------------------------------------------------------
void CObjectSentrygun::SetTechnology( bool bSmarter, bool bSensors )
{
	m_bSmarter = bSmarter;
	m_bSensors = bSensors;

	// Smarter sentryguns turn faster
	if ( m_bSmarter )
	{
		m_iBaseTurnRate	= 6;
	}
	else
	{
		m_iBaseTurnRate	= 4;
	}
}

//========================================================================================================
// SENTRYGUN TYPES
//========================================================================================================
IMPLEMENT_SERVERCLASS_ST(CObjectSentrygunPlasma, DT_ObjectSentrygunPlasma)
END_SEND_TABLE();

IMPLEMENT_SERVERCLASS_ST(CObjectSentrygunRocketlauncher, DT_ObjectSentrygunRocketlauncher)
END_SEND_TABLE();

void CObjectSentrygunPlasma::Spawn( void )
{
	m_iHealth = obj_sentrygun_plasma_health.GetInt();

	SetModel( SG_PLASMA_MODEL );
	BaseClass::Spawn();

	SetType( OBJ_SENTRYGUN_PLASMA );

	m_flNextAmmoRecharge = gpGlobals->curtime + PLASMA_SENTRYGUN_RECHARGE_TIME;
	m_nBurstCount = PLASMA_SENTRY_BURST_COUNT;

	m_iAmmo = m_iMaxAmmo = 50;
	m_iAmmoType = GetAmmoDef()->Index( "ShotgunEnergy" );
}

void CObjectSentrygunRocketlauncher::Spawn( void )
{
	m_iHealth = obj_sentrygun_rocketlauncher_health.GetInt();

	SetModel( SG_ROCKETLAUNCHER_MODEL );
	BaseClass::Spawn();

	SetType( OBJ_SENTRYGUN_ROCKET_LAUNCHER );

	m_iAmmo = m_iMaxAmmo = 50;
	m_iAmmoType = GetAmmoDef()->Index( "Rockets" );
}

//-----------------------------------------------------------------------------
// Purpose: Plasma sentrygun's fire
//-----------------------------------------------------------------------------
bool CObjectSentrygunPlasma::Fire( void )
{
	Vector vecSrc = EyePosition();
	Vector vecTarget = m_vecFireTarget;
	Vector vecAim;
	QAngle vecAng;

	// Because the plasma sentrygun always thinks it has ammo (see below)
	// we might not have ammo here, in which case we should just abort.
	if ( !m_iAmmo )
		return true;

	GetAttachment( "muzzle", vecSrc, vecAng );

	// Get the distance to the target
	float targetDist = (vecTarget - vecSrc).Length();
	float targetTime = targetDist / PLASMA_VELOCITY;

	// If we're not suppressing, calculate where the target's going to be in that time
	if ( !m_bSuppressing )
	{
		Vector vecVelocity = m_hEnemy->GetSmoothedVelocity();
		// Dampen Z velocity to prevent jumping people screwing the aim
		vecVelocity.z *= 0.25;
		// Get the target point to aim for
		Vector vecEnd = vecTarget + ( vecVelocity * targetTime );
		vecAim = (vecEnd - vecSrc);
	}
	else
	{
		vecAim = (vecTarget - vecSrc);
	}
	VectorNormalize( vecAim );

	int damageType = GetAmmoDef()->DamageType( m_iAmmoType );
	CBasePlasmaProjectile *pPlasma = CBasePlasmaProjectile::Create( vecSrc + (vecAim * 32), vecAim, damageType, this );
	pPlasma->SetDamage( 15 );
	pPlasma->SetMaxRange( obj_sentrygun_plasma_range.GetFloat() );
	pPlasma->m_hOwner = GetBuilder(); 

	EmitSound( "ObjectSentrygun.Fire" );
	SetSentryAnim( TFTURRET_ANIM_FIRE );
	DoMuzzleFlash();

	m_iAmmo -= 1;

	float flAttackTime;
	if (--m_nBurstCount > 0)
	{
		flAttackTime = 0.2f;
	}
	else
	{
		flAttackTime = random->RandomFloat( 1.0f, 2.0f );
		m_nBurstCount = PLASMA_SENTRY_BURST_COUNT + random->RandomInt( 0, PLASMA_SENTRY_BURST_COUNT );
	}

	// If I'm EMPed, slow the firing rate down
	if ( HasPowerup(POWERUP_EMP) )
	{
		flAttackTime *= 3;
	}

	m_flNextAttack = gpGlobals->curtime + flAttackTime;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Plasma sentry regenerates ammo, so always assume it has ammo left.
//			This is to prevent it from continually unlocking & relocking when it's
//			ammo is flickering between 0 and 1.
//-----------------------------------------------------------------------------
bool CObjectSentrygunPlasma::HasAmmo( void ) 
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Plasma sentrygun recharges it's ammo
//-----------------------------------------------------------------------------
void CObjectSentrygunPlasma::CheckShield( void )
{
	// ROBIN: Disabled recharging for now
	/*
	if ( m_flNextAmmoRecharge < gpGlobals->curtime )
	{
		if ( m_iAmmo < m_iMaxAmmo )
		{
			m_iAmmo++;
		}

		m_flNextAmmoRecharge = gpGlobals->curtime + PLASMA_SENTRYGUN_RECHARGE_TIME;
	}
	*/

	BaseClass::CheckShield();
}


//-----------------------------------------------------------------------------
// Purpose: Rocket launcher sentrygun's fire
//-----------------------------------------------------------------------------
bool CObjectSentrygunRocketlauncher::Fire()
{
	Vector vecSrc = EyePosition();
	Vector vecTarget = m_vecFireTarget;
	Vector vecAim;

	// Get the distance to the target
	float targetDist = (vecTarget - vecSrc).Length();
	float targetTime = targetDist / ROCKET_VELOCITY;

	// If we're not suppressing, calculate where the target's going to be in that time
	if ( !m_bSuppressing )
	{
		Vector vecVelocity = m_hEnemy->GetSmoothedVelocity();
		// Dampen velocity to prevent people rapidly switching strafe
		vecVelocity *= 0.5;
		// Dampen Z velocity to prevent jumping people screwing the aim
		vecVelocity.z *= 0.5;
		// Get the target point to aim for
		Vector vecEnd = vecTarget + ( vecVelocity * targetTime );
		vecAim = (vecEnd - vecSrc);
	}
	else
	{
		vecAim = (vecTarget - vecSrc);
	}

	CGrenadeRocket::Create( vecSrc, vecAim, edict(), GetOwner() );

	EmitSound( "ObjectSentrygunRocketlauncher.Fire" );

	m_iAmmo -= 1;

	m_flNextAttack = gpGlobals->curtime + 1.5f;
	return true;
}


void CObjectSentrygunRocketlauncher::SetTechnology( bool bSmarter, bool bSensors )
{
	BaseClass::SetTechnology( bSmarter, bSensors );
	m_iBaseTurnRate	= 2;
}
