//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEVIEWPORT_H
#define BASEVIEWPORT_H

// viewport interface for the rest of the dll
#include "game/client/iviewport.h"

#include <utlqueue.h> // a vector based queue template to manage our VGUI menu queue
#include "vgui_controls/Frame.h"
#include "vguitextwindow.h"
#include "vgui/isurface.h"
#include "commandmenu.h"
#include "igameevents.h"

using namespace vgui;

class IBaseFileSystem;
class IGameUIFuncs;
class IGameEventManager;

//==============================================================================
class CBaseViewport : public vgui::EditablePanel, public IViewPort, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CBaseViewport, vgui::EditablePanel );

public: 
	CBaseViewport();
	virtual ~CBaseViewport();

	virtual IViewPortPanel* CreatePanelByName(const char *szPanelName);
	virtual IViewPortPanel* FindPanelByName(const char *szPanelName);
	virtual IViewPortPanel* GetActivePanel( void );
	virtual void RemoveAllPanels( void);
	virtual void RecreatePanel( const char *szPanelName );

	virtual void ShowPanel( const char *pName, bool state, KeyValues *data, bool autoDeleteData );
	virtual void ShowPanel( const char *pName, bool state );
	virtual void ShowPanel( IViewPortPanel* pPanel, bool state );
	virtual bool AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName );
	virtual void CreateDefaultPanels( void );
	virtual void UpdateAllPanels( void );
	virtual void PostMessageToPanel( const char *pName, KeyValues *pKeyValues );

	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 *pGameEventManager );
	virtual void SetParent(vgui::VPANEL parent);

	virtual void ReloadScheme(const char *fromFile);
	virtual void ActivateClientUI();
	virtual void HideClientUI();
	virtual bool AllowedToPrintText( void );

	void LoadHudLayout( void );

	virtual vgui::VPANEL GetSchemeSizingVPanel( void );

#ifndef _XBOX
	virtual int GetViewPortScheme() { return m_pBackGround->GetScheme(); }
	virtual VPANEL GetViewPortPanel() { return m_pBackGround->GetVParent(); }
#endif
	virtual AnimationController *GetAnimationController() { return m_pAnimController; }

	virtual void ShowBackGround(bool bShow) 
	{ 
#ifndef _XBOX
		m_pBackGround->SetVisible( bShow ); 
#endif
	}

	virtual int GetDeathMessageStartHeight( void );	

	// virtual void ChatInputPosition( int *x, int *y );

public: // IGameEventListener:
	virtual void FireGameEvent( IGameEvent * event);


protected:

	bool LoadHudAnimations( void );

#ifndef _XBOX
	class CBackGroundPanel : public vgui::Frame
	{
	private:
		typedef vgui::Frame BaseClass;
	public:
		CBackGroundPanel( vgui::Panel *parent) : Frame( parent, "ViewPortBackGround" ) 
		{
			SetScheme("ClientScheme");

			SetTitleBarVisible( false );
			SetMoveable(false);
			SetSizeable(false);
			SetProportional(true);
		}
	private:

		virtual void ApplySchemeSettings(IScheme *pScheme)
		{
			BaseClass::ApplySchemeSettings(pScheme);
			SetBgColor(pScheme->GetColor("ViewportBG", Color( 0,0,0,0 ) )); 
		}

		virtual void PerformLayout() 
		{
			int w,h;
			GetHudSize(w, h);

			// fill the screen
			SetBounds(0,0,w,h);

			BaseClass::PerformLayout();
		}

		virtual void OnMousePressed(MouseCode code) { }// don't respond to mouse clicks
		virtual vgui::VPANEL IsWithinTraverse( int x, int y, bool traversePopups )
		{
			return NULL;
		}

	};
#endif
protected:

	virtual void Paint();
	virtual void OnThink(); 
	virtual void OnScreenSizeChanged(int iOldWide, int iOldTall);
	void PostMessageToPanel( IViewPortPanel* pPanel, KeyValues *pKeyValues );

	void SetAsFullscreenViewportInterface( void );
	bool IsFullscreenViewport() const;

protected:
	IGameUIFuncs*		m_GameuiFuncs; // for key binding details
	IGameEventManager2*	m_GameEventManager;
#ifndef _XBOX
	CBackGroundPanel	*m_pBackGround;
#endif
	CUtlDict<IViewPortPanel*,int>	m_Panels;
	CUtlVector< IViewPortPanel* >	m_UnorderedPanels;

	bool				m_bHasParent; // Used to track if child windows have parents or not.
	bool				m_bInitialized;
	bool				m_bFullscreenViewport;
	IViewPortPanel		*m_pActivePanel;
	IViewPortPanel		*m_pLastActivePanel;
	vgui::HCursor		m_hCursorNone;
	vgui::AnimationController *m_pAnimController;
	int					m_OldSize[2];

private:
	virtual void InitViewportSingletons( void );
};


#endif // BASEVIEWPORT_H
