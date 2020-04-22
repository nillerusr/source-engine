//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_c4.h"
#include "in_buttons.h"
#include "cs_gamerules.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "KeyValues.h"
#include "fx_cs_shared.h"
#include "obstacle_pushaway.h"

#if defined( CLIENT_DLL )
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "explode.h"
	#include "mapinfo.h"
	#include "team.h"
	#include "func_bomb_target.h"
	#include "vguiscreen.h"
	#include "bot.h"
	#include "cs_player.h"
	#include <KeyValues.h>

//=============================================================================
// HPE_BEGIN
// [dwenger] Necessary for stats tracking
//=============================================================================
#include "cs_gamestats.h"
#include "cs_achievement_constants.h"
//=============================================================================
// HPE_END
//=============================================================================
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define BLINK_INTERVAL 2.0
#define PLANTED_C4_MODEL "models/weapons/w_c4_planted.mdl"
#define HEIST_MODE_C4_TIME 25

int g_sModelIndexC4Glow = -1;

#define WEAPON_C4_ARM_TIME	3.0


#ifdef CLIENT_DLL

#else


	LINK_ENTITY_TO_CLASS( planted_c4, CPlantedC4 );
	PRECACHE_REGISTER( planted_c4 );

	BEGIN_DATADESC( CPlantedC4 )
		DEFINE_FUNCTION( C4Think )
	END_DATADESC()
	

	IMPLEMENT_SERVERCLASS_ST( CPlantedC4, DT_PlantedC4 )
		SendPropBool( SENDINFO(m_bBombTicking) ),
		SendPropFloat( SENDINFO(m_flC4Blow), 0, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO(m_flTimerLength), 0, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO(m_flDefuseLength), 0, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO(m_flDefuseCountDown), 0, SPROP_NOSCALE ),
	END_SEND_TABLE()

	
BEGIN_PREDICTION_DATA( CPlantedC4 )
END_PREDICTION_DATA()



	CUtlVector< CPlantedC4* > g_PlantedC4s;


	CPlantedC4::CPlantedC4()
	{
		g_PlantedC4s.AddToTail( this );
        //=============================================================================
        // HPE_BEGIN:        
        //=============================================================================
         
        // [tj] No planter initially
        m_pPlanter = NULL;

        // [tj] Assume this is the original owner
        m_bPlantedAfterPickup = false;
         
        //=============================================================================
        // HPE_END
        //=============================================================================
        
	}

	CPlantedC4::~CPlantedC4()
	{
		g_PlantedC4s.FindAndRemove( this );

		int i;
		// Kill the control panels
		for ( i = m_hScreens.Count(); --i >= 0; )
		{
			DestroyVGuiScreen( m_hScreens[i].Get() );
		}
		m_hScreens.RemoveAll();
	}

	int CPlantedC4::UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CPlantedC4::ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		// Terrorists always need this object for the radar
		// Everybody needs it for hiding the round timer and showing the planted C4 scenario icon
		return FL_EDICT_ALWAYS;
	}

	void CPlantedC4::Precache()
	{
		g_sModelIndexC4Glow = PrecacheModel( "sprites/ledglow.vmt" );
		PrecacheModel( PLANTED_C4_MODEL );
		PrecacheVGuiScreen( "c4_panel" );

		engine->ForceModelBounds( PLANTED_C4_MODEL, Vector( -7, -13, -3 ), Vector( 9, 12, 11 ) );

		PrecacheParticleSystem( "bomb_explosion_huge" );
	}

	void CPlantedC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_panel";
	}

	void CPlantedC4::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "vgui_screen";
	}

	//-----------------------------------------------------------------------------
	// This is called by the base object when it's time to spawn the control panels
	//-----------------------------------------------------------------------------
	void CPlantedC4::SpawnControlPanels()
	{
		char buf[64];

		// FIXME: Deal with dynamically resizing control panels?

		// If we're attached to an entity, spawn control panels on it instead of use
		CBaseAnimating *pEntityToSpawnOn = this;
		const char *pOrgLL = "controlpanel%d_ll";
		const char *pOrgUR = "controlpanel%d_ur";
		const char *pAttachmentNameLL = pOrgLL;
		const char *pAttachmentNameUR = pOrgUR;

		Assert( pEntityToSpawnOn );

		// Lookup the attachment point...
		int nPanel;
		for ( nPanel = 0; true; ++nPanel )
		{
			Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
			int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
			{
				// Try and use my panels then
				pEntityToSpawnOn = this;
				Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
				nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
				if (nLLAttachmentIndex <= 0)
					return;
			}

			Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
			int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
			{
				// Try and use my panels then
				Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
				nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
				if (nURAttachmentIndex <= 0)
					return;
			}

			const char *pScreenName;
			GetControlPanelInfo( nPanel, pScreenName );
			if (!pScreenName)
				continue;

			const char *pScreenClassname;
			GetControlPanelClassName( nPanel, pScreenClassname );
			if ( !pScreenClassname )
				continue;

			// Compute the screen size from the attachment points...
			matrix3x4_t	panelToWorld;
			pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

			matrix3x4_t	worldToPanel;
			MatrixInvert( panelToWorld, worldToPanel );

			// Now get the lower right position + transform into panel space
			Vector lr, lrlocal;
			pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
			MatrixGetColumn( panelToWorld, 3, lr );
			VectorTransform( lr, worldToPanel, lrlocal );

			float flWidth = lrlocal.x;
			float flHeight = lrlocal.y;

			CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
			pScreen->ChangeTeam( GetTeamNumber() );
			pScreen->SetActualSize( flWidth, flHeight );
			pScreen->SetActive( true );
			pScreen->MakeVisibleOnlyToTeammates( false );
			int nScreen = m_hScreens.AddToTail( );
			m_hScreens[nScreen].Set( pScreen );			
		}
	}

	void CPlantedC4::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
	{
		// Are we already marked for transmission?
		if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
			return;

		BaseClass::SetTransmit( pInfo, bAlways );

		// Force our screens to be sent too.
		for ( int i=0; i < m_hScreens.Count(); i++ )
		{
			CVGuiScreen *pScreen = m_hScreens[i].Get();
			pScreen->SetTransmit( pInfo, bAlways );
		}
	}

	CPlantedC4* CPlantedC4::ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles )
	{
		CPlantedC4 *pGrenade = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4" ) );
		if ( pGrenade )
		{
			vecAngles[0] = 0;
			vecAngles[2] = 0;
			pGrenade->Init( pevOwner, vecStart, vecAngles );
			return pGrenade;
		}
		else
		{
			Warning( "Can't create planted_c4 entity!\n" );
			return NULL;
		}
	}


	void CPlantedC4::Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles )
	{
		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_NONE );

		SetModel( PLANTED_C4_MODEL );	// Change this to c4 model

		SetCollisionBounds( Vector( 0, 0, 0 ), Vector( 8, 8, 8 ) );

		SetAbsOrigin( vecStart );
		SetAbsAngles( vecAngles );
		SetOwnerEntity( pevOwner );
        
        //=============================================================================
        // HPE_BEGIN:
        // [tj] Set the planter when the bomb is planted.
        //=============================================================================
         
        SetPlanter( pevOwner );
         
        //=============================================================================
        // HPE_END
        //=============================================================================
        
		
		// Detonate in "time" seconds
		SetThink( &CPlantedC4::C4Think );

		SetNextThink( gpGlobals->curtime + 0.1f );
		
		m_flTimerLength = mp_c4timer.GetInt();

		m_flC4Blow = gpGlobals->curtime + m_flTimerLength;
		m_flNextDefuse = 0;

		m_bStartDefuse = false;
		m_bBombTicking = true;
		SetFriction( 0.9 );

		m_flDefuseLength = 0.0f;
		
		SpawnControlPanels();
	}

	void CPlantedC4::C4Think()
	{
		if (!IsInWorld())
		{
			UTIL_Remove( this );
			return;
		}

		//Bomb is dead, don't think anymore
		if( !m_bBombTicking )
		{
			SetThink( NULL );
			return;
		}
				

		SetNextThink( gpGlobals->curtime + 0.12 );

#ifndef CLIENT_DLL
		// let the bots hear the bomb beeping
		// BOTPORT: Emit beep events at same time as client effects
		IGameEvent * event = gameeventmanager->CreateEvent( "bomb_beep" );
		if( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}
#endif
		
		// IF the timer has expired ! blow this bomb up!
		if (m_flC4Blow <= gpGlobals->curtime)
		{
			// give the defuser credit for defusing the bomb
			CCSPlayer* pBombOwner = ToCSPlayer(GetOwnerEntity());
			if ( pBombOwner )
			{
                if (CSGameRules()->m_iRoundWinStatus == WINNER_NONE)
				    pBombOwner->IncrementFragCount( 3 );
			}

			CSGameRules()->m_bBombDropped = false;

			trace_t tr;
			Vector vecSpot = GetAbsOrigin();
			vecSpot[2] += 8;

			UTIL_TraceLine( vecSpot, vecSpot + Vector ( 0, 0, -40 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			Explode( &tr, DMG_BLAST );

			CSGameRules()->m_bBombPlanted = false;

			CCS_GameStats.Event_BombExploded(pBombOwner);

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_exploded" );
			if( event )
			{
				event->SetInt( "userid", pBombOwner?pBombOwner->GetUserID():-1 );
				event->SetInt( "site", m_iBombSiteIndex );
				event->SetInt( "priority", 9 );
				gameeventmanager->FireEvent( event );
			}

			// skip additional processing once the bomb has exploded
			return;
		}

		//if the defusing process has started
		if ((m_bStartDefuse == true) && (m_pBombDefuser != NULL))
		{
			//if the defusing process has not ended yet
			if ( m_flDefuseCountDown > gpGlobals->curtime)
			{
				int iOnGround = FBitSet( m_pBombDefuser->GetFlags(), FL_ONGROUND );

				//if the bomb defuser has stopped defusing the bomb
				if( m_flNextDefuse < gpGlobals->curtime || !iOnGround )
				{
					if ( !iOnGround && m_pBombDefuser->IsAlive() )
						ClientPrint( m_pBombDefuser, HUD_PRINTCENTER, "#C4_Defuse_Must_Be_On_Ground");

					// release the player from being frozen
					m_pBombDefuser->m_bIsDefusing = false;

#ifndef CLIENT_DLL
					// tell the bots someone has aborted defusing
					IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
					if( event )
					{
						event->SetInt("userid", m_pBombDefuser->GetUserID() );
						event->SetInt( "priority", 6 );
						gameeventmanager->FireEvent( event );
					}
#endif

					//cancel the progress bar
					m_pBombDefuser->SetProgressBarTime( 0 );
                    m_pBombDefuser->OnCanceledDefuse();
					m_pBombDefuser = NULL;
					m_bStartDefuse = false;
					m_flDefuseCountDown = 0;
					m_flDefuseLength = 0;	//force it to show completely defused
				}

				return;
			}

			//if the defuse process has ended, kill the c4
			if ( !m_pBombDefuser->IsDead() )
			{
                //=============================================================================
                // HPE_BEGIN
                // [dwenger] Stats update for bomb defusing
                //=============================================================================
                CCS_GameStats.Event_BombDefused( m_pBombDefuser );
                //=============================================================================
                // HPE_END
                //=============================================================================

				IGameEvent * event = gameeventmanager->CreateEvent( "bomb_defused" );
				if( event )
				{
					event->SetInt("userid", m_pBombDefuser->GetUserID() );
					event->SetInt("site", m_iBombSiteIndex );
					event->SetInt( "priority", 9 );
					gameeventmanager->FireEvent( event );

                    //=============================================================================
                    // HPE_BEGIN
                    // [dwenger] Server-side processing for defusing bombs
                    //=============================================================================
                    m_pBombDefuser->AwardAchievement(CSWinBombDefuse);

                    float   timeToDetonation = (m_flC4Blow - gpGlobals->curtime);

					if ((timeToDetonation > 0.0f) && (timeToDetonation <= AchievementConsts::BombDefuseCloseCall_MaxTimeRemaining))
                    {
                        // Give achievement for defusing with < 1 second before detonation
                        m_pBombDefuser->AwardAchievement(CSBombDefuseCloseCall);
                    }

                    if ((timeToDetonation > 0.0f) && (m_pBombDefuser->HasDefuser()) && (timeToDetonation < AchievementConsts::BombDefuseNeededKit_MaxTime))
                    {
                        // Give achievement for defusing with a defuse kit when not having the kit would have taken too long
                        m_pBombDefuser->AwardAchievement(CSDefuseAndNeededKit);
                    }

                    // [dwenger] Added for fun-fact support
                    if ( m_pBombDefuser->PickedUpDefuser() )
                    {
                        // Defuser kit was picked up, so set the fun fact
                        m_pBombDefuser->SetDefusedWithPickedUpKit(true);
                    }

                    //=============================================================================
                    // HPE_END
                    //=============================================================================
				}

			
				Vector soundPosition = m_pBombDefuser->GetAbsOrigin() + Vector( 0, 0, 5 );
				CPASAttenuationFilter filter( soundPosition );

				EmitSound( filter, entindex(), "c4.disarmfinish" );
								
				// The bomb has just been disarmed.. Check to see if the round should end now
				m_bBombTicking = false;

				// release the player from being frozen
				m_pBombDefuser->m_bIsDefusing = false;

				CSGameRules()->m_bBombDefused = true;
				//=============================================================================
				// HPE_BEGIN:
				// [menglish] Give the bomb defuser an mvp if they ended the round
				//=============================================================================				 
				bool roundWasAlreadyWon = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);

				if(CSGameRules()->CheckWinConditions() && !roundWasAlreadyWon)
				{
					m_pBombDefuser->IncrementNumMVPs( CSMVP_BOMBDEFUSE );
				}				 
				//=============================================================================
				// HPE_END
				//=============================================================================

				// give the defuser credit for defusing the bomb
				m_pBombDefuser->IncrementFragCount( 3 );

				CSGameRules()->m_bBombDropped = false;
				CSGameRules()->m_bBombPlanted = false;
				
				// Clear their progress bar.
				m_pBombDefuser->SetProgressBarTime( 0 );

				m_pBombDefuser = NULL;
				m_bStartDefuse = false;

				m_flDefuseLength = 10;

				return;
			}

			//if it gets here then the previouse defuser has taken off or been killed

#ifndef CLIENT_DLL
			// tell the bots someone has aborted defusing
			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
			if ( event )
			{
				event->SetInt("userid", m_pBombDefuser->GetUserID() );
				event->SetInt( "priority", 6 );
				gameeventmanager->FireEvent( event );
			}
#endif

			// release the player from being frozen
			m_pBombDefuser->m_bIsDefusing = false;
			m_bStartDefuse = false;
			m_pBombDefuser = NULL;
		}
	}

	// Regular explosions
	void CPlantedC4::Explode( trace_t *pTrace, int bitsDamageType )
	{
		// Check to see if the round is over after the bomb went off...
		CSGameRules()->m_bTargetBombed = true;
		m_bBombTicking = false;
		//=============================================================================
		// HPE_BEGIN:
		// [tj] Saving off this value so we can see if the detonation is what caused the round to end.
		//=============================================================================
		bool roundWasAlreadyWon = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);
		//=============================================================================
		// HPE_END
		//=============================================================================		

		bool bWin = CSGameRules()->CheckWinConditions();

        //=============================================================================
        // HPE_BEGIN
        //=============================================================================		

        // [dwenger] Server-side processing for winning round by planting a bomb
        if (bWin)
        {
            CCSPlayer *pBombOwner = ToCSPlayer( GetOwnerEntity() );
            if ( pBombOwner )
            {
                pBombOwner->AwardAchievement(CSWinBombPlant);

                //[tj]more specific achievement for planting the bomb after recovering it.
                if (m_bPlantedAfterPickup)
                {
                    pBombOwner->AwardAchievement(CSWinBombPlantAfterRecovery);
                }
				// [menglish] awarding mvp to bomb planter
				if (!roundWasAlreadyWon)
				{
					pBombOwner->IncrementNumMVPs( CSMVP_BOMBPLANT );
				}
            }
        }

        //=============================================================================
        // HPE_END
        //=============================================================================

		// Do the Damage
		float flBombRadius = 500;
		if ( g_pMapInfo )
			flBombRadius = g_pMapInfo->m_flBombRadius;

		// Output to the bomb target ent
		CBaseEntity *pTarget = NULL;
		variant_t emptyVariant;
		while ((pTarget = gEntList.FindEntityByClassname( pTarget, "func_bomb_target" )) != NULL)
		{
			//Adrian - But only to the one we want!
			if ( pTarget->entindex() != m_iBombSiteIndex )
				 continue;
			
			pTarget->AcceptInput( "BombExplode", this, this, emptyVariant, 0 );
				break;
		}	

		// Pull out of the wall a bit
		if ( pTrace->fraction != 1.0 )
		{
			SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
		}

		{
			Vector pos = GetAbsOrigin() + Vector( 0,0,8 );

			// add an explosion TE so it affects clientside physics
			CPASFilter filter( pos );
			te->Explosion( filter, 0.0,
				&pos, 
				g_sModelIndexFireball,
				50.0, 
				25,
				TE_EXPLFLAG_NONE,
				flBombRadius * 3.5,
				200 );
		}

		// Sound! for everyone
		CBroadcastRecipientFilter filter;
		EmitSound( filter, entindex(), "c4.explode" );


		// Decal!
		UTIL_DecalTrace( pTrace, "Scorch" );

		
		// Shake!
		UTIL_ScreenShake( pTrace->endpos, 25.0, 150.0, 1.0, 3000, SHAKE_START );


		SetOwnerEntity( NULL ); // can't traceline attack owner if this is set

		CSGameRules()->RadiusDamage( 
			CTakeDamageInfo( this, GetOwnerEntity(), flBombRadius, bitsDamageType ),
			GetAbsOrigin(),
			flBombRadius * 3.5,	//Matt - don't ask me, this is how CS does it.
			CLASS_NONE,
			true );	// IGNORE THE WORLD!!

		// send director message, that something important happed here
		/*
		MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
			WRITE_BYTE ( 9 );	// command length in bytes
			WRITE_BYTE ( DRC_CMD_EVENT );	// bomb explode
			WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
			WRITE_SHORT( 0 );	// index number of secondary entity
			WRITE_LONG( 15 | DRC_FLAG_FINAL );   // eventflags (priority and flags)
		MESSAGE_END();
		*/
	}

	
	// For CTs to defuse the c4
	void CPlantedC4::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		//Can't defuse if its already defused or if it has blown up
		if( !m_bBombTicking )
		{
			SetUse( NULL );
			return;
		}

		CCSPlayer *player = dynamic_cast< CCSPlayer* >( pActivator );

		if ( !player || player->GetTeamNumber() != TEAM_CT )
		 	return;

		if ( m_bStartDefuse )
		{
			if ( player != m_pBombDefuser )
			{
				if ( player->m_iNextTimeCheck < gpGlobals->curtime )
				{
					ClientPrint( player, HUD_PRINTCENTER, "#Bomb_Already_Being_Defused" );
					player->m_iNextTimeCheck = gpGlobals->curtime + 1;
				}
				return;
			}

			m_flNextDefuse = gpGlobals->curtime + 0.5;
		}
		else
		{
			// freeze the player in place while defusing

			IGameEvent * event = gameeventmanager->CreateEvent("bomb_begindefuse" );
			if( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				if ( player->HasDefuser() )
				{
					event->SetInt( "haskit", 1 );
					// TODO show messages on clients on event 
					ClientPrint( player, HUD_PRINTCENTER, "#Defusing_Bomb_With_Defuse_Kit" );
				}
				else
				{
					event->SetInt( "haskit", 0 );
					// TODO show messages on clients on event 
					ClientPrint( player, HUD_PRINTCENTER, "#Defusing_Bomb_Without_Defuse_Kit" );
				}
				event->SetInt( "priority", 8 );
                gameeventmanager->FireEvent( event );
			}

			Vector soundPosition = player->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );

			EmitSound( filter, entindex(), "c4.disarmstart" );

			m_flDefuseLength = player->HasDefuser() ? 5 : 10;


			m_flNextDefuse = gpGlobals->curtime + 0.5;
			m_pBombDefuser = player;
			m_bStartDefuse = TRUE;
			player->m_bIsDefusing = true;
			
			m_flDefuseCountDown = gpGlobals->curtime + m_flDefuseLength;

			//start the progress bar
			player->SetProgressBarTime( m_flDefuseLength );


            player->OnStartedDefuse();
		}
	}


#endif



// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( C4, DT_WeaponC4 )

BEGIN_NETWORK_TABLE( CC4, DT_WeaponC4 )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bStartedArming ) ),
		RecvPropBool( RECVINFO( m_bBombPlacedAnimation ) ),
		RecvPropFloat( RECVINFO( m_fArmedTime ) )
	#else
		SendPropBool( SENDINFO( m_bStartedArming ) ),
		SendPropBool( SENDINFO( m_bBombPlacedAnimation ) ),
		SendPropFloat( SENDINFO( m_fArmedTime ), 0, SPROP_NOSCALE )
	#endif
END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CC4 )
	DEFINE_PRED_FIELD( m_bStartedArming, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bBombPlacedAnimation, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fArmedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_c4, CC4 );
PRECACHE_WEAPON_REGISTER( weapon_c4 );



// -------------------------------------------------------------------------------- //
// Globals.
// -------------------------------------------------------------------------------- //

CUtlVector< CC4* > g_C4s;



// -------------------------------------------------------------------------------- //
// CC4 implementation.
// -------------------------------------------------------------------------------- //

CC4::CC4()
{
	g_C4s.AddToTail( this );
    m_bDroppedFromDeath = false;

#if defined( CLIENT_DLL )
	m_szScreenText[0] = '\0';
#endif

}


CC4::~CC4()
{
	g_C4s.FindAndRemove( this );
}

void CC4::Spawn()
{
	BaseClass::Spawn();

	//Don't allow players to shoot the C4 around
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	//Don't be damaged / moved by explosions
	m_takedamage = DAMAGE_NO;

	m_bBombPlanted = false;
}

void CC4::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Disable all the firing code.. the C4 grenade is all custom.
	if ( pPlayer->m_nButtons & IN_ATTACK )
	{
		PrimaryAttack();
	}
	else
	{
		WeaponIdle();
	}
}

#if defined( CLIENT_DLL )

	bool CC4::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 7001 )
		{
			//set the screen text to the string in 'options'
			Q_strncpy( m_szScreenText, options, 16 );

			return true;
		}
		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	char *CC4::GetScreenText( void )
	{
		if( m_bStartedArming )
			return m_szScreenText;
		else
			return "";
	}

#endif //CLIENT_DLL

#ifdef GAME_DLL
	
		
	unsigned int CC4::PhysicsSolidMaskForEntity( void ) const
	{
		return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_PLAYERCLIP;
	}

	void CC4::Precache()
	{
		PrecacheVGuiScreen( "c4_view_panel" );
		
		PrecacheScriptSound( "c4.disarmfinish" );
		PrecacheScriptSound( "c4.explode" );
		PrecacheScriptSound( "c4.disarmstart" );
		PrecacheScriptSound( "c4.plant" );
		PrecacheScriptSound( "C4.PlantSound" );

		BaseClass::Precache();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Gets info about the control panels
	//-----------------------------------------------------------------------------
	void CC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_view_panel";
	}

	bool CC4::Holster( CBaseCombatWeapon *pSwitchingTo )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
			pPlayer->SetProgressBarTime( 0 );

		if ( m_bStartedArming )
		{
			AbortBombPlant();
		}
		
		return BaseClass::Holster( pSwitchingTo );
	}


	bool CC4::ShouldRemoveOnRoundRestart()
	{
		// Doesn't matter if we have an owner or not.. always remove the C4 when the round restarts.
		// The gamerules will give another C4 to some lucky player.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer && pPlayer->GetActiveWeapon() == this )
			engine->ClientCommand( pPlayer->edict(), "lastinv reset\n" );
		return true;
	}

#endif


void CC4::PrimaryAttack()
{
	bool	bArmingTimeSatisfied = false;
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	int onGround = FBitSet( pPlayer->GetFlags(), FL_ONGROUND );
	CBaseEntity *groundEntity = (onGround) ? pPlayer->GetGroundEntity() : NULL;
	if ( groundEntity )
	{
		// Don't let us stand on players, breakables, or pushaway physics objects to plant
		if ( groundEntity->IsPlayer() ||
			IsPushableEntity( groundEntity ) ||
#ifndef CLIENT_DLL
			IsBreakableEntity( groundEntity ) ||
#endif // !CLIENT_DLL
			IsPushAwayEntity( groundEntity ) )
		{
			onGround = false;
		}
	}

	if( m_bStartedArming == false && m_bBombPlanted == false )
	{
		if( pPlayer->m_bInBombZone && onGround )
		{
			m_bStartedArming = true;
			m_fArmedTime = gpGlobals->curtime + WEAPON_C4_ARM_TIME;
			m_bBombPlacedAnimation = false;


#if !defined( CLIENT_DLL )			
			// init the beep flags
			int i;
			for( i=0;i<NUM_BEEPS;i++ )
				m_bPlayedArmingBeeps[i] = false;

			// freeze the player in place while planting

			// player "arming bomb" animation
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
			pPlayer->SetNextAttack( gpGlobals->curtime );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_beginplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}
#endif

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );

			FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_PLANT );
		}
		else
		{
			if ( !pPlayer->m_bInBombZone )
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#C4_Plant_At_Bomb_Spot");
			}
			else
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#C4_Plant_Must_Be_On_Ground");
			}

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}
	else
	{
		if ( !onGround || !pPlayer->m_bInBombZone )
		{
			if( !pPlayer->m_bInBombZone )
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#C4_Arming_Cancelled" );
			}
			else
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#C4_Plant_Must_Be_On_Ground" );
			}

			AbortBombPlant();

			if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			{
				SendWeaponAnim( ACT_VM_DRAW );
			}
			else
			{
				SendWeaponAnim( ACT_VM_IDLE );
			}
			
			return;
		}
		else
		{
#ifndef CLIENT_DLL
			PlayArmingBeeps();
#endif

			if( gpGlobals->curtime >= m_fArmedTime ) //the c4 is ready to be armed
			{
				//check to make sure the player is still in the bomb target area
				bArmingTimeSatisfied = true;
			}
			else if( ( gpGlobals->curtime >= (m_fArmedTime - 0.75) ) && ( !m_bBombPlacedAnimation ) )
			{
				//call the c4 Placement animation 
				m_bBombPlacedAnimation = true;

				SendWeaponAnim( ACT_VM_SECONDARYATTACK );
				
#if !defined( CLIENT_DLL )
				// player "place" animation
				//pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif
			}
		}
	}

	if ( bArmingTimeSatisfied && m_bStartedArming )
	{
		m_bStartedArming = false;
		m_fArmedTime = 0;
		
		if( pPlayer->m_bInBombZone )
		{
#if !defined( CLIENT_DLL )
			CPlantedC4 *pC4 = CPlantedC4::ShootSatchelCharge( pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsAngles() );

			if ( pC4 )
			{
				pC4->SetBombSiteIndex( pPlayer->m_iBombSiteIndex );

				trace_t tr;
				UTIL_TraceEntity( pC4, GetAbsOrigin(), GetAbsOrigin() + Vector(0,0,-200), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
				pC4->SetAbsOrigin( tr.endpos );

				CBombTarget *pBombTarget = (CBombTarget*)UTIL_EntityByIndex( pPlayer->m_iBombSiteIndex );
				
				if ( pBombTarget )
				{
					CBaseEntity *pAttachPoint = gEntList.FindEntityByName( NULL, pBombTarget->GetBombMountTarget() );

					if ( pAttachPoint )
					{
						pC4->SetAbsOrigin( pAttachPoint->GetAbsOrigin() );
						pC4->SetAbsAngles( pAttachPoint->GetAbsAngles() );
						pC4->SetParent( pAttachPoint );
					}

					variant_t emptyVariant;
					pBombTarget->AcceptInput( "BombPlanted", pC4, pC4, emptyVariant, 0 );
				}

				// [tj] If the bomb is planted by someone that picked it up after the 
				//      original owner was killed, pass that along to the planted bomb
				pC4->SetPlantedAfterPickup( m_bDroppedFromDeath );
			}

            //=============================================================================
            // HPE_BEGIN
            // [dwenger] Stats update for bomb planting
            //=============================================================================

            // Determine how elapsed time from start of round until the bomb was planted
            float   plantingTime = gpGlobals->curtime - CSGameRules()->GetRoundStartTime();

            // Award achievement to bomb planter if time <= 25 seconds
            if ((plantingTime > 0.0f) && (plantingTime <= AchievementConsts::FastBombPlant_Time))
            {
                pPlayer->AwardAchievement(CSPlantBombWithin25Seconds);
            }

            CCS_GameStats.Event_BombPlanted( pPlayer );

            //=============================================================================
            // HPE_END
            //=============================================================================

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_planted" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt("posx", pPlayer->GetAbsOrigin().x );
				event->SetInt("posy", pPlayer->GetAbsOrigin().y );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}

			// Fire a beep event also so the bots have a chance to hear the bomb
			event = gameeventmanager->CreateEvent( "bomb_beep" );

			if ( event )
			{
				event->SetInt( "entindex", entindex() );
				gameeventmanager->FireEvent( event );
			}

			pPlayer->SetProgressBarTime( 0 );

			CSGameRules()->m_bBombDropped = false;
			CSGameRules()->m_bBombPlanted = true;

			// Play the plant sound.
			Vector plantPosition = pPlayer->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( plantPosition );
			EmitSound( filter, entindex(), "c4.plant" );

			// No more c4!
			pPlayer->Weapon_Drop( this, NULL, NULL );
			UTIL_Remove( this );
#endif

			//don't allow the planting to start over again next frame.
			m_bBombPlanted = true;

			return;
		}
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#C4_Activated_At_Bomb_Spot" );

#if !defined( CLIENT_DLL )
			//pPlayer->SetAnimation( PLAYER_HOLDBOMB );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}
#endif

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
	SetWeaponIdleTime( gpGlobals->curtime + SharedRandomFloat("C4IdleTime", 10, 15 ) );
}

void CC4::WeaponIdle()
{
	// if the player releases the attack button cancel the arming sequence
	if ( m_bStartedArming )
	{
		AbortBombPlant();

		CCSPlayer *pPlayer = GetPlayerOwner();

		// TODO: make this use SendWeaponAnim and activities when the C4 has the activities hooked up.
		if ( pPlayer )
		{
			SendWeaponAnim( ACT_VM_IDLE );
			pPlayer->SetNextAttack( gpGlobals->curtime );
		}

		if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			SendWeaponAnim( ACT_VM_DRAW );
		else
			SendWeaponAnim( ACT_VM_IDLE );
	}
}

void CC4::UpdateShieldState( void )
{
	//ADRIANTODO
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	
	if ( pPlayer->HasShield() )
	{
		pPlayer->SetShieldDrawnState( false );

		CBaseViewModel *pVM = pPlayer->GetViewModel( 1 );

		if ( pVM )
		{
			pVM->AddEffects( EF_NODRAW );
		}
			//pPlayer->SetHitBoxSet( 3 );
	}
	else
		BaseClass::UpdateShieldState();
}


int m_iBeepFrames[NUM_BEEPS] = { 27, 37, 45, 51, 57, 63, 67 };
int iNumArmingAnimFrames = 83;

void CC4::PlayArmingBeeps( void )
{
	float flStartTime = m_fArmedTime - WEAPON_C4_ARM_TIME;

	float flProgress = ( gpGlobals->curtime - flStartTime ) / ( WEAPON_C4_ARM_TIME - 0.75 );

	int currentFrame = (int)( (float)iNumArmingAnimFrames * flProgress );

	int i;
	for( i=0;i<NUM_BEEPS;i++ )
	{
		if( currentFrame <= m_iBeepFrames[i] )
		{
			break;
		}
		else if( !m_bPlayedArmingBeeps[i] )
		{
			m_bPlayedArmingBeeps[i] = true;

			CCSPlayer *owner = GetPlayerOwner();
			Vector soundPosition = owner->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );

			filter.RemoveRecipient( owner );

			// remove anyone that is first person spec'ing the planter
			int i;
			CBasePlayer *pPlayer;
			for( i=1;i<=gpGlobals->maxClients;i++ )
			{
				pPlayer = UTIL_PlayerByIndex( i );

				if ( !pPlayer )
					continue;

				if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == GetOwner() )
				{
					filter.RemoveRecipient( pPlayer );
				}
			}

			EmitSound(filter, entindex(), "c4.click");
			
			break;
		}
	}
}

float CC4::GetMaxSpeed() const
{
	if ( m_bStartedArming )
		return CS_PLAYER_SPEED_STOPPED;
	else
		return BaseClass::GetMaxSpeed();
}


void CC4::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

#if !defined( CLIENT_DLL )
	CCSPlayer *pPlayer = dynamic_cast<CCSPlayer *>( pNewOwner );

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_pickup" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "priority", 6 );
		gameeventmanager->FireEvent( event );
	}

	if ( pPlayer->m_bShowHints && !(pPlayer->m_iDisplayHistoryBits & DHF_BOMB_RETRIEVED) )
	{
		pPlayer->m_iDisplayHistoryBits |= DHF_BOMB_RETRIEVED;
		pPlayer->HintMessage( "#Hint_you_have_the_bomb", false );
	}
	else
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#Got_bomb" );
	}

    pPlayer->SetBombPickupTime(gpGlobals->curtime);
#endif
}

// HACK - Ask Mike Booth...
#ifndef CLIENT_DLL
	#include "cs_bot.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CC4::Drop( const Vector &vecVelocity )
{
#if !defined( CLIENT_DLL )
	if ( !CSGameRules()->m_bBombPlanted ) // its not dropped if its planted
	{
		// tell the bots about the dropped bomb
		TheCSBots()->SetLooseBomb( this );

		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(GetOwnerEntity());
		Assert( pPlayer );
		if ( pPlayer )
		{
			IGameEvent * event = gameeventmanager->CreateEvent("bomb_dropped" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetInt( "priority", 6 );
				gameeventmanager->FireEvent( event );
			}
		}
	}
#endif

	if ( m_bStartedArming )
		AbortBombPlant();  // stop arming sequence

	BaseClass::Drop( vecVelocity );
}

void CC4::AbortBombPlant()
{
	m_bStartedArming = false; 

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#if !defined( CLIENT_DLL )
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;

	pPlayer->SetProgressBarTime( 0 );

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
	if( event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		event->SetInt("site", pPlayer->m_iBombSiteIndex );
		event->SetInt( "priority", 8 );
		gameeventmanager->FireEvent( event );
	}

#endif 

	FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_ABORT );
}