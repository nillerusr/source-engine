//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_FOGUIPANEL_H
#define CL_FOGUIPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>


namespace vgui
{
class Button;
class TextEntry;
class CheckButton;
class Label;
class Slider;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CFogUIPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFogUIPanel, vgui::Frame );

public:
	CFogUIPanel( vgui::Panel *parent );
	~CFogUIPanel();

	virtual void OnTick();

	// Command issued
	virtual void	OnCommand(const char *command);
	virtual void	OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	static	void	InstallFogUI( vgui::Panel *parent );
	
	void			UpdateFogStartSlider();
	void			UpdateFogEndSlider();

	void			UpdateFogColorRedSlider();
	void			UpdateFogColorGreenSlider();
	void			UpdateFogColorBlueSlider();
	
	void			UpdateFogColors();

	void			UpdateFarZSlider();

protected:
	
	MESSAGE_FUNC_PARAMS( OnTextNewLine, "TextNewLine", data );
	MESSAGE_FUNC_PARAMS( OnTextKillFocus, "TextKillFocus", data );

	void			HandleInput( bool active );

	// World fog
	vgui::CheckButton	*m_pFogOverride;
	vgui::CheckButton	*m_pFogEnable;

	vgui::Slider		*m_pFogStart;
	vgui::TextEntry		*m_pFogStartText;
	vgui::Slider		*m_pFogEnd;
	vgui::TextEntry		*m_pFogEndText;

	vgui::Slider		*m_pFogColorRed;
	vgui::TextEntry		*m_pFogColorRedText;
	vgui::Slider		*m_pFogColorGreen;
	vgui::TextEntry		*m_pFogColorGreenText;
	vgui::Slider		*m_pFogColorBlue;
	vgui::TextEntry		*m_pFogColorBlueText;

	// Skybox fog
	vgui::CheckButton	*m_pFogEnableSky;

	vgui::Slider		*m_pFogStartSky;
	vgui::TextEntry		*m_pFogStartTextSky;
	vgui::Slider		*m_pFogEndSky;
	vgui::TextEntry		*m_pFogEndTextSky;

	vgui::Slider		*m_pFogColorRedSky;
	vgui::TextEntry		*m_pFogColorRedTextSky;
	vgui::Slider		*m_pFogColorGreenSky;
	vgui::TextEntry		*m_pFogColorGreenTextSky;
	vgui::Slider		*m_pFogColorBlueSky;
	vgui::TextEntry		*m_pFogColorBlueTextSky;

	// FarZ
	vgui::CheckButton	*m_pFarZOverride;
	vgui::Slider		*m_pFarZ;
	vgui::TextEntry		*m_pFarZText;
};

extern CFogUIPanel *g_pFogUI;

#endif // CL_FOGUIPANEL_H
