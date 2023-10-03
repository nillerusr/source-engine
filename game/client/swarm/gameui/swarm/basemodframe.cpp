//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <ctype.h>
#include "basemodframe.h"
#include "basemodpanel.h"
#include "EngineInterface.h"

#include "VFooterPanel.h"
#include "VGenericConfirmation.h"
#include "VFlyoutMenu.h"
#include "IGameUIFuncs.h"

// vgui controls
#include <vgui/IVGui.h>
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ilocalize.h"

#include "filesystem.h"
#include "fmtstr.h"
#include "cdll_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern class IMatchSystem *matchsystem;
extern const char *COM_GetModDirectory( void );
extern IGameUIFuncs *gameuifuncs;

//=============================================================================
//
//=============================================================================
CUtlVector< IBaseModFrameListener * > CBaseModFrame::m_FrameListeners;

bool CBaseModFrame::m_DrawTitleSafeBorder = false;

ConVar ui_gameui_modal( "ui_gameui_modal", "1", FCVAR_RELEASE, "If set, the game UI pages will take modal input focus." );

//=============================================================================
CBaseModFrame::CBaseModFrame( vgui::Panel *parent, const char *panelName, bool okButtonEnabled, 
	bool cancelButtonEnabled, bool imgBloodSplatterEnabled, bool doButtonEnabled ):
		BaseClass(parent, panelName, true, false ),
		m_ActiveControl(0),
		m_UpperGarnishEnabled(false),
		m_LowerGarnishEnabled(false),
		m_OkButtonEnabled(okButtonEnabled),
		m_CancelButtonEnabled(cancelButtonEnabled),
		m_WindowType(WT_NONE),
		m_WindowPriority(WPRI_NORMAL),
		m_CanBeActiveWindowType(false)
{
	m_TitleInsetX = 6;
	m_TitleInsetY = 4;
	m_bIsFullScreen = false;
	m_bLayoutLoaded = false;
	m_bDelayPushModalInputFocus = false;

	SetConsoleStylePanel( true );

	// bodge to disable the frames title image and display our own 
	//(the frames title has an invalid zpos and does not draw over the garnish)
	Frame::SetTitle("", false);
	m_LblTitle = new Label(this, "LblTitle", "");

	Q_snprintf(m_ResourceName, sizeof( m_ResourceName ), "Resource/UI/BaseModUI/%s.res", panelName);

#ifdef _X360
	m_PassUnhandledInput = false;
#endif
	m_NavBack = NULL;
	m_bCanNavBack = false;

	SetScheme(CBaseModPanel::GetSingleton().GetScheme());

	DisableFadeEffect();

	// used for dialog styles which are artificially larger to allow dropdowns
	m_hOriginalTall = 0;

	m_nTopBorderImageId = -1;
	m_nBottomBorderImageId = -1;
}

//=============================================================================
CBaseModFrame::~CBaseModFrame()
{
	// Purposely not destroying our texture IDs, they are finds.
	// The naive create/destroy pattern causes excessive i/o for no benefit.
	// These images will always have to be there anyways.

	delete m_LblTitle;
}

//=============================================================================
void CBaseModFrame::SetTitle(const char *title, bool surfaceTitle)
{
	m_LblTitle->SetText(title);

	int wide, tall;
	m_LblTitle->GetContentSize(wide, tall);
	m_LblTitle->SetSize(wide, tall);
}

//=============================================================================
void CBaseModFrame::SetTitle(const wchar_t *title, bool surfaceTitle)
{
	m_LblTitle->SetText(title);

	int wide, tall;
	m_LblTitle->GetContentSize(wide, tall);
	m_LblTitle->SetSize(wide, tall);
}

//=============================================================================
void CBaseModFrame::LoadLayout()
{
	// default alignment of title text, can be overriden in res file
	m_LblTitle->SetPos( m_TitleInsetX, m_TitleInsetY );

	int wide, tall;
	m_LblTitle->GetContentSize( wide, tall );
	m_LblTitle->SetSize( wide, tall );

	LoadControlSettings( m_ResourceName );
	MakeReadyForUse();

	SetTitleBarVisible(true);
	SetMoveable(false);
	SetCloseButtonVisible(false);
	SetMinimizeButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetMenuButtonVisible(false);
	SetMinimizeToSysTrayButtonVisible(false);
	SetSizeable(false);

	// determine if we're full screen
	int screenwide,screentall;
	vgui::surface()->GetScreenSize( screenwide, screentall );
	if ( ( GetWide() == screenwide ) && ( GetTall() == screentall ) )
	{
		m_bIsFullScreen = true;
	}

	m_bLayoutLoaded = true;

	// if we were supposed to take modal input focus, do it now
	if ( m_bDelayPushModalInputFocus )
	{
		PushModalInputFocus();
		m_bDelayPushModalInputFocus = false;
	}
}

//=============================================================================

void CBaseModFrame::SetDataSettings( KeyValues *pSettings )
{
	return;
}

void CBaseModFrame::ReloadSettings()
{
	LoadControlSettings( m_ResourceName );
	InvalidateLayout( false, true );
	MakeReadyForUse();
}

//=============================================================================
void CBaseModFrame::OnKeyCodePressed(KeyCode keycode)
{
	int lastUser = GetJoystickForCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( lastUser );

	vgui::KeyCode code = GetBaseButtonCode( keycode );

	switch(code)
	{
	case KEY_XBUTTON_A:
		if(m_OkButtonEnabled)
		{
			BaseClass::OnKeyCodePressed(keycode);
		}
		break;
	case KEY_XBUTTON_B:
		if(m_CancelButtonEnabled)
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			NavigateBack();
		}
		break;
	case KEY_XBUTTON_STICK1:
#ifdef BASEMOD360UI_DEBUG
		ToggleTitleSafeBorder();
#endif
		break;
	case KEY_XBUTTON_STICK2:
#ifdef BASEMOD360UI_DEBUG
		{
			//DEBUG: Remove this later
			CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
			if( footer )
			{
				footer->LoadLayout();
			}

			LoadLayout();
			//ENDDEBUG
			break;
		}
#endif
	default:
		BaseClass::OnKeyCodePressed(keycode);
		break;
	}

	// HACK: Allow F key bindings to operate even here
	if ( IsPC() && keycode >= KEY_F1 && keycode <= KEY_F12 )
	{
		// See if there is a binding for the FKey
		const char *binding = gameuifuncs->GetBindingForButtonCode( keycode );
		if ( binding && binding[0] )
		{
			// submit the entry as a console commmand
			char szCommand[256];
			Q_strncpy( szCommand, binding, sizeof( szCommand ) );
			engine->ClientCmd_Unrestricted( szCommand );
		}
	}
}

#ifndef _X360
void CBaseModFrame::OnKeyCodeTyped( vgui::KeyCode code )
{
	// For PC, this maps space bar to OK and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;

	case KEY_ESCAPE:
		// close active menu if there is one, else navigate back
		if ( FlyoutMenu::GetActiveMenu() )
		{
			FlyoutMenu::CloseActiveMenu( FlyoutMenu::GetActiveMenu()->GetNavFrom() );
		}
		else
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
		break;
	}

	BaseClass::OnKeyTyped( code );}
#endif


void CBaseModFrame::OnMousePressed( vgui::MouseCode code )
{
	if( FlyoutMenu::GetActiveMenu() )
	{
		FlyoutMenu::CloseActiveMenu( FlyoutMenu::GetActiveMenu()->GetNavFrom() );
	}
	BaseClass::OnMousePressed( code );
}

//=============================================================================
void CBaseModFrame::OnOpen()
{
	Panel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		if ( GetLowerGarnishEnabled() )
		{
			pFooter->SetVisible( true );
			pFooter->MoveToFront();
		}
		else
		{
			pFooter->SetVisible( false );
		}	
	}
	
	SetAlpha(255);//make sure we show up.
	Activate();

#ifdef _X360
	if(m_ActiveControl != 0)
	{
		m_ActiveControl->NavigateTo();
	}
#endif // _X360

	// close active menu if there is one
	FlyoutMenu::CloseActiveMenu();

	if ( ui_gameui_modal.GetBool() )
	{
		PushModalInputFocus();
	}
}

//======================================================================\=======
void CBaseModFrame::OnClose()
{
	FindAndSetActiveControl();

	if ( ui_gameui_modal.GetBool() )
	{
		PopModalInputFocus();
	}

	WINDOW_PRIORITY pri = GetWindowPriority();
	WINDOW_TYPE wt = GetWindowType();
	bool bLetBaseKnowOnFrameClosed = true;

	if ( wt == WT_GENERICWAITSCREEN )
	{
		CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().GetWindow( wt );
		if ( pFrame && pFrame != this )
		{
			// Hack for generic waitscreen closing after another
			// waitscreen spawned
			bLetBaseKnowOnFrameClosed = false;
		}
	}

	BaseClass::OnClose();

	if ( bLetBaseKnowOnFrameClosed )
	{
		CBaseModPanel::GetSingleton().OnFrameClosed( pri, wt );
	}
}

//=============================================================================
Panel* CBaseModFrame::NavigateBack()
{
	// don't do anything if we have nowhere to navigate back to
	if ( !CanNavBack() )
		return NULL;

	CBaseModFrame* navBack = GetNavBack();
	CBaseModFrame* mainMenu = CBaseModPanel::GetSingleton().GetWindow(WT_MAINMENU);

	// fix to cause panels navigating back to the main menu to close without
	// fading so weird blends do not occur
	bool bDisableFade = (mainMenu != NULL && GetNavBack() == mainMenu);

	if ( bDisableFade )
	{
		SetFadeEffectDisableOverride(true);
	}

	Close();

	// If our nav back pointer is NULL, that means the thing we are supposed to nav back to has closed.  Just nav back to 
	// the topmost active window.
	if ( !navBack )
	{
		WINDOW_TYPE wt = CBaseModPanel::GetSingleton().GetActiveWindowType();
		if ( wt > WT_NONE )
		{
			navBack = CBaseModPanel::GetSingleton().GetWindow( wt );
		}
	}

	if ( navBack )
	{
		navBack->SetVisible( true );
		CBaseModPanel::GetSingleton().SetActiveWindow( navBack );
	}	

	if ( bDisableFade )
	{
		SetFadeEffectDisableOverride(false);
	}

	return navBack;
}

//=============================================================================
void CBaseModFrame::PostChildPaint()
{
	BaseClass::PostChildPaint();

	if(m_DrawTitleSafeBorder)
	{
		int screenwide, screenTall;
		surface()->GetScreenSize(screenwide, screenTall);

		int titleSafeWide, titleSafeTall;

		int x, y, wide, tall;
		GetBounds(x, y, wide, tall);

		// the 90 percent border (5 percent on each side)
		titleSafeWide = screenwide * 0.05f;
		titleSafeTall = screenTall * 0.05f;

		if( (titleSafeWide >= x)
		 && (titleSafeTall >= y) 
		 && ((screenwide - titleSafeWide) <= (wide - x))
		 && ((screenTall - titleSafeTall) <= (tall - y)) )
		{
			surface()->DrawSetColor(255, 0, 0, 255);
			surface()->DrawOutlinedRect(titleSafeWide - x, titleSafeTall - y, 
				(screenwide - titleSafeWide) - x, (screenTall - titleSafeTall) - y);
		}

		// the 80 percent border (10 percent on each side)
		titleSafeWide = screenwide * 0.1f;
		titleSafeTall = screenTall * 0.1f;

		if( (titleSafeWide >= x)
		 && (titleSafeTall >= y) 
		 && ((screenwide - titleSafeWide) <= (wide - x))
		 && ((screenTall - titleSafeTall) <= (tall - y)) )
		{
			surface()->DrawSetColor(255, 255, 0, 255);
			surface()->DrawOutlinedRect(titleSafeWide - x, titleSafeTall - y, 
				(screenwide - titleSafeWide) - x, (screenTall - titleSafeTall) - y);
		}
	}
}

//=============================================================================
void CBaseModFrame::PaintBackground()
{
	BaseClass::PaintBackground();
}

//=============================================================================
void CBaseModFrame::FindAndSetActiveControl()
{
	/*
	m_ActiveControl = NULL;
	for(int i = 0; i < GetChildCount(); ++i)
	{
		if(GetChild(i)->HasFocus())
		{
			m_ActiveControl = GetChild(i);
		}
	}
	*/
}

//=============================================================================
void CBaseModFrame::RunFrame()
{
}

//=============================================================================
void CBaseModFrame::SetHelpText(const char* helpText)
{
	BaseModUI::CBaseModPanel::GetSingleton().SetHelpText( helpText );
}

//-----------------------------------------------------------------------------
void CBaseModFrame::OnNavigateTo( const char* panelName )
{
	for(int i = 0; i < GetChildCount(); ++i)
	{
		Panel* child = GetChild(i);
		if(child != NULL && (!Q_strcmp(panelName, child->GetName())))
		{
			const char* helpText = child->GetTooltip()->GetText();
			if(helpText && *helpText)
			{
				SetHelpText(helpText);
			}
			else
			{
				SetHelpText("");
			}
						
			m_ActiveControl = child;
			break;
		}
	}
}

//=============================================================================
void CBaseModFrame::AddFrameListener( IBaseModFrameListener * frameListener )
{
	if(m_FrameListeners.Find(frameListener) == -1)
	{
		int index = m_FrameListeners.AddToTail();
		m_FrameListeners[index] = frameListener;
	}
}

//=============================================================================
void CBaseModFrame::RemoveFrameListener( IBaseModFrameListener * frameListener )
{
	m_FrameListeners.FindAndRemove(frameListener);
}

//=============================================================================
void CBaseModFrame::RunFrameOnListeners()
{
	for(int i = 0; i < m_FrameListeners.Count(); ++i)
	{
		m_FrameListeners[i]->RunFrame();
	}
}

//=============================================================================
bool CBaseModFrame::GetLowerGarnishEnabled()
{
	return m_LowerGarnishEnabled;
}

//=============================================================================
void CBaseModFrame::CloseWithoutFade()
{
	SetFadeEffectDisableOverride(true);

	FindAndSetActiveControl();

	Close();

	SetFadeEffectDisableOverride(false);
}

//=============================================================================
void CBaseModFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	const char *pFont = NULL;
	pFont = pScheme->GetResourceString( "FrameTitleBar.Font" );
	m_LblTitle->SetFont( pScheme->GetFont( (pFont && *pFont) ? pFont : "Default", IsProportional() ) );
	
	const char *pResourceString = pScheme->GetResourceString( "Frame.TitleTextInsetX" );
	if ( pResourceString )
	{
		m_TitleInsetX = atoi( pResourceString );
	}

	pResourceString = pScheme->GetResourceString( "Frame.TitleTextInsetY" );
	if ( pResourceString )
	{
		m_TitleInsetY = atoi( pResourceString );
	}

	const char *pTopImageName = pScheme->GetResourceString( "Frame.TopBorderImage" );
	m_nTopBorderImageId = vgui::surface()->DrawGetTextureId( pTopImageName );
	if ( m_nTopBorderImageId == -1 )
	{
		m_nTopBorderImageId = vgui::surface()->CreateNewTextureID();
		// @TODO: why is this string empty?
		if ( pTopImageName != NULL && pTopImageName[0] != '\0' )
		{
			vgui::surface()->DrawSetTextureFile( m_nTopBorderImageId, pTopImageName, true, false );	
		}
	}

	const char *pBotImageName = pScheme->GetResourceString( "Frame.BottomBorderImage" );
	m_nBottomBorderImageId = vgui::surface()->DrawGetTextureId( pBotImageName );
	if ( m_nBottomBorderImageId == -1 )
	{
		m_nBottomBorderImageId = vgui::surface()->CreateNewTextureID();
		// @TODO: why is this string empty?
		if ( pBotImageName != NULL && pBotImageName[0] != '\0' )
		{
			vgui::surface()->DrawSetTextureFile( m_nBottomBorderImageId, pBotImageName, true, false );	
		}
	}

	m_smearColor = pScheme->GetColor( "Frame.SmearColor", Color( 0, 0, 0, 225 ) );

	LoadLayout();
	DisableFadeEffect();
}

// Load the control settings 
void CBaseModFrame::LoadControlSettings( const char *dialogResourceName, const char *pathID, KeyValues *pPreloadedKeyValues, KeyValues *pConditions )
{
	// Use the keyvalues they passed in or load them using special hook for flyouts generation
	KeyValues *rDat = pPreloadedKeyValues;
	if ( !rDat )
	{
		// load the resource data from the file
		rDat  = new KeyValues(dialogResourceName);

		// check the skins directory first, if an explicit pathID hasn't been set
		bool bSuccess = false;
		if ( !IsX360() && !pathID )
		{
			bSuccess = rDat->LoadFromFile( g_pFullFileSystem, dialogResourceName, "SKIN" );
		}
		if ( !bSuccess )
		{
			bSuccess = rDat->LoadFromFile( g_pFullFileSystem, dialogResourceName, pathID );
		}
		if ( bSuccess )
		{
			if ( IsX360() )
			{
				rDat->ProcessResolutionKeys( surface()->GetResolutionKey() );
			}
			if ( pConditions && pConditions->GetFirstSubKey() )
			{
				GetBuildGroup()->ProcessConditionalKeys( rDat, pConditions );
			}
		}
	}

	// Find the auto-generated-chapter hook
	if ( KeyValues *pHook = rDat->FindKey( "FlmChapterXXautogenerated" ) )
	{
		const int numMaxAutogeneratedFlyouts = 20;
		for ( int k = 1; k <= numMaxAutogeneratedFlyouts; ++ k )
		{
			KeyValues *pFlyoutInfo = pHook->MakeCopy();
			
			CFmtStr strName( "FlmChapter%d", k );
			pFlyoutInfo->SetName( strName );
			pFlyoutInfo->SetString( "fieldName", strName );
			
			pFlyoutInfo->SetString( "ResourceFile", CFmtStr( "FlmChapterXXautogenerated_%d/%s", k, pHook->GetString( "ResourceFile" ) ) );

			rDat->AddSubKey( pFlyoutInfo );
		}

		rDat->RemoveSubKey( pHook );
		pHook->deleteThis();
	}

	BaseClass::LoadControlSettings( dialogResourceName, pathID, rDat, pConditions );
	if ( rDat != pPreloadedKeyValues )
	{
		rDat->deleteThis();
	}
}

//=============================================================================
void CBaseModFrame::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings( inResourceData );
	
	PostApplySettings();
	DisableFadeEffect();
}

//=============================================================================
void CBaseModFrame::PostApplySettings()
{
}

//=============================================================================
void CBaseModFrame::SetOkButtonEnabled(bool enabled)
{
	m_OkButtonEnabled = enabled;
	BaseModUI::CBaseModPanel::GetSingleton().SetOkButtonEnabled( enabled );
}

//=============================================================================
void CBaseModFrame::SetCancelButtonEnabled(bool enabled)
{
	m_CancelButtonEnabled = enabled;
	BaseModUI::CBaseModPanel::GetSingleton().SetCancelButtonEnabled( enabled );
}

//=============================================================================
void CBaseModFrame::SetUpperGarnishEnabled(bool enabled)
{
	m_UpperGarnishEnabled = enabled;
}

//=============================================================================
void CBaseModFrame::SetLowerGarnishEnabled(bool enabled)
{
	m_LowerGarnishEnabled = enabled;
	
}

//=============================================================================
void CBaseModFrame::ToggleTitleSafeBorder()
{
	m_DrawTitleSafeBorder = !m_DrawTitleSafeBorder;
	SetPostChildPaintEnabled(true);
}


//=============================================================================
// Takes modal app input focus so all keyboard and mouse input only goes to 
// this panel and its children.  Prevents the problem of clicking outside a panel
// and activating some other panel that is underneath.  PC only since it's a mouse
// issue.  Maintains a stack of previous panels that had focus.
void CBaseModFrame::PushModalInputFocus()
{
	if ( IsPC() )
	{
		if ( !m_bLayoutLoaded )
		{
			// if we haven't been loaded yet, we've just been created and will load our layout shortly,
			// delay this action until then
			m_bDelayPushModalInputFocus = true;
		}
		else if ( !m_bIsFullScreen )
		{
			// only need to do this if NOT full screen -- if we are full screen then you can't click outside
			// the topmost window.

			// maintain a stack of who had modal input focus
			HPanel handle = vgui::ivgui()->PanelToHandle( vgui::input()->GetAppModalSurface() );
			if ( handle )
			{
				m_vecModalInputFocusStack.AddToHead( handle );
			}

			// set ourselves modal so you can't mouse click outside this panel to a child panel
			vgui::input()->SetAppModalSurface( GetVPanel() );		
		}
	}
}


//=============================================================================
void CBaseModFrame::PopModalInputFocus()
{
	// only need to do this if NOT full screen -- if we are full screen then you can't click outside
	// the topmost window.
	if ( IsPC() && !m_bIsFullScreen )
	{
		// In general we should have modal input focus since we should only pop modal input focus
		// if we are top most.  However, if all windows get closed at once we can get closed out of 
		// order so we may not actually have modal input focus.  Similarly, we should in general
		// NOT appear on the modal input focus stack but we may if we get closed out of order,
		// so remove ourselves from the stack if we're on it.
		bool bHaveModalInputFocus = ( vgui::input()->GetAppModalSurface() == GetVPanel() );

		HPanel handle = ivgui()->PanelToHandle( GetVPanel() );
		m_vecModalInputFocusStack.FindAndRemove( handle );

		if ( bHaveModalInputFocus )
		{
			// if we have modal input focus, figure out who is supposed to get it next

			// start from the top of the stack
			while ( m_vecModalInputFocusStack.Count() > 0 )
			{
				handle = m_vecModalInputFocusStack[0];

				// remove top stack entry no matter what
				m_vecModalInputFocusStack.Remove( 0 );

				VPANEL panel = ivgui()->HandleToPanel( handle );

				if ( !panel )
					continue;

				// if the panel on the modal input focus stack isn't visible any more, ignore it and go on
				if ( ipanel()->IsVisible( panel ) == false )
					continue;
				
				// found the top panel on the stack, give it the modal input focus
				vgui::input()->SetAppModalSurface( panel );
				break;
			}

			// there are no valid panels on the stack to receive modal input focus after us, so set it to NULL
			if ( m_vecModalInputFocusStack.Count() == 0 )
			{
				vgui::input()->SetAppModalSurface( NULL );
			}			
		}
	}
}

//=============================================================================
// If on PC and not logged into Steam, displays an error and returns true.  
// Returns false if everything is OK.
bool CBaseModFrame::CheckAndDisplayErrorIfNotLoggedIn()
{
	// only check if PC
	if ( !IsPC() )
		return false;

#ifndef _X360
#ifndef SWDS
	// if we have Steam interfaces and user is logged on, everything is OK
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamMatchmaking() )
	{
		/*
		// Try starting to log on
		if ( !steamapicontext->SteamUser()->BLoggedOn() )
		{
		steamapicontext->SteamUser()->LogOn();
		}
		*/

		return false;
	}
	
#endif
#endif

	// Steam is not running or user not logged on, display error

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY ), false ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#L4D360UI_MsgBx_LoginRequired";
	data.pMessageText = "#L4D360UI_MsgBx_SteamRequired";
	data.bOkButtonEnabled = true;
	confirmation->SetUsageData(data);

	return true;
}

//=============================================================================
void CBaseModFrame::SetWindowType(WINDOW_TYPE windowType)
{
	m_WindowType = windowType;
}

//=============================================================================
WINDOW_TYPE CBaseModFrame::GetWindowType()
{
	return m_WindowType;
}

//=============================================================================
void CBaseModFrame::SetWindowPriority( WINDOW_PRIORITY pri )
{
	m_WindowPriority = pri;
}

//=============================================================================
WINDOW_PRIORITY CBaseModFrame::GetWindowPriority()
{
	return m_WindowPriority;
}

//=============================================================================
void CBaseModFrame::SetCanBeActiveWindowType(bool allowed)
{
	m_CanBeActiveWindowType = allowed;
}

//=============================================================================
bool CBaseModFrame::GetCanBeActiveWindowType()
{
	return m_CanBeActiveWindowType;
}

//=============================================================================
CBaseModFrame* CBaseModFrame::SetNavBack( CBaseModFrame* navBack )
{
	CBaseModFrame* lastNav = m_NavBack.Get();
	m_NavBack = navBack;
	m_bCanNavBack = navBack != NULL;

	return lastNav;
}

void CBaseModFrame::DrawGenericBackground()
{
	int wide, tall;
	GetSize( wide, tall );
	int iHalfWide = wide * 0.5f;
	//DrawHollowBox( 0, 0, wide, tall, Color( 150, 150, 150, 255 ), 1.0f );
	//DrawBox( 2, 2, wide-4, tall-4, Color( 48, 48, 48, 255 ), 1.0f );

	float flAlpha = 200.0f / 255.0f;

	// fill background
	vgui::surface()->DrawSetColor( Color( 0, 0, 0, 255 * flAlpha ) );
	vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );

	vgui::surface()->DrawSetColor( Color( 53, 86, 117, 255 * flAlpha ) );
	//vgui::surface()->DrawFilledRect( 0, YRES( 4 ), wide, tall - YRES( 4 ) );

	int nBarPosY = YRES( 4 );
	int nBarHeight = tall - nBarPosY * 2;
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );

	nBarPosY = tall - YRES( 2 );
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );

	// draw highlights
	nBarHeight = YRES( 2 );
	nBarPosY = 0;
	vgui::surface()->DrawSetColor( Color( 97, 210, 255, 255 * flAlpha ) );
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );

	nBarPosY = tall - YRES( 2 );
	vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
	vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );
}

#define VERTICAL_GAP			10
#define TOP_BORDER_HEIGHT		21
#define BOTTOM_BORDER_HEIGHT	21

void CBaseModFrame::DrawDialogBackground( const char *pMajor, const wchar_t *pMajorFormatted, const char *pMinor, const wchar_t *pMinorFormatted,
									  DialogMetrics_t *pMetrics /*= NULL*/, bool bAllCapsTitle /*= false*/, int iTitleXOffset /*= INT_MAX*/ )
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	vgui::HFont	hTitleFont = pScheme->GetFont( "ScreenTitle", true );
	vgui::HFont	hDefaultFont = pScheme->GetFont( "DefaultMedium", true );

	int topBorderTall = scheme()->GetProportionalScaledValue( TOP_BORDER_HEIGHT );
	int bottomBorderTall = scheme()->GetProportionalScaledValue( BOTTOM_BORDER_HEIGHT );

	// resolve the major title
	wchar_t szMajor[256];
	int majorWide, majorTall;
	if ( pMajor )
	{
		wchar_t *pMajorString = g_pVGuiLocalize->Find( pMajor );
		if ( !pMajorString )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMajor, szMajor, sizeof( szMajor ) );
		}
		else
		{
			Q_wcsncpy( szMajor, pMajorString, sizeof( szMajor ) );
		}
	}
	else if ( pMajorFormatted )
	{
		// already resolved
		Q_wcsncpy( szMajor, pMajorFormatted, sizeof( szMajor ) );
	}
	else
	{
		// huh, no title?
		return;
	}

	if ( bAllCapsTitle )
	{
		for ( int i = Q_wcslen( szMajor ) - 1; i >= 0; --i )
		{
			szMajor[ i ] = towupper( szMajor[ i ] );
		}
	}

	vgui::surface()->GetTextSize( hTitleFont, szMajor, majorWide, majorTall );

	int wide;
	int tall;
	GetSize( wide, tall );
	
	bool bNeedMaxRoom = false;
	if ( majorWide > ( wide * 0.85 ) ) 
	{
		bNeedMaxRoom = true;
		hTitleFont = pScheme->GetFont( "FrameTitle", true );
	}

	// minor title is optional
	wchar_t szMinor[256];
	wchar_t *pMinorString = NULL;
	int minorWide;
	int minorTall = 0;
	if ( pMinor )
	{
		// resolve the minor title
		pMinorString = g_pVGuiLocalize->Find( pMinor );
		if ( !pMinorString )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMinor, szMinor, sizeof( szMinor ) );
			pMinorString = szMinor;
		}
	}
	else if ( pMinorFormatted )
	{
		pMinorString = (wchar_t *)pMinorFormatted;
	}
	vgui::surface()->GetTextSize( hDefaultFont, pMinorString, minorWide, minorTall );

	tall = m_hOriginalTall;

	int titleTall = topBorderTall + 1.0f * ( majorTall + minorTall ) + bottomBorderTall;
	int majorX = ( bNeedMaxRoom ) ? ( 0.05f * wide ) : ( 0.10f * wide );
	int majorY = ( titleTall - ( majorTall + minorTall ) )/2;

	int minorX = majorX;
	int minorY = majorY + majorTall;

	if ( pMetrics )
	{
		if ( IsX360() )
		{
			int panelX, panelY;
			GetPos( panelX, panelY );
			pMetrics->titleY = panelY;
			pMetrics->titleHeight = titleTall;
		}
		else
		{
			pMetrics->titleY = m_iHeaderY;
			pMetrics->titleHeight = m_iHeaderTall;
		}
	}

	Color bgColor( 0, 0, 0, 200 );
	surface()->DrawSetColor( bgColor );

	// draw title header band
	int y = DrawSmearBackground( 0, 0, wide, titleTall );
	int yBottom = tall;

	// draw major title in header band
	vgui::surface()->DrawSetTextFont( hTitleFont );
	vgui::surface()->DrawSetTextPos( majorX, majorY );
	vgui::surface()->DrawSetTextColor( 200, 200, 200, 255 );
	vgui::surface()->DrawPrintText( szMajor, V_wcslen( szMajor ) );

	if ( pMinorString )
	{
		// draw minor title in header band
		vgui::surface()->DrawSetTextFont( hDefaultFont );
		vgui::surface()->DrawSetTextPos( minorX, minorY );
		vgui::surface()->DrawSetTextColor( 200, 200, 200, 255 );
		vgui::surface()->DrawPrintText( pMinorString, V_wcslen( pMinorString ) );
	}

	// draw dialog body background
	DrawSmearBackground( 0, y, wide, yBottom - y );

	if ( pMetrics )
	{
		pMetrics->dialogY = y;
		pMetrics->dialogHeight = yBottom - y;
	}
}

void CBaseModFrame::SetupAsDialogStyle()
{
	// ensure screen size and center
	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	m_hOriginalTall = GetTall();

	// get footer position
	int fx = 0;
	int fy = 0;
	CBaseModFooterPanel *pFooter = CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter && pFooter->HasContent() )
	{
		pFooter->GetPosition( fx, fy );
	}

	// center dialog in the vertical space above the footer
	int y = 0;
	if ( fy )
	{
		// have footer
		y = ( fy - m_hOriginalTall ) / 2;
		if ( y + m_hOriginalTall > fy )
		{
			// cannot center without overlapping footer
			// push the dialog up
			y = fy - m_hOriginalTall;
		}
	}
	else
	{
		// no footer, center in entire screen
		y = ( screenTall - m_hOriginalTall ) / 2;
	}

	// force panel to be larger so drawing can exceed
	SetBounds( 0, y, screenWide, screenTall - y );
}

int CBaseModFrame::DrawSmearBackground( int x, int y, int wide, int tall, bool bIsFooter )
{
	int topTall = scheme()->GetProportionalScaledValue( TOP_BORDER_HEIGHT );
	int bottomTall = scheme()->GetProportionalScaledValue( BOTTOM_BORDER_HEIGHT );

	if ( bIsFooter )
	{
		topTall  = 0.75f * topTall;
		bottomTall = 0.75f * bottomTall;
	}

	int middleTall = tall - ( topTall + bottomTall );
	if ( middleTall < 0 )
	{
		middleTall = 0;
	}

	surface()->DrawSetColor( m_smearColor );

	// top
	surface()->DrawSetTexture( m_nTopBorderImageId );
	surface()->DrawTexturedSubRect( x, y, x + wide, y + topTall, 0, 0, 1, 1 );
	y += topTall;

	if ( middleTall )
	{
		// middle
		surface()->DrawFilledRect( x, y, x + wide, y + middleTall );
		y += middleTall;
	}

	// bottom
	surface()->DrawSetTexture( m_nBottomBorderImageId );
	surface()->DrawTexturedSubRect( x, y, x + wide, y + bottomTall, 0, 0, 1, 1 );
	y += bottomTall;

	return topTall + middleTall + bottomTall;
}
