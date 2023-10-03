//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Tip display during level loads.
//
//===========================================================================//

#ifndef LOADING_TIP_PANEL_H
#define LOADING_TIP_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/imagepanel.h"
#include "vgui_controls/editablepanel.h"
#include "vgui_controls/label.h"

#define MAX_TIP_LENGTH 64

struct sTipInfo
{
	char szTipTitle[MAX_TIP_LENGTH];
	char szTipString[MAX_TIP_LENGTH];
	char szTipImage[MAX_TIP_LENGTH];
};

enum eTipMode
{
	TIP_MODE_SURVIVOR,
	TIP_MODE_INFECTED,
	TIP_MODE_ACHIEVEMENTS,

	TIP_MODE_COUNT,
};

class CLoadingTipPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CLoadingTipPanel, vgui::EditablePanel )

public:
	CLoadingTipPanel( Panel *pParent );
	~CLoadingTipPanel();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground( void );
	void ReloadScheme( void );

	void NextTip( void );

private:
	void SetupTips( void );
	int	 DrawSmearBackgroundFade( int x, int y, int wide, int tall );

	Color m_smearColor;

	vgui::ImagePanel *m_pTipIcon;

	CUtlVector< sTipInfo > m_Tips;

	float m_flLastTipTime;
	int m_iCurrentTip;
};

void PrecacheLoadingTipIcons();


#endif // LOADING_TIP_PANEL_H
