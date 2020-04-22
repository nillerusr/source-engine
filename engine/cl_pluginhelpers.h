//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//=============================================================================//

//-----------------------------------------------------------------------------
// Purpose: the plugin message handler
//-----------------------------------------------------------------------------
#include <vgui_controls/Panel.h>
#include "engine/iserverplugin.h"
#include "netmessages.h"

class CPluginGameUIDialog;
class CPluginHudMessage;

class CPluginUIManager : public vgui::Panel
{
private:
	DECLARE_CLASS_SIMPLE( CPluginUIManager, vgui::Panel );

public:
	CPluginUIManager();
	~CPluginUIManager();

	void Show( DIALOG_TYPE type, KeyValues *kv );
	void OnPanelClosed();
	void Shutdown();

	void GetHudMessagePosition( int &x, int &y, int &wide, int &tall ); // Gets the position of the plugin HUD message. The askconnect dialog is placed here.

protected:
	void OnTick();


	int m_iCurPriority;
	int m_iMessageDisplayUntil;
	int m_iHudDisplayUntil;

	bool m_bShutdown;

	CPluginGameUIDialog *m_pGameUIDialog;
	CPluginHudMessage *m_pHudMessage;
};

extern CPluginUIManager *g_PluginManager;


void PluginHelpers_Menu( SVC_Menu *msg );

