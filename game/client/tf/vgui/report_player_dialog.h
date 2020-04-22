//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef REPORT_PLAYER_DIALOG_H
#define REPORT_PLAYER_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ComboBox.h"

class CReportPlayerDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CReportPlayerDialog, vgui::Frame );

public:
	CReportPlayerDialog( vgui::Panel *parent );
	~CReportPlayerDialog();

	virtual void Activate();

private:
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );
	virtual void OnCommand( const char *command );

	void ReportPlayer();
	void RefreshPlayerProperties();
	bool IsValidPlayerSelected();

	void OnKeyCodePressed( vgui::KeyCode code )
	{
		if ( code == KEY_XBUTTON_B )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed( code );
		}
	}

	vgui::ListPanel *m_pPlayerList;
	vgui::Button *m_pReportButton;
	vgui::ComboBox *m_pReasonBox;
};

bool ReportPlayerAccount( CSteamID steamID, int nReason );

#endif // REPORT_PLAYER_DIALOG_H
