//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's resupply beacon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include "tf_obj_resupply.h"
#include "engine/IEngineSound.h"
#include "tf_player.h"
#include "tf_team.h"
#include "VGuiScreen.h"
#include "world.h"

#define RESUPPLY_HEAL_AMT				100
#define RESUPPLY_AMMO_AMT				0.25f

// Wall mounted version
#define RESUPPLY_WALL_MODEL				"models/objects/obj_resupply.mdl"
#define RESUPPLY_WALL_MODEL_ALIEN		"models/objects/alien_obj_resupply.mdl"
#define RESUPPLY_WALL_MINS				Vector(-10, -10, -40)
#define RESUPPLY_WALL_MAXS				Vector( 10,  10, 40)

// Ground placed version
#define RESUPPLY_GROUND_MODEL			"models/objects/obj_resupply_ground.mdl"
#define RESUPPLY_GROUND_MODEL_HUMAN		"models/objects/human_obj_resupply_ground.mdl"
#define RESUPPLY_GROUND_MINS			Vector(-20, -20, 0)
#define RESUPPLY_GROUND_MAXS			Vector( 20,  20, 55)

IMPLEMENT_SERVERCLASS_ST( CObjectResupply, DT_ObjectResupply )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(obj_resupply, CObjectResupply);
PRECACHE_REGISTER(obj_resupply);

ConVar	obj_resupply_health( "obj_resupply_health","100", FCVAR_NONE, "Resupply Station health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectResupply::CObjectResupply()
{
	m_iHealth = obj_resupply_health.GetInt();
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResupply::Spawn()
{
	SetModel( RESUPPLY_WALL_MODEL );
	SetSolid( SOLID_BBOX );

	UTIL_SetSize(this, RESUPPLY_WALL_MINS, RESUPPLY_WALL_MAXS);
	m_takedamage = DAMAGE_YES;

	SetType( OBJ_RESUPPLY );
	m_fObjectFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ;

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Spawn the vgui control screens on the object
//-----------------------------------------------------------------------------
void CObjectResupply::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_resupply";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResupply::Precache()
{
	BaseClass::Precache();
	PrecacheModel( RESUPPLY_WALL_MODEL );
	PrecacheModel( RESUPPLY_WALL_MODEL_ALIEN );
	PrecacheModel( RESUPPLY_GROUND_MODEL );
	PrecacheModel( RESUPPLY_GROUND_MODEL_HUMAN );
	PrecacheVGuiScreen( "screen_obj_resupply" );

	PrecacheScriptSound( "ObjectResupply.InsufficientFunds" );
	PrecacheScriptSound( "BaseCombatCharacter.AmmoPickup" );
}


//-----------------------------------------------------------------------------
// Resupply Health 
//-----------------------------------------------------------------------------
bool CObjectResupply::ResupplyHealth( CBaseTFPlayer *pPlayer, float flFraction )
{
	// Calculate the amount to heal
	float flAmountToHeal = flFraction * RESUPPLY_HEAL_AMT;
	if (flAmountToHeal > (pPlayer->m_iMaxHealth - pPlayer->m_iHealth))
	{
		flAmountToHeal = (pPlayer->m_iMaxHealth - pPlayer->m_iHealth);
	}

	if ( flAmountToHeal > 0 )
	{
		pPlayer->TakeHealth( flAmountToHeal, 0 );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CObjectResupply::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	// NOTE: Must match ResupplyBuyType_t
	static float s_Costs[] =
	{
		RESUPPLY_AMMO_COST,
		RESUPPLY_HEALTH_COST,
		RESUPPLY_GRENADES_COST,
		RESUPPLY_ALL_COST
	};

	COMPILE_TIME_ASSERT( RESUPPLY_BUY_TYPE_COUNT == 4 );

	if ( FStrEq( pCmd, "buy" ) )
	{
		if ( pArg->Argc() < 2 )
			return true;

		// I can't do anything if I'm not active
		if ( !ShouldBeActive() ) 
			return true;

		// Do we have enough resources to activate it?
		if (pPlayer->GetBankResources() <= 0)
		{
			// Play a sound indicating it didn't work...
			CSingleUserRecipientFilter filter( pPlayer );
			EmitSound( filter, pPlayer->entindex(), "ObjectResupply.InsufficientFunds" );
			return true;
		}

		bool bUsedResupply = false;
		ResupplyBuyType_t type = (ResupplyBuyType_t)atoi( pArg->Argv(1) );
		if (type >= RESUPPLY_BUY_TYPE_COUNT)
			return true;

		// Get the potential cost.
		float flCost = s_Costs[type];
//		flCost += pPlayer->ClassCostAdjustment( type );

		float flFraction = pPlayer->GetBankResources() / flCost;
		if (flFraction > 1.0f)
			flFraction = 1.0f;

		switch( type )
		{
		case RESUPPLY_BUY_HEALTH:
			// Calculate the amount to heal
			if (ResupplyHealth(pPlayer, flFraction))
			{
				bUsedResupply = true;
			}
			break;

		case RESUPPLY_BUY_AMMO:
			// Refill the player's ammo too
			if (pPlayer->ResupplyAmmo( flFraction * RESUPPLY_AMMO_AMT, RESUPPLY_AMMO_FROM_STATION ))
			{
				bUsedResupply = true;
			}
			break;

		case RESUPPLY_BUY_GRENADES:
			// Refill the player's ammo too
			if (pPlayer->ResupplyAmmo( flFraction * RESUPPLY_AMMO_AMT, RESUPPLY_GRENADES_FROM_STATION ))
			{
				bUsedResupply = true;
			}
			break;

		case RESUPPLY_BUY_ALL:
			// Calculate the amount to heal
			if (ResupplyHealth(pPlayer, flFraction))
			{
				bUsedResupply = true;
			}

			// Refill the player's ammo too
			if (pPlayer->ResupplyAmmo( flFraction * RESUPPLY_AMMO_AMT, RESUPPLY_ALL_FROM_STATION ))
			{
				bUsedResupply = true;
			}
			break;
		}

		if (bUsedResupply)
		{
			// Play an ammo pickup just to this player
			CSingleUserRecipientFilter filter( pPlayer );
			pPlayer->EmitSound( filter, pPlayer->entindex(), "BaseCombatCharacter.AmmoPickup" );

			pPlayer->RemoveBankResources( flFraction * flCost );
		}

		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResupply::DestroyObject( void )
{
	if ( GetTeam() )
	{
		((CTFTeam*)GetTeam())->RemoveResupply( this );
	}
	BaseClass::DestroyObject();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTeam - 
//-----------------------------------------------------------------------------
void CObjectResupply::ChangeTeam( int iTeamNum )
{
	CTFTeam *pExisting = (CTFTeam*)GetTeam();
	CTFTeam *pTeam = (CTFTeam*)GetGlobalTeam( iTeamNum );

	// Already on this team
	if ( GetTeamNumber() == iTeamNum )
		return;

	if ( pExisting )
	{
		// Remove it from current team ( if it's in one ) and give it to new team
		pExisting->RemoveResupply( this );
	}
		
	// Change to new team
	BaseClass::ChangeTeam( iTeamNum );
	
	// Add this object to the team's list
	if (pTeam)
	{
		pTeam->AddResupply( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Resupply always wants to use the wall mount for attachment points
//-----------------------------------------------------------------------------
void CObjectResupply::SetupAttachedVersion( void )
{
	BaseClass::SetupAttachedVersion();

	if ( GetTeamNumber() == TEAM_ALIENS )
	{
		SetModel( RESUPPLY_WALL_MODEL_ALIEN );
	}
	else
	{
		SetModel( RESUPPLY_WALL_MODEL );
	}

	UTIL_SetSize(this, RESUPPLY_WALL_MINS, RESUPPLY_WALL_MAXS);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CObjectResupply::CalculatePlacement( CBaseTFPlayer *pPlayer )
{
	trace_t tr;
	Vector vecAiming;
	// Get an aim vector. Don't use GetAimVector() because we don't want autoaiming.
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	pPlayer->EyeVectors( &vecAiming );
	Vector vecTarget;
	VectorMA( vecSrc, 90, vecAiming, vecTarget );
	m_vecBuildOrigin = vecTarget;

	// Angle it towards me
	Vector vecForward = pPlayer->WorldSpaceCenter() - m_vecBuildOrigin;
	SetLocalAngles( QAngle( 0, UTIL_VecToYaw( vecForward ), 0 ) );

	// Is there something to attach to? 
	// Use my bounding box, not the build box, so I fit to the wall
	UTIL_TraceLine( vecSrc, vecTarget, MASK_SOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);
	//UTIL_TraceHull( vecSrc, vecTarget, WorldAlignMins(), WorldAlignMaxs(), MASK_SOLID, pPlayer, TFCOLLISION_GROUP_OBJECT, &tr );
	m_vecBuildOrigin = tr.endpos;
	bool bTryToPlaceGroundVersion = false;
	if ( tr.allsolid || (tr.fraction == 1.0) )
	{
		bTryToPlaceGroundVersion = true;
	}
	else 
	{
		// Make sure we're planting on the world
		CBaseEntity *pEntity = tr.m_pEnt;
		if ( pEntity != GetWorldEntity() )
		{
			bTryToPlaceGroundVersion = true;
		}
	}

	// Make sure the wall we've touched is vertical
	if ( !bTryToPlaceGroundVersion && fabs(tr.plane.normal.z) > 0.3 )
	{
		bTryToPlaceGroundVersion = true;
	}

	// Aborting?
	if ( bTryToPlaceGroundVersion )
	{
		// We couldn't find a wall, so try and place a ground version instead
		if ( GetTeamNumber() == TEAM_HUMANS )
		{
			SetModel( RESUPPLY_GROUND_MODEL_HUMAN );
		}
		else
		{
			SetModel( RESUPPLY_GROUND_MODEL );
		}
		UTIL_SetSize(this, RESUPPLY_GROUND_MINS, RESUPPLY_GROUND_MAXS);
		m_vecBuildMins = WorldAlignMins() - Vector( 4,4,0 );
		m_vecBuildMaxs = WorldAlignMaxs() + Vector( 4,4,0 );
		return BaseClass::CalculatePlacement( pPlayer );
	}

	SetupAttachedVersion();
	m_vecBuildMins = WorldAlignMins() - Vector( 4,4,0 );
	m_vecBuildMaxs = WorldAlignMaxs() + Vector( 4,4,0 );

	// Set the angles
	vecForward = tr.plane.normal;
	SetLocalAngles( QAngle( 0, UTIL_VecToYaw( vecForward ), 0 ) );

	// Trace back from the corners
	Vector vecMins, vecMaxs, vecModelMins, vecModelMaxs;
	const model_t *pModel = GetModel();
	modelinfo->GetModelBounds( pModel, vecModelMins, vecModelMaxs );

	// Check the four build points
	Vector vecPointCheck = (vecForward * 32);
	Vector vecUp = Vector(0,0,1);
	Vector vecRight;
	CrossProduct( vecUp, vecForward, vecRight );
	float flWidth = fabs(vecModelMaxs.y - vecModelMins.y) * 0.5;
	float flHeight = fabs(vecModelMaxs.z - vecModelMins.z) * 0.5;

	bool bResult = true;
	if ( bResult )
	{
		bResult = CheckBuildPoint( m_vecBuildOrigin + (vecRight * flWidth) + (vecUp * flHeight), vecPointCheck );
	}
	if ( bResult )
	{
		bResult = CheckBuildPoint( m_vecBuildOrigin + (vecRight * flWidth) - (vecUp * flHeight), vecPointCheck );
	}
	if ( bResult )
	{
		bResult = CheckBuildPoint( m_vecBuildOrigin - (vecRight * flWidth) + (vecUp * flHeight), vecPointCheck );
	}
	if ( bResult )
	{
		bResult = CheckBuildPoint( m_vecBuildOrigin - (vecRight * flWidth) - (vecUp * flHeight), vecPointCheck );
	}

	AttemptToFindPower();

	return bResult;
}
