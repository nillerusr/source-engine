//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODWINPANEL_H
#define DODWINPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"

#include "dod_shareddefs.h"

using namespace vgui;

class CDODWinPanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CDODWinPanel, EditablePanel );

public:
	CDODWinPanel( const char *pElementName, int iTeam );

	virtual void Reset();
	virtual void Init();
	virtual void VidInit();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void FireGameEvent( IGameEvent * event );

	void Show();
	void Hide();

	virtual bool ShouldDraw( void );
	virtual void Paint( void );

	void SetupAvatar( const char *pSide, int pos, int iPlayerIndex );

protected:
	void SetFinalCaptureLabel( const char *szCappers, bool bAvatar );
	void DrawText( char *text, int x, int y, Color clrText );

	CHudTexture *GetIconForCategory( int category );

	CHudTexture *m_pIcon;

	CHudTexture *m_pIconCap;
	CHudTexture *m_pIconDefended;
	CHudTexture *m_pIconBomb;
	CHudTexture *m_pIconKill;

	vgui::Label *m_pLastCapperHeader;
	vgui::Label *m_pLastBomberHeader;

	vgui::Label *m_pLastCapperLabel;		// list of names of the final cappers
	vgui::Label *m_pLastCapperLabel_Avatar;

	vgui::Label *m_pTimerStatusLabel;		// shows time remaining or elapsed at round end

	int m_iLeftCategory;
	vgui::Label *m_pLeftCategoryHeader;
	vgui::Label *m_pLeftCategoryLabels[3];
	int m_iLeftCategoryScores[3];

	int m_iRightCategory;
	vgui::Label *m_pRightCategoryHeader;
	vgui::Label *m_pRightCategoryLabels[3];
	int m_iRightCategoryScores[3];

	bool m_bShowTimerDefend;
	bool m_bShowTimerAttack;
	int m_iTimerTime;

	int m_iFinalEventType;



private:
	CPanelAnimationVarAliasType( int, m_iIconY, "icon_ypos", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconX_left, "icon_xpos_left", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconX_right, "icon_xpos_right", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconW, "icon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconH, "icon_h", "0", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_iIconSize, "icon_stat_size", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconLeftX, "icon_left_stat_x", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconLeftY1, "icon_left_stat_y1", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconLeftY2, "icon_left_stat_y2", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconLeftY3, "icon_left_stat_y3", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconRightX, "icon_right_stat_x", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconRightY1, "icon_right_stat_y1", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconRightY2, "icon_right_stat_y2", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconRightY3, "icon_right_stat_y3", "0", "proportional_int" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );

	int m_iTeam;
};

class CDODWinPanel_Allies : public CDODWinPanel
{
private:
	DECLARE_CLASS_SIMPLE( CDODWinPanel_Allies, CDODWinPanel );

public:
	CDODWinPanel_Allies( const char *pElementName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );
};

class CDODWinPanel_Axis : public CDODWinPanel
{
private:
	DECLARE_CLASS_SIMPLE( CDODWinPanel_Axis, CDODWinPanel );

public:
	CDODWinPanel_Axis( const char *pElementName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );
};

#endif //DODWINPANEL_H