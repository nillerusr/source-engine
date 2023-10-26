//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFLYOUTMENU_H__
#define __VFLYOUTMENU_H__

#include "basemodui.h"
#include "VGenericPanelList.h"

#define DEFAULT_STR_LEN 256

namespace BaseModUI
{

class BaseModHybridButton;

class FlyoutMenuListener
{
public:
	virtual void OnNotifyChildFocus( vgui::Panel* child ) = 0;
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo ) = 0;
	virtual void OnFlyoutMenuCancelled( ) = 0;
};

class FlyoutMenu : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( FlyoutMenu , vgui::EditablePanel );

public:
	static const int s_numSupportedChaptersFlyout = 15;

public:
	FlyoutMenu( vgui::Panel *parent, const char* panelName );
	~FlyoutMenu();

	void OpenMenu( vgui::Panel* flyFrom , vgui::Panel* initialSelection = NULL, bool reloadRes = false );
	void CloseMenu( vgui::Panel* flyTo );

	void SetListener( FlyoutMenuListener *listener );
	void NotifyChildFocus( vgui::Panel* child );
	vgui::Panel* GetLastChildNotified();
	vgui::Button* FindChildButtonByCommand( const char* command );
	vgui::Button* FindPrevChildButtonByCommand( const char* command );
	vgui::Button* FindNextChildButtonByCommand( const char* command );
	vgui::Panel* GetNavFrom( void ) { return m_navFrom; }

	void OnKeyCodePressed( vgui::KeyCode code );
	void OnCommand( const char* command );

	void SetInitialSelection( const char *szInitialSelection );

	void SetBGTall( int iTall );

	static FlyoutMenu *GetActiveMenu() { return sm_pActiveMenu; }
	static void CloseActiveMenu( vgui::Panel *pFlyTo = NULL );

	int GetOriginalTall() const;
	void SetOriginalTall( int t );

protected:
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void LoadControlSettings( const char *dialogResourceName, const char *pathID = NULL, KeyValues *pPreloadedKeyValues = NULL, KeyValues *pConditions = NULL );
	virtual void PaintBackground();

	vgui::Panel *m_navFrom; //the control that is 'attached' to the flyout menu
	vgui::Panel *m_defaultControl;
	vgui::Panel *m_lastChildNotified;

	FlyoutMenuListener* m_listener;

	static FlyoutMenu *sm_pActiveMenu;			// what menu is currently open

	int m_offsetX, m_offsetY;
	char m_resFile[ DEFAULT_STR_LEN ];
	char m_szInitialSelection[ MAX_PATH ];
	int m_FromOriginalTall;

	bool m_bOnlyActiveUser;
	bool m_bExpandUp;
	bool m_bUsingWideAtOpen;
};

};

#endif