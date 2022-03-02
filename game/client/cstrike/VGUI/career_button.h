//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CAREER_BUTTON_H
#define CAREER_BUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui/KeyCode.h>

//--------------------------------------------------------------------------------------------------------------
/**
 *  This class adds border functionality necessary for next/prev/start buttons on map and bot screens
 */
class CCareerButton : public vgui::Button
{
public:
	CCareerButton(vgui::Panel *parent, const char *buttonName, const char *buttonText, const char *image, bool textFirst );

	// Set armed button border attributes.  Added in CCareerButton.
	virtual void SetArmedBorder(vgui::IBorder *border);

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void Paint();

	void SetImage( const char *image );

protected:

	// Get button border attributes.
	virtual vgui::IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus);

	vgui::IBorder *m_armedBorder;
	vgui::IImage *m_image;
	vgui::TextImage *m_textImage;
	bool m_textFirst;
	int m_textPad;
	int m_imagePad;

	Color m_textNormalColor;
	Color m_textDisabledColor;

private:
	typedef vgui::Button BaseClass;
};

#endif // CAREER_BUTTON_H
