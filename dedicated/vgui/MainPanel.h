//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#ifndef CMAINPANEL_H
#define CMAINPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PHandle.h>
#include "utlvector.h"

//#include <GamePanelInfo.h>

#include "imanageserver.h"
//#include "gameserver.h"
#include "CreateMultiplayerGameServerPage.h"

class IAdminServer;

//-----------------------------------------------------------------------------
// Purpose: Root panel for dedicated server GUI
//-----------------------------------------------------------------------------
class CMainPanel : public vgui::Panel
{
public:
	// Construction/destruction
						CMainPanel( );
	virtual				~CMainPanel();

	virtual void		Initialize( );

	// displays the dialog, moves it into focus, updates if it has to
	virtual void		Open( void );

	// returns a pointer to a static instance of this dialog
	// valid for use only in sort functions
	static CMainPanel *GetInstance();
	virtual void StartServer(const char *cvars);

	void ActivateBuildMode();

	void *GetShutdownHandle() { return m_hShutdown; }

	void AddConsoleText(const char *msg);

	bool Stopping() { return m_bClosing; }

	bool IsInConfig() { return m_bIsInConfig; }

private:

	// called when dialog is shut down
	virtual void OnClose();
	virtual void OnTick();
	void DoStop();

	// GUI elements
	IManageServer *m_pGameServer;
	
	// the popup menu
	vgui::DHANDLE<vgui::ProgressBox> m_pProgressBox;
	CCreateMultiplayerGameServerPage *m_pConfigPage;

	// Event that lets the thread tell the main window it shutdown
	void *m_hShutdown;

	bool m_bStarting; // whether the server is currently starting
	bool m_bStarted; // whether the server has been started or not
	bool m_bClosing; // whether we are shutting down
	bool m_bIsInConfig;
	serveritem_t s1;
	int m_hResourceWaitHandle;
	float m_flPreviousSteamProgress;

	typedef vgui::Panel BaseClass;
	DECLARE_PANELMAP();

};

#endif // CMAINPANEL_H