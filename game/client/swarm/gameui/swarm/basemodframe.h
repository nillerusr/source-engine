//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __BASEMODFRAME_H__
#define __BASEMODFRAME_H__

#include "vgui_controls/Panel.h"
#include "vgui_controls/Frame.h"
#include "tier1/utllinkedlist.h"

#include "basemodpanel.h"

namespace BaseModUI {

	struct DialogMetrics_t
	{
		int titleY;
		int titleHeight;
		int dialogY;
		int dialogHeight;
	};

	//=============================================================================
	//
	//=============================================================================
	class IBaseModFrameListener
	{
	public:
		virtual void RunFrame() = 0;
	};

	//=============================================================================
	//
	//=============================================================================
	class CBaseModFrame : public vgui::Frame
	{
		DECLARE_CLASS_SIMPLE( CBaseModFrame, vgui::Frame );

	public:
		CBaseModFrame( vgui::Panel *parent, const char *panelName, bool okButtonEnabled = true, 
			bool cancelButtonEnabled = true, bool imgBloodSplatterEnabled = true, bool doButtonEnabled = true );
		virtual ~CBaseModFrame();

		virtual void SetTitle(const char *title, bool surfaceTitle);
		virtual void SetTitle(const wchar_t *title, bool surfaceTitle);
		
		virtual void SetDataSettings( KeyValues *pSettings );

		virtual void LoadLayout();

		void ReloadSettings();

		virtual void OnKeyCodePressed(vgui::KeyCode code);
#ifndef _X360
		virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif
		virtual void OnMousePressed( vgui::MouseCode code );

		virtual void OnOpen();
		virtual void OnClose();

		virtual vgui::Panel *NavigateBack();
		CBaseModFrame *SetNavBack( CBaseModFrame* navBack );
		CBaseModFrame *GetNavBack() { return m_NavBack.Get(); }	
		bool CanNavBack() { return m_bCanNavBack; }

		virtual void PostChildPaint();
		virtual void PaintBackground();

		virtual void FindAndSetActiveControl();

		virtual void RunFrame();

		// Load the control settings 
		virtual void LoadControlSettings( const char *dialogResourceName, const char *pathID = NULL, KeyValues *pPreloadedKeyValues = NULL, KeyValues *pConditions = NULL );

		virtual void SetHelpText(const char* helpText);

		MESSAGE_FUNC_CHARPTR( OnNavigateTo, "OnNavigateTo", panelName );

		static void AddFrameListener( IBaseModFrameListener * frameListener );
		static void RemoveFrameListener( IBaseModFrameListener * frameListener );
		static void RunFrameOnListeners();

		virtual bool GetLowerGarnishEnabled();

		void CloseWithoutFade();

		void ToggleTitleSafeBorder();

		void PushModalInputFocus();				// Makes this panel take modal input focus and maintains stack of previous panels with focus.  For PC only.
		void PopModalInputFocus();				// Removes modal input focus and gives focus to previous panel on stack. For PC only.

		bool CheckAndDisplayErrorIfNotLoggedIn();	// Displays error if not logged into Steam (no-op on X360)

		void DrawGenericBackground();
		void DrawDialogBackground( const char *pMajor, const wchar_t *pMajorFormatted, const char *pMinor, const wchar_t *pMinorFormatted, DialogMetrics_t *pMetrics = NULL, bool bAllCapsTitle = false, int iTitleXOffset = INT_MAX );
		void SetupAsDialogStyle();
		int DrawSmearBackground( int x, int y, int wide, int tall, bool bIsFooter = false );
			
	protected:
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void ApplySettings(KeyValues *inResourceData);

		virtual void PostApplySettings();

		void SetOkButtonEnabled(bool enabled);
		void SetCancelButtonEnabled(bool enabled);
		void SetUpperGarnishEnabled(bool enabled);
		void SetLowerGarnishEnabled(bool enabled);
		void SetBloodSplatterImageEnabled( bool enabled );


	protected:
		static CUtlVector< IBaseModFrameListener * > m_FrameListeners;
		static bool m_DrawTitleSafeBorder;

		vgui::Panel* m_ActiveControl;

		int m_TitleInsetX;
		int m_TitleInsetY;
		vgui::Label* m_LblTitle;

		bool m_LowerGarnishEnabled;

		bool m_UpperGarnishEnabled;

		bool m_OkButtonEnabled;
		bool m_CancelButtonEnabled;
		bool m_bLayoutLoaded;
		bool m_bIsFullScreen;
		bool m_bDelayPushModalInputFocus;		// set to true if we need to consider taking modal input focus, but can't do it yet

		char m_ResourceName[64];

	private:
		friend class CBaseModPanel;

		void SetWindowType(WINDOW_TYPE windowType);
		WINDOW_TYPE GetWindowType();

		void SetWindowPriority( WINDOW_PRIORITY pri );
		WINDOW_PRIORITY GetWindowPriority();

		void SetCanBeActiveWindowType(bool allowed);
		bool GetCanBeActiveWindowType();

		WINDOW_TYPE m_WindowType;
		WINDOW_PRIORITY m_WindowPriority;
		bool m_CanBeActiveWindowType;
		vgui::DHANDLE<CBaseModFrame> m_NavBack;			// panel to nav back to
		bool m_bCanNavBack;							// can we nav back: use this to distinguish between "no panel set" vs "panel has gone away" for nav back
		CUtlVector<vgui::HPanel> m_vecModalInputFocusStack;

		int m_hOriginalTall;

		int m_nTopBorderImageId;
		int m_nBottomBorderImageId;
		Color m_smearColor;

	protected:
		CPanelAnimationVarAliasType( int, m_iHeaderY, "header_y", "19", "proportional_int" );
		CPanelAnimationVarAliasType( int, m_iHeaderTall, "header_tall", "41", "proportional_int" );
		CPanelAnimationVarAliasType( int, m_iTitleXOffset, "header_title_x", "180", "proportional_int" );		// pixels left of center
	};
};

#endif
