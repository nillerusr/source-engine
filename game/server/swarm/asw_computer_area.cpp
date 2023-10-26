#include "cbase.h"

#include "asw_computer_area.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_profile.h"
#include "asw_hack_computer.h"
#include "asw_objective_triggered.h"
#include "asw_marine_speech.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "point_camera.h"
#include "soundenvelope.h"
#include "asw_director.h"
#include "asw_util_shared.h"
#include "asw_gamerules.h"
#include "asw_achievements.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_simple_hacking( "asw_simple_hacking", "0", FCVAR_CHEAT, "Use simple progress bar computer hacking" );
ConVar asw_ai_computer_hacking_scale( "asw_ai_computer_hacking_scale", "0.1", FCVAR_CHEAT, "Computer hacking speed scale for AI marines" );
extern ConVar asw_tech_order_hack_range;

#define ASW_MEDAL_WORTHY_COMPUTER_HACK 20

LINK_ENTITY_TO_CLASS( trigger_asw_computer_area, CASW_Computer_Area );

BEGIN_DATADESC( CASW_Computer_Area )
	DEFINE_KEYFIELD(m_bIsLocked, FIELD_BOOLEAN, "locked" ),
	DEFINE_KEYFIELD(m_bUseAfterHack, FIELD_BOOLEAN, "useafterhack" ),

	DEFINE_KEYFIELD( m_SecurityCam1Name, FIELD_STRING, "SecurityCam1Name" ),	
	DEFINE_KEYFIELD( m_Turret1Name, FIELD_STRING, "Turret1Name" ),	
	DEFINE_KEYFIELD( m_MailFile, FIELD_STRING, "MailFile" ),
	DEFINE_KEYFIELD( m_NewsFile, FIELD_STRING, "NewsFile" ),
	DEFINE_KEYFIELD( m_StocksSeed, FIELD_STRING, "StocksSeed" ),
	DEFINE_KEYFIELD( m_WeatherSeed, FIELD_STRING, "WeatherSeed" ),
	DEFINE_KEYFIELD( m_PlantFile, FIELD_STRING, "PlantFile" ),
	DEFINE_KEYFIELD( m_PDAName, FIELD_STRING, "PDAName" ),
	DEFINE_KEYFIELD( m_DownloadObjectiveName, FIELD_STRING, "DownloadObjectiveName" ),

	DEFINE_KEYFIELD( m_bSecurityCam1Locked, FIELD_BOOLEAN, "SecurityCam1Locked" ),	
	DEFINE_KEYFIELD( m_bTurret1Locked, FIELD_BOOLEAN, "Turret1Locked" ),	
	DEFINE_KEYFIELD( m_bMailFileLocked, FIELD_BOOLEAN, "MailFileLocked" ),
	DEFINE_KEYFIELD( m_bNewsFileLocked, FIELD_BOOLEAN, "NewsFileLocked" ),
	DEFINE_KEYFIELD( m_bStocksFileLocked, FIELD_BOOLEAN, "StocksFileLocked" ),
	DEFINE_KEYFIELD( m_bWeatherFileLocked, FIELD_BOOLEAN, "WeatherFileLocked" ),
	DEFINE_KEYFIELD( m_bPlantFileLocked, FIELD_BOOLEAN, "PlantFileLocked" ),

	DEFINE_KEYFIELD(m_iHackLevel, FIELD_INTEGER, "HackDifficulty"),	
	DEFINE_KEYFIELD(m_iNumEntriesPerTumbler, FIELD_INTEGER, "EntriesPerTumbler"),
	DEFINE_KEYFIELD(m_fMoveInterval, FIELD_FLOAT, "TumblerMoveInterval"),	
	DEFINE_KEYFIELD(m_fDownloadTime, FIELD_FLOAT, "DownloadTime"),
	
	DEFINE_FIELD( m_bWaitingForInput, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bWasLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iActiveCam, FIELD_INTEGER ),
	DEFINE_FIELD( m_hComputerHack, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bIsInUse, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fHackProgress, FIELD_FLOAT ),
	DEFINE_FIELD( m_hSecurityCam1, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_hTurret1, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_SecurityCamLabel1, FIELD_STRING ),
	DEFINE_FIELD( m_SecurityCamLabel2, FIELD_STRING ),
	DEFINE_FIELD( m_SecurityCamLabel3, FIELD_STRING ),
	DEFINE_FIELD( m_TurretLabel1, FIELD_STRING ),
	DEFINE_FIELD( m_TurretLabel2, FIELD_STRING ),
	DEFINE_FIELD( m_TurretLabel3, FIELD_STRING ),
	DEFINE_FIELD( m_bDownloadedDocs, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDoSecureShout, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPlayedHalfwayChatter, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bViewingMail, FIELD_BOOLEAN ),
	DEFINE_FIELD(m_fAutoOverrideTime, FIELD_FLOAT),
	DEFINE_FIELD(m_fLastButtonUseTime, FIELD_TIME),
	DEFINE_SOUNDPATCH( m_pDownloadingSound ),
	DEFINE_SOUNDPATCH( m_pComputerInUseSound ),
	DEFINE_OUTPUT( m_OnComputerHackStarted, "OnComputerHackStarted" ),
	DEFINE_OUTPUT( m_OnComputerHackHalfway, "OnComputerHackHalfway" ),	
	DEFINE_OUTPUT( m_OnComputerHackCompleted, "OnComputerHackCompleted" ),
	DEFINE_OUTPUT( m_OnComputerActivated, "OnComputerActivated" ),
	DEFINE_OUTPUT( m_OnComputerDataDownloaded, "OnComputerDataDownloaded" ),
	DEFINE_OUTPUT( m_OnComputerViewMail1, "OnComputerViewMail1" ),
	DEFINE_OUTPUT( m_OnComputerViewMail2, "OnComputerViewMail2" ),
	DEFINE_OUTPUT( m_OnComputerViewMail3, "OnComputerViewMail3" ),
	DEFINE_OUTPUT( m_OnComputerViewMail4, "OnComputerViewMail4" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Computer_Area, DT_ASW_Computer_Area)	
	SendPropInt			(SENDINFO(m_iHackLevel)),
	SendPropFloat		(SENDINFO(m_fDownloadTime)),
	SendPropBool		(SENDINFO(m_bIsLocked)),
	SendPropBool		(SENDINFO(m_bWaitingForInput)),
	SendPropBool		(SENDINFO(m_bIsInUse)),
	SendPropFloat		(SENDINFO(m_fHackProgress)),

	SendPropEHandle( SENDINFO( m_hSecurityCam1 ) ),	
	SendPropEHandle( SENDINFO( m_hTurret1 ) ),	

	SendPropString( SENDINFO( m_SecurityCamLabel1 ) ),
	SendPropString( SENDINFO( m_SecurityCamLabel2 ) ),
	SendPropString( SENDINFO( m_SecurityCamLabel3 ) ),
	SendPropString( SENDINFO( m_TurretLabel1 ) ),
	SendPropString( SENDINFO( m_TurretLabel2 ) ),
	SendPropString( SENDINFO( m_TurretLabel3 ) ),

	SendPropString( SENDINFO( m_MailFile ) ),
	SendPropString( SENDINFO( m_NewsFile ) ),
	SendPropString( SENDINFO( m_StocksSeed ) ),
	SendPropString( SENDINFO( m_WeatherSeed ) ),
	SendPropString( SENDINFO( m_PlantFile ) ),
	SendPropString( SENDINFO( m_PDAName ) ),

	SendPropString( SENDINFO( m_DownloadObjectiveName ) ),
	SendPropBool		(SENDINFO(m_bDownloadedDocs)),

	SendPropBool		(SENDINFO(m_bSecurityCam1Locked)),	
	SendPropBool		(SENDINFO(m_bTurret1Locked)),	
	SendPropBool		(SENDINFO(m_bMailFileLocked)),
	SendPropBool		(SENDINFO(m_bNewsFileLocked)),
	SendPropBool		(SENDINFO(m_bStocksFileLocked)),
	SendPropBool		(SENDINFO(m_bWeatherFileLocked)),
	SendPropBool		(SENDINFO(m_bPlantFileLocked)),	
END_SEND_TABLE()

CASW_Computer_Area::CASW_Computer_Area()
{
	//Msg("[S] CASW_Computer_Area created\n");
	m_hComputerHack = NULL;
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	m_bIsInUse = false;
	m_fHackProgress = 0;
	m_bDownloadedDocs = false;
	m_bDoSecureShout = true;
	m_iActiveCam = 1; // asw temp: should be 0 and get set when the player uses a particular camera
	m_fNextSecureShoutCheck = 0;
	m_flStopUsingTime = 0.0f;

	m_pDownloadingSound = NULL;
	m_pComputerInUseSound = NULL;
	m_fLastPositiveSoundTime = 0;
	m_iAliensKilledBeforeHack = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASW_Computer_Area::Spawn( void )
{
	//CBaseEntity *pTarget;

	BaseClass::Spawn();
	Precache();

	FindTurretsAndCams();

	if ( m_bIsLocked )
	{
		m_bWasLocked = true;
	}

	UpdateWaitingForInput();
	UpdatePanelSkin();
}

void CASW_Computer_Area::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound("ASWComputer.Downloading");
	PrecacheScriptSound("ASWComputer.MenuButton");	
	PrecacheScriptSound("ASWComputer.NumberAligned");
	PrecacheScriptSound("ASWComputer.Loop");
	PrecacheScriptSound("ASWComputer.ColumnTick");
	PrecacheScriptSound("ASWComputer.TimeOut");
}

void CASW_Computer_Area::FindTurretsAndCams()
{
	CBaseEntity* pTarget;
	// find securit cam and turrent entities
	if (m_SecurityCam1Name != NULL_STRING)
	{
		pTarget = gEntList.FindEntityByName( NULL, m_SecurityCam1Name );
		if( pTarget )
			m_hSecurityCam1 = pTarget;
	}	
	if (m_Turret1Name != NULL_STRING)
	{
		pTarget = gEntList.FindEntityByName( NULL, m_Turret1Name );
		if( pTarget )
			m_hTurret1 = pTarget;
	}	
}

void CASW_Computer_Area::Override( CASW_Marine *pMarine )
{
	Assert( pMarine );
	if (m_iHackLevel > 5)						
		pMarine->GetMarineSpeech()->Chatter(CHATTER_HACK_LONG_STARTED);
	else
		pMarine->GetMarineSpeech()->Chatter(CHATTER_HACK_STARTED);
	//  launch the hack puzzle by choosing the special hack option
	GetCurrentHack()->SelectHackOption(ASW_HACK_OPTION_OVERRIDE);
	m_OnComputerHackStarted.FireOutput(pMarine, this);
}

void CASW_Computer_Area::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	if ( !pMarine || !pMarine->GetMarineProfile() )
		return;

	if (m_bIsInUse && pMarine->m_hUsingEntity.Get() == this)
	{
		// check overriding
		if (pMarine->GetMarineProfile()->CanHack() && GetCurrentHack() && m_bIsLocked
				&& GetCurrentHack()->m_iShowOption.Get() != ASW_HACK_OPTION_OVERRIDE)
		{
			Override( pMarine );
			return;
		}
		// check for stopping use of a computer
		//if (m_bIsInUse && UseIconType == USE_ICON_LOG_OFF && pMarine->m_hUsingEntity.Get() == this)
		
		pMarine->StopUsing();
		return;
	}
			
	// player has used this item
	if ( !m_bIsLocked || pMarine->GetMarineProfile()->CanHack() )
	{
		if ( !m_bIsInUse )
		{
			if ( pMarine->StartUsing( this ) )
			{
				if ( !m_bIsLocked )
				{
					pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);
				}
				else
				{
					if ( GetHackProgress() <= 0 && pMarine->GetMarineResource() )
					{
						pMarine->GetMarineResource()->m_iDamageTakenDuringHack = 0;
					}
					m_iAliensKilledBeforeHack = ASWGameResource() ? ASWGameResource()->GetAliensKilledInThisMission() : 0;
				}

				m_OnComputerActivated.FireOutput(pMarine, this);
				m_fAutoOverrideTime = gpGlobals->curtime + 4.0f;

				//if (!asw_simple_hacking.GetBool())		// if doing complex hacking, launch the interface for it
				if ( pMarine->IsInhabited() )
				{
					if (!GetCurrentHack())	// if we haven't created a hack object for this computer yet, then create one
						m_hComputerHack = (CASW_Hack_Computer*) CreateEntityByName( "asw_hack_computer" );
					if (GetCurrentHack())
					{
						//for (int i=0;i<10;i++)
						//{
							GetCurrentHack()->InitHack( pMarine->GetCommander(), pMarine, this );
						//}
					}
				}
			}
		}
		else
		{
			Msg("Panel already in use");
		}		
	}
	else
	{
		// can't hack
		EmitSound("ASWComputer.AccessDenied");

		// check for a nearby AI tech marine
		float flMarineDistance;
		CASW_Marine *pTech = UTIL_ASW_NearestMarine( WorldSpaceCenter(), flMarineDistance, MARINE_CLASS_TECH, true );
		if ( pTech && flMarineDistance < asw_tech_order_hack_range.GetFloat() )
		{
			pTech->OrderHackArea( this );
		}
		return;
	}
}

void CASW_Computer_Area::ActivateUnlockedComputer(CASW_Marine* pMarine)
{
	if ( !pMarine )
		return;

	m_fLastButtonUseTime = gpGlobals->curtime;

	// send our 'activated' output
	m_OnComputerActivated.FireOutput(pMarine, this);

	// Fire event
	IGameEvent * event = gameeventmanager->CreateEvent( "button_area_used" );
	if ( event )
	{
		CASW_Player *pPlayer = pMarine->GetCommander();

		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
		event->SetInt( "entindex", entindex() );
		gameeventmanager->FireEvent( event );
	}
}


CASW_Hack_Computer* CASW_Computer_Area::GetCurrentHack()
{
	return dynamic_cast<CASW_Hack_Computer*>(m_hComputerHack.Get());
}

// traditional Swarm hacking
void CASW_Computer_Area::MarineUsing(CASW_Marine* pMarine, float deltatime)
{
	if ( asw_simple_hacking.GetBool() || !pMarine->IsInhabited() )
	{
		if ( m_bIsInUse && HasDownloadObjective() && GetHackProgress() < 1.0f )
		{
			float flOldHackProgress = m_fHackProgress;
			float fTime = (deltatime * (1.0f/((float)m_iHackLevel)));
			// boost fTime by the marine's hack skill
			fTime *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE);
			fTime *= asw_ai_computer_hacking_scale.GetFloat();
			m_fHackProgress += fTime;

			if ( GetHackProgress() > 0.0f && flOldHackProgress == 0.0f )
			{
				m_OnComputerHackStarted.FireOutput( pMarine, this );
			}
			if ( GetHackProgress() >= 0.3f && flOldHackProgress < 0.3f )		// fire at first 3rd for AI marines, since we have the extra download step after hacking is complete
			{
				m_OnComputerHackHalfway.FireOutput( pMarine, this );
			}
			if ( GetHackProgress() >= 0.6f && flOldHackProgress < 0.6f )
			{
				m_OnComputerHackCompleted.FireOutput( pMarine, this );
			}
			
			if ( m_fHackProgress >= 1.0f )
			{
				m_fHackProgress = 1.0f;
				
				pMarine->StopUsing();

				if (GetCurrentHack())
					GetCurrentHack()->OnHackComplete();

				// unlock it
				m_OnComputerHackCompleted.FireOutput(pMarine, this);
				m_bIsLocked = false;
				UpdateWaitingForInput();
				UpdatePanelSkin();

				// if set to use on unlock, then do so
				if ( m_bUseAfterHack )
				{
					ActivateUnlockedComputer(pMarine);
				}

				OnComputerDataDownloaded( pMarine );

				StopDownloadingSound();				
				EmitSound("ASWComputer.MenuButton");
			}
		}
	}
	else if (GetCurrentHack())
	{
		if (GetCurrentHack()->IsDownloadingFiles() && m_fHackProgress < 1.0f)
		{
			float fTime = (deltatime * (1.0f/((float)m_fDownloadTime)));
			// boost fTime by the marine's hack skill
			bool bTech = (pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanHack());
			if (bTech)
				fTime *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE);
			else
				fTime *= 0.5f;
			m_fHackProgress += fTime;
			if (m_fHackProgress >= 1.0f)
			{
				m_fHackProgress = 1.0f;
								
				// finish downloading the files
				OnComputerDataDownloaded( pMarine );

				StopDownloadingSound();				
				EmitSound("ASWComputer.MenuButton");				
			}
			else
			{
				StartDownloadingSound();
			}
		}

		if ( pMarine && m_flStopUsingTime > 0.0f && gpGlobals->curtime > m_flStopUsingTime )
		{
			pMarine->StopUsing();
			m_flStopUsingTime = 0.0f;
		}
	}
}

void CASW_Computer_Area::MarineStartedUsing(CASW_Marine* pMarine)
{
	m_flStopUsingTime = 0.0f;
	m_bIsInUse = true;
	UpdateWaitingForInput();

	const char *pszSound = "ASWComputer.Loop";
	CPASAttenuationFilter filter( this );
	m_pComputerInUseSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, pszSound, ATTN_NORM );

	CSoundEnvelopeController::GetController().Play( m_pComputerInUseSound, 0.1, 100 );
	if (m_pComputerInUseSound)
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pComputerInUseSound, 1, 1.5 );
	}

	if ( ASWDirector() && m_bIsLocked )
	{
		ASWDirector()->OnMarineStartedHack( pMarine, this );
	}
}

void CASW_Computer_Area::MarineStoppedUsing(CASW_Marine* pMarine)
{
	m_bIsInUse = false;
	UpdateWaitingForInput();

	if (GetCurrentHack())	// notify our current hack that we've stopped using the console
	{
		GetCurrentHack()->MarineStoppedUsing(pMarine);
	}
	if (m_pComputerInUseSound)
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pComputerInUseSound );
		m_pComputerInUseSound = NULL;
	}
	StopDownloadingSound();
	CPointCamera* pCam = dynamic_cast<CPointCamera*>(m_hSecurityCam1.Get());
	if (pCam)
	{
		pCam->SetActive(false);
	}
}

void CASW_Computer_Area::StartDownloadingSound()
{
	if( !m_pDownloadingSound )
	{
		Msg("Starting downloading sound\n");
		// Don't set this up until the code calls for it.
		const char *pszSound = "ASWComputer.Downloading"; //GetMoanSound( m_iMoanSound );

		CPASAttenuationFilter filter( this );
		m_pDownloadingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, pszSound, ATTN_NORM );

		CSoundEnvelopeController::GetController().Play( m_pDownloadingSound, 0.1, 100 );
		if (m_pDownloadingSound)
		{
			//CSoundEnvelopeController::GetController().SoundChangePitch( m_pDownloadingSound, 120, 1.0 );
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pDownloadingSound, 1, 1.5 );
		}
	}
}

void CASW_Computer_Area::StopDownloadingSound()
{
	if (m_pDownloadingSound)
	{		
		CSoundEnvelopeController::GetController().SoundDestroy( m_pDownloadingSound );
		m_pDownloadingSound = NULL;
	}
}

bool CASW_Computer_Area::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "MailFile" ) )
	{
		Q_strncpy( m_MailFile.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "NewsFile" ) )
	{
		Q_strncpy( m_NewsFile.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "StocksSeed" ) )
	{
		Q_strncpy( m_StocksSeed.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "WeatherSeed" ) )
	{
		Q_strncpy( m_WeatherSeed.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "PlantFile" ) )
	{
		Q_strncpy( m_PlantFile.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "PDAName" ) )
	{
		Q_strncpy( m_PDAName.GetForModify(), szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "SecurityCam1Label" ) )
	{
		Q_strncpy( m_SecurityCamLabel1.GetForModify(), szValue, 255 );
		return true;
	}	
	if ( FStrEq( szKeyName, "DownloadObjectiveName" ) )
	{
		Q_strncpy( m_DownloadObjectiveName.GetForModify(), szValue, 255 );
		return true;
	}
	return BaseClass::KeyValue( szKeyName, szValue );
}

void CASW_Computer_Area::OnComputerDataDownloaded( CASW_Marine *pMarine )
{
	// check haven't already downloaded them
	if (m_bDownloadedDocs)
		return;
	// flag objective as complete
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_DownloadObjectiveName.Get());
	if (pEntity)
	{
		CASW_Objective_Triggered* pObj = dynamic_cast<CASW_Objective_Triggered*>(pEntity);
		if (pObj)
		{
			pObj->SetComplete(true);
		}
	}

	if ( pMarine && pMarine->GetMarineResource() && pMarine->GetMarineResource()->m_iDamageTakenDuringHack <= 0 )
	{
		int nAliensKilled = ASWGameResource() ? ASWGameResource()->GetAliensKilledInThisMission() : 0;
		if ( ( nAliensKilled - m_iAliensKilledBeforeHack ) > 10 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
			{
				CASW_Player* pPlayer = dynamic_cast<CASW_Player*>( UTIL_PlayerByIndex( i ) );
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

	// fire output
	m_OnComputerDataDownloaded.FireOutput(this, this);
	m_bDownloadedDocs = true;	

	// auto close the computer if it only contained a download
	if ( GetNumMenuOptions() <= 1 && pMarine )
	{
		m_flStopUsingTime = gpGlobals->curtime + 1.0f;
	}
}

void CASW_Computer_Area::OnViewMail(CASW_Marine *pMarine, int iMail)
{
	bool bIsPDA = (m_PDAName.Get()[0] != 0);
	if (!pMarine || iMail < 1 || iMail > 4 || (!m_bViewingMail && !bIsPDA))
		return;

	switch (iMail)
	{
		case 4:	m_OnComputerViewMail4.FireOutput(pMarine, this);	break;
		case 3:	m_OnComputerViewMail3.FireOutput(pMarine, this);	break;
		case 2:	m_OnComputerViewMail2.FireOutput(pMarine, this);	break;
		case 1:	default: m_OnComputerViewMail1.FireOutput(pMarine, this);	break;
	}
}

// computer has been unlocked from the hacking puzzle
void CASW_Computer_Area::UnlockFromHack(CASW_Marine *pMarine)
{
	if (m_bIsLocked)
	{
		m_bIsLocked = false;

		UpdateWaitingForInput();
		UpdatePanelSkin();

		bool bFast = false;
		if ( pMarine )
		{			
			// was this a fast hack?
			if (pMarine->GetMarineResource() && GetCurrentHack())
			{
				if (gpGlobals->curtime <= GetCurrentHack()->m_fFastFinishTime
						&& GetCurrentHack()->m_iNumTumblers.Get() >= ASW_MIN_TUMBLERS_FAST_HACK)	// has to be more than x tumblers to quality for the medal
				{
					pMarine->GetMarineResource()->m_iFastComputerHacks++;
					if ( pMarine->IsInhabited() && pMarine->GetCommander() )
					{
						pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_FAST_COMPUTER_HACKS );
					}
					bFast = true;
					pMarine->GetMarineSpeech()->QueueChatter(CHATTER_HACK_FINISHED, gpGlobals->curtime + 2.0f, gpGlobals->curtime + 3.0f);
				}
			}
		}
		m_OnComputerHackCompleted.FireOutput(pMarine, this);
	}
}

// hack puzzle has 50% or more of the tumblers correct
void CASW_Computer_Area::HackHalfway(CASW_Marine *pMarine)
{
	if (pMarine && !m_bPlayedHalfwayChatter)
	{
		pMarine->GetMarineSpeech()->Chatter(CHATTER_HACK_HALFWAY);
		m_bPlayedHalfwayChatter = true;
	}
	m_OnComputerHackHalfway.FireOutput(pMarine, this);
}

void CASW_Computer_Area::ActivateMultiTrigger(CBaseEntity *pActivator)
{
	if (GetNextThink() > gpGlobals->curtime)
		return;         // still waiting for reset time

	BaseClass::ActivateMultiTrigger(pActivator);

	// check for shouting out about a locked computer
	if (m_bDoSecureShout && m_bIsLocked)
	{
		if (gpGlobals->curtime > m_fNextSecureShoutCheck)
		{
			CASW_Marine *pMarine = CASW_Marine::AsMarine( pActivator );
			if ( pMarine )
			{
				// techs doesn't call for help, cos he's got the skillz already
				if (pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanHack())
					return;
				// check if there's a hacker nearby
				CASW_Game_Resource *pGameResource = ASWGameResource();
				if (pGameResource)
				{
					int iFound = 0;
					for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pActivator != pOtherMarine
									&& (pMarine->GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 800)
									&& pOtherMarine->GetMarineProfile() && pOtherMarine->GetMarineProfile()->CanHack() )
						{
							iFound++;
						}
					}
					if (iFound <= 0)
					{
						m_fNextSecureShoutCheck = gpGlobals->curtime + 10.0f;
						return;
					}

					int iChosen = random->RandomInt(0, iFound-1);
					for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
					{
						CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
						CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
						if (pOtherMarine && pActivator != pOtherMarine
									&& (pMarine->GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 800)
									&& pOtherMarine->GetMarineProfile() && pOtherMarine->GetMarineProfile()->CanHack() )
						{
							if (iChosen <= 0)
							{
								int iChatter = CHATTER_LOCKED_TERMINAL;
								// shout to this marine
								if (random->RandomFloat() < 0.5f)	// do a specific call 50% of the time
								{
									if (pOtherMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH)
										iChatter = CHATTER_LOCKED_TERMINAL_CRASH;
									else if (pOtherMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_FLYNN)
										iChatter = CHATTER_LOCKED_TERMINAL_FLYNN;
									else if (pOtherMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_VEGAS)
										iChatter = CHATTER_LOCKED_TERMINAL_VEGAS;
								}
	
								pMarine->GetMarineSpeech()->QueueChatter(iChatter, gpGlobals->curtime + 0.8f, gpGlobals->curtime + 1.0f);
								m_bDoSecureShout = false;								
								//m_fNextSecureShoutCheck = gpGlobals->curtime + 2.0f;	// check again sooner if we tried to say the line but couldn't for some reason
								return;
							}
							iChosen--;
						}
					}
					m_fNextSecureShoutCheck = gpGlobals->curtime + 10.0f;
				}				
			}
		}
	}
}

void CASW_Computer_Area::StopLoopingSounds(void)
{
	BaseClass::StopLoopingSounds();
	
	CSoundEnvelopeController::GetController().SoundDestroy( m_pDownloadingSound );
	m_pDownloadingSound = NULL;
	CSoundEnvelopeController::GetController().SoundDestroy( m_pComputerInUseSound );
	m_pComputerInUseSound = NULL;
}

void CASW_Computer_Area::PlayPositiveSound(CASW_Player *pHackingPlayer)
{
	if (pHackingPlayer && 
		gpGlobals->curtime > m_fLastPositiveSoundTime + 0.6f)
	{
		m_fLastPositiveSoundTime = gpGlobals->curtime;
		CPASFilter filter( GetAbsOrigin() );
		if (gpGlobals->maxClients > 1)	// in multiplayer games, the hacking client will play this sounds clientside
			filter.RemoveRecipient( pHackingPlayer );
		EmitSound(filter, entindex(), "ASWComputer.NumberAligned");
	}
}

void CASW_Computer_Area::PlayNegativeSound(CASW_Player *pHackingPlayer)
{
	// none atm
}

bool CASW_Computer_Area::WaitingForInputVismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer )
{
	CASW_Computer_Area *pComputerArea = static_cast< CASW_Computer_Area* >( pVisibleEntity );
	return pComputerArea->m_bWaitingForInput;
}

bool CASW_Computer_Area::WaitingForInputVismonCallback( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer )
{
	CASW_Computer_Area *pComputerArea = static_cast< CASW_Computer_Area* >( pVisibleEntity );
	IGameEvent * event = gameeventmanager->CreateEvent( "button_area_active" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "entindex", pComputerArea->entindex() );
		event->SetInt( "prop", pComputerArea->m_hPanelProp.Get() ? pComputerArea->m_hPanelProp.Get()->entindex() : pComputerArea->entindex() );
		event->SetBool( "locked", pComputerArea->IsLocked() );
		gameeventmanager->FireEvent( event );
	}

	return false;
}

void CASW_Computer_Area::UpdateWaitingForInput()
{
	bool bOldWaitingForInput = m_bWaitingForInput;

	m_bWaitingForInput = ASWGameRules()->GetGameState() == ASW_GS_INGAME && 
						 ( !m_bIsInUse && ( m_bIsLocked || ( !m_bWasLocked && m_DownloadObjectiveName.Get()[ 0 ] != '\0' && m_fLastButtonUseTime == 0 ) ) );

	if ( !bOldWaitingForInput && m_bWaitingForInput )
	{
		VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, &CASW_Computer_Area::WaitingForInputVismonCallback, &CASW_Computer_Area::WaitingForInputVismonEvaluator );
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
void CASW_Computer_Area::UpdatePanelSkin()
{
	if (!m_hPanelProp.Get())
		return;

	CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>(m_hPanelProp.Get());
	while (pAnim)
	{
		if (m_bIsLocked)
			pAnim->m_nSkin = 0;	// locked skin
		else
			pAnim->m_nSkin = 1;	// unlocked skin	

		if (m_bMultiplePanelProps)
			pAnim = dynamic_cast<CBaseAnimating*>(gEntList.FindEntityByName( pAnim, m_szPanelPropName ));
		else
			pAnim = NULL;
	}
}

int CASW_Computer_Area::GetNumMenuOptions()
{
	int n=0;

	if (m_DownloadObjectiveName.Get()[0] != 0 && GetHackProgress() < 1.0f) n++;
	if (m_MailFile.Get()[0] != 0) n++;
	if (m_NewsFile.Get()[0] != 0) n++;
	if (m_StocksSeed.Get()[0] != 0) n++;
	if (m_WeatherSeed.Get()[0] != 0) n++;
	if (m_PlantFile.Get()[0] != 0) n++;

	if (m_hSecurityCam1.Get() != NULL) n++;	
	if (m_hTurret1.Get() != NULL) n++;	
	
	if (n > 6)	// clamp it to 6 options, since that's all our UI supports
		n = 6;

	return n;
}

bool CASW_Computer_Area::HasDownloadObjective()
{
	return ( m_DownloadObjectiveName.Get()[0] != 0 );
}