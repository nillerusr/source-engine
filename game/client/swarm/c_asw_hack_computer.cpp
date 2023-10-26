#include "cbase.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include "asw_vgui_computer_frame.h"
#include "asw_vgui_computer_menu.h"
#include "asw_vgui_frame.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include "vgui_controls/frame.h"
#include "iclientmode.h"
#include <vgui/IScheme.h>
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Hack_Computer, DT_ASW_Hack_Computer, CASW_Hack_Computer)		
	RecvPropInt		(RECVINFO(m_iNumTumblers)),
	RecvPropInt		(RECVINFO(m_iEntriesPerTumbler)),	
	RecvPropBool	(RECVINFO(m_bLastAllCorrect)),
	RecvPropFloat	(RECVINFO(m_fMoveInterval)),
	RecvPropFloat	(RECVINFO(m_fNextMoveTime)),	
	RecvPropFloat	(RECVINFO(m_fFastFinishTime)),
	RecvPropArray3( RECVINFO_ARRAY(m_iTumblerPosition), RecvPropInt( RECVINFO(m_iTumblerPosition[0]))),	
	RecvPropArray3( RECVINFO_ARRAY(m_iTumblerCorrectNumber), RecvPropInt( RECVINFO(m_iTumblerCorrectNumber[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iTumblerDirection), RecvPropInt( RECVINFO(m_iTumblerDirection[0]))),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Hack_Computer )	
	DEFINE_PRED_FIELD( m_fNextMoveTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),	
	DEFINE_PRED_FIELD( m_bLastAllCorrect, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),	
	DEFINE_PRED_ARRAY( m_iTumblerDirection, FIELD_INTEGER,  ASW_HACK_COMPUTER_MAX_TUMBLERS, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_iTumblerPosition, FIELD_INTEGER,  ASW_HACK_COMPUTER_MAX_TUMBLERS, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

C_ASW_Hack_Computer::C_ASW_Hack_Computer()
{
	m_hFrame = NULL;
	m_hComputerFrame = NULL;
	m_fTumblerDiffTime = -1;
	m_bLaunchedHackPanel = false;
	m_iShowOption = 0;
	m_iOldShowOption = 0;
	m_bLastAllCorrect = false;
	m_fStartedHackTime = 0;
	SetPredictionEligible( true );
	for (int i=0;i<ASW_HACK_COMPUTER_MAX_TUMBLERS;i++)
	{		
		m_iNewTumblerDirection[i] = 0;
		m_iNewTumblerPosition[i] = -1;
	}
}

C_ASW_Hack_Computer::~C_ASW_Hack_Computer()
{
	if (m_hFrame.Get() && m_hFrame->m_pNotifyHackOnClose == this)
		m_hFrame->m_pNotifyHackOnClose = NULL;

	ASWInput()->SetCameraFixed( false );
}

void C_ASW_Hack_Computer::FrameDeleted(vgui::Panel *pPanel)
{
	if (pPanel == m_hFrame.Get())
	{
		m_hFrame = NULL;
	}

	ASWInput()->SetCameraFixed( false );
}

void C_ASW_Hack_Computer::ClientThink()
{
	HACK_GETLOCALPLAYER_GUARD( "Need to support launching multiple hack panels on one machine (1 for each splitscreen player) for this to make sense." );
	if (m_bLaunchedHackPanel && GetHackerMarineResource() == NULL)	// if we've launched our hack window, but the hack has lost its hacking marine, then close our window down
	{
		m_bLaunchedHackPanel = false;
		m_iOldShowOption = 0;	// reset our option
		if (m_hFrame.Get())
		{
			m_hFrame->SetVisible(false);
			m_hFrame->MarkForDeletion();
			m_hFrame = NULL;

			ASWInput()->SetCameraFixed( false );
		}
	}
	// if we haven't launched the window and data is all present, launch it
	if (GetHackerMarineResource() != NULL)
	{
		if (!m_bLaunchedHackPanel)
		{
			if ( C_BasePlayer::IsLocalPlayer( GetHackerMarineResource()->GetCommander() ) )
			{
				vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");

				if (GetComputerArea() && GetComputerArea()->IsPDA())
					m_hFrame = new CASW_VGUI_Computer_Container( GetClientMode()->GetViewport(), "ComputerContainer", "#asw_syntek_pda" );
				else
					m_hFrame = new CASW_VGUI_Computer_Container( GetClientMode()->GetViewport(), "ComputerContainer", "#asw_terminal_access" );
				
				m_hFrame->SetScheme(scheme);								

				m_hComputerFrame = new CASW_VGUI_Computer_Frame( m_hFrame.Get(), "VGUIComputerFrame", this );			
				m_hComputerFrame->SetScheme(scheme);	
				m_hComputerFrame->ASWInit();

				m_hFrame->MoveToFront();
				m_hFrame->RequestFocus();
				m_hFrame->SetVisible(true);
				m_hFrame->SetEnabled(true);			
				
				m_bLaunchedHackPanel = true;
			}
		}
		else
		{
			if (m_iOldShowOption != m_iShowOption)
			{
				if (m_fStartedHackTime == 0 && m_iShowOption == ASW_HACK_OPTION_OVERRIDE)
					m_fStartedHackTime = gpGlobals->curtime;
				Msg("C_ASW_Hack_Computer calling sethackoption as m_iShowOption = %d and m_iOldShowOption = %d\n", 
					m_iShowOption, m_iOldShowOption);
				if (m_hComputerFrame.Get())
					m_hComputerFrame->SetHackOption(m_iShowOption);
				m_iOldShowOption = m_iShowOption;
			}			
		}
	}
	// check for hiding the panel if the player has a different marine selected, or if the selected marine is remote controlling a turret
	if (m_bLaunchedHackPanel && GetHackerMarineResource() && m_hFrame.Get())
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();		
		if (!pPlayer)
		{
			if (m_hFrame->IsVisible())
			{
				ASWInput()->SetCameraFixed( false );
				m_hFrame->SetVisible(false);
				//C_BaseEntity::StopSound(-1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Loop");
			}
		}
		else
		{
			bool bLocalPlayerControllingHacker = (GetHackerMarineResource()->IsInhabited() && GetHackerMarineResource()->GetCommanderIndex() == pPlayer->entindex());
			bool bMarineControllingTurret = (GetHackerMarineResource()->GetMarineEntity() && GetHackerMarineResource()->GetMarineEntity()->IsControllingTurret());

			if ( bLocalPlayerControllingHacker && !bMarineControllingTurret )
			{
				ASWInput()->SetCameraFixed( true );

				if ( !m_hFrame->IsVisible() )
				{
					m_hFrame->SetVisible(true);
					//CLocalPlayerFilter filter;
					//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Loop" );
				}
			}
			else
			{
				ASWInput()->SetCameraFixed( false );

				if ( m_hFrame->IsVisible() )
				{
					m_hFrame->SetVisible(false);
					//C_BaseEntity::StopSound(-1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Loop");
				}
			}
		}
	}
}

void C_ASW_Hack_Computer::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		return;
	}
}

// predicted if the marine hacking us is inhabited locally
bool C_ASW_Hack_Computer::ShouldPredict()
{
	C_ASW_Marine_Resource * RESTRICT pResource = GetHackerMarineResource();
	return ( pResource && pResource->IsInhabited() && pResource->IsLocal() );
}

// this returns if we can override the security (checks if the backdrop of the window is red, i.e. is the access denied screen up)
//  also allows overriding during the splash screen
bool C_ASW_Hack_Computer::CanOverrideHack()
{
	if (m_hComputerFrame.Get())
	{
		if (GetComputerArea() && GetComputerArea()->IsLocked())
		{
			if (m_hComputerFrame->m_iBackdropType == 1 || m_hComputerFrame->m_bPlayingSplash)
			{
				if (m_hComputerFrame->m_pMenuPanel && m_hComputerFrame->m_pMenuPanel->IsHacking())	// already hacking
					return false;
				return true;
			}
		}
	}
	return false;
}

int C_ASW_Hack_Computer::GetTumblerPosition(int i)
{
	if (i < 0 || i >= ASW_HACK_COMPUTER_MAX_TUMBLERS)
		return 0;
	
	if (m_iNewTumblerPosition[i] != -1)
		return m_iNewTumblerPosition[i];

	return m_iTumblerPosition[i];
}

float C_ASW_Hack_Computer::GetTumblerDiffTime()
{
	if (m_fTumblerDiffTime != -1)
	{
		// version used for prediction
		float fDiff = m_fTumblerDiffTime / 0.5f;
		if (fDiff >=1.0f)
			fDiff = 0;
		return fDiff;
	}

	float fDiff = (gpGlobals->curtime - GetNextMoveTime()) / 0.5f;
	if (fDiff > 1.0f)
		fDiff = 1.0f;
	return fDiff;
}