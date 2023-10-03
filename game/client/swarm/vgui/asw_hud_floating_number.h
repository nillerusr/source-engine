#ifndef ASW_HUD_FLOATING_NUMBER_H
#define ASW_HUD_FLOATING_NUMBER_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Label.h>

namespace vgui
{
	class ImagePanel;
};

//---------------------------------------------------------------------------------------------
// Purpose:  Scrolls a floating number up the screen
//---------------------------------------------------------------------------------------------

enum floating_number_directions
{
	FN_DIR_UP,
	FN_DIR_DOWN,
	FN_DIR_LEFT,
	FN_DIR_RIGHT,
};

struct floating_number_params_t
{
	int x;
	int y;
	vgui::Label::Alignment alignment;
	floating_number_directions iDir;
	vgui::Panel* pParent;
	vgui::HFont hFont;
	Color rgbColor;
	float flStartDelay;
	float flMoveDuration;
	float flFadeDuration;
	float flFadeStart;
	bool bShowPlus;
	bool bWorldSpace;
	Vector vecPos;

	floating_number_params_t( void )
	{
		x = 0;
		y = 0;
		bShowPlus = true;
		flMoveDuration = 2.0f;
		flFadeStart = 2.0f;
		flFadeDuration = 1.0f;
		alignment = vgui::Label::a_west;
		iDir = FN_DIR_UP;
		hFont = NULL;
		rgbColor = Color( 128, 128, 128, 255 );
		flStartDelay = 0.0f;
		bWorldSpace = false;
		vecPos = vec3_origin;
	}
};

class CFloatingNumber : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CFloatingNumber, vgui::EditablePanel );
public:
	CFloatingNumber( int iProgress, const floating_number_params_t &params, vgui::Panel* pParent );
	CFloatingNumber( const char *pchText, const floating_number_params_t &params, vgui::Panel* pParent );

	virtual ~CFloatingNumber();

	void Initialize( const char *pchText );
	
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();

protected:
	void PositionNumber( int x, int y );
	void PositionWorldSpaceNumber();

	vgui::Label *m_pNumberLabel;
	floating_number_params_t m_params;
	float m_fStartTime;
	int m_iTextWide;
	int m_iTextTall;

	CPanelAnimationVarAliasType( int, m_iScrollDistance, "ScrollDistance", "40", "proportional_int" );		// how far the floating number will scroll up before disappearing completely
};

#endif	// ASW_HUD_FLOATING_NUMBER_H