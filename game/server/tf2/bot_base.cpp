//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic BOT handling.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "tf_player.h"
#include "menu_base.h"
#include "in_buttons.h"
#include "movehelper_server.h"
#include "weapon_twohandedcontainer.h"

void ParseCommand( CBaseTFPlayer *pPlayer, const char *pcmd, const char *pargs );
void ClientPutInServer( edict_t *pEdict, const char *playername );
void Bot_Think( CBaseTFPlayer *pBot );

ConVar bot_forcefireweapon( "bot_forcefireweapon", "", 0, "Force bots with the specified weapon to fire." );
ConVar bot_forceattack2( "bot_forceattack2", "0", 0, "When firing, use attack2." );
ConVar bot_forceattackon( "bot_forceattackon", "0", 0, "When firing, don't tap fire, hold it down." );
ConVar bot_flipout( "bot_flipout", "0", 0, "When on, all bots fire their guns." );
ConVar bot_defend( "bot_defend", "0", 0, "Set to a team number, and that team will all keep their combat shields raised." );
ConVar bot_changeclass( "bot_changeclass", "0", 0, "Force all bots to change to the specified class." );
static ConVar bot_mimic( "bot_mimic", "0", 0, "Bot uses usercmd of player by index." );

static int BotNumber = 1;
static int g_iNextBotTeam = -1;
static int g_iNextBotClass = -1;

//-----------------------------------------------------------------------------
// Purpose: Create a new Bot and put it in the game.
// Output : Pointer to the new Bot, or NULL if there's no free clients.
//-----------------------------------------------------------------------------
CBasePlayer *BotPutInServer( bool bFrozen, int iTeam, int iClass )
{
	g_iNextBotTeam = iTeam;
	g_iNextBotClass = iClass;

	char botname[ 64 ];
	Q_snprintf( botname, sizeof( botname ), "Bot%02i", BotNumber );

	edict_t *pEdict = NULL;
	{
		bool oldLock = engine->LockNetworkStringTables( false );
		pEdict = engine->CreateFakeClient( botname );
		engine->LockNetworkStringTables( oldLock );
	}

	if ( !pEdict )
	{
		Msg( "Failed to create Bot.\n");
		return NULL;
	}

	// Allocate a CBasePlayer for the bot, and call spawn
	ClientPutInServer( pEdict, botname );
	CBaseTFPlayer *pPlayer = ((CBaseTFPlayer *)CBaseEntity::Instance( pEdict ));
	pPlayer->ClearFlags();
	pPlayer->AddFlag( FL_CLIENT | FL_FAKECLIENT );

	if ( bFrozen )
		pPlayer->AddEFlags( EFL_BOT_FROZEN );

  	if ( iTeam != -1 )
	{
		pPlayer->ChangeTeam( iTeam );
	}
	pPlayer->ForceRespawn();

	BotNumber++;

	return pPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: Run through all the Bots in the game and let them think.
//-----------------------------------------------------------------------------
void Bot_RunAll( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer && (pPlayer->GetFlags() & FL_FAKECLIENT) )
		{
			Bot_Think( pPlayer );
		}
	}
}

typedef struct
{
	bool			backwards;

	float			nextturntime;
	bool			lastturntoright;

	float			nextstrafetime;
	float			sidemove;

	QAngle			forwardAngle;
	QAngle			lastAngles;
} botdata_t;

static botdata_t g_BotData[ MAX_PLAYERS ];

bool RunMimicCommand( CUserCmd& cmd )
{
	if ( bot_mimic.GetInt() <= 0 )
		return false;

	if ( bot_mimic.GetInt() > gpGlobals->maxClients )
		return false;

	
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( bot_mimic.GetInt()  );
	if ( !pPlayer )
		return false;

	if ( !pPlayer->GetLastUserCommand() )
		return false;

	cmd = *pPlayer->GetLastUserCommand();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Simulates a single frame of movement for a player
// Input  : *fakeclient - 
//			*viewangles - 
//			forwardmove - 
//			sidemove - 
//			upmove - 
//			buttons - 
//			impulse - 
//			msec - 
// Output : 	virtual void
//-----------------------------------------------------------------------------
static void RunPlayerMove( CBaseTFPlayer *fakeclient, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime )
{
	if ( !fakeclient )
		return;

	CUserCmd cmd;

	// Store off the globals.. they're gonna get whacked
	float flOldFrametime = gpGlobals->frametime;
	float flOldCurtime = gpGlobals->curtime;

	float flTimeBase = gpGlobals->curtime + gpGlobals->frametime - frametime;
	fakeclient->SetTimeBase( flTimeBase );

	Q_memset( &cmd, 0, sizeof( cmd ) );

	if ( !RunMimicCommand( cmd ) )
	{
		VectorCopy( viewangles, cmd.viewangles );
		cmd.forwardmove = forwardmove;
		cmd.sidemove = sidemove;
		cmd.upmove = upmove;
		cmd.buttons = buttons;
		cmd.impulse = impulse;
		cmd.random_seed = random->RandomInt( 0, 0x7fffffff );
	}

	MoveHelperServer()->SetHost( fakeclient );
	fakeclient->PlayerRunCommand( &cmd, MoveHelperServer() );

	// save off the last good usercmd
	fakeclient->SetLastUserCommand( cmd );

	// Clear out any fixangle that has been set
	fakeclient->pl.fixangle = FIXANGLE_NONE;

	// Restore the globals..
	gpGlobals->frametime = flOldFrametime;
	gpGlobals->curtime = flOldCurtime;
}

//-----------------------------------------------------------------------------
// Purpose: Run this Bot's AI for one frame.
//-----------------------------------------------------------------------------
void Bot_Think( CBaseTFPlayer *pBot )
{
	// Hack to make Bots use Menus
	if ( pBot->m_pCurrentMenu == gMenus[MENU_CLASS] )
	{
		int iClass = g_iNextBotClass;
		if ( iClass == -1 )
			iClass = random->RandomInt( 1, TFCLASS_CLASS_COUNT );

		pBot->m_pCurrentMenu->Input( pBot, iClass );
	}
	else if ( bot_changeclass.GetInt() && bot_changeclass.GetInt() != pBot->PlayerClass() )
	{
		pBot->m_pCurrentMenu = gMenus[MENU_CLASS];
		pBot->m_pCurrentMenu->Input( pBot, bot_changeclass.GetInt() );
	}

	// Make sure we stay being a bot
	pBot->AddFlag( FL_FAKECLIENT );

	botdata_t *botdata = &g_BotData[ ENTINDEX( pBot->edict() ) - 1 ];

	QAngle vecViewAngles;
	float forwardmove = 0.0;
	float sidemove = botdata->sidemove;
	float upmove = 0.0;
	unsigned short buttons = 0;
	byte  impulse = 0;
	float frametime = gpGlobals->frametime;

	vecViewAngles = pBot->GetLocalAngles();


	// Create some random values
	if ( pBot->IsAlive() && (pBot->GetSolid() == SOLID_BBOX) )
	{
		trace_t trace;

		// Stop when shot
		if ( !pBot->IsEFlagSet(EFL_BOT_FROZEN) )
		{
			if ( pBot->m_iHealth == 100 )
			{
				forwardmove = 600 * ( botdata->backwards ? -1 : 1 );
				if ( botdata->sidemove != 0.0f )
				{
					forwardmove *= random->RandomFloat( 0.1, 1.0f );
				}
			}
			else
			{
				forwardmove = 0;
			}
		}

		// Only turn if I haven't been hurt
		if ( !pBot->IsEFlagSet(EFL_BOT_FROZEN) && pBot->m_iHealth == 100 )
		{
			Vector vecEnd;
			Vector forward;

			QAngle angle;
			float angledelta = 15.0;

			int maxtries = (int)360.0/angledelta;

			if ( botdata->lastturntoright )
			{
				angledelta = -angledelta;
			}

			angle = pBot->GetLocalAngles();

			Vector vecSrc;
			while ( --maxtries >= 0 )
			{
				AngleVectors( angle, &forward );

				vecSrc = pBot->GetLocalOrigin() + Vector( 0, 0, 36 );

				vecEnd = vecSrc + forward * 10;

				UTIL_TraceHull( vecSrc, vecEnd, VEC_HULL_MIN_SCALED( pBot ), VEC_HULL_MAX_SCALED( pBot ), 
					MASK_PLAYERSOLID, pBot, COLLISION_GROUP_NONE, &trace );

				if ( trace.fraction == 1.0 )
				{
					if ( gpGlobals->curtime < botdata->nextturntime )
					{
						break;
					}
				}

				angle.y += angledelta;

				if ( angle.y > 180 )
					angle.y -= 360;
				else if ( angle.y < -180 )
					angle.y += 360;

				botdata->nextturntime = gpGlobals->curtime + 2.0;
				botdata->lastturntoright = random->RandomInt( 0, 1 ) == 0 ? true : false;

				botdata->forwardAngle = angle;
				botdata->lastAngles = angle;

			}


			if ( gpGlobals->curtime >= botdata->nextstrafetime )
			{
				botdata->nextstrafetime = gpGlobals->curtime + 1.0f;

				if ( random->RandomInt( 0, 5 ) == 0 )
				{
					botdata->sidemove = -600.0f + 1200.0f * random->RandomFloat( 0, 2 );
				}
				else
				{
					botdata->sidemove = 0;
				}
				sidemove = botdata->sidemove;

				if ( random->RandomInt( 0, 20 ) == 0 )
				{
					botdata->backwards = true;
				}
				else
				{
					botdata->backwards = false;
				}
			}

			pBot->SetLocalAngles( angle );
			vecViewAngles = angle;
		}

		// Is my team being forced to defend?
		if ( bot_defend.GetInt() == pBot->GetTeamNumber() )
		{
			buttons |= IN_ATTACK2;
		}
		// If bots are being forced to fire a weapon, see if I have it
		else if ( bot_forcefireweapon.GetString() )
		{
			CBaseCombatWeapon *pWeapon = pBot->Weapon_OwnsThisType( bot_forcefireweapon.GetString() );
			if ( pWeapon )
			{
				// Switch to it if we don't have it out
				CBaseCombatWeapon *pActiveWeapon = pBot->GetActiveWeapon();
				// Is it a twohandedweapon? If so, get the left weapon
				CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pActiveWeapon );
				if ( pContainer )
				{
					pActiveWeapon = pContainer->GetLeftWeapon();
				}

				// Switch?
				if ( pActiveWeapon != pWeapon )
				{
					pBot->Weapon_Switch( pWeapon );
				}
				else
				{
					// Start firing
					// Some weapons require releases, so randomise firing
					if ( bot_forceattackon.GetBool() || (RandomFloat(0.0,1.0) > 0.5) )
					{
						buttons |= bot_forceattack2.GetBool() ? IN_ATTACK2 : IN_ATTACK;
					}
				}
			}
		}

		if ( bot_flipout.GetInt() )
		{
			if ( bot_forceattackon.GetBool() || (RandomFloat(0.0,1.0) > 0.5) )
			{
				buttons |= bot_forceattack2.GetBool() ? IN_ATTACK2 : IN_ATTACK;
			}
		}
	}
	else
	{
		// Wait for Reinforcement wave
		if ( !pBot->IsAlive() )
		{
			// Try hitting my buttons occasionally
			if ( random->RandomInt( 0, 100 ) > 80 )
			{
				// Respawn the bot
				if ( random->RandomInt( 0, 1 ) == 0 )
				{
					buttons |= IN_JUMP;
				}
				else
				{
					buttons = 0;
				}
			}
		}
	}

	if ( bot_flipout.GetInt() >= 2 )
	{

		QAngle angOffset = RandomAngle( -1, 1 );

		botdata->lastAngles += angOffset;

		for ( int i = 0 ; i < 2; i++ )
		{
			if ( fabs( botdata->lastAngles[ i ] - botdata->forwardAngle[ i ] ) > 15.0f )
			{
				if ( botdata->lastAngles[ i ] > botdata->forwardAngle[ i ] )
				{
					botdata->lastAngles[ i ] = botdata->forwardAngle[ i ] + 15;
				}
				else
				{
					botdata->lastAngles[ i ] = botdata->forwardAngle[ i ] - 15;
				}
			}
		}

		botdata->lastAngles[ 2 ] = 0;

		pBot->SetLocalAngles( botdata->lastAngles );
	}

	RunPlayerMove( pBot, pBot->GetLocalAngles(), forwardmove, sidemove, upmove, buttons, impulse, frametime );
}


