//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//

#ifndef OPTIONS_SUB_VOICE_H
#define OPTIONS_SUB_VOICE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

typedef struct IVoiceTweak_s IVoiceTweak;

class CCvarSlider;
class CCvarToggleCheckButton;
//-----------------------------------------------------------------------------
// Purpose: Voice Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVoice : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubVoice, vgui::PropertyPage );

public:
	COptionsSubVoice(vgui::Panel *parent);
	~COptionsSubVoice();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
	virtual void OnThink();							// called every frame before painting, but only if panel is visible

private:
    virtual void    OnCommand( const char *command );

	MESSAGE_FUNC( OnPageHide, "PageHide" );
    MESSAGE_FUNC_INT( OnSliderMoved, "SliderMoved", position );
    MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );
    MESSAGE_FUNC( OnControlModified, "ControlModified" );

    void            StartTestMicrophone();
    void            EndTestMicrophone();

    void            UseCurrentVoiceParameters();
    void            ResetVoiceParameters();

	IVoiceTweak				*m_pVoiceTweak;		// Engine voice tweak API.
	vgui::CheckButton		*m_pMicBoost;

    vgui::ImagePanel        *m_pMicMeter;
    vgui::ImagePanel        *m_pMicMeter2;
    vgui::Button            *m_pTestMicrophoneButton;
    vgui::Label             *m_pMicrophoneSliderLabel;
	vgui::Slider			*m_pMicrophoneVolume;
    vgui::Label             *m_pReceiveSliderLabel;
    CCvarSlider             *m_pReceiveVolume;
	
	// "Open mic" settings
	vgui::Label             *m_pThresholdSliderLabel;
	CCvarSlider             *m_pThresholdVolume;
	CCvarToggleCheckButton  *m_pOpenMicEnableCheckButton;

    CCvarToggleCheckButton  *m_pVoiceEnableCheckButton;

	int						m_nVoiceThresholdValue;
	int                     m_nMicVolumeValue;
    bool                    m_bMicBoostSelected;
	bool					m_bOpenMicSelected;
    float                   m_fReceiveVolume;
    int                     m_nReceiveSliderValue;

    bool            m_bVoiceOn;
};



#endif // OPTIONS_SUB_VOICE_H