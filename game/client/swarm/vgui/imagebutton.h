#ifndef _INCLUDED_IMAGE_BUTTON_H
#define _INCLUDED_IMAGE_BUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

namespace vgui
{
	class Label;
	class ImagePanel;
}

// a Button with an image panel backdrop and a label for the text

class ImageButton : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ImageButton, vgui::EditablePanel );
public:
	ImageButton(vgui::Panel *parent, const char *panelName, const char *text);
	ImageButton(vgui::Panel *parent, const char *panelName, const wchar_t *wszText);
	virtual ~ImageButton();
	virtual void ButtonInit();

	enum IB_ActivationType_t
	{
		ACTIVATE_ONRELEASED,
		ACTIVATE_ONPRESSED,
	};

	virtual void SetButtonTexture(const char *szFilename);
	virtual void SetButtonOverTexture(const char *szFilename);
	virtual void SetFractionalSize(float xfraction, float yfraction);
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual KeyValues *GetCommand();
	virtual void SetCancelCommand( KeyValues *message );
	virtual KeyValues *GetCancelCommand();
	virtual void FireActionSignal();
	virtual void FireCancelSignal();
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void SetCommand( KeyValues *message );
	virtual void SetText(const char *text);
	virtual void SetText(const wchar_t *text);
	virtual void SetCurrentTextColor(Color col);
	virtual void SetColors(Color ImageColor, Color DisabledImageColor, Color TextColor, Color DisabledTextColor, Color MouseOverTextColor);
	virtual void SetContentAlignment(vgui::Label::Alignment Alignment);
	virtual void SetButtonEnabled(bool bEnabled);
	virtual void UpdateColors();
	virtual void SetBorders(const char* szBorder, const char *szBorderDisabled);
	virtual void SetFont(vgui::HFont font);
	virtual void SetActivationType(IB_ActivationType_t t) { m_ActivationType = t; }	
	virtual void SetDoMouseOvers(bool bChange) { m_bDoMouseOvers = bChange; }

	IB_ActivationType_t m_ActivationType;
	vgui::ImagePanel *m_pBackdrop;
	vgui::Label *m_pLabel;
	vgui::HFont m_Font;
	char m_szButtonTexture[256];
	char m_szButtonOverTexture[256];
	char m_szBorder[128];
	char m_szBorderDisabled[128];
	bool m_bUsingOverTexture;
	bool m_bButtonEnabled;
	bool m_bDoMouseOvers;
	KeyValues		  *_actionMessage;
	KeyValues		  *_cancelMessage;
	Color m_ImageColor;
	Color m_DisabledImageColor;
	Color m_TextColor;
	Color m_DisabledTextColor;
	Color m_MouseOverTextColor;
};


#endif // _INCLUDED_IMAGE_BUTTON_H