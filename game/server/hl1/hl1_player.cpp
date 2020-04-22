//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl1_player.h"
#include "gamerules.h"
#include "trains.h"
#include "hl1_basecombatweapon_shared.h"
#include "vcollide_parse.h"
#include "in_buttons.h"
#include "igamemovement.h"
#include "ai_hull.h"
#include "hl2_shareddefs.h"
#include "info_camera_link.h"
#include "point_camera.h"
#include "ndebugoverlay.h"
#include "globals.h"
#include "ai_interactions.h"
#include "engine/IEngineSound.h"
#include "vphysics/player_controller.h"
#include "vphysics/constraints.h"
#include "predicted_viewmodel.h"
#include "physics_saverestore.h"
#include "gamestats.h"

#define DMG_FREEZE		DMG_VEHICLE
#define DMG_SLOWFREEZE	DMG_DISSOLVE

// HL1_DMG_SHOWNHUD: Add DMG_VEHICLE because HL2 hijacked those bits from DMG_FREEZE, which is what they are in HL1
// HL1_DMG_SHOWNHUD: Add DMG_DISSOLVE because HL2 hijacked those bits from DMG_SLOWFREEZE, which is what they are in HL1
// See Halflife1 GameRules - Damage_GetShowOnHud()
//#define HL1_DMG_SHOWNHUD	(DMG_POISON | DMG_ACID | DMG_FREEZE | DMG_SLOWFREEZE | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK)

// TIME BASED DAMAGE AMOUNT
// tweak these values based on gameplay feedback:
#define PARALYZE_DURATION	2		// number of 2 second intervals to take damage
#define PARALYZE_DAMAGE		1.0		// damage to take each 2 second interval

#define NERVEGAS_DURATION	2
#define NERVEGAS_DAMAGE		5.0

#define POISON_DURATION		5
#define POISON_DAMAGE		2.0

#define RADIATION_DURATION	2
#define RADIATION_DAMAGE	1.0

#define ACID_DURATION		2
#define ACID_DAMAGE			5.0

#define SLOWBURN_DURATION	2
#define SLOWBURN_DAMAGE		1.0

#define SLOWFREEZE_DURATION	2
#define SLOWFREEZE_DAMAGE	1.0

#define	FLASH_DRAIN_TIME	 1.2 //100 units/3 minutes
#define	FLASH_CHARGE_TIME	 0.2 // 100 units/20 seconds  (seconds per unit)

ConVar player_showpredictedposition( "player_showpredictedposition", "0" );
ConVar player_showpredictedposition_timestep( "player_showpredictedposition_timestep", "1.0" );

ConVar sv_hl1_allowpickup( "sv_hl1_allowpickup", "0" );

LINK_ENTITY_TO_CLASS( player, CHL1_Player );
PRECACHE_REGISTER(player);

BEGIN_DATADESC( CHL1_Player )
	DEFINE_FIELD( m_nControlClass, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_vecMissPositions, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_nNumMissPositions, FIELD_INTEGER ),

	DEFINE_FIELD( m_flIdleTime, FIELD_TIME ),
	DEFINE_FIELD( m_flMoveTime, FIELD_TIME ),
	DEFINE_FIELD( m_flLastDamageTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTargetFindTime, FIELD_TIME ),
	DEFINE_FIELD( m_bHasLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nFlashBattery, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFlashLightTime, FIELD_TIME ),

	DEFINE_FIELD( m_flStartCharge, FIELD_FLOAT ),
	DEFINE_FIELD( m_flAmmoStartCharge, FIELD_FLOAT ),
	DEFINE_FIELD( m_flPlayAftershock, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextAmmoBurn, FIELD_FLOAT ),

	DEFINE_PHYSPTR( m_pPullConstraint ),
	DEFINE_FIELD( m_hPullObject, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bIsPullingObject, FIELD_BOOLEAN ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST( CHL1_Player, DT_HL1Player )
	SendPropInt( SENDINFO( m_bHasLongJump ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nFlashBattery ), 8, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bIsPullingObject ) ),

	SendPropFloat( SENDINFO( m_flStartCharge ) ),
	SendPropFloat( SENDINFO( m_flAmmoStartCharge ) ),
	SendPropFloat( SENDINFO( m_flPlayAftershock ) ),
	SendPropFloat( SENDINFO( m_flNextAmmoBurn ) )

END_SEND_TABLE()

CHL1_Player::CHL1_Player()
{
	m_nNumMissPositions	= 0;
}

void CHL1_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Player.FlashlightOn" );
	PrecacheScriptSound( "Player.FlashlightOff" );
	PrecacheScriptSound( "Player.UseTrain" );
}


//-----------------------------------------------------------------------------
// Purpose: Allow pre-frame adjustments on the player
//-----------------------------------------------------------------------------
void CHL1_Player::PreThink(void)
{
	if ( player_showpredictedposition.GetBool() )
	{
		Vector	predPos;

		UTIL_PredictedPosition( this, player_showpredictedposition_timestep.GetFloat(), &predPos );

		NDebugOverlay::Box( predPos, NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), 0, 255, 0, 0, 0.01f );
		NDebugOverlay::Line( GetAbsOrigin(), predPos, 0, 255, 0, 0, 0.01f );
	}

	int buttonsChanged;
	buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	UpdatePullingObject();

	g_pGameRules->PlayerThink( this );

	if ( g_fGameOver || IsPlayerLockedInPlace() )
		return;         // intermission or finale

	ItemPreFrame( );
	WaterMove();

	if ( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_Local.m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_Local.m_iHideHUD |= HIDEHUD_FLASHLIGHT;


	// checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();
	
	CheckTimeBasedDamage();

	CheckSuitUpdate();

	if (m_lifeState >= LIFE_DYING)
	{
		PlayerDeathThink();
		return;
	}

	// So the correct flags get sent to client asap.
	//
	if ( m_afPhysicsFlags & PFLAG_DIROVERRIDE )
		AddFlag( FL_ONTRAIN );
	else 
		RemoveFlag( FL_ONTRAIN );

	// Train speed control
	if ( m_afPhysicsFlags & PFLAG_DIROVERRIDE )
	{
		CBaseEntity *pTrain = GetGroundEntity();
		float vel;

		if ( pTrain )
		{
			if ( !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) )
				pTrain = NULL;
		}

		if ( !pTrain )
		{
			if ( GetActiveWeapon() && (GetActiveWeapon()->ObjectCaps() & FCAP_DIRECTIONAL_USE) )
			{
				m_iTrain = TRAIN_ACTIVE | TRAIN_NEW;

				if ( m_nButtons & IN_FORWARD )
				{
					m_iTrain |= TRAIN_FAST;
				}
				else if ( m_nButtons & IN_BACK )
				{
					m_iTrain |= TRAIN_BACK;
				}
				else
				{
					m_iTrain |= TRAIN_NEUTRAL;
				}
				return;
			}
			else
			{
				trace_t trainTrace;
				// Maybe this is on the other side of a level transition
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector(0,0,-38), 
					MASK_PLAYERSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trainTrace );

				if ( trainTrace.fraction != 1.0 && trainTrace.m_pEnt )
					pTrain = trainTrace.m_pEnt;


				if ( !pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(this) )
				{
//					Warning( "In train mode with no train!\n" );
					m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
					m_iTrain = TRAIN_NEW|TRAIN_OFF;
					return;
				}
			}
		}
		else if ( !( GetFlags() & FL_ONGROUND ) || pTrain->HasSpawnFlags( SF_TRACKTRAIN_NOCONTROL ) || (m_nButtons & (IN_MOVELEFT|IN_MOVERIGHT) ) )
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
			m_iTrain = TRAIN_NEW|TRAIN_OFF;
			return;
		}

		SetAbsVelocity( vec3_origin );
		vel = 0;
		if ( m_afButtonPressed & IN_FORWARD )
		{
			vel = 1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			vel = -1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}
		else if ( m_afButtonPressed & IN_JUMP )
		{
			m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
			m_iTrain = TRAIN_NEW|TRAIN_OFF;
		}

		if (vel)
		{
			m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
			m_iTrain |= TRAIN_ACTIVE|TRAIN_NEW;
		}
	} 
	else if (m_iTrain & TRAIN_ACTIVE)
	{
		m_iTrain = TRAIN_NEW; // turn off train
	}

	// THIS CODE DOESN'T SEEM TO DO ANYTHING!!!
	// WHY IS IT STILL HERE? (sjb)
	if (m_nButtons & IN_JUMP)
	{
		// If on a ladder, jump off the ladder
		// else Jump
		if( IsPullingObject() )
		{
			StopPullingObject();
		}

		Jump();
	}

	// If trying to duck, already ducked, or in the process of ducking
	if ((m_nButtons & IN_DUCK) || (GetFlags() & FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) )
		Duck();

	//
	// If we're not on the ground, we're falling. Update our falling velocity.
	//
	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		m_Local.m_flFallVelocity = -GetAbsVelocity().z;
	}

	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
	{
		SetAbsVelocity( vec3_origin );
	}
	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	//Find targets for NPC to shoot if they decide to miss us
	FindMissTargets();
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
Class_T  CHL1_Player::Classify ( void )
{
	// If player controlling another entity?  If so, return this class
	if (m_nControlClass != CLASS_NONE)
	{
		return m_nControlClass;
	}
	else
	{
		return CLASS_PLAYER;
	}
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CHL1_Player::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if ( interactionType == g_interactionBarnacleVictimDangle )
	{
		TakeDamage ( CTakeDamageInfo( sourceEnt, sourceEnt, m_iHealth + ArmorValue(), DMG_SLASH | DMG_ALWAYSGIB ) );
		return true;
	}
	
	if (interactionType ==	g_interactionBarnacleVictimReleased)
	{
		m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
		SetMoveType( MOVETYPE_WALK );
		return true;
	}
	else if (interactionType ==	g_interactionBarnacleVictimGrab)
	{
		m_afPhysicsFlags |= PFLAG_ONBARNACLE;
		ClearUseEntity();
		return true;
	}
	return false;
}


void CHL1_Player::PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper)
{
	// Handle FL_FROZEN.
	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
	{
		ucmd->forwardmove = 0;
		ucmd->sidemove = 0;
		ucmd->upmove = 0;
	}

	//Update our movement information
	if ( ( ucmd->forwardmove != 0 ) || ( ucmd->sidemove != 0 ) || ( ucmd->upmove != 0 ) )
	{
		m_flIdleTime -= TICK_INTERVAL * 2.0f;
		
		if ( m_flIdleTime < 0.0f )
		{
			m_flIdleTime = 0.0f;
		}

		m_flMoveTime += TICK_INTERVAL;

		if ( m_flMoveTime > 4.0f )
		{
			m_flMoveTime = 4.0f;
		}
	}
	else
	{
		m_flIdleTime += TICK_INTERVAL;
		
		if ( m_flIdleTime > 4.0f )
		{
			m_flIdleTime = 4.0f;
		}

		m_flMoveTime -= TICK_INTERVAL * 2.0f;
		
		if ( m_flMoveTime < 0.0f )
		{
			m_flMoveTime = 0.0f;
		}
	}

	//Msg("Player time: [ACTIVE: %f]\t[IDLE: %f]\n", m_flMoveTime, m_flIdleTime );

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}

//-----------------------------------------------------------------------------
// Purpose: Create and give the named item to the player. Then return it.
//-----------------------------------------------------------------------------
CBaseEntity	*CHL1_Player::GiveNamedItem( const char *pszName, int iSubType )
{
	// If I already own this type don't create one
	if ( Weapon_OwnsThisType(pszName, iSubType) )
		return NULL;

	// Msg( "giving %s\n", pszName );

	EHANDLE pent;

	pent = CreateEntityByName(pszName);
	if ( pent == NULL )
	{
		Msg( "NULL Ent in GiveNamedItem!\n" );
		return NULL;
	}

	pent->SetLocalOrigin( GetLocalOrigin() );
	pent->AddSpawnFlags( SF_NORESPAWN );

	if ( iSubType )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( (CBaseEntity*)pent );
		if ( pWeapon )
		{
			pWeapon->SetSubType( iSubType );
		}
	}

	DispatchSpawn( pent );

	if ( pent != NULL && !(pent->IsMarkedForDeletion()) ) 
	{
		pent->Touch( this );
	}

	return pent;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::StartPullingObject( CBaseEntity *pObject )
{
	if ( pObject->VPhysicsGetObject() == NULL || VPhysicsGetObject() == NULL )
	{
		 return;
	}

	if( !(GetFlags()&FL_ONGROUND) )
	{
		//Msg("Can't grab in air!\n");
		return;
	}

	if( GetGroundEntity() == pObject )
	{
		//Msg("Can't grab something you're standing on!\n");
		return;
	}

	constraint_ballsocketparams_t ballsocket;
	ballsocket.Defaults();
	ballsocket.constraint.Defaults();
	ballsocket.constraint.forceLimit = lbs2kg(1000);
	ballsocket.constraint.torqueLimit = lbs2kg(1000);
	ballsocket.InitWithCurrentObjectState( VPhysicsGetObject(), pObject->VPhysicsGetObject(), WorldSpaceCenter() );
	m_pPullConstraint = physenv->CreateBallsocketConstraint( VPhysicsGetObject(), pObject->VPhysicsGetObject(), NULL, ballsocket );

	m_hPullObject.Set(pObject);
	m_bIsPullingObject = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::StopPullingObject()
{
	if( m_pPullConstraint )
	{
		physenv->DestroyConstraint( m_pPullConstraint );
	}

	m_hPullObject.Set(NULL);
	m_pPullConstraint = NULL;
	m_bIsPullingObject = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::UpdatePullingObject()
{
	if( !IsPullingObject() )
		return;

	CBaseEntity *pObject= m_hPullObject.Get();

	if( !pObject || !pObject->VPhysicsGetObject() )
	{
		// Object broke or otherwise vanished.
		StopPullingObject();
		return;
	}

	if( m_afButtonReleased & IN_USE )
	{
		// Player released +USE
		StopPullingObject();
		return;
	}


	float flMaxDistSqr = Square(PLAYER_USE_RADIUS + 1.0f);
	
	Vector objectPos;
	QAngle angle;

	pObject->VPhysicsGetObject()->GetPosition( &objectPos, &angle );

	if( !FInViewCone(objectPos) )
	{
		// Player turned away.
		StopPullingObject();
	}
	else if( objectPos.DistToSqr(WorldSpaceCenter()) > flMaxDistSqr )
	{
		// Object got caught up on something and left behind
		StopPullingObject();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets HL1specific defaults.
//-----------------------------------------------------------------------------
void CHL1_Player::Spawn(void)
{
	// In multiplayer ,this is handled in the super class
	if ( !g_pGameRules->IsMultiplayer () )
		SetModel( "models/player.mdl" );

	BaseClass::Spawn();

	//
	// Our player movement speed is set once here. This will override the cl_xxxx
	// cvars unless they are set to be lower than this.
	//
	SetMaxSpeed( 1000 );

	SetDefaultFOV( 0 );

	m_nFlashBattery = 99;
	m_flFlashLightTime = 1;

	m_flFieldOfView	= 0.5;

	StopPullingObject();

	m_Local.m_iHideHUD = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::Event_Killed( const CTakeDamageInfo &info )
{
	StopPullingObject();
	BaseClass::Event_Killed(info);
}

void CHL1_Player::CheckTimeBasedDamage( void ) 
{
	int i;
	byte bDuration = 0;

	static float gtbdPrev = 0.0;

	// If we don't have any time based damage return.
	if ( !g_pGameRules->Damage_IsTimeBased( m_bitsDamageType ) )
		return;

	// only check for time based damage approx. every 2 seconds
	if (abs(gpGlobals->curtime - m_tbdPrev) < 2.0)
		return;
	
	m_tbdPrev = gpGlobals->curtime;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// Make sure the damage type is really time-based.
		// This is kind of hacky but necessary until we setup DamageType as an enum.
		int iDamage = ( DMG_PARALYZE << i );
		if ( !g_pGameRules->Damage_IsTimeBased( iDamage ) )
			continue;

		// make sure bit is set for damage type
		if ( m_bitsDamageType & iDamage )
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
//				OnTakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);	
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_PoisonRecover:
				OnTakeDamage( CTakeDamageInfo( this, this, POISON_DAMAGE, DMG_GENERIC ) );
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
//				OnTakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = MIN(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4;	// get up to 5*10 = 50 points back
				break;

			case itbd_Acid:
//				OnTakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
//				OnTakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
//				OnTakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (m_rgbTimeBasedDamage[i])
			{
				// decrement damage duration, detect when done.
				if (!m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);	
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}


class CPhysicsPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:
	int ShouldMoveTo( IPhysicsObject *pObject, const Vector &position )
	{
		CHL1_Player *pPlayer = (CHL1_Player *)pObject->GetGameData();
		if ( pPlayer )
		{
			if ( pPlayer->TouchedPhysics() )
			{
				return 0;
			}
		}
		return 1;
	}
};

static CPhysicsPlayerCallback playerCallback;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL1_Player::InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity )
{
	BaseClass::InitVCollision( vecAbsOrigin, vecAbsVelocity );

	// Setup the HL2 specific callback.
	GetPhysicsController()->SetEventHandler( &playerCallback );
}


CHL1_Player::~CHL1_Player( void )
{
}

extern int gEvilImpulse101;
void CHL1_Player::CheatImpulseCommands( int iImpulse )
{
	if ( !sv_cheats->GetBool() )
	{
		return;
	}

	switch( iImpulse )
	{
	case 101:
		gEvilImpulse101 = true;
		
		GiveNamedItem( "item_suit" );
		GiveNamedItem( "item_battery" );
		GiveNamedItem( "weapon_crowbar" );
		GiveNamedItem( "weapon_glock" );
		GiveNamedItem( "ammo_9mmclip" );
		GiveNamedItem( "weapon_shotgun" );
		GiveNamedItem( "ammo_buckshot" );
		GiveNamedItem( "weapon_mp5" );
		GiveNamedItem( "ammo_9mmar" );
		GiveNamedItem( "ammo_argrenades" );
		GiveNamedItem( "weapon_handgrenade" );
		GiveNamedItem( "weapon_tripmine" );
		GiveNamedItem( "weapon_357" );
		GiveNamedItem( "ammo_357" );
		GiveNamedItem( "weapon_crossbow" );
		GiveNamedItem( "ammo_crossbow" );
		GiveNamedItem( "weapon_egon" );
		GiveNamedItem( "weapon_gauss" );
		GiveNamedItem( "ammo_gaussclip" );
		GiveNamedItem( "weapon_rpg" );
		GiveNamedItem( "ammo_rpgclip" );
		GiveNamedItem( "weapon_satchel" );
		GiveNamedItem( "weapon_snark" );
		GiveNamedItem( "weapon_hornetgun" );

		gEvilImpulse101 = false;
		break;

	case 0:
	default:
		BaseClass::CheatImpulseCommands( iImpulse );
	}
}

void CHL1_Player::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );
	PointCameraSetupVisibility( this, area, pvs, pvssize );
}


#define ARMOR_RATIO	 0.2	// Armor Takes 80% of the damage
#define ARMOR_BONUS  0.5	// Each Point of Armor is work 1/x points of health


int	CHL1_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = inputInfo.GetDamageType();
	int ffound = true;
	int fmajor;
	int fcritical;
	int fTookDamage;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = m_iHealth;

	CTakeDamageInfo info = inputInfo;

	if ( info.GetDamage() > 0.0f )
	{
		m_flLastDamageTime = gpGlobals->curtime;
	}

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ( ( info.GetDamageType() & DMG_BLAST ) && g_pGameRules->IsMultiplayer() )
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Already dead
	if ( !IsAlive() )
		return 0;
	// go take the damage first

	
	if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
	{
		// Refuse the damage
		return 0;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = info.GetDamage();

	// Armor. 
	if ( ArmorValue() && 
		!(info.GetDamageType() & (DMG_FALL | DMG_DROWN | DMG_POISON)) &&	// armor doesn't protect against fall or drown damage!
		!(GetFlags() & FL_GODMODE) )
	{
		float flNew = info.GetDamage() * flRatio;

		float flArmor;

		flArmor = (info.GetDamage() - flNew) * flBonus;

		// Does this use more armor than we have?
		if ( flArmor > ArmorValue() )
		{
			flArmor = ArmorValue();
			flArmor *= (1/flBonus);
			flNew = info.GetDamage() - flArmor;
			SetArmorValue( 0 );
		}
		else
			SetArmorValue( ArmorValue() - flArmor );
		
		info.SetDamage( flNew );
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	info.SetDamage( (int)info.GetDamage() );
	fTookDamage = CBaseCombatCharacter::OnTakeDamage( info );	// Bypass CBasePlayer's

	if ( fTookDamage )
	{
		// add to the damage total for clients, which will be sent as a single
		// message at the end of the frame
		// todo: remove after combining shotgun blasts?
		if ( info.GetInflictor() && info.GetInflictor()->edict() )
			m_DmgOrigin = info.GetInflictor()->GetAbsOrigin();

		m_DmgTake += (int)info.GetDamage();
	}

	// Reset damage time countdown for each type of time based damage player just sustained
	for (int i = 0; i < CDMG_TIMEBASED; i++)
	{
		// Make sure the damage type is really time-based.
		// This is kind of hacky but necessary until we setup DamageType as an enum.
		int iDamage = ( DMG_PARALYZE << i );
		if ( ( info.GetDamageType() & iDamage ) && g_pGameRules->Damage_IsTimeBased( iDamage ) )
		{
			m_rgbTimeBasedDamage[i] = 0;
		}
	}

	// Display any effect associate with this damage type
	DamageEffect(info.GetDamage(),bitsDamage);

	// how bad is it, doc?
	ftrivial = (m_iHealth > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (m_iHealth < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

		// DMG_BURN	
		// DMG_FREEZE
		// DMG_BLAST
		// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	bool bTimeBasedDamage = g_pGameRules->Damage_IsTimeBased( bitsDamage );
	while (fTookDamage && (!ftrivial || bTimeBasedDamage) && ffound && bitsDamage)
	{
		ffound = false;

		if (bitsDamage & DMG_CLUB)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC);	// minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = true;
		}
		if (bitsDamage & (DMG_FALL | DMG_CRUSH))
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", false, SUIT_NEXT_IN_30SEC);	// major fracture
			else
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC);	// minor fracture
	
			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = true;
		}
		
		if (bitsDamage & DMG_BULLET)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", false, SUIT_NEXT_IN_30SEC);	// blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC);	// minor laceration
			
			bitsDamage &= ~DMG_BULLET;
			ffound = true;
		}

		if (bitsDamage & DMG_SLASH)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", false, SUIT_NEXT_IN_30SEC);	// major laceration
			else
				SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = true;
		}
		
		if (bitsDamage & DMG_SONIC)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", false, SUIT_NEXT_IN_1MIN);	// internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = true;
		}

		if (bitsDamage & (DMG_POISON | DMG_PARALYZE))
		{
			if (bitsDamage & DMG_POISON)
			{
				m_nPoisonDmg += info.GetDamage();
				m_tbdPrev = gpGlobals->curtime;
				m_rgbTimeBasedDamage[itbd_PoisonRecover] = 0;
			}

			SetSuitUpdate("!HEV_DMG3", false, SUIT_NEXT_IN_1MIN);	// blood toxins detected
			bitsDamage &= ~( DMG_POISON | DMG_PARALYZE );
			ffound = true;
		}

		if (bitsDamage & DMG_ACID)
		{
			SetSuitUpdate("!HEV_DET1", false, SUIT_NEXT_IN_1MIN);	// hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = true;
		}

		if (bitsDamage & DMG_NERVEGAS)
		{
			SetSuitUpdate("!HEV_DET0", false, SUIT_NEXT_IN_1MIN);	// biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = true;
		}

		if (bitsDamage & DMG_RADIATION)
		{
			SetSuitUpdate("!HEV_DET2", false, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = true;
		}
		if (bitsDamage & DMG_SHOCK)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = true;
		}
	}

	m_Local.m_vecPunchAngle.SetX( -2 );

	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75) 
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", false, SUIT_NEXT_IN_30MIN);	// automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", false, SUIT_NEXT_IN_30MIN);	// morphine shot
	}
	
	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (m_iHealth < 6)
			SetSuitUpdate("!HEV_HLTH3", false, SUIT_NEXT_IN_10MIN);	// near death
		else if (m_iHealth < 20)
			SetSuitUpdate("!HEV_HLTH2", false, SUIT_NEXT_IN_10MIN);	// health critical
	
		// give critical health warnings
		if (!random->RandomInt(0,3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && g_pGameRules->Damage_IsTimeBased( info.GetDamageType() ) && flHealthPrev < 75)
		{
			if (flHealthPrev < 50)
			{
				if (!random->RandomInt(0,3))
					SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
			}
			else
				SetSuitUpdate("!HEV_HLTH1", false, SUIT_NEXT_IN_10MIN);	// health dropping
		}

	// Do special explosion damage effect
	if ( bitsDamage & DMG_BLAST )
	{
		OnDamagedByExplosion( info );
	}

	gamestats->Event_PlayerDamage( this, info );

	return fTookDamage;
}


int CHL1_Player::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int nRet;
	int nSavedHealth = m_iHealth;

	// Drown
	if( info.GetDamageType() & DMG_DROWN )
	{
		if( m_idrowndmg == m_idrownrestored )
		{
			EmitSound( "Player.DrownStart" );
		}
		else
		{
			EmitSound( "Player.DrownContinue" );
		}
	}

	nRet = BaseClass::OnTakeDamage_Alive( info );

	if ( GetFlags() & FL_GODMODE )
	{
		m_iHealth = nSavedHealth;
	}
	else if (m_debugOverlays & OVERLAY_BUDDHA_MODE)
	{
		if ( m_iHealth <= 0 )
		{
			m_iHealth = 1;
		}
	}

	return nRet;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL1_Player::FindMissTargets( void )
{
	if ( m_flTargetFindTime > gpGlobals->curtime )
		return;

	m_flTargetFindTime	= gpGlobals->curtime + 1.0f;
	m_nNumMissPositions = 0;

	CBaseEntity *pEnts[256];
	Vector		radius( 80, 80, 80);

	int numEnts = UTIL_EntitiesInBox( pEnts, 256, GetAbsOrigin()-radius, GetAbsOrigin()+radius, 0 );

	for ( int i = 0; i < numEnts; i++ )
	{
		if ( pEnts[i] == NULL )
			continue;

		if ( m_nNumMissPositions >= 16 )
			return;

		//See if it's a good target candidate
		if ( FClassnameIs( pEnts[i], "prop_dynamic" ) || 
			 FClassnameIs( pEnts[i], "dynamic_prop" ) || 
			 FClassnameIs( pEnts[i], "prop_physics" ) || 
			 FClassnameIs( pEnts[i], "physics_prop" ) )
		{
			//NDebugOverlay::Cross3D( pEnts[i]->WorldSpaceCenter(), -Vector(4,4,4), Vector(4,4,4), 0, 255, 0, true, 1.0f );

			m_vecMissPositions[m_nNumMissPositions++] = pEnts[i]->WorldSpaceCenter();
			continue;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Good position to shoot at
//-----------------------------------------------------------------------------
bool CHL1_Player::GetMissPosition( Vector *position )
{
	if ( m_nNumMissPositions == 0 )
		return false;

	(*position) = m_vecMissPositions[ random->RandomInt( 0, m_nNumMissPositions-1 ) ];

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL1_Player::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::FlashlightTurnOn( void )
{
	if ( IsSuitEquipped() )
	{
		AddEffects( EF_DIMLIGHT );
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Player.FlashlightOn" );	

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->curtime;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL1_Player::FlashlightTurnOff( void )
{
	RemoveEffects( EF_DIMLIGHT );
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Player.FlashlightOff" );	
	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->curtime;
}


void CHL1_Player::UpdateClientData( void )
{
	if (m_DmgTake || m_DmgSave || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = GetLocalOrigin();
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		damageOrigin = m_DmgOrigin;

		// only send down damage type that have hud art
		int iShowHudDamage = g_pGameRules->Damage_GetShowOnHud();
		int visibleDamageBits = m_bitsDamageType & iShowHudDamage;

		m_DmgTake = clamp( m_DmgTake, 0, 255 );
		m_DmgSave = clamp( m_DmgSave, 0, 255 );

		CSingleUserRecipientFilter user( this );
		user.MakeReliable();
		UserMessageBegin( user, "Damage" );
			WRITE_BYTE( m_DmgSave );
			WRITE_BYTE( m_DmgTake );
			WRITE_LONG( visibleDamageBits );
			WRITE_FLOAT( damageOrigin.x );	//BUG: Should be fixed point (to hud) not floats
			WRITE_FLOAT( damageOrigin.y );	//BUG: However, the HUD does _not_ implement bitfield messages (yet)
			WRITE_FLOAT( damageOrigin.z );	//BUG: We use WRITE_VEC3COORD for everything else
		MessageEnd();
	
		m_DmgTake = 0;
		m_DmgSave = 0;
		m_bitsHUDDamage = m_bitsDamageType;
		
		// Clear off non-time-based damage indicators
		int iDamage = g_pGameRules->Damage_GetTimeBased();
		m_bitsDamageType &= iDamage;
	}

	// Update Flashlight
	if ( ( m_flFlashLightTime ) && ( m_flFlashLightTime <= gpGlobals->curtime ) )
	{
		if ( FlashlightIsOn() )
		{
			if ( m_nFlashBattery )
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->curtime;
				m_nFlashBattery--;
				
				if ( !m_nFlashBattery )
					FlashlightTurnOff();
			}
		}
		else
		{
			if ( m_nFlashBattery < 100 )
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->curtime;
				m_nFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}
	}

	BaseClass::UpdateClientData();
}

void CHL1_Player::OnSave( IEntitySaveUtils *pUtils )
{
	// If I'm pulling a box, stop.
	StopPullingObject();

	BaseClass::OnSave(pUtils);
}

void CHL1_Player::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this );
		m_hViewModel.Set( index, vm );
	}
}	

void CHL1_Player::OnRestore( void )
{
	BaseClass::OnRestore();

	// If we are controlling a train, resend our train status
	if( !FBitSet( m_iTrain, TRAIN_OFF ) )
	{
		m_iTrain |= TRAIN_NEW;
	}
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void MatrixOrthogonalize( matrix3x4_t &matrix, int column )
{
	Vector columns[3];
	int i;

	for ( i = 0; i < 3; i++ )
	{
		MatrixGetColumn( matrix, i, columns[i] );
	}

	int index0 = column;
	int index1 = (column+1)%3;
	int index2 = (column+2)%3;

	columns[index2] = CrossProduct( columns[index0], columns[index1] );
	columns[index1] = CrossProduct( columns[index2], columns[index0] );
	VectorNormalize( columns[index2] );
	VectorNormalize( columns[index1] );
	MatrixSetColumn( columns[index1], index1, matrix );
	MatrixSetColumn( columns[index2], index2, matrix );
}

#define SIGN(x) ( (x) < 0 ? -1 : 1 )

static QAngle AlignAngles( const QAngle &angles, float cosineAlignAngle )
{
	matrix3x4_t alignMatrix;
	AngleMatrix( angles, alignMatrix );

	for ( int j = 0; j < 3; j++ )
	{
		Vector vec;
		MatrixGetColumn( alignMatrix, j, vec );
		for ( int i = 0; i < 3; i++ )
		{
			if ( fabs(vec[i]) > cosineAlignAngle )
			{
				vec[i] = SIGN(vec[i]);
				vec[(i+1)%3] = 0;
				vec[(i+2)%3] = 0;
				MatrixSetColumn( vec, j, alignMatrix );
				MatrixOrthogonalize( alignMatrix, j );
				break;
			}
		}
	}

	QAngle out;
	MatrixAngles( alignMatrix, out );
	return out;
}


static void TraceCollideAgainstBBox( const CPhysCollide *pCollide, const Vector &start, const Vector &end, const QAngle &angles, const Vector &boxOrigin, const Vector &mins, const Vector &maxs, trace_t *ptr )
{
	physcollision->TraceBox( boxOrigin, boxOrigin + (start-end), mins, maxs, pCollide, start, angles, ptr );

	if ( ptr->DidHit() )
	{
		ptr->endpos = start * (1-ptr->fraction) + end * ptr->fraction;
		ptr->startpos = start;
		ptr->plane.dist = -ptr->plane.dist;
		ptr->plane.normal *= -1;
	}
}

//---------------------------------------------

#include "player_pickup.h"
#include "props.h"
#include "vphysics/friction.h"
#include "physics_saverestore.h"
ConVar hl2_normspeed( "hl2_normspeed", "190" );
ConVar player_throwforce( "player_throwforce", "1000" );
ConVar physcannon_maxmass( "physcannon_maxmass", "250" );

// derive from this so we can add save/load data to it
struct game_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( game_shadowcontrol_params_t )
	
	DEFINE_FIELD( targetPosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( targetRotation,		FIELD_VECTOR ),
	DEFINE_FIELD( maxAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( dampFactor, FIELD_FLOAT ),
	DEFINE_FIELD( teleportDistance,	FIELD_FLOAT ),

END_DATADESC()

// when looking level, hold bottom of object 8 inches below eye level
#define PLAYER_HOLD_LEVEL_EYES	-8

// when looking down, hold bottom of object 0 inches from feet
#define PLAYER_HOLD_DOWN_FEET	2

// when looking up, hold bottom of object 24 inches above eye level
#define PLAYER_HOLD_UP_EYES		24

// use a +/-30 degree range for the entire range of motion of pitch
#define PLAYER_LOOK_PITCH_RANGE	30

// player can reach down 2ft below his feet (otherwise he'll hold the object above the bottom)
#define PLAYER_REACH_DOWN_DISTANCE	24

static void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out )
{
	if ( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();
	
	// 0-360 / -180-180
	//angles.x = init ? 0 : AngleDistance( angles.x, 0 );
	//angles.x = clamp( angles.x, -PLAYER_LOOK_PITCH_RANGE, PLAYER_LOOK_PITCH_RANGE );
	angles.x = 0;

	float feet = pPlayer->GetAbsOrigin().z + pPlayer->WorldAlignMins().z;
	float eyes = origin.z;
	float zoffset = 0;
	// moving up (negative pitch is up)
	if ( angles.x < 0 )
	{
		zoffset = RemapVal( angles.x, 0, -PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_UP_EYES );
	}
	else
	{
		zoffset = RemapVal( angles.x, 0, PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_DOWN_FEET + (feet - eyes) );
	}
	origin.z += zoffset;
	angles.x = 0;
	AngleMatrix( angles, origin, out );
}

//-----------------------------------------------------------------------------
class CGrabController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:

	CGrabController( void );
	~CGrabController( void );
	void AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon = false );
	void DetachEntity();
	void OnRestore();

	bool UpdateObject( CBasePlayer *pPlayer, float flError );

	void SetTargetPosition( const Vector &target, const QAngle &targetOrientation );
	float ComputeError();
	float GetLoadWeight( void ) const { return m_flLoadWeight; }
	void SetAngleAlignment( float alignAngleCosine ) { m_angleAlignment = alignAngleCosine; }
	void SetIgnorePitch( bool bIgnore ) { m_bIgnoreRelativePitch = bIgnore; }
	QAngle TransformAnglesToPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer );
	QAngle TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer );

	CBaseEntity *GetAttached() { return (CBaseEntity *)m_attachedEntity; }

	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	float GetSavedMass( IPhysicsObject *pObject );

private:
	// Compute the max speed for an attached object
	void ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics );

	game_shadowcontrol_params_t	m_shadow;
	float			m_timeToArrive;
	float			m_errorTime;
	float			m_error;
	float			m_contactAmount;
	float			m_angleAlignment;
	bool			m_bCarriedEntityBlocksLOS;
	bool			m_bIgnoreRelativePitch;

	float			m_flLoadWeight;
	float			m_savedRotDamping[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	float			m_savedMass[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	EHANDLE			m_attachedEntity;
	QAngle			m_vecPreferredCarryAngles;
	bool			m_bHasPreferredCarryAngles;

	QAngle			m_attachedAnglesPlayerSpace;
	Vector			m_attachedPositionObjectSpace;

	IPhysicsMotionController *m_controller;

	friend class CWeaponPhysCannon;
};

BEGIN_SIMPLE_DATADESC( CGrabController )

	DEFINE_EMBEDDED( m_shadow ),

	DEFINE_FIELD( m_timeToArrive,		FIELD_FLOAT ),
	DEFINE_FIELD( m_errorTime,			FIELD_FLOAT ),
	DEFINE_FIELD( m_error,				FIELD_FLOAT ),
	DEFINE_FIELD( m_contactAmount,		FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_savedRotDamping,	FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_savedMass,	FIELD_FLOAT ),
	DEFINE_FIELD( m_flLoadWeight,		FIELD_FLOAT ),
	DEFINE_FIELD( m_bCarriedEntityBlocksLOS, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIgnoreRelativePitch, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_attachedEntity,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_angleAlignment, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecPreferredCarryAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_bHasPreferredCarryAngles, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_attachedAnglesPlayerSpace, FIELD_VECTOR ),
	DEFINE_FIELD( m_attachedPositionObjectSpace, FIELD_VECTOR ),

	// Physptrs can't be inside embedded classes
	// DEFINE_PHYSPTR( m_controller ),

END_DATADESC()

const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
const float REDUCED_CARRY_MASS = 1.0f;

CGrabController::CGrabController( void )
{
	m_shadow.dampFactor = 1.0;
	m_shadow.teleportDistance = 0;
	m_errorTime = 0;
	m_error = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = m_shadow.maxAngular;
	m_attachedEntity = NULL;
	m_vecPreferredCarryAngles = vec3_angle;
	m_bHasPreferredCarryAngles = false;
}

CGrabController::~CGrabController( void )
{
	DetachEntity();
}

void CGrabController::OnRestore()
{
	if ( m_controller )
	{
		m_controller->SetEventHandler( this );
	}
}

void CGrabController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

	m_timeToArrive = gpGlobals->frametime;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		
		if ( pObj != NULL )
		{
			pObj->Wake();
		}
		else
		{
			DetachEntity();
		}
	}
}

float CGrabController::ComputeError()
{
	if ( m_errorTime <= 0 )
		return 0;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		Vector pos;
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		if ( pObj )
		{
			pObj->GetShadowPosition( &pos, NULL );
			float error = (m_shadow.targetPosition - pos).Length();
			if ( m_errorTime > 0 )
			{
				if ( m_errorTime > 1 )
				{
					m_errorTime = 1;
				}
				float speed = error / m_errorTime;
				if ( speed > m_shadow.maxSpeed )
				{
					error *= 0.5;
				}
				m_error = (1-m_errorTime) * m_error + error * m_errorTime;
			}
		}
		else
		{
			DevMsg( "Object attached to Physcannon has no physics object\n" );
			DetachEntity();
			return 9999; // force detach
		}
	}
	
	if ( pAttached->IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
	{
		m_error *= 3.0f;
	}

	m_errorTime = 0;

	return m_error;
}


#define MASS_SPEED_SCALE	60
#define MAX_MASS			40


void CGrabController::ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics )
{
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;

	// Compute total mass...
	float flMass = PhysGetEntityMass( pEntity );
	float flMaxMass = physcannon_maxmass.GetFloat();
	if ( flMass <= flMaxMass )
		return;

	float flLerpFactor = clamp( flMass, flMaxMass, 500.0f );
	flLerpFactor = SimpleSplineRemapVal( flLerpFactor, flMaxMass, 500.0f, 0.0f, 1.0f );

	float invMass = pPhysics->GetInvMass();
	float invInertia = pPhysics->GetInvInertia().Length();

	float invMaxMass = 1.0f / MAX_MASS;
	float ratio = invMaxMass / invMass;
	invMass = invMaxMass;
	invInertia *= ratio;

	float maxSpeed = invMass * MASS_SPEED_SCALE * 200;
	float maxAngular = invInertia * MASS_SPEED_SCALE * 360;

	m_shadow.maxSpeed = Lerp( flLerpFactor, m_shadow.maxSpeed, maxSpeed );
	m_shadow.maxAngular = Lerp( flLerpFactor, m_shadow.maxAngular, maxAngular );
}


QAngle CGrabController::TransformAnglesToPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToLocalSpace( anglesIn, test );
	}
	return TransformAnglesToLocalSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}

QAngle CGrabController::TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToWorldSpace( anglesIn, test );
	}
	return TransformAnglesToWorldSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}


void CGrabController::AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon )
{
	// play the impact sound of the object hitting the player
	// used as feedback to let the player know he picked up the object
	PhysicsImpactSound( pPlayer, pPhys, CHAN_STATIC, pPhys->GetMaterialIndex(), pPlayer->VPhysicsGetObject()->GetMaterialIndex(), 1.0, 64 );
	Vector position;
	QAngle angles;
	pPhys->GetPosition( &position, &angles );
	// If it has a preferred orientation, use that instead.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );

//	ComputeMaxSpeed( pEntity, pPhys );

	// Carried entities can never block LOS
	m_bCarriedEntityBlocksLOS = pEntity->BlocksLOS();
	pEntity->SetBlocksLOS( false );
	m_controller = physenv->CreateMotionController( this );
	m_controller->AttachObject( pPhys, true );
	// Don't do this, it's causing trouble with constraint solvers.
	//m_controller->SetPriority( IPhysicsMotionController::HIGH_PRIORITY );

	pPhys->Wake();
	PhysSetGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
	SetTargetPosition( position, angles );
	m_attachedEntity = pEntity;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	m_flLoadWeight = 0;
	float damping = 10;
	float flFactor = count / 7.5f;
	if ( flFactor < 1.0f )
	{
		flFactor = 1.0f;
	}
	for ( int i = 0; i < count; i++ )
	{
		float mass = pList[i]->GetMass();
		pList[i]->GetDamping( NULL, &m_savedRotDamping[i] );
		m_flLoadWeight += mass;
		m_savedMass[i] = mass;

		// reduce the mass to prevent the player from adding crazy amounts of energy to the system
		pList[i]->SetMass( REDUCED_CARRY_MASS / flFactor );
		pList[i]->SetDamping( NULL, &damping );
	}
	
	// Give extra mass to the phys object we're actually picking up
	pPhys->SetMass( REDUCED_CARRY_MASS );
	pPhys->EnableDrag( false );

	m_errorTime = bIsMegaPhysCannon ? -1.5f : -1.0f; // 1 seconds until error starts accumulating
	m_error = 0;
	m_contactAmount = 0;

	m_attachedAnglesPlayerSpace = TransformAnglesToPlayerSpace( angles, pPlayer );
	if ( m_angleAlignment != 0 )
	{
		m_attachedAnglesPlayerSpace = AlignAngles( m_attachedAnglesPlayerSpace, m_angleAlignment );
	}

	VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );

	// If it's a prop, see if it has desired carry angles
	CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pEntity);
	if ( pProp )
	{
		m_bHasPreferredCarryAngles = pProp->GetPropDataAngles( "preferred_carryangles", m_vecPreferredCarryAngles );
	}
	else
	{
		m_bHasPreferredCarryAngles = false;
	}
}

static void ClampPhysicsVelocity( IPhysicsObject *pPhys, float linearLimit, float angularLimit )
{
	Vector vel;
	AngularImpulse angVel;
	pPhys->GetVelocity( &vel, &angVel );
	float speed = VectorNormalize(vel) - linearLimit;
	float angSpeed = VectorNormalize(angVel) - angularLimit;
	speed = speed < 0 ? 0 : -speed;
	angSpeed = angSpeed < 0 ? 0 : -angSpeed;
	vel *= speed;
	angVel *= angSpeed;
	pPhys->AddVelocity( &vel, &angVel );
}

void CGrabController::DetachEntity()
{
	CBaseEntity *pEntity = GetAttached();
	if ( pEntity )
	{
		// Restore the LS blocking state
		pEntity->SetBlocksLOS( m_bCarriedEntityBlocksLOS );
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
		for ( int i = 0; i < count; i++ )
		{
			IPhysicsObject *pPhys = pList[i];
			if ( !pPhys )
				continue;

			// on the odd chance that it's gone to sleep while under anti-gravity
			pPhys->EnableDrag( true );
			pPhys->Wake();
			pPhys->SetMass( m_savedMass[i] );
			pPhys->SetDamping( NULL, &m_savedRotDamping[i] );
			PhysClearGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
			if ( pPhys->GetContactPoint( NULL, NULL ) )
			{
				PhysForceClearVelocity( pPhys );
			}
			else
			{
				ClampPhysicsVelocity( pPhys, hl2_normspeed.GetFloat() * 1.5f, 2.0f * 360.0f );
			}

		}
	}

	m_attachedEntity = NULL;
	physenv->DestroyMotionController( m_controller );
	m_controller = NULL;
}

static bool InContactWithHeavyObject( IPhysicsObject *pObject, float heavyMass )
{
	bool contact = false;
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( !pOther->IsMoveable() || pOther->GetMass() > heavyMass )
		{
			contact = true;
			break;
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	return contact;
}

IMotionEvent::simresult_e CGrabController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	game_shadowcontrol_params_t shadowParams = m_shadow;
	if ( InContactWithHeavyObject( pObject, GetLoadWeight() ) )
	{
		m_contactAmount = Approach( 0.1f, m_contactAmount, deltaTime*2.0f );
	}
	else
	{
		m_contactAmount = Approach( 1.0f, m_contactAmount, deltaTime*2.0f );
	}
	shadowParams.maxAngular = m_shadow.maxAngular * m_contactAmount * m_contactAmount * m_contactAmount;
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, m_timeToArrive, deltaTime );
	
	// Slide along the current contact points to fix bouncing problems
	Vector velocity;
	AngularImpulse angVel;
	pObject->GetVelocity( &velocity, &angVel );
	PhysComputeSlideDirection( pObject, velocity, angVel, &velocity, &angVel, GetLoadWeight() );
	pObject->SetVelocityInstantaneous( &velocity, NULL );

	linear.Init();
	angular.Init();
	m_errorTime += deltaTime;

	return SIM_LOCAL_ACCELERATION;
}

float CGrabController::GetSavedMass( IPhysicsObject *pObject )
{
	CBaseEntity *pHeld = m_attachedEntity;
	if ( pHeld )
	{
		if ( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
					return m_savedMass[i];
			}
		}
	}
	return 0.0f;
}

bool CGrabController::UpdateObject( CBasePlayer *pPlayer, float flError )
{
	CBaseEntity *pEntity = GetAttached();
	if ( !pEntity || ComputeError() > flError || pPlayer->GetGroundEntity() == pEntity || !pEntity->VPhysicsGetObject() )
	{
		return false;
	}

	//Adrian: Oops, our object became motion disabled, let go!
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( pPhys && pPhys->IsMoveable() == false )
	{
		return false;
	}

	Vector forward, right, up;
	QAngle playerAngles = pPlayer->EyeAngles();
	float pitch = AngleDistance(playerAngles.x,0);
	playerAngles.x = clamp( pitch, -75, 75 );
	AngleVectors( playerAngles, &forward, &right, &up );
	
	// Now clamp a sphere of object radius at end to the player's bbox
	Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -forward );
	Vector player2d = pPlayer->CollisionProp()->OBBMaxs();
	float playerRadius = player2d.Length2D();
	float radius = playerRadius + fabs(DotProduct( forward, radial ));

	float distance = 24 + ( radius * 2.0f );

	Vector start = pPlayer->Weapon_ShootPosition();
	Vector end = start + ( forward * distance );

	trace_t	tr;
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEntity, COLLISION_GROUP_NONE );
	Ray_t ray;
	ray.Init( start, end );
	enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	if ( tr.fraction < 0.5 )
	{
		end = start + forward * (radius*0.5f);
	}
	else if ( tr.fraction <= 1.0f )
	{
		end = start + forward * ( distance - radius );
	}
	Vector playerMins, playerMaxs, nearest;
	pPlayer->CollisionProp()->WorldSpaceAABB( &playerMins, &playerMaxs );
	Vector playerLine = pPlayer->CollisionProp()->WorldSpaceCenter();
	CalcClosestPointOnLine( end, playerLine+Vector(0,0,playerMins.z), playerLine+Vector(0,0,playerMaxs.z), nearest, NULL );

	Vector delta = end - nearest;
	float len = VectorNormalize(delta);
	if ( len < radius )
	{
		end = nearest + radius * delta;
	}
/*
	//Show overlays of radius
	if ( g_debug_physcannon.GetBool() )
	{
		NDebugOverlay::Box( end, -Vector( 2,2,2 ), Vector(2,2,2), 0, 255, 0, true, 0 );

		NDebugOverlay::Box( GetAttached()->WorldSpaceCenter(), 
							-Vector( radius, radius, radius), 
							Vector( radius, radius, radius ),
							255, 0, 0,
							true,
							0.0f );
	}
*/

	QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, pPlayer );
	
	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );

	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}

	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_attachedPositionObjectSpace, attachedToWorld, offset );

	SetTargetPosition( end - offset, angles );
	return true;
}

//-----------------------------------------------------------------------------
// Player pickup controller
//-----------------------------------------------------------------------------

class CPlayerPickupController : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CPlayerPickupController, CBaseEntity );
public:
	void Init( CBasePlayer *pPlayer, CBaseEntity *pObject );
	void Shutdown( bool bThrown = false );
	bool OnControls( CBaseEntity *pControls ) { return true; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void OnRestore()
	{
		m_grabController.OnRestore();
	}
	void VPhysicsUpdate( IPhysicsObject *pPhysics ){}
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics ) {}

	bool IsHoldingEntity( CBaseEntity *pEnt );
	CGrabController &GetGrabController() { return m_grabController; }

private:
	CGrabController		m_grabController;
	CBasePlayer			*m_pPlayer;
};

LINK_ENTITY_TO_CLASS( player_pickup, CPlayerPickupController );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CPlayerPickupController )

	DEFINE_EMBEDDED( m_grabController ),

	// Physptrs can't be inside embedded classes
	DEFINE_PHYSPTR( m_grabController.m_controller ),

	DEFINE_FIELD( m_pPlayer,		FIELD_CLASSPTR ),
	
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pObject - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Init( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
	// Holster player's weapon
	if ( pPlayer->GetActiveWeapon() )
	{
		if ( !pPlayer->GetActiveWeapon()->Holster() )
		{
			Shutdown();
			return;
		}
	}

	// If the target is debris, convert it to non-debris
	if ( pObject->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pObject->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	// done so I'll go across level transitions with the player
	SetParent( pPlayer );
	m_grabController.SetIgnorePitch( true );
	m_grabController.SetAngleAlignment( DOT_30DEGREE );
	m_pPlayer = pPlayer;
	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	Pickup_OnPhysGunPickup( pObject, m_pPlayer );
	m_grabController.AttachEntity( pPlayer, pObject, pPhysics );
	
	m_pPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	m_pPlayer->SetUseEntity( this );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::Shutdown( bool bThrown )
{
	CBaseEntity *pObject = m_grabController.GetAttached();

	m_grabController.DetachEntity();

	if ( pObject != NULL )
	{
		Pickup_OnPhysGunDrop( pObject, m_pPlayer, bThrown ? THROWN_BY_PLAYER : DROPPED_BY_PLAYER );
	}

	if ( m_pPlayer )
	{
		m_pPlayer->SetUseEntity( NULL );
		if ( m_pPlayer->GetActiveWeapon() )
		{
			m_pPlayer->GetActiveWeapon()->Deploy();
		}

		m_pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	}
	Remove();
}


void CPlayerPickupController::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ToBasePlayer(pActivator) == m_pPlayer )
	{
		CBaseEntity *pAttached = m_grabController.GetAttached();

		// UNDONE: Use vphysics stress to decide to drop objects
		// UNDONE: Must fix case of forcing objects into the ground you're standing on (causes stress) before that will work
		if ( !pAttached || useType == USE_OFF || (m_pPlayer->m_nButtons & IN_ATTACK2) || m_grabController.ComputeError() > 12 )
		{
			Shutdown();
			return;
		}
		
		//Adrian: Oops, our object became motion disabled, let go!
		IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
		if ( pPhys && pPhys->IsMoveable() == false )
		{
			Shutdown();
			return;
		}

#if STRESS_TEST
		vphysics_objectstress_t stress;
		CalculateObjectStress( pAttached->VPhysicsGetObject(), pAttached, &stress );
		if ( stress.exertedStress > 250 )
		{
			Shutdown();
			return;
		}
#endif
		// +ATTACK will throw phys objects
		if ( m_pPlayer->m_nButtons & IN_ATTACK )
		{
			Shutdown( true );
			Vector vecLaunch;
			m_pPlayer->EyeVectors( &vecLaunch );
			// JAY: Scale this with mass because some small objects really go flying
			float massFactor = clamp( pAttached->VPhysicsGetObject()->GetMass(), 0.5, 15 );
			massFactor = RemapVal( massFactor, 0.5, 15, 0.5, 4 );
			vecLaunch *= player_throwforce.GetFloat() * massFactor;

			pAttached->VPhysicsGetObject()->ApplyForceCenter( vecLaunch );
			AngularImpulse aVel = RandomAngularImpulse( -10, 10 ) * massFactor;
			pAttached->VPhysicsGetObject()->ApplyTorqueCenter( aVel );
			return;
		}

		if ( useType == USE_SET )
		{
			// update position
			m_grabController.UpdateObject( m_pPlayer, 12 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPlayerPickupController::IsHoldingEntity( CBaseEntity *pEnt )
{
	return ( m_grabController.GetAttached() == pEnt );
}

ConVar hl1_new_pull( "hl1_new_pull", "1" );
void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
	if( hl1_new_pull.GetBool() )
	{
		CHL1_Player	*pHL1Player = dynamic_cast<CHL1_Player*>(pPlayer);
		if( pHL1Player && !pHL1Player->IsPullingObject() )
		{
			pHL1Player->StartPullingObject(pObject);
		}
	}
}
