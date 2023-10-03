//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VAUDIOVIDEO_H__
#define __VAUDIOVIDEO_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class DropDownMenu;
class SliderControl;

class AudioVideo : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( AudioVideo, CBaseModFrame );

public:
	AudioVideo(vgui::Panel *parent, const char *panelName);
	~AudioVideo();

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	Panel* NavigateBack();
	void ResetLanguage();

protected:
	virtual void Activate();
	virtual void OnThink();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );

private:
	static void AcceptLanguageChangeCallback();
	static void CancelLanguageChangeCallback();
	void UpdateFooter();

	SliderControl* m_sldBrightness;
	DropDownMenu* m_drpColorMode;
	SliderControl* m_sldFilmGrain;
	DropDownMenu* m_drpSplitScreenDirection;

	SliderControl* m_sldGameVolume;
	SliderControl* m_sldMusicVolume;
	DropDownMenu* m_drpLanguage;
	DropDownMenu* m_drpCaptioning;

	DropDownMenu* m_drpGore;

	bool m_bOldForceEnglishAudio;
	bool m_bDirtyVideoConfig;

};

};

#endif
