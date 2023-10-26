//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "OptionsSubAudio.h"

#include "CvarSlider.h"
#include "EngineInterface.h"
#include "ModInfo.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/QueryBox.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "vgui/IInput.h"
#include "steam/steam_api.h"
#include "tier1/strtools.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// This member is static so that the updated audio language can be referenced during shutdown
char* COptionsSubAudio::m_pchUpdatedAudioLanguage = (char*)GetLanguageShortName( k_Lang_English );

enum SoundQuality_e
{
	SOUNDQUALITY_LOW,
	SOUNDQUALITY_MEDIUM,
	SOUNDQUALITY_HIGH,
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubAudio::COptionsSubAudio(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_pSFXSlider = new CCvarSlider( this, "SFXSlider", "#GameUI_SoundEffectVolume", 0.0f, 1.0f, "volume" );
	m_pMusicSlider = new CCvarSlider( this, "MusicSlider", "#GameUI_MusicVolume", 0.0f, 1.0f, "Snd_MusicVolume" );
	
	m_pCloseCaptionCombo = new ComboBox( this, "CloseCaptionCheck", 6, false );
	m_pCloseCaptionCombo->AddItem( "#GameUI_NoClosedCaptions", NULL );
	m_pCloseCaptionCombo->AddItem( "#GameUI_SubtitlesAndSoundEffects", NULL );
	m_pCloseCaptionCombo->AddItem( "#GameUI_Subtitles", NULL );

	m_pSoundQualityCombo = new ComboBox( this, "SoundQuality", 6, false );
	m_pSoundQualityCombo->AddItem( "#GameUI_High", new KeyValues("SoundQuality", "quality", SOUNDQUALITY_HIGH) );
	m_pSoundQualityCombo->AddItem( "#GameUI_Medium", new KeyValues("SoundQuality", "quality", SOUNDQUALITY_MEDIUM) );
	m_pSoundQualityCombo->AddItem( "#GameUI_Low", new KeyValues("SoundQuality", "quality", SOUNDQUALITY_LOW) );

	m_pSpeakerSetupCombo = new ComboBox( this, "SpeakerSetup", 6, false );
	m_pSpeakerSetupCombo->AddItem( "#GameUI_Headphones", new KeyValues("SpeakerSetup", "speakers", 0) );
	m_pSpeakerSetupCombo->AddItem( "#GameUI_2Speakers", new KeyValues("SpeakerSetup", "speakers", 2) );
	m_pSpeakerSetupCombo->AddItem( "#GameUI_4Speakers", new KeyValues("SpeakerSetup", "speakers", 4) );
	m_pSpeakerSetupCombo->AddItem( "#GameUI_5Speakers", new KeyValues("SpeakerSetup", "speakers", 5) );
	m_pSpeakerSetupCombo->AddItem( "#GameUI_7Speakers", new KeyValues("SpeakerSetup", "speakers", 7) );

   m_pSpokenLanguageCombo = new ComboBox (this, "AudioSpokenLanguage", 6, false );

	LoadControlSettings("Resource\\OptionsSubAudio.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubAudio::~COptionsSubAudio()
{
}

//-----------------------------------------------------------------------------
// Purpose: Reloads data
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnResetData()
{
	m_bRequireRestart = false;
	m_pSFXSlider->Reset();
	m_pMusicSlider->Reset();


	// reset the combo boxes

	// close captions
	CGameUIConVarRef closecaption("closecaption");
	CGameUIConVarRef cc_subtitles("cc_subtitles");
	if (closecaption.GetBool())
	{
		if (cc_subtitles.GetBool())
		{
			m_pCloseCaptionCombo->ActivateItem(2);
		}
		else
		{
			m_pCloseCaptionCombo->ActivateItem(1);
		}
	}
	else
	{
		m_pCloseCaptionCombo->ActivateItem(0);
	}

	// speakers
	CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	int speakers = snd_surround_speakers.GetInt();
	{for (int itemID = 0; itemID < m_pSpeakerSetupCombo->GetItemCount(); itemID++)
	{
		KeyValues *kv = m_pSpeakerSetupCombo->GetItemUserData(itemID);
		if (kv && kv->GetInt("speakers") == speakers)
		{
			m_pSpeakerSetupCombo->ActivateItem(itemID);
		}
	}}
	
	// sound quality is made up from several cvars
	CGameUIConVarRef Snd_PitchQuality("Snd_PitchQuality");
	CGameUIConVarRef dsp_slow_cpu("dsp_slow_cpu");
	int quality = SOUNDQUALITY_LOW;
	if (dsp_slow_cpu.GetBool() == false)
	{
		quality = SOUNDQUALITY_MEDIUM;
	}
	if (Snd_PitchQuality.GetBool())
	{
		quality = SOUNDQUALITY_HIGH;
	}
	// find the item in the list and activate it
	{for (int itemID = 0; itemID < m_pSoundQualityCombo->GetItemCount(); itemID++)
	{
		KeyValues *kv = m_pSoundQualityCombo->GetItemUserData(itemID);
		if (kv && kv->GetInt("quality") == quality)
		{
			m_pSoundQualityCombo->ActivateItem(itemID);
		}
	}}

   //
   // Audio Languages
   //
   char szCurrentLanguage[50] = "";
   char szAvailableLanguages[512] = "";
   szAvailableLanguages[0] = NULL;

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

   // Check to see if we have a list of languages from Steam
   if ( V_strlen( szAvailableLanguages ) )
   {
      // Populate the combo box with each available language
      CSplitString languagesList( szAvailableLanguages, "," );

      for ( int i=0; i < languagesList.Count(); i++ )
      {
         const ELanguage languageCode = PchLanguageToELanguage( languagesList[i] );
         m_pSpokenLanguageCombo->AddItem( GetLanguageVGUILocalization( languageCode ), new KeyValues ("Audio Languages", "language", languageCode) );
      }
   }
   else
   {
      // Add the current language to the combo
      m_pSpokenLanguageCombo->AddItem( GetLanguageVGUILocalization( m_nCurrentAudioLanguage ), new KeyValues ("Audio Languages", "language", m_nCurrentAudioLanguage) );
   }

   // Activate the current language in the combo
   {for (int itemID = 0; itemID < m_pSpokenLanguageCombo->GetItemCount(); itemID++)
   {
      KeyValues *kv = m_pSpokenLanguageCombo->GetItemUserData( itemID );
      if ( kv && kv->GetInt( "language" ) == m_nCurrentAudioLanguage )
      {
         m_pSpokenLanguageCombo->ActivateItem( itemID );
         break;
      }
   }}
}

//-----------------------------------------------------------------------------
// Purpose: Applies changes
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnApplyChanges()
{
	m_pSFXSlider->ApplyChanges();
	m_pMusicSlider->ApplyChanges();

	// set the cvars appropriately
	// Tracker 28933:  Note we can't do this because closecaption is marked
	//  FCVAR_USERINFO and it won't get sent to server is we direct set it, we
	//  need to pass it along to the engine parser!!!
	// ConVar *closecaption = (ConVar *)cvar->FindVar("closecaption");
	int closecaption_value = 0;

	CGameUIConVarRef cc_subtitles( "cc_subtitles" );
	switch (m_pCloseCaptionCombo->GetActiveItem())
	{
	default:
	case 0:
		closecaption_value = 0;
		cc_subtitles.SetValue( 0 );
		break;
	case 1:
		closecaption_value = 1;
		cc_subtitles.SetValue( 0 );
		break;
	case 2:
		closecaption_value = 1;
		cc_subtitles.SetValue( 1 );
		break;
	}

	// Stuff the close caption change to the console so that it can be
	//  sent to the server (FCVAR_USERINFO) so that you don't have to restart
	//  the level for the change to take effect.
	char cmd[ 64 ];
	Q_snprintf( cmd, sizeof( cmd ), "closecaption %i\n", closecaption_value );
	engine->ClientCmd_Unrestricted( cmd );

	CGameUIConVarRef snd_surround_speakers( "Snd_Surround_Speakers" );
	int speakers = m_pSpeakerSetupCombo->GetActiveItemUserData()->GetInt( "speakers" );
	snd_surround_speakers.SetValue( speakers );

	// quality
	CGameUIConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	CGameUIConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
	int quality = m_pSoundQualityCombo->GetActiveItemUserData()->GetInt( "quality" );
	switch ( quality )
	{
	case SOUNDQUALITY_LOW:
		dsp_slow_cpu.SetValue(true);
		Snd_PitchQuality.SetValue(false);
		break;
	case SOUNDQUALITY_MEDIUM:
		dsp_slow_cpu.SetValue(false);
		Snd_PitchQuality.SetValue(false);
		break;
	default:
		Assert("Undefined sound quality setting.");
	case SOUNDQUALITY_HIGH:
		dsp_slow_cpu.SetValue(false);
		Snd_PitchQuality.SetValue(true);
		break;
	};

	// headphones at high quality get enhanced stereo turned on
	CGameUIConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );
	if (speakers == 0 && quality == SOUNDQUALITY_HIGH)
	{
		dsp_enhance_stereo.SetValue( 1 );
	}
	else
	{
		dsp_enhance_stereo.SetValue( 0 );
	}

   // Audio spoken language
   KeyValues *kv = m_pSpokenLanguageCombo->GetItemUserData( m_pSpokenLanguageCombo->GetActiveItem() );
   const ELanguage nUpdatedAudioLanguage = (ELanguage)( kv ? kv->GetInt( "language" ) : k_Lang_English );

   if ( nUpdatedAudioLanguage != m_nCurrentAudioLanguage )
   {
      // Store new language in static member so that it can be accessed during shutdown when this instance is gone
      m_pchUpdatedAudioLanguage = (char *) GetLanguageShortName( nUpdatedAudioLanguage );
      
      // Inform user that they need to restart in order change language at this time
      QueryBox *qb = new QueryBox( "#GameUI_ChangeLanguageRestart_Title", "#GameUI_ChangeLanguageRestart_Info", GetParent()->GetParent()->GetParent() );
      if (qb != NULL)
      {
         qb->SetOKCommand( new KeyValues( "Command", "command", "RestartWithNewLanguage" ) );
         qb->SetOKButtonText( "#GameUI_ChangeLanguageRestart_OkButton" );
         qb->SetCancelButtonText( "#GameUI_ChangeLanguageRestart_CancelButton" );
         qb->AddActionSignalTarget( GetParent()->GetParent()->GetParent() );
         qb->DoModal();
      }
   }
}

//-----------------------------------------------------------------------------
// Purpose: Called on controls changing, enables the Apply button
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the engine needs to be restarted
//-----------------------------------------------------------------------------
bool COptionsSubAudio::RequiresRestart()
{
	// nothing in audio requires a restart like now
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnCommand( const char *command )
{
	if ( !stricmp( command, "TestSpeakers" ) )
	{
		// ask them if they REALLY want to test the speakers if they're in a game already.
		if (engine->IsConnected())
		{
			QueryBox *qb = new QueryBox("#GameUI_TestSpeakersWarning_Title", "#GameUI_TestSpeakersWarning_Info" );
			if (qb != NULL)
			{
				qb->SetOKCommand(new KeyValues("RunTestSpeakers"));
				qb->SetOKButtonText("#GameUI_TestSpeakersWarning_OkButton");
				qb->SetCancelButtonText("#GameUI_TestSpeakersWarning_CancelButton");
				qb->AddActionSignalTarget( this );
				qb->DoModal();
			}
			else
			{
				// couldn't create the warning dialog for some reason, so just test the speakers.
				RunTestSpeakers();
			}
		}
		else
		{
			// player isn't connected to a game so there's no reason to warn them about being disconnected.
			// create the command to execute
			RunTestSpeakers();
		}
	}
   else if ( !stricmp( command, "ShowThirdPartyAudioCredits" ) )
   {
      OpenThirdPartySoundCreditsDialog();
   }

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Run the test speakers map.
//-----------------------------------------------------------------------------
void COptionsSubAudio::RunTestSpeakers()
{
	engine->ClientCmd_Unrestricted( "disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers 1\n\nhostname \"Speaker Test\"\nprogress_enable\nmap test_speakers\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Open third party audio credits dialog
//-----------------------------------------------------------------------------
void COptionsSubAudio::OpenThirdPartySoundCreditsDialog()
{
   if (!m_OptionsSubAudioThirdPartyCreditsDlg.Get())
   {
      m_OptionsSubAudioThirdPartyCreditsDlg = new COptionsSubAudioThirdPartyCreditsDlg(GetVParent());
   }
   m_OptionsSubAudioThirdPartyCreditsDlg->Activate();
}


COptionsSubAudioThirdPartyCreditsDlg::COptionsSubAudioThirdPartyCreditsDlg( vgui::VPANEL hParent ) : BaseClass( NULL, NULL )
{
	SetProportional( true );

#ifdef SWARM_DLL
	// parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)
	SetScheme( "SwarmFrameScheme" );
#endif

	SetTitle( "#GameUI_ThirdPartyAudio_Title", true );
	SetSize( 
		vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 500 ),
		vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 200 ) );

	MoveToCenterOfScreen();
	SetSizeable( false );
	SetDeleteSelfOnClose( true );
}

void COptionsSubAudioThirdPartyCreditsDlg::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	LoadControlSettings( "resource/OptionsSubAudioThirdPartyDlg.res" );
}

void COptionsSubAudioThirdPartyCreditsDlg::Activate()
{
	BaseClass::Activate();

	input()->SetAppModalSurface(GetVPanel());
}

void COptionsSubAudioThirdPartyCreditsDlg::OnKeyCodeTyped(vgui::KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (code == KEY_ESCAPE)
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}