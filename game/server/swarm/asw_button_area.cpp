#include "cbase.h"
#include "asw_button_area.h"
#include "asw_door.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_profile.h"
#include "asw_hack_wire_tile.h"
#include "asw_marine_speech.h"
#include "asw_util_shared.h"
#include "asw_director.h"
#include "asw_gamerules.h"
#include "asw_achievements.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( trigger_asw_button_area, CASW_Button_Area );

#define ASW_MEDAL_WORTHY_WIRE_HACK 60

BEGIN_DATADESC( CASW_Button_Area )
	DEFINE_FIELD(m_bIsDoorButton, FIELD_BOOLEAN),
	DEFINE_FIELD(m_hDoorHack, FIELD_EHANDLE),
	DEFINE_FIELD(m_bIsInUse, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fHackProgress, FIELD_FLOAT),
	DEFINE_FIELD(m_fStartedHackTime, FIELD_FLOAT),
	DEFINE_FIELD(m_fLastButtonUseTime, FIELD_TIME),
	DEFINE_FIELD(m_bWaitingForInput, FIELD_BOOLEAN ),
	DEFINE_FIELD(m_bWasLocked, FIELD_BOOLEAN ),

	//DEFINE_KEYFIELD(m_iHackLevel, FIELD_INTEGER, "hacklevel" ),
	DEFINE_KEYFIELD(m_bIsLocked, FIELD_BOOLEAN, "locked" ),
	DEFINE_KEYFIELD(m_bNoPower, FIELD_BOOLEAN, "nopower" ),
	
	DEFINE_KEYFIELD(m_bUseAfterHack, FIELD_BOOLEAN, "useafterhack" ),
	DEFINE_KEYFIELD(m_bDisableAfterUse, FIELD_BOOLEAN, "disableafteruse" ),

	DEFINE_KEYFIELD( m_iWireColumns, FIELD_INTEGER, "wirecolumns"),
	DEFINE_KEYFIELD( m_iWireRows, FIELD_INTEGER, "wirerows"),
	DEFINE_KEYFIELD( m_iNumWires, FIELD_INTEGER, "numwires"),

	DEFINE_INPUTFUNC( FIELD_VOID,	"PowerOn",	InputPowerOn ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"PowerOff",	InputPowerOff ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ResetHack",	InputResetHack ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Unlock",	InputUnlock ),
	DEFINE_OUTPUT( m_OnButtonHackStarted, "OnButtonHackStarted" ),
	DEFINE_OUTPUT( m_OnButtonHackAt25Percent, "OnButtonHackAt25Percent" ),
	DEFINE_OUTPUT( m_OnButtonHackAt50Percent, "OnButtonHackAt50Percent" ),
	DEFINE_OUTPUT( m_OnButtonHackAt75Percent, "OnButtonHackAt75Percent" ),	
	DEFINE_OUTPUT( m_OnButtonHackCompleted, "OnButtonHackCompleted" ),
	DEFINE_OUTPUT( m_OnButtonActivated, "OnButtonActivated" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Button_Area, DT_ASW_Button_Area)	
	SendPropInt			(SENDINFO(m_iHackLevel)),
	SendPropBool		(SENDINFO(m_bIsLocked)),
	SendPropBool		(SENDINFO(m_bIsDoorButton)),
	SendPropBool		(SENDINFO(m_bIsInUse)),
	SendPropFloat		(SENDINFO(m_fHackProgress)),
	SendPropBool		(SENDINFO(m_bNoPower)),	
	SendPropBool		(SENDINFO(m_bWaitingForInput)),	
	SendPropString		(SENDINFO( m_NoPowerMessage ) ),
END_SEND_TABLE()

ConVar asw_ai_button_hacking_scale( "asw_ai_button_hacking_scale", "0.3", FCVAR_CHEAT, "Button panel hacking speed scale for AI marines" );
ConVar asw_tech_order_hack_range( "asw_tech_order_hack_range", "1200", FCVAR_CHEAT, "Max range when searching for a nearby AI tech to hack for you" );
ConVar asw_debug_button_skin( "asw_debug_button_skin", "0", FCVAR_CHEAT, "If set, button panels will output skin setting details" );
extern ConVar asw_simple_hacking;
extern ConVar asw_debug_medals;

CASW_Button_Area::CASW_Button_Area()
{
	//Msg("[S] CASW_Button_Area created\n");
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_iAliensKilledBeforeHack = 0;

	m_iHackLevel = 6;
}

CASW_Button_Area::~CASW_Button_Area()
{
	if (m_hDoorHack.Get())
	{
		UTIL_Remove(m_hDoorHack.Get());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASW_Button_Area::Spawn( void )
{
	if (m_flWait == -1)
		m_bDisableAfterUse = true;

	BaseClass::Spawn();
	Precache();

	// check if this button is linked to a door or not
	CASW_Door* pDoor = dynamic_cast<CASW_Door*>(m_hUseTarget.Get());	
	if (pDoor)
	{
		m_bIsDoorButton = true;
	}
	else
	{
		m_bIsDoorButton = false;
	}

	if ( m_bIsLocked )
	{
		m_bWasLocked = true;
	}

	UpdateWaitingForInput();
	UpdatePanelSkin();
}

void CASW_Button_Area::Precache()
{
	PrecacheScriptSound("ASWComputer.HackComplete");
	PrecacheScriptSound("ASWComputer.AccessDenied");
	PrecacheScriptSound("ASWButtonPanel.TileLit");
}

void CASW_Button_Area::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	// player has used this item
	//Msg("Player has activated a button area\n");
	if ( !HasPower() || !ASWGameResource() )
	{
		// todo: launch a window on the client saying there's no power, or whatever message is desired
		//Msg("We don't have the power cap'n\n");
		return;
	}
	if ( m_bIsLocked )
	{
		if ( pMarine->GetMarineProfile()->CanHack() )
		{
			// can hack, get the player to launch his hacking window				
			if ( !m_bIsInUse )
			{				
				if ( pMarine->StartUsing(this) )
				{
					if ( GetHackProgress() <= 0 && pMarine->GetMarineResource() )
					{
						pMarine->GetMarineResource()->m_iDamageTakenDuringHack = 0;
					}
					m_iAliensKilledBeforeHack = ASWGameResource()->GetAliensKilledInThisMission();

					m_OnButtonHackStarted.FireOutput( pMarine, this );
					if ( !asw_simple_hacking.GetBool() && pMarine->IsInhabited() )
					{
						if ( !GetCurrentHack() )	// if we haven't created a hack object for this computer yet, then create one	
						{
							m_hDoorHack = (CASW_Hack_Wire_Tile*) CreateEntityByName( "asw_hack_wire_tile" );
						}

						if ( GetCurrentHack() )
						{
							GetCurrentHack()->InitHack( pMarine->GetCommander(), pMarine, this );
						}
					}

					if ( m_iHackLevel > 20 )
					{
						pMarine->GetMarineSpeech()->Chatter( CHATTER_HACK_LONG_STARTED );
					}
					else
					{
						pMarine->GetMarineSpeech()->Chatter( CHATTER_HACK_STARTED );
					}
				}
			}
			else
			{
				if ( pMarine->m_hUsingEntity.Get() == this )
				{
					pMarine->StopUsing();
				}
				//Msg("Panel already in use");
			}
			//Msg("Unlocked button\n");
			// test hack puzzle so far
			/*
			m_hDoorHack = (CASW_Hack_Door*) CreateEntityByName( "asw_hack_door" );
			if (GetCurrentHack())
			{
				//for (int i=0;i<10;i++)
				//{
					GetCurrentHack()->InitHack(pPlayer, pMarine, this);
					GetCurrentHack()->BuildPuzzle(7);
					GetCurrentHack()->ShowPuzzleStatus();
				//}
			}
			*/
			
			
		}
		else
		{
			// can't hack (play some access denied sound)
			//Msg("Access denied\n");
			EmitSound("ASWComputer.AccessDenied");

			// check for a nearby AI tech marine
			float flMarineDistance;
			CASW_Marine *pTech = UTIL_ASW_NearestMarine( WorldSpaceCenter(), flMarineDistance, MARINE_CLASS_TECH, true );
			if ( pTech && flMarineDistance < asw_tech_order_hack_range.GetFloat() )
			{
				//Msg( "Told tech to hack panel\n" );
				pTech->OrderHackArea( this );
			}
			return;
		}
	}
	else
	{
		if ( pMarine )
		{
			pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);
		}
		ActivateUnlockedButton( pMarine );
	}	
}

void CASW_Button_Area::ActivateUnlockedButton(CASW_Marine* pMarine)
{
	// don't use the button if we're in the delay between using
	if ( m_fLastButtonUseTime != 0 && gpGlobals->curtime < m_fLastButtonUseTime + m_flWait )
		return;
	if ( !pMarine )
		return;

	if( !RequirementsMet( pMarine ) )
		return;

	if ( m_bIsDoorButton )
	{
		// if the door isn't sealed (or greater than a certain amount of damage?)
		//  then make it open
		CASW_Door* pDoor = GetDoor();
		if ( pDoor )
		{
			if (pDoor->GetSealAmount() > 0)
			{
				//Msg("Door mechanism not responding.  Maintenance Division has been notified of the problem.\n");
			}
			else
			{				
				//Msg("Toggling door...\n");
				variant_t emptyVariant;
				pDoor->AcceptInput("Toggle", pMarine, this, emptyVariant, 0);
			}
		}
	}

	// send our 'activated' output
	m_OnButtonActivated.FireOutput(pMarine, this);

	// Fire event
	IGameEvent * event = gameeventmanager->CreateEvent( "button_area_used" );
	if ( event )
	{
		CASW_Player *pPlayer = pMarine->GetCommander();

		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
		event->SetInt( "entindex", entindex() );
		gameeventmanager->FireEvent( event );
	}

	m_fLastButtonUseTime = gpGlobals->curtime;

	UpdateWaitingForInput();

	if ( m_bDisableAfterUse )
	{
		UTIL_Remove(this);
	}
}


CASW_Door* CASW_Button_Area::GetDoor()
{
	return dynamic_cast<CASW_Door*>(m_hUseTarget.Get());
}

CASW_Hack* CASW_Button_Area::GetCurrentHack()
{
	return dynamic_cast<CASW_Hack*>(m_hDoorHack.Get());
}

// traditional Swarm hacking
void CASW_Button_Area::MarineUsing(CASW_Marine* pMarine, float deltatime)
{
	if (m_bIsInUse && m_bIsLocked && ( asw_simple_hacking.GetBool() || !pMarine->IsInhabited() ) )
	{
		float fTime = (deltatime * (1.0f/((float)m_iHackLevel)));
		// boost fTime by the marine's hack skill
		fTime *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE);
		fTime *= asw_ai_button_hacking_scale.GetFloat();
		SetHackProgress(m_fHackProgress + fTime, pMarine);
	}
}

void CASW_Button_Area::MarineStartedUsing(CASW_Marine* pMarine)
{
	m_bIsInUse = true;
	UpdateWaitingForInput();

	if ( ASWDirector() && m_bIsLocked )
	{
		ASWDirector()->OnMarineStartedHack( pMarine, this );
	}
}

void CASW_Button_Area::MarineStoppedUsing(CASW_Marine* pMarine)
{
	//Msg( "Marine stopped using button area\n" );
	m_bIsInUse = false;
	UpdateWaitingForInput();

	if (GetCurrentHack())	// notify our current hack that we've stopped using the console
	{
		GetCurrentHack()->MarineStoppedUsing(pMarine);
	}
}

bool CASW_Button_Area::IsActive( void )
{
	return ( m_bIsInUse || !m_bIsLocked );
}

void CASW_Button_Area::InputPowerOn( inputdata_t &inputdata )
{
	m_bNoPower = false;
	UpdateWaitingForInput();
	UpdatePanelSkin();
}

void CASW_Button_Area::InputPowerOff( inputdata_t &inputdata )
{
	m_bNoPower = true;
	UpdateWaitingForInput();
	UpdatePanelSkin();
}

void CASW_Button_Area::InputUnlock( inputdata_t &inputdata )
{
	if (m_bIsLocked)
	{
		m_bIsLocked = false;
		m_fHackProgress = 1.0f;
		UpdateWaitingForInput();
		UpdatePanelSkin();

		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(inputdata.pActivator);

		// if set to use on unlock, then do so
		if (m_bUseAfterHack && pMarine)
		{
			ActivateUnlockedButton(pMarine);
		}
	}
}

void CASW_Button_Area::InputResetHack( inputdata_t &inputdata )
{
	if ( !m_bIsInUse )
	{
		m_fStartedHackTime = false;
		SetHackProgress(0, NULL);
		m_bIsLocked = true;
		UpdateWaitingForInput();
		UpdatePanelSkin();
		if (m_hDoorHack.Get())
		{
			// need to delete the hack
			UTIL_Remove(m_hDoorHack.Get());
			m_hDoorHack = NULL;
		}
	}
}

void CASW_Button_Area::SetHackProgress(float f, CASW_Marine *pMarine)
{
	CBaseEntity *pActor = pMarine;
	if ( !pActor )
	{
		pActor = this;
	}

	if (m_fHackProgress <= 0 && f > 0 && m_fStartedHackTime == 0)
	{
		m_fStartedHackTime = gpGlobals->curtime;
	}
	if (m_fHackProgress < 0.25f && f >= 0.25f)
	{
		m_OnButtonHackAt25Percent.FireOutput(pActor, this);
	}
	if (m_fHackProgress < 0.5f && f >= 0.5f)
	{
		m_OnButtonHackAt50Percent.FireOutput(pActor, this);
		pMarine->GetMarineSpeech()->Chatter(CHATTER_HACK_HALFWAY);
	}
	if (m_fHackProgress < 0.75f && f >= 0.75f)
	{
		m_OnButtonHackAt75Percent.FireOutput(pActor, this);
	}
	if (m_fHackProgress < 1.0f && f >= 1.0f)
	{	
		if ( asw_simple_hacking.GetBool() )
		{
			pMarine->StopUsing();
		}

		float time_taken = gpGlobals->curtime - m_fStartedHackTime;
		// decide if this was a fast hack or not
		float fSpeedScale = 1.0f;
		if (pMarine)
			fSpeedScale *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE);
		float ideal_time = UTIL_ASW_CalcFastDoorHackTime(m_iWireRows, m_iWireColumns, m_iNumWires, m_iHackLevel, fSpeedScale);

		bool bFastHack = time_taken <= ideal_time;
		if (asw_debug_medals.GetBool())
		{
			Msg("Finished door hack, fast = %d.\n", bFastHack);
			Msg(" ideal time is %f you took %f\n", ideal_time, time_taken);
		}
		if (bFastHack && pMarine && pMarine->GetMarineResource())
		{
			pMarine->GetMarineResource()->m_iFastDoorHacks++;
			if ( pMarine->IsInhabited() && pMarine->GetCommander() )
			{
				pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_FAST_WIRE_HACKS );
			}
		}

		if ( GetCurrentHack() )
		{
			GetCurrentHack()->OnHackComplete();
		}

		if ( pMarine->GetMarineResource() && pMarine->GetMarineResource()->m_iDamageTakenDuringHack <= 0 )
		{
			int nAliensKilled = ASWGameResource() ? ASWGameResource()->GetAliensKilledInThisMission() : 0;
			if ( ( nAliensKilled - m_iAliensKilledBeforeHack ) > 10 )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
				{
					CASW_Player *pPlayer = dynamic_cast<CASW_Player*>( UTIL_PlayerByIndex( i ) );
					if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->GetMarine() || pPlayer->GetMarine() == pMarine )
						continue;

					if ( pPlayer->GetMarine()->GetMarineResource() )
					{
						pPlayer->GetMarine()->GetMarineResource()->m_bProtectedTech = true;
					}
					pPlayer->AwardAchievement( ACHIEVEMENT_ASW_PROTECT_TECH );
				}
			}
		}

		// unlock it
		m_OnButtonHackCompleted.FireOutput(pActor, this);
		m_bIsLocked = false;
		UpdateWaitingForInput();
		UpdatePanelSkin();

		EmitSound("ASWComputer.HackComplete");

		if ( pMarine && bFastHack )
		{
			pMarine->GetMarineSpeech()->QueueChatter(CHATTER_HACK_BUTTON_FINISHED, gpGlobals->curtime + 2.0f, gpGlobals->curtime + 3.0f);
		}

		// if set to use on unlock, then do so
		if ( m_bUseAfterHack && pMarine )
		{
			ActivateUnlockedButton(pMarine);
		}

		if ( pMarine && !pMarine->IsInhabited() )
		{
			pMarine->StopUsing();
		}
	}

	m_fHackProgress = clamp( f, 0.0f, 1.0f );
}

bool CASW_Button_Area::KeyValue( const char *szKeyName, const char *szValue )
{	
	if ( FStrEq( szKeyName, "nopowermessage" ) )
	{
		Q_strncpy( m_NoPowerMessage.GetForModify(), szValue, 255 );
		return true;
	}	
	return BaseClass::KeyValue( szKeyName, szValue );
}

bool CASW_Button_Area::WaitingForInputVismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer )
{
	CASW_Button_Area *pButtonArea = static_cast< CASW_Button_Area* >( pVisibleEntity );
	return pButtonArea->m_bWaitingForInput;
}

bool CASW_Button_Area::WaitingForInputVismonCallback( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer )
{
	CASW_Button_Area *pButtonArea = static_cast< CASW_Button_Area* >( pVisibleEntity );
	IGameEvent * event = gameeventmanager->CreateEvent( "button_area_active" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "entindex", pButtonArea->entindex() );
		event->SetInt( "prop", pButtonArea->m_hPanelProp.Get() ? pButtonArea->m_hPanelProp.Get()->entindex() : pButtonArea->entindex() );
		event->SetBool( "locked", pButtonArea->IsLocked() );
		gameeventmanager->FireEvent( event );
	}

	return false;
}

void CASW_Button_Area::UpdateWaitingForInput()
{
	bool bOldWaitingForInput = m_bWaitingForInput;

	bool bSet = false;

	if ( m_bIsDoorButton )
	{
		CASW_Door *pDoor = GetDoor();
		if ( pDoor && pDoor->IsAutoOpen() )
		{
			m_bWaitingForInput = false;
			bSet = true;
		}
	}

	if ( !bSet )
	{
		m_bWaitingForInput = ASWGameRules()->GetGameState() == ASW_GS_INGAME && 
							 ( !m_bNoPower && !m_bIsInUse && ( m_bIsLocked || ( !m_bWasLocked && m_fLastButtonUseTime == 0 ) ) );
	}

	if ( !bOldWaitingForInput && m_bWaitingForInput )
	{
		VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, &CASW_Button_Area::WaitingForInputVismonCallback, &CASW_Button_Area::WaitingForInputVismonEvaluator );
	}
	else if ( bOldWaitingForInput && !m_bWaitingForInput )
	{
		VisibilityMonitor_RemoveEntity( this );

		IGameEvent * event = gameeventmanager->CreateEvent( "button_area_inactive" );
		if ( event )
		{
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}
	}
}

// updates the panel prop (if any) with a skin to reflect the button's state
void CASW_Button_Area::UpdatePanelSkin()
{
	if ( !m_hPanelProp.Get() )
		return;

	if ( asw_debug_button_skin.GetBool() )
	{
		Msg( "CASW_Button_Area:%d: UpdatePanelSkin\n", entindex() );
	}
	CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>(m_hPanelProp.Get());
	while (pAnim)
	{		
		if (HasPower())
		{
			if (m_bIsLocked)
			{
				pAnim->m_nSkin = 1;	// locked skin
				if ( asw_debug_button_skin.GetBool() )
				{
					Msg( "  Panel is locked, setting prop %d skin to %d\n", pAnim->entindex(), pAnim->m_nSkin.Get() );
				}
			}
			else
			{
				pAnim->m_nSkin = 2;	// unlocked skin
				if ( asw_debug_button_skin.GetBool() )
				{
					Msg( "  Panel is unlocked, setting prop %d skin to %d\n", pAnim->entindex(), pAnim->m_nSkin.Get() );
				}
			}
		}
		else
		{
			pAnim->m_nSkin = 0;	// no power skin
			if ( asw_debug_button_skin.GetBool() )
			{
				Msg( "  Panel is no power, setting prop %d skin to %d\n", pAnim->entindex(), pAnim->m_nSkin.Get() );
			}
		}

		if (m_bMultiplePanelProps)
			pAnim = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName( pAnim, m_szPanelPropName ));
		else
			pAnim = NULL;
	}
}
