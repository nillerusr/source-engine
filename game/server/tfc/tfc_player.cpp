//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tfc_player.h"
#include "tfc_gamerules.h"
#include "KeyValues.h"
#include "viewport_panel_names.h"
#include "client.h"
#include "team.h"
#include "weapon_tfcbase.h"
#include "tfc_client.h"
#include "tfc_mapitems.h"
#include "tfc_timer.h"
#include "tfc_engineer.h"
#include "tfc_team.h"


#define TFC_PLAYER_MODEL "models/player/pyro.mdl"


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
					{
					}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED )
	SendPropInt( SENDINFO( m_nData ), 32 )
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
	CPVSFilter filter( pPlayer->EyePosition() );
	
	// The player himself doesn't need to be sent his animation events 
	// unless cs_showanimstate wants to show them.
	if ( !ToolsEnabled() && ( cl_showanimstate.GetInt() == pPlayer->entindex() ) )
	{
		filter.RemoveRecipient( pPlayer );
	}

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}


// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( player, CTFCPlayer );
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST( CTFCPlayer, DT_TFCPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11 ),

	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_TFCPlayerShared ) )
END_SEND_TABLE()


// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT );


CTFCPlayer::CTFCPlayer()
{
	m_PlayerAnimState = CreatePlayerAnimState( this );
	item_list = 0;

	UseClientSideAnimation();
	m_angEyeAngles.Init();
	m_pCurStateInfo = NULL;
	m_lifeState = LIFE_DEAD; // Start "dead".

	SetViewOffset( TFC_PLAYER_VIEW_OFFSET );

	SetContextThink( &CTFCPlayer::TFCPlayerThink, gpGlobals->curtime, "TFCPlayerThink" );
}


void CTFCPlayer::TFCPlayerThink()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnThink )
		(this->*m_pCurStateInfo->pfnThink)();

	SetContextThink( &CTFCPlayer::TFCPlayerThink, gpGlobals->curtime, "TFCPlayerThink" );
}


CTFCPlayer::~CTFCPlayer()
{
	m_PlayerAnimState->Release();
}


CTFCPlayer *CTFCPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CTFCPlayer::s_PlayerEdict = ed;
	return (CTFCPlayer*)CreateEntityByName( className );
}


void CTFCPlayer::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}


void CTFCPlayer::Precache()
{
	for ( int i=0; i < PC_LASTCLASS; i++ )
		PrecacheModel( GetTFCClassInfo( i )->m_pModelName );

	PrecacheScriptSound( "Player.Spawn" );

	BaseClass::Precache();
}


void CTFCPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	State_Enter( STATE_WELCOME );
}


void CTFCPlayer::Spawn()
{
	SetModel( GetTFCClassInfo( m_Shared.GetPlayerClass() )->m_pModelName );

	SetMoveType( MOVETYPE_WALK );
	m_iLegDamage = 0;

	BaseClass::Spawn();

	// Kind of lame, but CBasePlayer::Spawn resets a lot of the state that we initially want on.
	// So if we're in the welcome state, call its enter function to reset 
	if ( m_Shared.State_Get() == STATE_WELCOME )
	{
		State_Enter_WELCOME();
	}

	// If they were dead, then they're respawning. Put them in the active state.
	if ( m_Shared.State_Get() == STATE_DYING )
	{
		State_Transition( STATE_ACTIVE );
	}

	// If they're spawning into the world as fresh meat, give them items and stuff.
	if ( m_Shared.State_Get() == STATE_ACTIVE )
	{
		EmitSound( "Player.Spawn" );
		GiveDefaultItems();
	}
}


void CTFCPlayer::ForceRespawn()
{
	//TFCTODO: goldsrc tfc has a big function for this.. doing what I'm doing here may not work.
	respawn( this, false );
}


void CTFCPlayer::GiveDefaultItems()
{
	switch( m_Shared.GetPlayerClass() )
	{
		case PC_HWGUY:
		{
			GiveNamedItem( "weapon_crowbar" );
			
			GiveNamedItem( "weapon_minigun" );
			GiveAmmo( 176, TFC_AMMO_SHELLS );
		}
		break;

		case PC_PYRO:
		{
			GiveNamedItem( "weapon_crowbar" );
		}
		break;

		case PC_ENGINEER:
		{
			GiveNamedItem( "weapon_spanner" );
			GiveNamedItem( "weapon_super_shotgun" );
			GiveAmmo( 20, TFC_AMMO_SHELLS );
		}
		break;

		case PC_SCOUT:
		{
			GiveNamedItem( "weapon_crowbar" );
			GiveNamedItem( "weapon_shotgun" );
			GiveNamedItem( "weapon_nailgun" );
			GiveAmmo( 25, TFC_AMMO_SHELLS );
			GiveAmmo( 100, TFC_AMMO_NAILS );
		}
		break;

		case PC_SNIPER:
		{
			GiveNamedItem( "weapon_crowbar" );
		}
		break;

		case PC_SOLDIER:
		{
			GiveNamedItem( "weapon_crowbar" );
		}
		break;

		case PC_DEMOMAN:
		{
			GiveNamedItem( "weapon_crowbar" );
		}
		break;

		case PC_SPY:
		{
			GiveNamedItem( "weapon_knife" );
		}
		break;

		case PC_MEDIC:
		{
			GiveNamedItem( "weapon_medikit" );
			GiveNamedItem( "weapon_super_nailgun" );
			GiveAmmo( 100, TFC_AMMO_NAILS );
		}
		break;
	}
}


void CTFCPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}


void CTFCPlayer::State_Transition( TFCPlayerState newState )
{
	State_Leave();
	State_Enter( newState );
}


void CTFCPlayer::State_Enter( TFCPlayerState newState )
{
	m_Shared.m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo( newState );

	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState)();
}


void CTFCPlayer::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


CPlayerStateInfo* CTFCPlayer::State_LookupInfo( TFCPlayerState state )
{
	// This table MUST match the 
	static CPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CTFCPlayer::State_Enter_ACTIVE,		NULL,	NULL },
		{ STATE_WELCOME,		"STATE_WELCOME",		&CTFCPlayer::State_Enter_WELCOME,		NULL,	NULL },
		{ STATE_PICKINGTEAM,	"STATE_PICKINGTEAM",	&CTFCPlayer::State_Enter_PICKINGTEAM,	NULL,	NULL },
		{ STATE_PICKINGCLASS,	"STATE_PICKINGCLASS",	&CTFCPlayer::State_Enter_PICKINGCLASS,	NULL,	NULL },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CTFCPlayer::State_Enter_OBSERVER_MODE,	NULL,	NULL },
		{ STATE_DYING,			"STATE_DYING",			&CTFCPlayer::State_Enter_DYING,			NULL,	NULL }
	};

	for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
	{
		if ( playerStateInfos[i].m_iPlayerState == state )
			return &playerStateInfos[i];
	}

	return NULL;
}


void CTFCPlayer::State_Enter_WELCOME()
{
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
	PhysObjectSleep();
	
	// Show info panel (if it's not a simple demo map).
	KeyValues *data = new KeyValues("data");
	data->SetString( "title", "Message of the Day" ); // info panel title
	data->SetString( "type",  "3" );				  // show a file
	data->SetString( "msg",   "motd.txt" );			  // this file
	data->SetString( "cmd",   "joingame" );			  // exec this command if panel closed

	ShowViewPortPanel( "info", true, data );

	data->deleteThis();
}


void CTFCPlayer::State_Enter_PICKINGTEAM()
{
	ShowViewPortPanel( PANEL_TEAM ); // show the team menu
}


void CTFCPlayer::State_Enter_PICKINGCLASS()
{
	// go to spec mode, if dying keep deathcam
	if ( GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		StartObserverMode( OBS_MODE_DEATHCAM );
	}
	else
	{
		StartObserverMode( OBS_MODE_ROAMING );
	}
	
	PhysObjectSleep();

	// show the class menu:
	ShowViewPortPanel( PANEL_CLASS );
}


void CTFCPlayer::State_Enter_OBSERVER_MODE()
{
	StartObserverMode( m_iObserverLastMode );
	PhysObjectSleep();
}


void CTFCPlayer::State_Enter_ACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveEffects( EF_NODRAW );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
    m_Local.m_iHideHUD = 0;
	PhysObjectWake();
}


void CTFCPlayer::State_Enter_DYING()
{
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
}


void CTFCPlayer::PhysObjectSleep()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Sleep();
}


void CTFCPlayer::PhysObjectWake()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Wake();
}


void CTFCPlayer::HandleCommand_JoinTeam( const char *pTeamName )
{
	int iTeam = TEAM_RED;
	if ( stricmp( pTeamName, "auto" ) == 0 )
	{
		iTeam = RandomInt( 0, 1 ) ? TEAM_RED : TEAM_BLUE;
	}
	else if ( stricmp( pTeamName, "spectate" ) == 0 )
	{
		iTeam = TEAM_SPECTATOR;
	}
	else
	{
		for ( int i=0; i < TEAM_MAXCOUNT; i++ )
		{
			if ( stricmp( pTeamName, teamnames[i] ) == 0 )
			{
				iTeam = i;
				break;
			}
		}
	}

	if ( iTeam == TEAM_SPECTATOR )
	{
		// Prevent this is the cvar is set
		if ( !mp_allowspectators.GetInt() && !IsHLTV() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			return;
		}
		
		if ( GetTeamNumber() != TEAM_UNASSIGNED && !IsDead() )
		{
			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			IncrementFragCount( 1 );
		}

		ChangeTeam( TEAM_SPECTATOR );

		// do we have fadetoblack on? (need to fade their screen back in)
		if ( mp_fadetoblack.GetInt() )
		{
			color32_s clr = { 0,0,0,0 };
			UTIL_ScreenFade( this, clr, 0.001, 0, FFADE_IN );
		}
	}
	else
	{
		ChangeTeam( iTeam );
		State_Transition( STATE_PICKINGCLASS );
	}
}


void CTFCPlayer::ChangeTeam( int iTeamNum )
{
	if ( !GetGlobalTeam( iTeamNum ) )
	{
		Warning( "CCSPlayer::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;

	BaseClass::ChangeTeam( iTeamNum );

	if ( iTeamNum == TEAM_UNASSIGNED )
	{
		State_Transition( STATE_OBSERVER_MODE );
	}
	else if ( iTeamNum == TEAM_SPECTATOR )
	{
		State_Transition( STATE_OBSERVER_MODE );
	}
	else // active player
	{
		if ( iOldTeam == TEAM_SPECTATOR )
		{
			// If they're switching from being a spectator to ingame player
			GetIntoGame();
		}

		if ( !IsDead() && iOldTeam != TEAM_UNASSIGNED )
		{
			// Kill player if switching teams while alive
			CommitSuicide();
		}

		// Put up the class selection menu.
		State_Transition( STATE_PICKINGCLASS );
	}
}


void CTFCPlayer::HandleCommand_JoinClass( const char *pClassName )
{
	int iClass = RandomInt( 0, PC_LAST_NORMAL_CLASS );
	if ( stricmp( pClassName, "random" ) != 0 )
	{
		for ( int i=0; i < PC_LASTCLASS; i++ )
		{
			if ( stricmp( pClassName, GetTFCClassInfo( i )->m_pClassName ) == 0 )
			{
				iClass = i;
				break;
			}
		}
		if ( i == PC_LAST_NORMAL_CLASS )
		{
			Warning( "HandleCommand_JoinClass( %s ) - invalid class name.\n", pClassName );
		}
	}

	m_Shared.SetPlayerClass( iClass );
	
	if ( !IsAlive() )
		GetIntoGame();
}


void CTFCPlayer::GetIntoGame()
{
	State_Transition( STATE_ACTIVE );
	Spawn();
}


bool CTFCPlayer::ClientCommand( const CCommand& args )
{
	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "joingame" ) )
	{
		// player just closed MOTD dialog
		if ( m_Shared.m_iPlayerState == STATE_WELCOME )
		{
			State_Transition( STATE_PICKINGTEAM );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "jointeam" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			HandleCommand_JoinTeam( args[1] );
			return true;
		}
	}
	else if ( FStrEq( pcmd, "joinclass" ) ) 
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad joinclass syntax\n" );
		}

		HandleCommand_JoinClass( args[1] );
		return true;
	}

	return BaseClass::ClientCommand( args );
}


bool CTFCPlayer::IsAlly( CBaseEntity *pEnt ) const
{
	return pEnt->GetTeamNumber() == GetTeamNumber();
}


void CTFCPlayer::TF_AddFrags( int nFrags )
{
	// TFCTODO: implement frags
}


void CTFCPlayer::ResetMenu()
{
	current_menu = 0;
}


int CTFCPlayer::GetNumFlames() const
{
	// TFCTODO: implement flames
	return 0;
}


void CTFCPlayer::SetNumFlames( int nFlames )
{
	// TFCTODO: implement frags
	Assert( 0 );
}

int CTFCPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	int bResult = false;

	// If the bit's set, ignore the monster's max health and add over it
	if ( bitsDamageType & DMG_IGNORE_MAXHEALTH )
	{
		int iDamage = g_pGameRules->Damage_GetTimeBased();
		m_bitsDamageType &= ~(bitsDamageType & ~iDamage);
		m_iHealth += flHealth;
		bResult = true;
	}
	else
	{
		bResult = BaseClass::TakeHealth( flHealth, bitsDamageType );
	}

	// Leg Healing
	if (m_iLegDamage > 0)
	{
		// Allow even at full health
		if ( GetHealth() >= (GetMaxHealth() - 5))
			m_iLegDamage = 0;
		else
			m_iLegDamage -= (GetHealth() + flHealth) / 20;
		if (m_iLegDamage < 1)
			m_iLegDamage = 0;

		TeamFortress_SetSpeed();
		bResult = true;
	}

	return bResult;
}


void CTFCPlayer::TeamFortress_SetSpeed()
{
	int playerclass = m_Shared.GetPlayerClass();
	float maxfbspeed;

	// Spectators can move while in Classic Observer mode
	if ( IsObserver() )
	{
		if ( GetObserverMode() == OBS_MODE_ROAMING )
			SetMaxSpeed( GetTFCClassInfo( PC_SCOUT )->m_flMaxSpeed );
		else
			SetMaxSpeed( 0 );

		return;
	}

	// Check for any reason why they can't move at all
	if ( (m_Shared.GetStateFlags() & TFSTATE_CANT_MOVE) || (playerclass == PC_UNDEFINED) )
	{
		SetAbsVelocity( vec3_origin );
		SetMaxSpeed( 1 );
		return;
	}

	// First, get their max class speed
	maxfbspeed = GetTFCClassInfo( playerclass )->m_flMaxSpeed;

	// 2nd, see if any GoalItems are slowing them down
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "item_tfgoal" );
	while ( pEnt )
	{
		CTFGoal *pGoal = dynamic_cast<CTFGoal*>( pEnt );
		if ( pGoal )
		{
			if ( pGoal->GetOwnerEntity() == this )
			{
				if (pGoal->goal_activation & TFGI_SLOW)
				{
					maxfbspeed = maxfbspeed / 2;
				}
				else if (pGoal->speed_reduction)
				{
					float flPercent = ((float)pGoal->speed_reduction) / 100.0;
					maxfbspeed = flPercent * maxfbspeed;
				}
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "item_tfgoal" );
	}

	// 3rd, See if they're tranquilised
	if (m_Shared.GetStateFlags() & TFSTATE_TRANQUILISED)
	{
		maxfbspeed = maxfbspeed / 2;
	}

	// 4th, check for leg wounds
	if (m_iLegDamage)
	{
		if (m_iLegDamage > 6)
			m_iLegDamage = 6;

		// reduce speed by 10% per leg wound
		maxfbspeed = (maxfbspeed * ((10 - m_iLegDamage) / 10));
	}

	// 5th, if they're a sniper, and they're aiming, their speed must be 80 or less
	if (m_Shared.GetStateFlags() & TFSTATE_AIMING)
	{
		if (maxfbspeed > 80)
			maxfbspeed = 80;
	}

	// Set the speed
	SetMaxSpeed( maxfbspeed );
}


void CTFCPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	DoAnimationEvent( PLAYERANIMEVENT_DIE );
	State_Transition( STATE_DYING );	// Transition into the dying state.

	// Remove all items..
	RemoveAllItems( true );

	BaseClass::Event_Killed( info );

	// Don't overflow the value for this.
	m_iHealth = 0;
}


void CTFCPlayer::ClientHearVox( const char *pSentence )
{
	//TFCTODO: implement this.
}


//=========================================================================
// Check all stats to make sure they're good for this class
void CTFCPlayer::TeamFortress_CheckClassStats()
{
	// Check armor
	if (armortype > armor_allowed)
		armortype = armor_allowed;
	
	if (ArmorValue() > GetClassInfo()->m_iMaxArmor)
		SetArmorValue( GetClassInfo()->m_iMaxArmor );

	if (ArmorValue() < 0)
		SetArmorValue( 0 );
	
	if (armortype < 0)
		armortype = 0;
	
	// Check ammo
	for ( int iAmmoType=0; iAmmoType < TFC_NUM_AMMO_TYPES; iAmmoType++ )
	{
		if ( GetAmmoCount( iAmmoType ) > GetClassInfo()->m_MaxAmmo[iAmmoType] )
			RemoveAmmo( GetAmmoCount( iAmmoType ) - GetClassInfo()->m_MaxAmmo[iAmmoType], iAmmoType );
	}

	// Check Grenades
	Assert( GetAmmoCount( TFC_AMMO_GRENADES1 ) >= 0 );
	Assert( GetAmmoCount( TFC_AMMO_GRENADES2 ) >= 0 );
	
	// Limit Nails
	if ( no_grenades_1() > g_nMaxGrenades[tp_grenades_1()] )
		RemoveAmmo( TFC_AMMO_GRENADES1, no_grenades_1() - g_nMaxGrenades[tp_grenades_1()] );

	if ( no_grenades_2() > g_nMaxGrenades[tp_grenades_2()] )
		RemoveAmmo( TFC_AMMO_GRENADES2, no_grenades_2() - g_nMaxGrenades[tp_grenades_2()] );

	// Check health
	if (GetHealth() > GetMaxHealth() && !(m_Shared.GetItemFlags() & IT_SUPERHEALTH))
		SetHealth( GetMaxHealth() );
	
	if (GetHealth() < 0)
		SetHealth( 0 );

	// Update armor picture
	m_Shared.RemoveItemFlags( IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3 );
	if (armortype >= 0.8)
		m_Shared.AddItemFlags( IT_ARMOR3 );
	else if (armortype >= 0.6)
		m_Shared.AddItemFlags( IT_ARMOR2 );
	else if (armortype >= 0.3)
		m_Shared.AddItemFlags( IT_ARMOR1 );
}


//======================================================================
// DISGUISE HANDLING
//======================================================================
// Reset spy skin and color or remove invisibility
void CTFCPlayer::Spy_RemoveDisguise()
{
	if (m_Shared.GetPlayerClass() == PC_SPY)
	{
		if ( undercover_team || undercover_skin )
			ClientPrint( this, HUD_PRINTCENTER, "#Disguise_Lost" );

		// Set their color 
		undercover_team = 0;
		undercover_skin = 0;

		immune_to_check = gpGlobals->curtime + 10;
		is_undercover = 0;

		// undisguise weapon
		TeamFortress_SetSkin();
		TeamFortress_SpyCalcName();

		Spy_ResetExternalWeaponModel();

		// get them out of any disguise menus
		if ( current_menu == MENU_SPY || current_menu == MENU_SPY_SKIN || current_menu == MENU_SPY_COLOR )
		{
			ResetMenu();
		}

		// Remove the Disguise timer
		CTimer *pTimer = Timer_FindTimer( this, TF_TIMER_DISGUISE );
		if (pTimer)
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Disguise_stop" );
			Timer_Remove( pTimer );
		}
	}
}


// when the spy loses disguise reset his weapon
void CTFCPlayer::Spy_ResetExternalWeaponModel( void )
{
	// we don't show any weapon models if we're feigning
	if ( is_feigning )
		return;

#ifdef TFCTODO // spy
	pev->weaponmodel = MAKE_STRING( m_pszSavedWeaponModel );
	strcpy( m_szAnimExtention, m_szSavedAnimExtention );
	m_iCurrentAnimationState = 0; // force the current animation sequence to be recalculated
#endif
}


//=========================================================================
// Try and find the player's name who's skin and team closest fit the 
// current disguise of the spy
void CTFCPlayer::TeamFortress_SpyCalcName()
{
	CBaseEntity *last_target = undercover_target;// don't redisguise self as this person

	undercover_target = NULL;
	
	// Find a player on the team the spy is disguised as to pretend to be
	if (undercover_team != 0)
	{
		CTFCPlayer *pPlayer = NULL;

		// Loop through players
		int i;
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToTFCPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
			{
				if ( pPlayer == last_target )
				{
					// choose someone else, we're trying to rid ourselves of a disguise as this one
					continue;
				}

				// First, try to find a player with same color and skins
				if (pPlayer->GetTeamNumber() == undercover_team && pPlayer->m_Shared.GetPlayerClass() == undercover_skin)
				{
					undercover_target = pPlayer;
					return;
				}
			}
		}

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToTFCPlayer( UTIL_PlayerByIndex( i ) );
			
			if ( pPlayer )
			{
				if (pPlayer->GetTeamNumber() == undercover_team)
				{
					undercover_target = pPlayer;
					return;
				}
			}
		}
	}
}


//=========================================================================
// Set the skin of a player based on his/her class
void CTFCPlayer::TeamFortress_SetSkin()
{
	immune_to_check = gpGlobals->curtime + 10;

	// Find out whether we should show our actual class or a disguised class
	int iClassToUse = m_Shared.GetPlayerClass();
	if (iClassToUse == PC_SPY && undercover_skin != 0)
		iClassToUse = undercover_skin;
	
	int iTeamToUse = GetTeamNumber();
	if (m_Shared.GetPlayerClass() == PC_SPY && undercover_team != 0)
		iTeamToUse = undercover_team;

// TFCTODO: handle replacement_model here.

	SetModel( GetTFCClassInfo( iClassToUse )->m_pModelName );

	// Skins in the models should be setup using the team IDs in tfc_shareddefs.h, subtracting 1
	// so they're 0-based.
	m_nSkin = iTeamToUse - 1;

	if ( FBitSet(GetFlags(), FL_DUCKING) ) 
		UTIL_SetSize(this, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(this, VEC_HULL_MIN, VEC_HULL_MAX);
}



//=========================================================================
// Displays the state of the items specified by the Goal passed in
void CTFCPlayer::DisplayLocalItemStatus( CTFGoal *pGoal )
{
	for (int i = 0; i < 4; i++)
	{
		if (pGoal->display_item_status[i] != 0)
		{
			CTFGoalItem *pItem = Finditem(pGoal->display_item_status[i]);
			if (pItem)
				DisplayItemStatus(pGoal, this, pItem);
			else
				ClientPrint( this, HUD_PRINTTALK, "#Item_missing" );
		}
	}
}


//=========================================================================
// Removes all the Engineer's buildings
void CTFCPlayer::Engineer_RemoveBuildings()
{
	// If the player's building already, stop
	if (is_building == 1)
	{
		m_Shared.RemoveStateFlags( TFSTATE_CANT_MOVE );
		TeamFortress_SetSpeed();

		// Remove the timer
		CTimer *pTimer = Timer_FindTimer( this, TF_TIMER_BUILD );
		if (pTimer)
			Timer_Remove(pTimer);
		
		// Remove the building
		UTIL_Remove( building );
		building = NULL;
		is_building = 0;

		// Stop Build Sound
		StopSound( "Engineer.Building" );
		//STOP_SOUND( ENT(pev), CHAN_STATIC, "weapons/building.wav" );

		if ( GetActiveWeapon() )
			GetActiveWeapon()->Deploy();
	}

	DestroyBuilding(this, "building_dispenser");
	DestroyBuilding(this, "building_sentrygun");
	DestroyTeleporter(this, BUILD_TELEPORTER_ENTRY);
	DestroyTeleporter(this, BUILD_TELEPORTER_EXIT);
}


//=========================================================================
// Removes all grenades that persist for a period of time from the world
void CTFCPlayer::TeamFortress_RemoveLiveGrenades( void )
{
	RemoveOwnedEnt( "tf_weapon_napalmgrenade" );
	RemoveOwnedEnt( "tf_weapon_nailgrenade" );
	RemoveOwnedEnt( "tf_weapon_gasgrenade" );
	RemoveOwnedEnt( "tf_weapon_caltrop" );
}


//=========================================================================
// Removes all rockets the player has fired into the world
// (this prevents a team kill cheat where players would fire rockets 
// then change teams to kill their own team)
void CTFCPlayer::TeamFortress_RemoveRockets( void )
{
	RemoveOwnedEnt( "tf_rpg_rocket" );
	RemoveOwnedEnt( "tf_ic_rocket" );
}

// removes the player's pipebombs with no explosions
void CTFCPlayer::RemovePipebombs( void )
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "tf_gl_pipebomb" );
	while ( pEnt )
	{
		CTFCPlayer *pOwner = ToTFCPlayer( pEnt->GetOwnerEntity() );
		if ( pOwner == this )
		{
			pOwner->m_iPipebombCount--;
			pEnt->AddFlag( FL_KILLME );
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "tf_gl_pipebomb" );
	}
}


//=========================================================================
// Stops the setting of the detpack
void CTFCPlayer::TeamFortress_DetpackStop( void )
{
	CTimer *pTimer = Timer_FindTimer( this, TF_TIMER_DETPACKSET );

	if (!pTimer)
	    return;

    ClientPrint( this, HUD_PRINTNOTIFY, "#Detpack_retrieve" );

	// Return the detpack
	GiveAmmo( 1, TFC_AMMO_DETPACK );
	Timer_Remove(pTimer);

	// Release player
	m_Shared.RemoveStateFlags( TFSTATE_CANT_MOVE );
	is_detpacking = 0;
	TeamFortress_SetSpeed();

	// Return their weapon
	if ( GetActiveWeapon() )
		GetActiveWeapon()->Deploy();
}


//=========================================================================
// Removes any detpacks the player may have set
BOOL CTFCPlayer::TeamFortress_RemoveDetpacks( void )
{
	// Remove all detpacks owned by the player
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "detpack" );
	while ( pEnt )
	{
		// if the player owns this detpack, remove it
		if ( pEnt->GetOwnerEntity() == this )
		{
			UTIL_Remove( pEnt );
			return TRUE;
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "detpack" );
	}

	return FALSE;
}


//=========================================================================
// Remove all of an ent owned by this player
void CTFCPlayer::RemoveOwnedEnt( char *pEntName )
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, pEntName );
	while ( pEnt )
	{
		// if the player owns this entity, remove it
		if ( pEnt->GetOwnerEntity() == this )
			pEnt->AddFlag( FL_KILLME );
		
		pEnt = gEntList.FindEntityByClassname( pEnt, pEntName );
	}
}


