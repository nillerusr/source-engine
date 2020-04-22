//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Half-Life 1 game rules, such as the relationship tables and ammo
//			damage cvars.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl1_gamerules.h"
#include "ammodef.h"


#ifdef CLIENT_DLL

#else

	#include "player.h"
	#include "game.h"
	#include "gamerules.h"
	#include "teamplay_gamerules.h"
	#include "hl1_player.h"
	#include "voice_gamemgr.h"
	#include "hl1mp_weapon_satchel.h"
#endif


REGISTER_GAMERULES_CLASS( CHalfLife1 );


ConVar sk_plr_dmg_crowbar			( "sk_plr_dmg_crowbar",			"0", FCVAR_REPLICATED );

ConVar sk_npc_dmg_9mm_bullet		( "sk_npc_dmg_9mm_bullet",		"0", FCVAR_REPLICATED );
ConVar sk_plr_dmg_9mm_bullet		( "sk_plr_dmg_9mm_bullet",		"0", FCVAR_REPLICATED );
ConVar sk_max_9mm_bullet			( "sk_max_9mm_bullet",			"0", FCVAR_REPLICATED );

ConVar sk_npc_dmg_9mmAR_bullet		( "sk_npc_dmg_9mmAR_bullet",	"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_357_bullet		( "sk_plr_dmg_357_bullet",		"0", FCVAR_REPLICATED );
ConVar sk_max_357_bullet			( "sk_max_357_bullet",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_buckshot			( "sk_plr_dmg_buckshot",		"0", FCVAR_REPLICATED );
ConVar sk_max_buckshot				( "sk_max_buckshot",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_mp5_grenade		( "sk_plr_dmg_mp5_grenade",		"0", FCVAR_REPLICATED );
ConVar sk_max_mp5_grenade			( "sk_max_mp5_grenade",			"0", FCVAR_REPLICATED );
ConVar sk_mp5_grenade_radius		( "sk_mp5_grenade_radius",		"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_rpg				( "sk_plr_dmg_rpg",				"0", FCVAR_REPLICATED );
ConVar sk_max_rpg_rocket			( "sk_max_rpg_rocket",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_xbow_bolt_plr		( "sk_plr_dmg_xbow_bolt_plr",	"0", FCVAR_REPLICATED );
ConVar sk_plr_dmg_xbow_bolt_npc		( "sk_plr_dmg_xbow_bolt_npc",	"0", FCVAR_REPLICATED );
ConVar sk_max_xbow_bolt				( "sk_max_xbow_bolt",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_egon_narrow		( "sk_plr_dmg_egon_narrow",		"0", FCVAR_REPLICATED );
ConVar sk_plr_dmg_egon_wide			( "sk_plr_dmg_egon_wide",		"0", FCVAR_REPLICATED );
ConVar sk_max_uranium				( "sk_max_uranium",				"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_gauss				( "sk_plr_dmg_gauss",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_grenade			( "sk_plr_dmg_grenade",			"0", FCVAR_REPLICATED );
ConVar sk_max_grenade				( "sk_max_grenade",				"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_hornet			( "sk_plr_dmg_hornet",			"0", FCVAR_REPLICATED );
ConVar sk_npc_dmg_hornet			( "sk_npc_dmg_hornet",			"0", FCVAR_REPLICATED );
ConVar sk_max_hornet				( "sk_max_hornet",				"0", FCVAR_REPLICATED );

ConVar sk_max_snark					( "sk_max_snark",				"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_tripmine			( "sk_plr_dmg_tripmine",		"0", FCVAR_REPLICATED );
ConVar sk_max_tripmine				( "sk_max_tripmine",			"0", FCVAR_REPLICATED );

ConVar sk_plr_dmg_satchel			( "sk_plr_dmg_satchel",			"0", FCVAR_REPLICATED );
ConVar sk_max_satchel				( "sk_max_satchel",				"0", FCVAR_REPLICATED );

ConVar sk_npc_dmg_12mm_bullet		( "sk_npc_dmg_12mm_bullet",		"0", FCVAR_REPLICATED );

ConVar sk_mp_dmg_multiplier ( "sk_mp_dmg_multiplier", "2.0" );

// Damage Queries.

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CHalfLife1::Damage_GetShowOnHud( void )
{
	int iDamage = (DMG_POISON | DMG_ACID | DMG_DISSOLVE/*DMG_FREEZE | DMG_SLOWFREEZE*/ | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK);
	return iDamage;
}

#ifndef CLIENT_DLL

	extern bool		g_fGameOver;

	const char *CHalfLife1::GetGameDescription( void )
	{
		if ( IsMultiplayer() )
		{
			return "Half-Life 1: Deathmatch";
		}
		else
		{
			return "Half-Life 1";
		}
	}

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
		{
			return true;
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

	//-----------------------------------------------------------------------------
	// Purpose:
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	CHalfLife1::CHalfLife1()
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: called each time a player uses a "cmd" command
	// Input  : *pEdict - the player who issued the command
	//			Use engine.Cmd_Argv,  engine.Cmd_Argv, and engine.Cmd_Argc to get 
	//			pointers the character string command.
	//-----------------------------------------------------------------------------
	bool CHalfLife1::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		if( BaseClass::ClientCommand( pEdict, args ) )
			return true;

		CHL1_Player *pPlayer = (CHL1_Player *) pEdict;
		
		if ( pPlayer->ClientCommand( args ) )
			return true;
			
		return false;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------
	void CHalfLife1::PlayerSpawn( CBasePlayer *pPlayer )
	{
	//	pPlayer->EquipSuit();
	}


	//-----------------------------------------------------------------------------
	// Purpose: MULTIPLAYER BODY QUE HANDLING
	//-----------------------------------------------------------------------------
	class CCorpse : public CBaseAnimating
	{
		DECLARE_CLASS( CCorpse, CBaseAnimating );
	public:

		DECLARE_SERVERCLASS();

		virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }	

	public:
		CNetworkVar( int, m_nReferencePlayer );
	};

	IMPLEMENT_SERVERCLASS_ST(CCorpse, DT_Corpse)
		SendPropInt( SENDINFO(m_nReferencePlayer), 10, SPROP_UNSIGNED )
	END_SEND_TABLE()

	LINK_ENTITY_TO_CLASS( bodyque, CCorpse );


	CCorpse		*g_pBodyQueueHead;

	void InitBodyQue(void)
	{
		CCorpse *pEntity = ( CCorpse * )CreateEntityByName( "bodyque" );
		pEntity->AddEFlags( EFL_KEEP_ON_RECREATE_ENTITIES );
		g_pBodyQueueHead = pEntity;
		CCorpse *p = g_pBodyQueueHead;
		
		// Reserve 3 more slots for dead bodies
		for ( int i = 0; i < 3; i++ )
		{
			CCorpse *next = ( CCorpse * )CreateEntityByName( "bodyque" );
			next->AddEFlags( EFL_KEEP_ON_RECREATE_ENTITIES );
			p->SetOwnerEntity( next );
			p = next;
		}
		
		p->SetOwnerEntity( g_pBodyQueueHead );
	}

	//-----------------------------------------------------------------------------
	// Purpose: make a body que entry for the given ent so the ent can be respawned elsewhere
	// GLOBALS ASSUMED SET:  g_eoBodyQueueHead
	//-----------------------------------------------------------------------------
	void CopyToBodyQue( CBaseAnimating *pCorpse ) 
	{
		if ( pCorpse->IsEffectActive( EF_NODRAW ) )
			return;

		CCorpse *pHead	= g_pBodyQueueHead;

		pHead->CopyAnimationDataFrom( pCorpse );

		pHead->SetMoveType( MOVETYPE_FLYGRAVITY );
		pHead->SetAbsVelocity( pCorpse->GetAbsVelocity() );
		pHead->ClearFlags();
		pHead->m_nReferencePlayer	= ENTINDEX( pCorpse );

		pHead->SetLocalAngles( pCorpse->GetAbsAngles() );
		UTIL_SetOrigin(pHead, pCorpse->GetAbsOrigin());

		UTIL_SetSize(pHead, pCorpse->WorldAlignMins(), pCorpse->WorldAlignMaxs());
		g_pBodyQueueHead = (CCorpse *)pHead->GetOwnerEntity();
	}

	//------------------------------------------------------------------------------
	// Purpose : Initialize all default class relationships
	// Input   :
	// Output  :
	//------------------------------------------------------------------------------
	void CHalfLife1::InitDefaultAIRelationships( void )
	{
		int i, j;

		//  Allocate memory for default relationships
		CBaseCombatCharacter::AllocateDefaultRelationships();

		// --------------------------------------------------------------
		// First initialize table so we can report missing relationships
		// --------------------------------------------------------------
		for (i=0;i<NUM_AI_CLASSES;i++)
		{
			for (j=0;j<NUM_AI_CLASSES;j++)
			{
				// By default all relationships are neutral of priority zero
				CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
			}
		}


		// ------------------------------------------------------------
		//	> CLASS_NONE
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_PLAYER,			D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_HUMAN_PASSIVE,	D_NU, 0 );		
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_PLAYER_ALLY,		D_NU, 0 );		
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_ALIEN_PREY,		D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_ALIEN_MONSTER,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_ALIEN_PREDATOR,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_HUMAN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_NONE,				CLASS_INSECT,			D_NU, 0 );
		
		
		// ------------------------------------------------------------
		//	> CLASS_HUMAN_PASSIVE
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_PLAYER,			D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_HUMAN_PASSIVE,	D_LI, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_PLAYER_ALLY,		D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_ALIEN_PREY,		D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_ALIEN_MILITARY,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_ALIEN_PREDATOR,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_PASSIVE,		CLASS_INSECT,			D_NU, 0 );
		
		

		// ------------------------------------------------------------
		//	> CLASS_PLAYER
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_PLAYER,			D_LI, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_HUMAN_PASSIVE,	D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_PLAYER_ALLY,		D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,		  	    CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,		  	    CLASS_ALIEN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_ALIEN_PREDATOR,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_MACHINE,			D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_ALIEN_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_PLAYER_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER,				CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_PLAYER_ALLY
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_PLAYER,			D_LI, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_HUMAN_PASSIVE,	D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_PLAYER_ALLY,		D_LI, 0 );	
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_ALIEN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_ALIEN_PREDATOR,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_MACHINE,			D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_ALLY,		CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_ALIEN_PREY
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_ALIEN_PREY,		D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_ALIEN_MONSTER,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_ALIEN_PREDATOR,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREY,			CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_ALIEN_MILITARY
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_ALIEN_PREY,		D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_ALIEN_MONSTER,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_ALIEN_PREDATOR,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_MACHINE,			D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MILITARY,		CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_ALIEN_MONSTER
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_ALIEN_PREY,		D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_ALIEN_MONSTER,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_ALIEN_PREDATOR,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_MACHINE,			D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_MONSTER,		CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_ALIEN_PREDATOR
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_ALIEN_MONSTER,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_ALIEN_PREDATOR,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_PREDATOR,		CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_HUMAN_MILITARY
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_NONE,				D_NU, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_ALIEN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_ALIEN_PREDATOR,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_HUMAN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_HUMAN_MILITARY,		CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_MACHINE
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_NONE,				D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_HUMAN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_ALIEN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_ALIEN_PREDATOR,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_ALIEN_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_PLAYER_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_MACHINE,			CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_ALIEN_BIOWEAPON
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_NONE,				D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_ALIEN_MILITARY,	D_LI, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_ALIEN_PREDATOR,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_PLAYER_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_ALIEN_BIOWEAPON,	CLASS_INSECT,			D_NU, 0 );

		// ------------------------------------------------------------
		//	> CLASS_PLAYER_BIOWEAPON
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_NONE,				D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_MACHINE,			D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_PLAYER,			D_HT, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_HUMAN_PASSIVE,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_HUMAN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_ALIEN_MILITARY,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_ALIEN_MONSTER,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_ALIEN_PREY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_ALIEN_PREDATOR,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_PLAYER_ALLY,		D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_ALIEN_BIOWEAPON,	D_HT, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_PLAYER_BIOWEAPON,	CLASS_INSECT,			D_NU, 0 );
		
		
		// ------------------------------------------------------------
		//	> CLASS_INSECT
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_NONE,				D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_MACHINE,			D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_PLAYER,			D_FR, 0 );			
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_HUMAN_PASSIVE,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_HUMAN_MILITARY,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_ALIEN_MILITARY,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_ALIEN_MONSTER,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_ALIEN_PREY,		D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_ALIEN_PREDATOR,	D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_PLAYER_ALLY,		D_FR, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_ALIEN_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_PLAYER_BIOWEAPON,	D_NU, 0 );
		CBaseCombatCharacter::SetDefaultRelationship( CLASS_INSECT,				CLASS_INSECT,			D_NU, 0 );
	}

	//------------------------------------------------------------------------------
	// Purpose : Return classify text for classify type
	// Input   :
	// Output  :
	//------------------------------------------------------------------------------
	const char* CHalfLife1::AIClassText(int classType)
	{
		switch (classType)
		{
			case CLASS_NONE:			return "CLASS_NONE";
			case CLASS_HUMAN_PASSIVE:	return "CLASS_HUMAN_PASSIVE";
			case CLASS_PLAYER_ALLY:		return "CLASS_PLAYER_ALLY";
			case CLASS_PLAYER:			return "CLASS_PLAYER";
			case CLASS_ALIEN_PREY:		return "CLASS_ALIEN_PREY";
			case CLASS_ALIEN_MILITARY:	return "CLASS_ALIEN_MILITARY"; 
			case CLASS_ALIEN_MONSTER:	return "CLASS_ALIEN_MONSTER";
			case CLASS_ALIEN_PREDATOR:	return "CLASS_ALIEN_PREDATOR";
			case CLASS_HUMAN_MILITARY:	return "CLASS_HUMAN_MILITARY";
			case CLASS_MACHINE:			return "CLASS_MACHINE";
			case CLASS_ALIEN_BIOWEAPON:	return "CLASS_ALIEN_BIOWEAPON";
			case CLASS_PLAYER_BIOWEAPON: return "CLASS_PLAYER_BIOWEAPON";
		
			default:					return "MISSING CLASS in ClassifyText()";
		}
	}


	void CHalfLife1::PlayerThink( CBasePlayer *pPlayer )
	{
	}


	bool CHalfLife1::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		if ( FClassnameIs( pWeapon, "weapon_satchel" ) )
		{
			CWeaponSatchel *satchel = static_cast< CWeaponSatchel * >( pPlayer->Weapon_OwnsThisType( "weapon_satchel" ) );
			if ( satchel )
			{
				if ( satchel->HasChargeDeployed() )
				{
					return false;
				}
			}
		}

		return true;
	}

		
	float CHalfLife1::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		CBaseEntity *pGroundEntity = pPlayer->GetGroundEntity();

		if( pGroundEntity && pGroundEntity->ClassMatches( "func_breakable" ) )
		{		
			// FIXME touchtrace will be wrong.
			pGroundEntity->Touch( pPlayer );

			if( pGroundEntity->m_iHealth <= 0 )
			{
				// The breakable broke when we hit it, don't take falling damage
				return 0;
			}
		}

		return BaseClass::FlPlayerFallDamage( pPlayer );
	}


	class CTraceFilterHitAllExcept : public CTraceFilter
	{
	public:
		// It does have a base, but we'll never network anything below here..
		DECLARE_CLASS_NOBASE( CTraceFilterHitAllExcept );

		CTraceFilterHitAllExcept( const IHandleEntity *passedict )
		{
			m_pPassEnt = passedict;
		}

		bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
		{ 
			if ( m_pPassEnt && ( m_pPassEnt == pServerEntity ) )
			{
				return false;
			}
		
			return true;
		}

	private:
		const IHandleEntity *m_pPassEnt;
	};

	void CHalfLife1::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;

		Vector vecSrc = vecSrcIn;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;
		
		vecSrc.z += 1;// in case grenade is lying on the ground

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
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

				CTraceFilterHitAllExcept traceFilter( info.GetInflictor() );
  				UTIL_TraceLine( vecSrc, vecSpot, CONTENTS_SOLID, &traceFilter, &tr );
				
				if ( tr.fraction == 1.0 || tr.m_pEnt == pEntity )
				{// the explosion can 'see' this entity, so hurt them!
					if (tr.startsolid)
					{
						// if we're stuck inside them, fixup the position and distance
						tr.endpos = vecSrc;
						tr.fraction = 0.0;
					}
					
					// decrease damage for an ent that's farther from the bomb.
					flAdjustedDamage = ( vecSrc - tr.endpos ).Length() * falloff;
					flAdjustedDamage = info.GetDamage() - flAdjustedDamage;
				
					if ( flAdjustedDamage > 0 )
					{
						CTakeDamageInfo adjustedInfo = info;
						adjustedInfo.SetDamage( flAdjustedDamage );

	 					Vector vecDir = vecSpot - vecSrc;
	 					VectorNormalize( vecDir );

						// If we don't have a damage force, manufacture one
						if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
						{
							CalculateExplosiveDamageForce( &adjustedInfo, vecDir, vecSrc );
						}
						else
						{
							// Assume the force passed in is the maximum force. Decay it based on falloff.
							float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
							adjustedInfo.SetDamageForce( vecDir * flForce );
							adjustedInfo.SetDamagePosition( vecSrc );
						}

						if (tr.fraction != 1.0)
						{
							ClearMultiDamage( );
							Vector dir = tr.endpos - vecSrc;
							VectorNormalize( dir );
							pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
							ApplyMultiDamage();
						}
						else
						{
							pEntity->TakeDamage( adjustedInfo );
						}
						// Now hit all triggers along the way that respond to damage... 
						Vector dir = tr.endpos - vecSrc;
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, tr.endpos, dir );
					}
				}
			}
		}
	}
#endif



// ------------------------------------------------------------------------------------- //
// CHalfLife1 shared implementation.
// ------------------------------------------------------------------------------------- //

bool CHalfLife1::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	// HL2 treats movement and tracing against players the same, so just remap here
	if ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		collisionGroup0 = COLLISION_GROUP_PLAYER;
	}

	if( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		collisionGroup1 = COLLISION_GROUP_PLAYER;
	}

	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		int tmp = collisionGroup0;
		collisionGroup0 = collisionGroup1;
		collisionGroup1 = tmp;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

// ------------------------------------------------------------------------------------- //
// Global functions.
// ------------------------------------------------------------------------------------- //

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)

CAmmoDef *GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;
		
		def.AddAmmoType( "9mmRound",		DMG_BULLET | DMG_NEVERGIB,	TRACER_LINE, "sk_plr_dmg_9mm_bullet",	"sk_npc_dmg_9mm_bullet","sk_max_9mm_bullet",	BULLET_IMPULSE(500, 1325), 0 );
		def.AddAmmoType( "357Round",		DMG_BULLET | DMG_NEVERGIB,	TRACER_NONE, "sk_plr_dmg_357_bullet",	NULL,					"sk_max_357_bullet",	BULLET_IMPULSE(650, 6000), 0 );
		def.AddAmmoType( "Buckshot",		DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE, "sk_plr_dmg_buckshot",		NULL,					"sk_max_buckshot",		BULLET_IMPULSE(200, 1200), 0 );
		def.AddAmmoType( "XBowBolt",		DMG_BULLET | DMG_NEVERGIB,	TRACER_LINE, "sk_plr_dmg_xbow_bolt_plr",NULL,					"sk_max_xbow_bolt",		BULLET_IMPULSE( 200, 1200), 0 );
		def.AddAmmoType( "MP5_Grenade",		DMG_BURN | DMG_BLAST,		TRACER_NONE, "sk_plr_dmg_mp5_grenade",	NULL,					"sk_max_mp5_grenade",	0, 0 );
		def.AddAmmoType( "RPG_Rocket",		DMG_BURN | DMG_BLAST,		TRACER_NONE, "sk_plr_dmg_rpg",			NULL,					"sk_max_rpg_rocket",	0, 0 );
		def.AddAmmoType( "Uranium",			DMG_ENERGYBEAM,				TRACER_NONE, NULL,						NULL,					"sk_max_uranium",		0, 0 );
		def.AddAmmoType( "Grenade",			DMG_BURN | DMG_BLAST,		TRACER_NONE, "sk_plr_dmg_grenade",		NULL,					"sk_max_grenade",		0, 0 );
		def.AddAmmoType( "Hornet",			DMG_BULLET,					TRACER_NONE, "sk_plr_dmg_hornet",		"sk_npc_dmg_hornet",	"sk_max_hornet",		BULLET_IMPULSE(100, 1200), 0 );
		def.AddAmmoType( "Snark",			DMG_SLASH,					TRACER_NONE, "sk_snark_dmg_bite",		NULL,					"sk_max_snark",			0, 0 );
		def.AddAmmoType( "TripMine",		DMG_BURN | DMG_BLAST,		TRACER_NONE, "sk_plr_dmg_tripmine",		NULL,					"sk_max_tripmine",		0, 0 );
		def.AddAmmoType( "Satchel",			DMG_BURN | DMG_BLAST,		TRACER_NONE, "sk_plr_dmg_satchel",		NULL,					"sk_max_satchel",		0, 0 );

		def.AddAmmoType( "12mmRound",		DMG_BULLET | DMG_NEVERGIB,	TRACER_LINE, NULL,						"sk_npc_dmg_12mm_bullet",NULL,					BULLET_IMPULSE(300, 1200), 0 );

		def.AddAmmoType( "Gravity",			DMG_CRUSH,					TRACER_NONE, 0,							0,						8,					0, 0 );
	}

	return &def;
}
