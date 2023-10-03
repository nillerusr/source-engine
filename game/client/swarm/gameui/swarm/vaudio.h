//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VAUDIO_H__
#define __VAUDIO_H__


#include "basemodui.h"
#include "VFlyoutMenu.h"
#include "OptionsSubAudio.h"


#define MAX_DYNAMIC_AUDIO_LANGUAGES 15


typedef struct IVoiceTweak_s IVoiceTweak;
class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

struct AudioLangauge_t
{
	ELanguage languageCode;
};


class Audio : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( Audio, CBaseModFrame );

public:
	Audio(vgui::Panel *parent, const char *panelName);
	~Audio();

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();
	virtual void PerformLayout();

	Panel* NavigateBack();
	void UseSelectedLanguage();
	void ResetLanguage();

	static const char* GetUpdatedAudioLanguage() { return m_pchUpdatedAudioLanguage; }

protected:
	virtual void Activate();
	virtual void OnThink();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );

	void UpdateEnhanceStereo( void );

private:
	void		StartTestMicrophone();
	void		EndTestMicrophone();
	void		UpdateFooter( bool bEnableCloud );

	void OpenThirdPartySoundCreditsDialog();

	static void AcceptLanguageChangeCallback();
	static void CancelLanguageChangeCallback();

private:
	CNB_Header_Footer *m_pHeaderFooter;

	IVoiceTweak		*m_pVoiceTweak;		// Engine voice tweak API.

	SliderControl	*m_sldGameVolume;
	SliderControl	*m_sldMusicVolume;
	SliderControl	*m_sldVoiceThreshold;
	DropDownMenu	*m_drpSpeakerConfiguration;
	DropDownMenu	*m_drpSoundQuality;
	DropDownMenu	*m_drpLanguage;
	DropDownMenu	*m_drpCaptioning;

	DropDownMenu	*m_drpVoiceCommunication;
	DropDownMenu	*m_drpVoiceCommunicationStyle;
	SliderControl	*m_sldTransmitVolume;
	SliderControl	*m_sldRecieveVolume;
	DropDownMenu	*m_drpBoostMicrophoneGain;
	BaseModHybridButton	*m_btnTestMicrophone;

	vgui::ImagePanel	*m_pMicMeter;
	vgui::ImagePanel	*m_pMicMeter2;
	vgui::ImagePanel	*m_pMicMeterIndicator;

	BaseModHybridButton	*m_btnCancel;

	BaseModHybridButton	*m_btn3rdPartyCredits;

	ELanguage		m_nSelectedAudioLanguage;
	ELanguage		m_nCurrentAudioLanguage;
	int				m_nNumAudioLanguages;
	AudioLangauge_t	m_nAudioLanguages[ MAX_DYNAMIC_AUDIO_LANGUAGES ];

	vgui::DHANDLE<class COptionsSubAudioThirdPartyCreditsDlg> m_OptionsSubAudioThirdPartyCreditsDlg;

	static const char	*m_pchUpdatedAudioLanguage;
};

};

#endif
