//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VAudio.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "ivoicetweak.h"
#include "materialsystem/materialsystem_config.h"
#include "cdll_util.h"
#include "nb_header_footer.h"
#include "vgui_controls/ImagePanel.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define VIDEO_LANGUAGE_COMMAND_PREFIX "_language"


using namespace vgui;
using namespace BaseModUI;


int GetScreenAspectMode( int width, int height );
void SetFlyoutButtonText( const char *pchCommand, FlyoutMenu *pFlyout, const char *pchNewText );


// This member is static so that the updated audio language can be referenced during shutdown
const char* Audio::m_pchUpdatedAudioLanguage = "";

extern ConVar ui_gameui_modal;

//=============================================================================
Audio::Audio(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	if ( ui_gameui_modal.GetBool() )
	{
		GameUI().PreventEngineHideGameUI();
	}

#if !defined( NO_VOICE )
	m_pVoiceTweak = engine->GetVoiceTweakAPI();
#else
	m_pVoiceTweak = NULL;
#endif

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( true );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 60, 340 );

	SetDeleteSelfOnClose(true);

	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_sldGameVolume = NULL;
	m_sldMusicVolume = NULL;
	m_drpSpeakerConfiguration = NULL;
	m_drpSoundQuality = NULL;
	m_drpLanguage = NULL;
	m_drpCaptioning = NULL;
	m_drpVoiceCommunication = NULL;
	m_sldTransmitVolume = NULL;
	m_sldRecieveVolume = NULL;
	m_drpBoostMicrophoneGain = NULL;
	m_btnTestMicrophone = NULL;
	m_sldVoiceThreshold = NULL;
	m_drpVoiceCommunicationStyle = NULL;

	m_pMicMeter = NULL;
	m_pMicMeter2 = NULL;
	m_pMicMeterIndicator = NULL;

	m_btnCancel = NULL;

	m_btn3rdPartyCredits = NULL;

	m_nSelectedAudioLanguage = k_Lang_None;
	m_nCurrentAudioLanguage = k_Lang_None;

	m_nNumAudioLanguages = 0;
}

//=============================================================================
Audio::~Audio()
{
	GameUI().AllowEngineHideGameUI();

	EndTestMicrophone();

	UpdateFooter( false );

	if ( m_pchUpdatedAudioLanguage[ 0 ] != '\0' )
	{
		PostMessage( &(CBaseModPanel::GetSingleton()), new KeyValues( "command", "command", "RestartWithNewLanguage" ), 0.01f );
	}
}

//=============================================================================
void Audio::Activate()
{
	BaseClass::Activate();

	if ( m_sldGameVolume )
	{
		m_sldGameVolume->Reset();
	}

	if ( m_sldMusicVolume )
	{
		m_sldMusicVolume->Reset();
	}

	if ( m_sldVoiceThreshold )
	{
		m_sldVoiceThreshold->Reset();
	}

	if ( m_drpSpeakerConfiguration )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");

		switch ( snd_surround_speakers.GetInt() )
		{
		case 2:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_2Speakers" );
			break;
		case 4:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_4Speakers" );
			break;
		case 5:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_5Speakers" );
			break;
		case 0:
		default:
			m_drpSpeakerConfiguration->SetCurrentSelection( "#GameUI_Headphones" );
			break;
		}

		FlyoutMenu *pFlyout = m_drpSpeakerConfiguration->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpSoundQuality )
	{
		CGameUIConVarRef Snd_PitchQuality("Snd_PitchQuality");
		CGameUIConVarRef dsp_slow_cpu("dsp_slow_cpu");

		int quality = 0;
		if ( dsp_slow_cpu.GetBool() == false )
		{
			quality = 1;
		}
		if ( Snd_PitchQuality.GetBool() )
		{
			quality = 2;
		}

		switch ( quality )
		{
		case 1:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_Medium" );
			break;
		case 2:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_High" );
			break;
		case 0:
		default:
			m_drpSoundQuality->SetCurrentSelection( "#GameUI_Low" );
			break;
		}

		FlyoutMenu *pFlyout = m_drpSoundQuality->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpCaptioning )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");

		if ( !closecaption.GetBool() )
		{
			m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionOff" );
		}
		else
		{
			if ( cc_subtitles.GetBool() )
			{
				m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionSubtitles" );
			}
			else
			{
				m_drpCaptioning->SetCurrentSelection( "#L4D360UI_AudioOptions_CaptionOn" );
			}
		}

		FlyoutMenu *pFlyout = m_drpCaptioning->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpLanguage )
	{
		char szCurrentLanguage[50] = "";
		char szAvailableLanguages[512] = "";
		szAvailableLanguages[0] = '\0';

		// Fallback to current engine language
		engine->GetUILanguage( szCurrentLanguage, sizeof( szCurrentLanguage ));

		// In a Steam environment we get the current language 
#if !defined( NO_STEAM )
		// When Steam isn't running we can't get the language info... 
		if ( steamapicontext->SteamApps() )
		{
			Q_strncpy( szCurrentLanguage, steamapicontext->SteamApps()->GetCurrentGameLanguage(), sizeof(szCurrentLanguage) );
			Q_strncpy( szAvailableLanguages, steamapicontext->SteamApps()->GetAvailableGameLanguages(), sizeof(szAvailableLanguages) );
		}
#endif

		// Get the spoken language and store it for comparison purposes
		m_nCurrentAudioLanguage = PchLanguageToELanguage( szCurrentLanguage );

		// Set up the base string for each button command
		char szCurrentButton[ 32 ];
		Q_strncpy( szCurrentButton, VIDEO_LANGUAGE_COMMAND_PREFIX, sizeof( szCurrentButton ) );

		int iCommandNumberPosition = Q_strlen( szCurrentButton );
		szCurrentButton[ iCommandNumberPosition + 1 ] = '\0';

		// Check to see if we have a list of languages from Steam
		if ( szAvailableLanguages[0] != '\0' )
		{
			int iSelectedLanguage = 0;

			FlyoutMenu *pLanguageFlyout = m_drpLanguage->GetCurrentFlyout();

			// Loop through all the languages
			CSplitString languagesList( szAvailableLanguages, "," );

			for ( m_nNumAudioLanguages = 0; m_nNumAudioLanguages < languagesList.Count() && m_nNumAudioLanguages < MAX_DYNAMIC_AUDIO_LANGUAGES; ++m_nNumAudioLanguages )
			{
				szCurrentButton[ iCommandNumberPosition ] = m_nNumAudioLanguages + '0';
				m_nAudioLanguages[ m_nNumAudioLanguages ].languageCode = PchLanguageToELanguage( languagesList[ m_nNumAudioLanguages ] );

				SetFlyoutButtonText( szCurrentButton, pLanguageFlyout, GetLanguageVGUILocalization( m_nAudioLanguages[ m_nNumAudioLanguages ].languageCode ) );

				if ( m_nCurrentAudioLanguage == m_nAudioLanguages[ m_nNumAudioLanguages ].languageCode )
				{
					iSelectedLanguage = m_nNumAudioLanguages;
				}
			}

			// Change the height to fit the active items
			pLanguageFlyout->SetBGTall( m_nNumAudioLanguages * 20 + 5 );

			// Disable the remaining possible choices
			for ( int i = m_nNumAudioLanguages; i < MAX_DYNAMIC_AUDIO_LANGUAGES; ++i )
			{
				szCurrentButton[ iCommandNumberPosition ] = i + '0';

				Button *pButton = pLanguageFlyout->FindChildButtonByCommand( szCurrentButton );
				if ( pButton )
				{
					pButton->SetVisible( false );
				}
			}

			// Set the current selection
			szCurrentButton[ iCommandNumberPosition ] = iSelectedLanguage + '0';

			m_drpLanguage->SetCurrentSelection( szCurrentButton );

			m_nSelectedAudioLanguage = m_nAudioLanguages[ iSelectedLanguage ].languageCode;
		}

		m_drpLanguage->SetVisible( m_nNumAudioLanguages > 1 );
	}

	if ( !m_pVoiceTweak )
	{
		SetControlVisible( "DrpVoiceCommunication", false );
		SetControlVisible( "SldVoiceTransmitVolume", false );
		SetControlVisible( "SldVoiceReceiveVolume", false );
		SetControlVisible( "DrpBoostMicrophone", false );
		SetControlVisible( "BtnTestMicrophone", false );
		SetControlVisible( "MicMeter", false );
		SetControlVisible( "MicMeter2", false );
	}
	else
	{
		CGameUIConVarRef voice_modenable("voice_modenable");
		CGameUIConVarRef voice_enable("voice_enable");

		bool bVoiceEnabled = voice_enable.GetBool() && voice_modenable.GetBool();

		if ( m_drpVoiceCommunication )
		{
			if ( bVoiceEnabled )
			{
				m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_Enabled" );
			}
			else
			{
				m_drpVoiceCommunication->SetCurrentSelection( "#L4D360UI_Disabled" );
			}

			FlyoutMenu *pFlyout = m_drpVoiceCommunication->GetCurrentFlyout();
			if ( pFlyout )
			{
				pFlyout->SetListener( this );
			}
		}

		CGameUIConVarRef voice_vox("voice_vox");

		if ( m_drpVoiceCommunicationStyle )
		{
			m_drpVoiceCommunicationStyle->SetEnabled( bVoiceEnabled );

			if ( voice_vox.GetBool() )
			{
				m_drpVoiceCommunicationStyle->SetCurrentSelection( "#L4D360UI_OpenMic" );
				SetControlEnabled( "SldVoiceVoxThreshold", bVoiceEnabled );
			}
			else
			{
				m_drpVoiceCommunicationStyle->SetCurrentSelection( "#L4D360UI_PushToTalk" );
				SetControlEnabled( "SldVoiceVoxThreshold", false );
			}

			FlyoutMenu *pFlyout = m_drpVoiceCommunicationStyle->GetCurrentFlyout();
			if ( pFlyout )
			{
				pFlyout->SetListener( this );
			}
		}

		if ( m_sldTransmitVolume )
		{
			bool bMicVolumeFound = m_pVoiceTweak->IsControlFound( MicrophoneVolume );
			float micVolume = m_pVoiceTweak->GetControlFloat( MicrophoneVolume );
			m_sldTransmitVolume->SetCurrentValue( (int)( 100.0f * micVolume ) );
			m_sldTransmitVolume->ResetSliderPosAndDefaultMarkers();
			m_sldTransmitVolume->SetEnabled( bVoiceEnabled && bMicVolumeFound );
		}

		if ( m_sldRecieveVolume )
		{
			float flRecVolume = m_pVoiceTweak->GetControlFloat( OtherSpeakerScale );
			m_sldRecieveVolume->Reset();
			m_sldRecieveVolume->SetCurrentValue( flRecVolume );
			m_sldRecieveVolume->SetEnabled( bVoiceEnabled );
		}

		if ( m_drpBoostMicrophoneGain )
		{
			float fMicBoost = m_pVoiceTweak->GetControlFloat( MicBoost );

			if ( fMicBoost != 0.0f )
			{
				m_drpBoostMicrophoneGain->SetCurrentSelection( "#L4D360UI_Enabled" );
			}
			else
			{
				m_drpBoostMicrophoneGain->SetCurrentSelection( "#L4D360UI_Disabled" );
			}

			m_drpBoostMicrophoneGain->SetEnabled( bVoiceEnabled );

			FlyoutMenu *pFlyout = m_drpBoostMicrophoneGain->GetCurrentFlyout();
			if ( pFlyout )
			{
				pFlyout->SetListener( this );
			}
		}

		if ( m_pMicMeter )
		{
			m_pMicMeter->SetVisible( bVoiceEnabled );
		}

		if ( m_pMicMeter2 )
		{
			m_pMicMeter2->SetVisible( false );
		}

		if ( m_pMicMeterIndicator )
		{
			m_pMicMeterIndicator->SetVisible( false );
		}
	}

	UpdateFooter( true );

	if ( m_sldGameVolume )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom( );
		m_sldGameVolume->NavigateTo();
		m_ActiveControl = m_sldGameVolume;
	}
}

void Audio::OnThink()
{
	BaseClass::OnThink();

	bool needsActivate = false;

	if( !m_sldGameVolume )
	{
		m_sldGameVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldGameVolume" ) );
		needsActivate = true;
	}

	if( !m_sldMusicVolume )
	{
		m_sldMusicVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldMusicVolume" ) );
		needsActivate = true;
	}

	if( !m_sldVoiceThreshold )
	{
		m_sldVoiceThreshold = dynamic_cast< SliderControl* >( FindChildByName( "SldVoiceVoxThreshold" ) );
		needsActivate = true;
	}

	if( !m_drpSpeakerConfiguration )
	{
		m_drpSpeakerConfiguration = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpSpeakerConfiguration" ) );
		needsActivate = true;
	}

	if( !m_drpSoundQuality )
	{
		m_drpSoundQuality = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpSoundQuality" ) );
		needsActivate = true;
	}

	if( !m_drpLanguage )
	{
		m_drpLanguage = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpLanguage" ) );
		needsActivate = true;
	}

	if( !m_drpCaptioning )
	{
		m_drpCaptioning = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpCaptioning" ) );
		needsActivate = true;
	}

	if( !m_drpVoiceCommunication )
	{
		m_drpVoiceCommunication = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpVoiceCommunication" ) );
		needsActivate = true;
	}

	if( !m_drpVoiceCommunicationStyle )
	{
		m_drpVoiceCommunicationStyle = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpVoiceCommunicationStyle" ) );
		needsActivate = true;
	}

	if( !m_sldTransmitVolume )
	{
		m_sldTransmitVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldVoiceTransmitVolume" ) );
		needsActivate = true;
	}

	if( !m_sldRecieveVolume )
	{
		m_sldRecieveVolume = dynamic_cast< SliderControl* >( FindChildByName( "SldVoiceReceiveVolume" ) );
		needsActivate = true;
	}

	if( !m_drpBoostMicrophoneGain )
	{
		m_drpBoostMicrophoneGain = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpBoostMicrophone" ) );
		needsActivate = true;
	}

	if( !m_btnTestMicrophone )
	{
		m_btnTestMicrophone = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnTestMicrophone" ) );
		needsActivate = true;
	}

	if( !m_pMicMeter )
	{
		m_pMicMeter = dynamic_cast< ImagePanel* >( FindChildByName( "MicMeter" ) );
		needsActivate = true;
	}

	if( !m_pMicMeter2 )
	{
		m_pMicMeter2 = dynamic_cast< ImagePanel* >( FindChildByName( "MicMeter2" ) );
		needsActivate = true;
	}

	if( !m_pMicMeterIndicator )
	{
		m_pMicMeterIndicator = dynamic_cast< ImagePanel* >( FindChildByName( "MicMeterIndicator" ) );
		needsActivate = true;
	}

// 	if( !m_btnCancel )
// 	{
// 		m_btnCancel = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnCancel" ) );
// 		needsActivate = true;
// 	}

	if( !m_btn3rdPartyCredits )
	{
		m_btn3rdPartyCredits = dynamic_cast< BaseModHybridButton* >( FindChildByName( "Btn3rdPartyCredits" ) );
		needsActivate = true;
	}

	if( needsActivate )
	{
		Activate();
	}

	if ( m_pVoiceTweak && m_pMicMeter && m_pMicMeter2 && m_pMicMeterIndicator && m_pMicMeter2->IsVisible() )
	{
		if ( !m_pVoiceTweak->IsStillTweaking() )
		{
			DevMsg( 1, "Lost Voice Tweak channels, resetting\n" );
			EndTestMicrophone();
		}
		else
		{
			int wide, tall;
			m_pMicMeter->GetSize(wide, tall);

			int indicatorWide, indicatorTall;
			m_pMicMeterIndicator->GetSize(indicatorWide, indicatorTall);

			int iXPos, iYPos;
			m_pMicMeter2->GetPos( iXPos, iYPos );

			int iFinalPos = iXPos - ( indicatorWide / 2 ) + static_cast<float>( wide ) * m_pVoiceTweak->GetControlFloat( SpeakingVolume );

			m_pMicMeterIndicator->GetPos( iXPos, iYPos );

			iFinalPos = Approach( iFinalPos, iXPos, vgui::scheme()->GetProportionalScaledValue( 4 ) );

			m_pMicMeterIndicator->SetPos( iFinalPos, iYPos );
		}
	}
}

void Audio::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void Audio::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		if ( m_nSelectedAudioLanguage != m_nCurrentAudioLanguage && m_drpLanguage && m_drpLanguage->IsVisible() )
		{
			UseSelectedLanguage();

			// Pop up a dialog to confirm changing the language
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation* confirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

			GenericConfirmation::Data_t data;

			data.pWindowTitle = "#GameUI_ChangeLanguageRestart_Title";
			data.pMessageText = "#GameUI_ChangeLanguageRestart_Info";

			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptLanguageChangeCallback;
			data.bCancelButtonEnabled = true;
			data.pfnCancelCallback = &CancelLanguageChangeCallback;

			// WARNING! WARNING! WARNING!
			// The nature of Generic Confirmation is that it will be silently replaced
			// with another Generic Confirmation if a system event occurs
			// e.g. user unplugs controller, user changes storage device, etc.
			// If that happens neither OK nor CANCEL callbacks WILL NOT BE CALLED
			// The state machine cannot depend on either callback advancing the
			// state because in some situations neither callback can fire and the
			// confirmation dismissed/closed/replaced.
			// State machine must implement OnThink and check if the required
			// confirmation box is still present!
			// This code implements no fallback - it will result in minor UI
			// bug that the language box will be changed, but the title not restarted.
			// Vitaliy -- 9/26/2009
			//
			confirmation->SetUsageData(data);
		}
		else
		{
			// Ready to write that data... go ahead and nav back
			BaseClass::OnKeyCodePressed(code);
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

//=============================================================================
void Audio::OnCommand(const char *command)
{
	if( Q_stricmp( "#GameUI_Headphones", command ) == 0 )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "0" );

		UpdateEnhanceStereo();
	}
	else if( Q_stricmp( "#GameUI_2Speakers", command ) == 0 )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "2" );

		// headphones at high quality get enhanced stereo turned on
		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if( Q_stricmp( "#GameUI_4Speakers", command ) == 0 )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "4" );

		// headphones at high quality get enhanced stereo turned on
		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if( Q_stricmp( "#GameUI_5Speakers", command ) == 0 )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "5" );

		// headphones at high quality get enhanced stereo turned on
		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if( Q_stricmp( "#GameUI_High", command ) == 0 )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue(false);
		Snd_PitchQuality.SetValue(true);

		UpdateEnhanceStereo();
	}
	else if( Q_stricmp( "#GameUI_Medium", command ) == 0 )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue(false);
		Snd_PitchQuality.SetValue(false);

		// headphones at high quality get enhanced stereo turned on
		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if( Q_stricmp( "#GameUI_Low", command ) == 0 )
	{
		CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
		CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
		dsp_slow_cpu.SetValue(true);
		Snd_PitchQuality.SetValue(false);

		// headphones at high quality get enhanced stereo turned on
		CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
		dsp_enhance_stereo.SetValue( 0 );
	}
	else if( Q_stricmp( "#L4D360UI_AudioOptions_CaptionOff", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 0 );
		cc_subtitles.SetValue( 0 );
	}
	else if( Q_stricmp( "#L4D360UI_AudioOptions_CaptionSubtitles", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 1 );
	}
	else if( Q_stricmp( "#L4D360UI_AudioOptions_CaptionOn", command ) == 0 )
	{
		CGameUIConVarRef closecaption("closecaption");
		CGameUIConVarRef cc_subtitles("cc_subtitles");
		closecaption.SetValue( 1 );
		cc_subtitles.SetValue( 0 );
	}
	else if ( StringHasPrefix( command, VIDEO_LANGUAGE_COMMAND_PREFIX ) )
	{
		int iCommandNumberPosition = Q_strlen( VIDEO_LANGUAGE_COMMAND_PREFIX );
		int iSelectedLanguage = clamp( command[ iCommandNumberPosition ] - '0', 0, m_nNumAudioLanguages - 1 );

		m_nSelectedAudioLanguage = m_nAudioLanguages[ iSelectedLanguage ].languageCode;
	}
	else if( Q_stricmp( "VoiceCommunicationEnabled", command ) == 0 )
	{
		CGameUIConVarRef voice_modenable("voice_modenable");
		CGameUIConVarRef voice_vox("voice_vox");
		CGameUIConVarRef voice_enable("voice_enable");
		voice_modenable.SetValue( 1 );
		voice_enable.SetValue( 1 );

		bool bMicVolumeFound = m_pVoiceTweak->IsControlFound( MicrophoneVolume );

		if ( voice_vox.GetBool() )
		{
			voice_vox.SetValue( 0 );
			voice_vox.SetValue( 1 );
		}

		SetControlEnabled( "SldVoiceTransmitVolume", bMicVolumeFound );
		SetControlEnabled( "SldVoiceReceiveVolume", true );
		SetControlEnabled( "DrpBoostMicrophone", true );
		SetControlVisible( "MicMeter", true );
		SetControlVisible( "MicMeter2", false );
		SetControlEnabled( "BtnTestMicrophone", true );
		SetControlEnabled( "DrpVoiceCommunicationStyle", true );
		SetControlEnabled( "SldVoiceVoxThreshold", voice_vox.GetBool() );
	}
	else if( Q_stricmp( "VoiceCommunicationDisabled", command ) == 0 )
	{
		CGameUIConVarRef voice_modenable("voice_modenable");
		CGameUIConVarRef voice_enable("voice_enable");
		voice_modenable.SetValue( 0 );
		voice_enable.SetValue( 0 );

		SetControlEnabled( "SldVoiceTransmitVolume", false );
		SetControlEnabled( "SldVoiceReceiveVolume", false );
		SetControlEnabled( "DrpBoostMicrophone", false );
		SetControlVisible( "MicMeter", false );
		SetControlVisible( "MicMeter2", false );
		SetControlEnabled( "BtnTestMicrophone", false );
		SetControlEnabled( "DrpVoiceCommunicationStyle", false );
		SetControlEnabled( "SldVoiceVoxThreshold", false);
	
		if ( m_drpBoostMicrophoneGain )
		{
			m_drpBoostMicrophoneGain->CloseDropDown();
			m_drpBoostMicrophoneGain->SetEnabled( false );
		}
	}
	else if( Q_stricmp( "VoiceCommunicationPushToTalk", command ) == 0 )
	{
		CGameUIConVarRef voice_vox("voice_vox");
		voice_vox.SetValue( 0 );

		SetControlEnabled( "SldVoiceVoxThreshold", false );
	}
	else if( Q_stricmp( "VoiceCommunicationOpenMic", command ) == 0 )
	{
		CGameUIConVarRef voice_vox("voice_vox");
		voice_vox.SetValue( 1 );

		SetControlEnabled( "SldVoiceVoxThreshold", true );
	}
	else if( Q_stricmp( "BoostMicrophoneEnabled", command ) == 0 )
	{
		if ( m_pVoiceTweak )
		{
			m_pVoiceTweak->SetControlFloat( MicBoost, 1.0f );
		}
	}
	else if( Q_stricmp( "BoostMicrophoneDisabled", command ) == 0 )
	{
		if ( m_pVoiceTweak )
		{
			m_pVoiceTweak->SetControlFloat( MicBoost, 0.0f );
		}
	}
	else if( Q_stricmp( "TestMicrophone", command ) == 0 )
	{
		FlyoutMenu::CloseActiveMenu();
		if ( m_pVoiceTweak && m_pMicMeter2 )
		{
			if ( !m_pMicMeter2->IsVisible() )
			{
				StartTestMicrophone();
			}
			else
			{
				EndTestMicrophone();
			}
		}
	}
	else if( !Q_strcmp( command, "Jukebox" ) )
	{
		if ( m_pVoiceTweak && m_pMicMeter2 )
		{
			if ( m_pMicMeter2->IsVisible() )
			{
				EndTestMicrophone();
			}
		}

		CBaseModPanel::GetSingleton().OpenWindow( WT_JUKEBOX, this, true );
	}
	else if( Q_stricmp( "Back", command ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if( Q_stricmp( "3rdPartyCredits", command ) == 0 )
	{
		if ( m_pVoiceTweak && m_pMicMeter2 )
		{
			if ( m_pMicMeter2->IsVisible() )
			{
				EndTestMicrophone();
			}
		}

		EndTestMicrophone();
		OpenThirdPartySoundCreditsDialog();
		FlyoutMenu::CloseActiveMenu();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void Audio::UpdateEnhanceStereo( void )
{
	// headphones at high quality get enhanced stereo turned on
	CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
	CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );

	if ( !dsp_slow_cpu.GetBool() && Snd_PitchQuality.GetBool() && snd_surround_speakers.GetInt() == 0 )
	{
		dsp_enhance_stereo.SetValue( 1 );
	}
	else
	{
		dsp_enhance_stereo.SetValue( 0 );
	}
}

void Audio::OnNotifyChildFocus( vgui::Panel* child )
{
}

void Audio::UpdateFooter( bool bEnableCloud )
{
	if ( !BaseModUI::CBaseModPanel::GetSingletonPtr() )
		return;

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

		footer->SetShowCloud( bEnableCloud );
	}
}

void Audio::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter( true );
}

void Audio::OnFlyoutMenuCancelled()
{
}

//=============================================================================
Panel* Audio::NavigateBack()
{
	if ( m_pVoiceTweak && m_sldTransmitVolume )
	{
		int nVal = m_sldTransmitVolume->GetCurrentValue();
		float val = static_cast<float>( nVal ) / 100.0f;
		m_pVoiceTweak->SetControlFloat( MicrophoneVolume, val );
	}

	if ( m_pVoiceTweak && m_sldRecieveVolume )
	{
		float flVal = m_sldRecieveVolume->GetCurrentValue();
		m_pVoiceTweak->SetControlFloat( OtherSpeakerScale, flVal );
	}

	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

	return BaseClass::NavigateBack();
}

void Audio::UseSelectedLanguage()
{
	m_pchUpdatedAudioLanguage = GetLanguageName( m_nSelectedAudioLanguage );
}

void Audio::ResetLanguage()
{
	m_pchUpdatedAudioLanguage = "";
}

void Audio::StartTestMicrophone()
{
	if ( m_pVoiceTweak && m_sldTransmitVolume )
	{
		int nVal = m_sldTransmitVolume->GetCurrentValue();
		float val = static_cast<float>( nVal ) / 100.0f;
		m_pVoiceTweak->SetControlFloat( MicrophoneVolume, val );
	}

	if ( m_pVoiceTweak && m_pVoiceTweak->StartVoiceTweakMode() )
	{
		if ( m_pMicMeter )
		{
			m_pMicMeter->SetVisible( false );
		}

		if ( m_pMicMeter2 )
		{
			m_pMicMeter2->SetVisible( true );
		}

		if ( m_pMicMeterIndicator )
		{
			m_pMicMeterIndicator->SetVisible( true );
		}

		SetControlEnabled( "SldVoiceTransmitVolume", false );
		SetControlEnabled( "SldVoiceReceiveVolume", false );

		if ( m_drpVoiceCommunication )
		{
			m_drpVoiceCommunication->CloseDropDown();
			m_drpVoiceCommunication->SetEnabled( false );
		}

		if ( m_drpBoostMicrophoneGain )
		{
			m_drpBoostMicrophoneGain->CloseDropDown();
			m_drpBoostMicrophoneGain->SetEnabled( false );
		}
	}
}

void Audio::EndTestMicrophone()
{
	if ( m_pVoiceTweak && m_pVoiceTweak->IsStillTweaking() )
	{
		m_pVoiceTweak->EndVoiceTweakMode();

		CGameUIConVarRef voice_vox("voice_vox");

		if ( voice_vox.GetBool() )
		{
			voice_vox.SetValue( 0 );
			voice_vox.SetValue( 1 );
		}
	}

	if ( m_pMicMeter )
	{
		CGameUIConVarRef voice_modenable("voice_modenable");
		CGameUIConVarRef voice_enable("voice_enable");
		m_pMicMeter->SetVisible( voice_enable.GetBool() && voice_modenable.GetBool() );
	}

	if ( m_pMicMeter2 )
	{
		m_pMicMeter2->SetVisible( false );
	}

	if ( m_pMicMeterIndicator )
	{
		m_pMicMeterIndicator->SetVisible( false );
	}

	bool bMicVolumeFound = m_pVoiceTweak->IsControlFound( MicrophoneVolume );

	SetControlEnabled( "DrpVoiceCommunication", true );
	SetControlEnabled( "SldVoiceTransmitVolume", bMicVolumeFound );
	SetControlEnabled( "SldVoiceReceiveVolume", true );
	SetControlEnabled( "DrpBoostMicrophone", true );
}

void Audio::OpenThirdPartySoundCreditsDialog()
{
	if (!m_OptionsSubAudioThirdPartyCreditsDlg.Get())
	{
		m_OptionsSubAudioThirdPartyCreditsDlg = new COptionsSubAudioThirdPartyCreditsDlg(GetVParent());
	}
	m_OptionsSubAudioThirdPartyCreditsDlg->Activate();
}

void Audio::AcceptLanguageChangeCallback() 
{
	Audio *self = static_cast< Audio * >( CBaseModPanel::GetSingleton().GetWindow( WT_AUDIO ) );
	if( self )
	{
		self->BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
}

//=============================================================================
void Audio::CancelLanguageChangeCallback()
{
	Audio *self = static_cast< Audio * >( CBaseModPanel::GetSingleton().GetWindow( WT_AUDIO ) );
	if( self )
	{
		self->ResetLanguage();
	}
}

void Audio::PaintBackground()
{
	//BaseClass::DrawDialogBackground( "#GameUI_Audio", NULL, "#L4D360UI_AudioVideo_Desc", NULL, NULL, true );
}

void Audio::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}
