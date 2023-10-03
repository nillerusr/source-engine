//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//

#include "OptionsSubVoice.h"
#include "CvarSlider.h"
#include <vgui/IVGui.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>
#include "EngineInterface.h"
#include "ivoicetweak.h"
#include "CvarToggleCheckButton.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubVoice::COptionsSubVoice(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
#if !defined( NO_VOICE ) //#ifndef _XBOX
	m_pVoiceTweak = engine->GetVoiceTweakAPI();
#endif
    m_pMicMeter = new ImagePanel(this, "MicMeter");
    m_pMicMeter2 = new ImagePanel(this, "MicMeter2");

    m_pReceiveSliderLabel = new Label(this, "ReceiveLabel", "#GameUI_VoiceReceiveVolume");
	m_pReceiveVolume = new CCvarSlider( this, "VoiceReceive", "#GameUI_ReceiveVolume", 0.0f, 1.0f, "voice_scale" );

    m_pMicrophoneSliderLabel = new Label(this, "MicrophoneLabel", "#GameUI_VoiceTransmitVolume");
	m_pMicrophoneVolume = new Slider( this, "#GameUI_MicrophoneVolume" );
	m_pMicrophoneVolume->SetRange( 0, 100 );
	m_pMicrophoneVolume->AddActionSignalTarget( this );

	m_pVoiceEnableCheckButton = new CCvarToggleCheckButton( this, "voice_modenable", "#GameUI_EnableVoice", "voice_modenable" );

	m_pMicBoost = new CheckButton(this, "MicBoost", "#GameUI_BoostMicrophone" );
	m_pMicBoost->AddActionSignalTarget( this );

	// Open mic controls
	m_pThresholdSliderLabel = new Label(this, "ThresholdLabel", "#GameUI_VoiceThreshold");
	m_pThresholdVolume = new CCvarSlider( this, "VoiceThreshold", "#GameUI_VoiceThreshold", 0, 16384, "voice_threshold" );
	m_pOpenMicEnableCheckButton = new CCvarToggleCheckButton( this, "voice_vox", "#GameUI_EnableOpenMic", "voice_vox" );

	m_pTestMicrophoneButton = new Button(this, "TestMicrophone", "#GameUI_TestMicrophone");

	LoadControlSettings("Resource\\OptionsSubVoice.res");

    m_bVoiceOn = false;
    m_pMicMeter2->SetVisible(false);
    // no voice tweak - then disable all buttons
    if (!m_pVoiceTweak)
    {
        m_pReceiveVolume->SetEnabled(false);
        m_pMicrophoneVolume->SetEnabled(false);
        m_pVoiceEnableCheckButton->SetEnabled(false);
        m_pMicBoost->SetEnabled(false);
        m_pTestMicrophoneButton->SetEnabled(false);
		m_pOpenMicEnableCheckButton->SetEnabled(false);
		m_pThresholdVolume->SetEnabled(false);
    }
    else
    {
        OnResetData();
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubVoice::~COptionsSubVoice()
{
    // turn off voice if it was on, since we're leaving this page
    if (m_bVoiceOn)
    {
        EndTestMicrophone();
    }

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnResetData()
{
    if (!m_pVoiceTweak)
        return;

	float micVolume = m_pVoiceTweak->GetControlFloat( MicrophoneVolume );
	m_pMicrophoneVolume->SetValue( (int)( 100.0f * micVolume ) );
    m_nMicVolumeValue = m_pMicrophoneVolume->GetValue();

	float fMicBoost = m_pVoiceTweak->GetControlFloat( MicBoost );
	m_pMicBoost->SetSelected( fMicBoost != 0.0f );
    m_bMicBoostSelected = m_pMicBoost->IsSelected();

    m_pReceiveVolume->Reset();
    m_fReceiveVolume = m_pReceiveVolume->GetSliderValue();

    m_pThresholdVolume->Reset();
	m_nVoiceThresholdValue = m_pThresholdVolume->GetSliderValue();
	
	m_pOpenMicEnableCheckButton->Reset();
	m_bOpenMicSelected = m_pOpenMicEnableCheckButton->IsSelected();

	m_pVoiceEnableCheckButton->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnSliderMoved( int position )
{
    if ( m_pVoiceTweak )
    {
        if ( m_pMicrophoneVolume->GetValue() != m_nMicVolumeValue )
        {
            PostActionSignal(new KeyValues("ApplyButtonEnable"));
        }
		
		if ( m_pThresholdVolume->GetSliderValue() != m_nVoiceThresholdValue )
		{
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
			m_pThresholdVolume->ApplyChanges();
			m_nVoiceThresholdValue = m_pThresholdVolume->GetSliderValue();
		}
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnCheckButtonChecked( int state )
{
    if ( m_pVoiceTweak )
    {
        // if our state is different
        if ( m_pMicBoost->IsSelected() != m_bMicBoostSelected)
        {
            PostActionSignal(new KeyValues("ApplyButtonEnable"));
        }
		
		if ( m_pOpenMicEnableCheckButton->IsSelected() != m_bOpenMicSelected )
		{
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
			m_pOpenMicEnableCheckButton->ApplyChanges();
			m_bOpenMicSelected = m_pOpenMicEnableCheckButton->IsSelected();
		}
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnApplyChanges()
{
    if (!m_pVoiceTweak)
        return;

    m_nMicVolumeValue = m_pMicrophoneVolume->GetValue();
    float fMicVolume = (float) m_nMicVolumeValue / 100.0f;
	m_pVoiceTweak->SetControlFloat( MicrophoneVolume, fMicVolume );

    m_bMicBoostSelected = m_pMicBoost->IsSelected();
    m_pVoiceTweak->SetControlFloat( MicBoost, m_bMicBoostSelected ? 1.0f : 0.0f );

    m_pReceiveVolume->ApplyChanges();
    m_fReceiveVolume = m_pReceiveVolume->GetSliderValue();

	m_pThresholdVolume->ApplyChanges();
	m_nVoiceThresholdValue = m_pThresholdVolume->GetSliderValue();

	m_pOpenMicEnableCheckButton->ApplyChanges();

	m_pVoiceEnableCheckButton->ApplyChanges();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::StartTestMicrophone()
{
    if (!m_pVoiceTweak || m_bVoiceOn)
        return;

    m_bVoiceOn = true;

    UseCurrentVoiceParameters();

    if (m_pVoiceTweak->StartVoiceTweakMode())
    {
        m_pTestMicrophoneButton->SetText("#GameUI_StopTestMicrophone");

        m_pReceiveVolume->SetEnabled(false);
        m_pMicrophoneVolume->SetEnabled(false);
        m_pVoiceEnableCheckButton->SetEnabled(false);
        m_pMicBoost->SetEnabled(false);
        m_pMicrophoneSliderLabel->SetEnabled(false);
        m_pReceiveSliderLabel->SetEnabled(false);

        m_pMicMeter2->SetVisible(true);
    }
    else
    {
        ResetVoiceParameters();

        // we couldn't start it
        m_bVoiceOn = false;
        return;
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::UseCurrentVoiceParameters()
{
    int nVal = m_pMicrophoneVolume->GetValue();
    float val = (float) nVal / 100.0f;
	m_pVoiceTweak->SetControlFloat( MicrophoneVolume, val );

    bool bSelected = m_pMicBoost->IsSelected();
    val = bSelected ? 1.0f : 0.0f;
    m_pVoiceTweak->SetControlFloat( MicBoost, val );

    // get where the current slider is
    m_nReceiveSliderValue = m_pReceiveVolume->GetValue();
    m_pReceiveVolume->ApplyChanges();

	m_nVoiceThresholdValue = m_pThresholdVolume->GetValue();
	m_pThresholdVolume->ApplyChanges();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::ResetVoiceParameters()
{
    float fMicVolume = (float) m_nMicVolumeValue / 100.0f;
	m_pVoiceTweak->SetControlFloat( MicrophoneVolume, fMicVolume );
    m_pVoiceTweak->SetControlFloat( MicBoost, m_bMicBoostSelected ? 1.0f : 0.0f );

    // restore the old value
	ConVarRef voice_scale( "voice_scale" );
	voice_scale.SetValue( m_fReceiveVolume );
    
	m_pReceiveVolume->Reset();

    // set the slider to 'new' value, but we've reset the 'start' value where it was
    m_pReceiveVolume->SetValue( m_nReceiveSliderValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::EndTestMicrophone()
{
    if (!m_pVoiceTweak || !m_bVoiceOn)
        return;

	if ( m_pVoiceTweak->IsStillTweaking() )
	{
		m_pVoiceTweak->EndVoiceTweakMode();
	}
    ResetVoiceParameters();
    m_pTestMicrophoneButton->SetText("#GameUI_TestMicrophone");
    m_bVoiceOn = false;

    m_pReceiveVolume->SetEnabled(true);
    m_pMicrophoneVolume->SetEnabled(true);
    m_pVoiceEnableCheckButton->SetEnabled(true);
    m_pMicBoost->SetEnabled(true);
    m_pMicrophoneSliderLabel->SetEnabled(true);
    m_pReceiveSliderLabel->SetEnabled(true);
    m_pMicMeter2->SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnCommand( const char *command)
{
    if (!stricmp(command, "TestMicrophone"))
    {
        if (!m_bVoiceOn)
        {
            StartTestMicrophone();
        }
        else
        {
            EndTestMicrophone();
        }
    }
    else
	{
        BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnPageHide()
{
    // turn off voice if it was on, since we're leaving this page
    if (m_bVoiceOn)
    {
        EndTestMicrophone();
    }
    BaseClass::OnPageHide();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

#define BAR_WIDTH 160
#define BAR_INCREMENT 8

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVoice::OnThink()
{
    BaseClass::OnThink();
    if ( m_bVoiceOn )
    {
		if ( m_pVoiceTweak->IsStillTweaking() == false )
		{
			DevMsg( 1, "Lost Voice Tweak channels, resetting\n" );
			EndTestMicrophone();
		}
		else
		{
			float val = m_pVoiceTweak->GetControlFloat( SpeakingVolume );
			int nValue = static_cast<int>( val*32768.0f + 0.5f );

			// Throttle this if they're using "open mic" style communication
			if ( m_pOpenMicEnableCheckButton->IsSelected() )
			{
				// Test against it our threshold value
				float flThreshold = ( (float) m_pThresholdVolume->GetSliderValue() / 32768.0f );
				if ( val < flThreshold )
				{
					nValue = 0;	// Zero the display
				}
			}

			int width = (BAR_WIDTH * nValue) / 32768;
			width = ((width + (BAR_INCREMENT-1)) / BAR_INCREMENT) * BAR_INCREMENT;  // round to nearest BAR_INCREMENT

			int wide, tall;
			m_pMicMeter2->GetSize(wide, tall);
			m_pMicMeter2->SetSize(width, tall);
			m_pMicMeter2->Repaint();
		}
    }
}
