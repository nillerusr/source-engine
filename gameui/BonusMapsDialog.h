//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BONUSMAPSDIALOG_H
#define BONUSMAPSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"

#include "BonusMapsDatabase.h"


//-----------------------------------------------------------------------------
// Purpose: Displays and loads available bonus maps
//-----------------------------------------------------------------------------
class CBonusMapsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBonusMapsDialog, vgui::Frame );

public:
	CBonusMapsDialog(vgui::Panel *parent);
	~CBonusMapsDialog();

	void SetSelectedBooleanStatus( const char *pchName, bool bValue );
	void RefreshData( void );

	int GetSelectedChallenge( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *command );

	void OnKeyCodeTyped( vgui::KeyCode code );
	void OnKeyCodePressed( vgui::KeyCode code );

private:
	bool ImportZippedBonusMaps( const char *pchZippedFileName );

	void BuildMapsList( void );

	void CreateBonusMapsList();
	int GetSelectedItemBonusMapIndex();

	void RefreshDialog( BonusMapDescription_t *pMap );
	void RefreshMedalDisplay( BonusMapDescription_t *pMap );
	void RefreshCompletionPercentage( void );

	MESSAGE_FUNC( OnPanelSelected, "PanelSelected" );
	MESSAGE_FUNC( OnControlModified, "ControlModified" );
	MESSAGE_FUNC( OnTextChanged, "TextChanged" )
	{
		OnControlModified();
	}
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

private:
	Color		m_PercentageBarBackgroundColor, m_PercentageBarColor;

	vgui::FileOpenDialog	*m_hImportBonusMapsDialog;
	vgui::PanelListPanel	*m_pGameList;
	vgui::ComboBox			*m_pChallengeSelection;
	vgui::ImagePanel		*m_pPercentageBarBackground;
	vgui::ImagePanel		*m_pPercentageBar;
};


extern CBonusMapsDialog *g_pBonusMapsDialog;


#endif // BONUSMAPSDIALOG_H
