//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tfc_gamerules.h"
#include "ammodef.h"
#include "KeyValues.h"
#include "weapon_tfcbase.h"


#ifdef CLIENT_DLL

	#include "c_tfc_player.h"

#else
	
	#include "voice_gamemgr.h"
	#include "team.h"
	#include "tfc_bot_temp.h"
	#include "tfc_player.h"
	#include "tfc_timer.h"
	#include "tfc_team.h"

#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS(info_player_terrorist, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_counterterrorist,CPointEntity);
#endif

REGISTER_GAMERULES_CLASS( CTFCGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CTFCGameRules, DT_TFCGameRules )
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( tfc_gamerules, CTFCGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( TFCGameRulesProxy, DT_TFCGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_TFCGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CTFCGameRules *pRules = TFCGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CTFCGameRulesProxy, DT_TFCGameRulesProxy )
		RecvPropDataTable( "tfc_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_TFCGameRules ), RecvProxy_TFCGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_TFCGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CTFCGameRules *pRules = TFCGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CTFCGameRulesProxy, DT_TFCGameRulesProxy )
		SendPropDataTable( "tfc_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_TFCGameRules ), SendProxy_TFCGameRules )
	END_SEND_TABLE()
#endif


ConVar mp_fadetoblack(
	"mp_fadetoblack",
	"0",
	FCVAR_REPLICATED,
	"fade a player's screen to black when he dies" );


// (We clamp ammo ourselves elsewhere).
ConVar ammo_max( "ammo_max", "5000", FCVAR_REPLICATED );


CTFCGameRules::CTFCGameRules()
{
	CTF_Map = true;

#ifdef GAME_DLL
	// Create the team managers
	for ( int i = 0; i < ARRAYSIZE( teamnames ); i++ )
	{
		CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "tfc_team_manager" ));
		pTeam->Init( teamnames[i], i );

		g_Teams.AddToTail( pTeam );
	}
#endif
}


#ifdef CLIENT_DLL


#else

	
	int cease_fire;
	int no_cease_fire_text;


	// --------------------------------------------------------------------------------------------------- //
	// Voice helper
	// --------------------------------------------------------------------------------------------------- //

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker )
		{
			// Dead players can only be heard by other dead team mates
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}

			return ( pListener->InSameTeam( pTalker ) );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;



	// --------------------------------------------------------------------------------------------------- //
	// Globals.
	// --------------------------------------------------------------------------------------------------- //

	// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
	char *sTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"Terrorist",
		"Counter-Terrorist"
	};


	// --------------------------------------------------------------------------------------------------- //
	// Global helper functions.
	// --------------------------------------------------------------------------------------------------- //
	
	// World.cpp calls this but we don't use it in TFC.
	void InitBodyQue()
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CTFCGameRules::~CTFCGameRules()
	{
		// Note, don't delete each team since they are in the gEntList and will 
		// automatically be deleted from there, instead.
		g_Teams.Purge();
	}

	//-----------------------------------------------------------------------------
	// Purpose: TF2 Specific Client Commands
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	bool CTFCGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		CTFCPlayer *pPlayer = ToTFCPlayer( pEdict );

		if( pPlayer->ClientCommand( args ) )
			return true;

		return BaseClass::ClientCommand( pEdict, args );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------

	void CTFCGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
	{
		RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, false );
	}

	// Add the ability to ignore the world trace
	void CTFCGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;
		Vector		vecToTarget;
		Vector		vecEndPos;

		Vector vecSrc = vecSrcIn;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;
		
		vecSrc.z += 1;// in case grenade is lying on the ground

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity() )
		{
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
				{// houndeyes don't hurt other houndeyes with their attack
					continue;
				}

				// blast's don't tavel into or out of water
				if (bInWater && pEntity->GetWaterLevel() == 0)
					continue;
				if (!bInWater && pEntity->GetWaterLevel() == 3)
					continue;

				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );



				bool bHit = false;

				if( bIgnoreWorld )
				{
					vecEndPos = vecSpot;
					bHit = true;
				}
				else
				{
					UTIL_TraceLine( vecSrc, vecSpot, MASK_SOLID_BRUSHONLY, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

					if (tr.startsolid)
					{
						// if we're stuck inside them, fixup the position and distance
						tr.endpos = vecSrc;
						tr.fraction = 0.0;
					}

					vecEndPos = tr.endpos;

					if( tr.fraction == 1.0 || tr.m_pEnt == pEntity )
					{
						bHit = true;
					}
				}

				if ( bHit )
				{
					// the explosion can 'see' this entity, so hurt them!
					//vecToTarget = ( vecSrc - vecEndPos );
					vecToTarget = ( vecEndPos - vecSrc );

					// decrease damage for an ent that's farther from the bomb.
					flAdjustedDamage = vecToTarget.Length() * falloff;
					flAdjustedDamage = info.GetDamage() - flAdjustedDamage;
				
					if ( flAdjustedDamage > 0 )
					{
						CTakeDamageInfo adjustedInfo = info;
						adjustedInfo.SetDamage( flAdjustedDamage );

						Vector dir = vecToTarget;
						VectorNormalize( dir );

						// If we don't have a damage force, manufacture one
						if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
						{
							CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc, 1.5	/* explosion scale! */ );
						}
						else
						{
							// Assume the force passed in is the maximum force. Decay it based on falloff.
							float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
							adjustedInfo.SetDamageForce( dir * flForce );
							adjustedInfo.SetDamagePosition( vecSrc );
						}

						pEntity->TakeDamage( adjustedInfo );
			
						// Now hit all triggers along the way that respond to damage... 
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecEndPos, dir );
					}
				}
			}
		}
	}

	void CTFCGameRules::Think()
	{
		Timer_UpdateAll();
		
		BaseClass::Think();
	}

	const char *CTFCGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
	{
		return "(chat prefix)";
	}


	bool CTFCGameRules::IsInPreMatch() const
	{
		// TFCTODO    return (cb_prematch_time > gpGlobals->time)
		return false;
	}

	float CTFCGameRules::GetPreMatchEndTime() const
	{
		//TFCTODO: implement this.
		return gpGlobals->curtime;
	}

	void CTFCGameRules::TFCGoToIntermission()
	{
		// TFCTODO: implement this.
		Assert( false );
	}


#endif


bool CTFCGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		swap(collisionGroup0,collisionGroup1);
	}
	
	//Don't stand on COLLISION_GROUP_WEAPONs
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}
	
	if ( collisionGroup0 == COLLISION_GROUP_PLAYER )
	{
		// Players don't collide with objects or other players
		if ( collisionGroup1 == COLLISION_GROUP_PLAYER  )
			 return false;
 	}

	if ( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		// This is only for probing, so it better not be on both sides!!!
		Assert( collisionGroup0 != COLLISION_GROUP_PLAYER_MOVEMENT );

		// No collide with players any more
		// Nor with objects or grenades
		switch ( collisionGroup0 )
		{
		default:
			break;
		case COLLISION_GROUP_PLAYER:
			return false;
		}
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}


//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			1	

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;
		
		// Start at 1 here and skip the dummy ammo type to make CAmmoDef use the same indices
		// as our #defines.
		for ( int i=1; i < TFC_NUM_AMMO_TYPES; i++ )
		{
			def.AddAmmoType( g_AmmoTypeNames[i], DMG_BULLET, TRACER_LINE, 0, 0, "ammo_max", 2400, 10, 14 );
			Assert( def.Index( g_AmmoTypeNames[i] ) == i );
		}
	}

	return &def;
}


