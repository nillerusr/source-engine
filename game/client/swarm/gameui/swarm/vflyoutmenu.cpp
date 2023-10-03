//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VFlyoutMenu.h"
#include "VGenericPanelList.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"

#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"

#include "filesystem.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

DECLARE_BUILD_FACTORY( FlyoutMenu );


// HACK HACK
// Fix up spacing
// Everything is tighter spacing on PC
// Instead of changing all the resources we're going to make the conversion here
//#define FLYOUTMENU_TALL_SCALE ( ( IsPC() ) ? ( 0.7f ) :( 1.0f ) )

#define FLYOUTMENU_TALL_SCALE 1.0f


//=============================================================================
// FlyoutMenu
//=============================================================================
FlyoutMenu::FlyoutMenu( vgui::Panel *parent, const char* panelName )
: BaseClass( parent, panelName )
{
	SetProportional( true );

	m_offsetX = 0;
	m_offsetY = 0;
	m_navFrom = NULL;
	m_lastChildNotified = NULL;
	m_listener = NULL;
	m_resFile[0] = '\0';
	m_defaultControl = NULL;

	m_szInitialSelection[0] = 0;
	m_FromOriginalTall = 0;

	m_bOnlyActiveUser = false;
	m_bExpandUp = false;
	m_bUsingWideAtOpen = false;
}

FlyoutMenu::~FlyoutMenu()
{
	if ( sm_pActiveMenu == this )
	{
		sm_pActiveMenu = NULL;
	}
}

void FlyoutMenu::PaintBackground()
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	Color borderColor = pScheme->GetColor( "HybridButton.BorderColor", Color( 0, 0, 0, 255 ) );

	int wide, tall;
	GetSize( wide, tall );

	int iHalfWide = wide/2;
	int iFourthWide = wide/4;

	int fadePoint = 220;
	if ( m_bUsingWideAtOpen )
	{
		// wide at open is hack that pulls the flyout back
		// but this generally then overlaps text underneath the flyout
		// so push the opaque region closer to far right edge to obscure overlap
		iFourthWide = 0.70f * iHalfWide;
		fadePoint = 245;
	}

	surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
	surface()->DrawFilledRect( 0, 0, iHalfWide, tall );
	surface()->DrawFilledRectFade( iHalfWide, 0, iHalfWide + iFourthWide, tall, 255, fadePoint, true );
	surface()->DrawFilledRectFade( iHalfWide + iFourthWide, 0, wide, tall, fadePoint, 0, true );

	// draw border lines
	surface()->DrawSetColor( borderColor );
	surface()->DrawFilledRectFade( 0, 0, wide, 2, 255, 0, true );
	surface()->DrawFilledRectFade( 0, tall-2, wide, tall, 255, 0, true );

	if ( m_bExpandUp )
	{
		surface()->DrawFilledRect( 0, 0, 2, tall-m_FromOriginalTall+2 );
	}
	else
	{
		surface()->DrawFilledRect( 0, m_FromOriginalTall-2, 2, tall );
	}
}

void FlyoutMenu::SetInitialSelection( const char *szInitialSelection )
{
	m_szInitialSelection[0] = 0;
	if ( szInitialSelection )
	{
		Q_strncpy( m_szInitialSelection, szInitialSelection, sizeof( m_szInitialSelection ) );
		if ( vgui::Panel *pNewDefault = dynamic_cast< vgui::Panel* >( FindChildByName( m_szInitialSelection ) ) )
		{
			m_defaultControl = pNewDefault;
		}
	}
}

void FlyoutMenu::SetBGTall( int iTall )
{
	Panel *bgPanel = FindChildByName( "PnlBackground" );
	if ( bgPanel )
	{
		bgPanel->SetTall( vgui::scheme()->GetProportionalScaledValue( iTall ) );
	}
}

void FlyoutMenu::OpenMenu( vgui::Panel * flyFrom, vgui::Panel* initialSelection, bool reloadRes )
{
	if ( GetActiveMenu() == this )
	{
		CloseMenu( NULL );
		return;
	}

	// If another flyout menu is currently open, close it.
	CloseActiveMenu( this );

	if ( reloadRes && m_resFile[0] != '\0' )
	{
		LoadControlSettings( m_resFile );
	}

	m_navFrom = flyFrom;
	if ( m_navFrom )
	{
		int x, y, wide, tall = 0;
		m_navFrom->GetBounds( x, y , wide, tall );

		// we don't want our parenting to be messed up if we are created from a dropdown, but we do want to get the same positioning.
		// Since the button that we naved from is inside the dropdown control it's x and y are going to be 0,0, we actually want
		// to get the x,y of it's parent.
		if ( m_navFrom->GetParent() )
		{
			int xParent, yParent;
			m_navFrom->GetParent()->GetPos( xParent, yParent );
			x += xParent;
			y += yParent;
		}

		int yButtonCompensate = 0;
		BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( m_navFrom );
		if ( button )
		{
			button->SetOpen();
			// If the parent button's bounds have increased due to drawing a texture that extends outside its button bounds,
			// take that into account.  Shift the flyout menu down by half of the increase so the flyout menu aligns with
			// the authored size of the button.
			if ( button->GetStyle() != BaseModHybridButton::BUTTON_GAMEMODE )
			{
				yButtonCompensate = ( tall - button->GetOriginalTall() ) / 2;
			}
			if ( button->GetStyle() == BaseModHybridButton::BUTTON_DROPDOWN || button->GetStyle() == BaseModHybridButton::BUTTON_GAMEMODE )
			{
				int wideAtOpen = button->GetWideAtOpen();
				if ( wideAtOpen )
				{
					wide = wideAtOpen;
					m_bUsingWideAtOpen = true;
				}
			}
			m_FromOriginalTall = button->GetOriginalTall();
		}

		if ( m_bExpandUp )
		{
			y -= ( GetTall() - m_FromOriginalTall );
		}

		SetPos( x+wide+m_offsetX, y+m_offsetY+yButtonCompensate );
	}	
	else
	{
		m_FromOriginalTall = 0;
	}

	bool navigated = false;
	
	// Highlight the default item
	if ( initialSelection )
	{
		m_defaultControl = initialSelection;
	}

	if ( m_defaultControl )
	{
		m_defaultControl->NavigateTo();
		navigated = true;
	}

	if ( !navigated )
	{
		NavigateTo();
	}

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		FooterFormat_t format = footer->GetFormat();
		if ( format == FF_MAINMENU )
		{
			// change to main menu format with flyout open
			format = FF_MAINMENU_FLYOUT;
		}

		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, format, footer->GetHelpTextEnabled() );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}

	// keep track of what menu is open
	sm_pActiveMenu = this;	

	SetVisible( true );
}

void FlyoutMenu::CloseMenu( vgui::Panel * flyTo )
{
	Assert( sm_pActiveMenu == NULL || sm_pActiveMenu == this );		// if we think there is an active menu right now, it should be us
	sm_pActiveMenu = NULL;

	//clear any items that may have been highlighted
	for ( int i = 0; i != GetChildCount(); ++i )
	{
		vgui::Panel *pPanel = GetChild(i);
		pPanel->NavigateFrom();
	}

	if ( flyTo )
	{
		flyTo->NavigateTo();
	}

	SetVisible( false );
	
	if ( m_listener )
	{
		m_listener->OnFlyoutMenuClose( flyTo );
	}

	BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( m_navFrom );
	if ( button )
	{
		button->SetClosed();
	}
}

void FlyoutMenu::ApplySettings( KeyValues* inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
	
	const char* resFile = inResourceData->GetString( "ResourceFile", NULL );
	if ( resFile )
	{
		V_snprintf( m_resFile, DEFAULT_STR_LEN, resFile );
		LoadControlSettings( resFile );
	}

	// cannot support arbitrary offsets with new look
	//
	m_offsetX = 0;
	m_offsetY = 0;

	const char* initFocus = inResourceData->GetString( "InitialFocus", NULL );

	// If explicitly overriding by code, honor code request
	if ( m_szInitialSelection[0] )
	{
		 initFocus = m_szInitialSelection;
	}

	if ( initFocus && initFocus[0] )
	{
		m_defaultControl = dynamic_cast< vgui::Panel* >( FindChildByName( initFocus ) );
	}

	m_bOnlyActiveUser = ( inResourceData->GetInt( "OnlyActiveUser", 0 ) != 0 );

	m_bExpandUp = ( inResourceData->GetInt( "ExpandUp", 0 ) != 0 );
}

void FlyoutMenu::ApplySchemeSettings( vgui::IScheme* pScheme )
{
	if ( pScheme )
	{
		vgui::Panel* bgPanel = FindChildByName( "PnlBackground" );
		if ( bgPanel )
		{
//			bgPanel->SetBgColor( pScheme->GetColor( "Flyout.BgColor" , Color( 0, 0, 0, 255 ) ) );
//			bgPanel->SetBorder( pScheme->GetBorder( "FlyoutBorder" ) );

			// just use the PnlBackground to set size, not needed for anything else
			int wide, tall;
			bgPanel->GetSize( wide, tall );

			tall *= FLYOUTMENU_TALL_SCALE;

			int iPanelWide, iPanelTall;

			GetSize( iPanelWide, iPanelTall );

			if ( wide != iPanelWide || tall != iPanelTall )
			{
				SetSize( wide, tall );

				for( int i = 0; i < GetChildCount(); ++i )
				{
					vgui::Button* button = dynamic_cast< vgui::Button* >( GetChild( i ) );
					if( button )
					{
						int iXPos, iYPos;
						button->GetPos( iXPos, iYPos );
						button->SetPos( iXPos, iYPos * FLYOUTMENU_TALL_SCALE );
					}
				}
			}

			bgPanel->SetVisible( false );
		}
	}

	SetPaintBackgroundEnabled( true );
}

// Load the control settings 
void FlyoutMenu::LoadControlSettings( const char *dialogResourceName, const char *pathID, KeyValues *pPreloadedKeyValues, KeyValues *pConditions )
{
	// Use the keyvalues they passed in or load them using special hook for flyouts generation
	KeyValues *rDat = pPreloadedKeyValues;
	if ( char const *szAutogenerateIdx = StringAfterPrefix( dialogResourceName, "FlmChapterXXautogenerated_" ) )
	{
		if ( pPreloadedKeyValues )
			pPreloadedKeyValues->deleteThis();
		pPreloadedKeyValues = NULL;

		// check the skins directory first, if an explicit pathID hasn't been set
		char const *szLoadFile = strchr( szAutogenerateIdx, '/' );
		Assert( szLoadFile );
		if ( szLoadFile )
			++ szLoadFile;
		else
			return;

		// load the resource data from the file
		rDat = new KeyValues( dialogResourceName );

		bool bSuccess = false;
		if ( !IsX360() && !pathID )
		{
			bSuccess = rDat->LoadFromFile(g_pFullFileSystem, szLoadFile, "SKIN");
		}
		if ( !bSuccess )
		{
			bSuccess = rDat->LoadFromFile(g_pFullFileSystem, szLoadFile, pathID);
		}
		if ( bSuccess )
		{
			if ( IsX360() )
			{
				rDat->ProcessResolutionKeys( surface()->GetResolutionKey() );
			}
		}


		// Find the auto-generated-chapter hook
		if ( KeyValues *pHook = rDat->FindKey( "BtnChapter" ) )
		{
			int numChapters = atoi( szAutogenerateIdx );

			if ( KeyValues *pBkgndTall = rDat->FindKey( "PnlBackground/tall" ) )
			{
				pBkgndTall->SetInt( NULL, pBkgndTall->GetInt() + ( numChapters - 1 ) * pHook->GetInt( "tall" ) );
			}

			for ( int k = 1; k <= numChapters; ++ k )
			{
				KeyValues *pChapter = pHook->MakeCopy();
				
				pChapter->SetName( CFmtStr( "%s%d", pHook->GetName(), k ) );
				pChapter->SetString( "navDown", CFmtStr( "%s%d", pHook->GetName(),
					1 + ( ( numChapters + k - 1 + 1 ) % numChapters ) )
					);
				pChapter->SetString( "navUp", CFmtStr( "%s%d", pHook->GetName(),
					1 + ( ( numChapters + k - 1 - 1 ) % numChapters ) )
					);

				pChapter->SetInt( "ypos", pHook->GetInt( "ypos" ) + ( k - 1 ) * pHook->GetInt( "tall" ) );
				
				char const *arrFields[] = { "fieldName", "labelText", "command" };
				for ( int j = 0; j < ARRAYSIZE( arrFields ); ++ j )
					pChapter->SetString( arrFields[j], CFmtStr( "%s%d", pHook->GetString( arrFields[j] ), k ) );
				
				rDat->AddSubKey( pChapter );
			}

			rDat->RemoveSubKey( pHook );
			pHook->deleteThis();
		}
	}

	BaseClass::LoadControlSettings( dialogResourceName, pathID, rDat, pConditions );
	if ( rDat != pPreloadedKeyValues )
	{
		rDat->deleteThis();
	}
}

void FlyoutMenu::SetListener( FlyoutMenuListener* listener )
{
	m_listener = listener;
}

void FlyoutMenu::NotifyChildFocus( vgui::Panel* child )
{
	m_lastChildNotified = child;
	if( m_listener )
	{
		m_listener->OnNotifyChildFocus( child );
	}
}

vgui::Panel* FlyoutMenu::GetLastChildNotified()
{
	return m_lastChildNotified;
}

vgui::Button* FlyoutMenu::FindChildButtonByCommand( const char* command )
{
	for( int i = 0; i < GetChildCount(); ++i )
	{
		vgui::Button* button = dynamic_cast< vgui::Button* >( GetChild( i ) );
		if( !button )
			continue;

		KeyValues* commandVal = button->GetCommand();
		if ( !commandVal )
			continue;
		const char* commandStr = commandVal->GetString( "command", NULL );
		if( commandStr && *commandStr && command )
		{
			if( !Q_stricmp( command, commandStr ) )
			{
				return button;
			}
		}
	}

	return NULL;
}

vgui::Button* FlyoutMenu::FindPrevChildButtonByCommand( const char* command )
{
	// Find the button by command name
	for ( int i = 0; i < GetChildCount(); ++i )
	{
		vgui::Button* button = dynamic_cast< vgui::Button* >( GetChild( i ) );
		if ( button )
		{
			KeyValues* commandVal = button->GetCommand();
			if ( commandVal )
			{
				const char* commandStr = commandVal->GetString( "command", NULL );
				if ( commandStr && *commandStr && command )
				{
					if ( !Q_stricmp( command, commandStr ) )
					{
						return dynamic_cast< vgui::Button* >( button->GetNavUp() );
					}
				}
			}
		}
	}
	return NULL;
}

vgui::Button* FlyoutMenu::FindNextChildButtonByCommand( const char* command )
{
	// Find the button by command name
	for ( int i = 0; i < GetChildCount(); ++i )
	{
		vgui::Button* button = dynamic_cast< vgui::Button* >( GetChild( i ) );
		if ( button )
		{
			KeyValues* commandVal = button->GetCommand();
			if ( commandVal )
			{
				const char* commandStr = commandVal->GetString( "command", NULL );			
				if ( commandStr && *commandStr && command )
				{
					if ( !Q_stricmp( command, commandStr ) )
					{
						return dynamic_cast< vgui::Button* >( button->GetNavDown() );
					}
				}
			}
		}
	}
	return NULL;
}

void FlyoutMenu::OnKeyCodePressed( vgui::KeyCode code )
{
	int iJoystick = GetJoystickForCode( code );

	if ( m_bOnlyActiveUser )
	{
		// Only allow input from the active userid
		int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();

		if( iJoystick != userId || iJoystick < 0 )
		{	
			return;
		}
	}

	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( iJoystick );

	vgui::KeyCode basecode = GetBaseButtonCode( code );

	switch( basecode )
	{
	case KEY_XBUTTON_B:
		if ( !s_NavLock )
		{
			s_NavLock = 2;
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			CloseMenu( m_navFrom );
			if( m_listener )
			{
				m_listener->OnFlyoutMenuCancelled();
			}
		}
		break;

	case KEY_XBUTTON_INACTIVE_START:
		if ( CBaseModFrame *pPanel = dynamic_cast< CBaseModFrame * >( m_listener ) )
		{
			pPanel->OnKeyCodePressed( code );
		}
		break;

	default:
		//BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void FlyoutMenu::OnCommand( const char* command )
{
	if ( m_navFrom )
	{
		s_NavLock = 2;
		CloseMenu( m_navFrom );
		if ( m_navFrom->GetParent() )
		{
			m_navFrom->GetParent()->OnCommand( command );
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void FlyoutMenu::CloseActiveMenu( vgui::Panel *pFlyTo )
{
	if ( sm_pActiveMenu )
	{
		if ( sm_pActiveMenu->IsVisible() )
		{
			FlyoutMenu *pOldActiveMenu = sm_pActiveMenu;
			sm_pActiveMenu = NULL;
			pOldActiveMenu->CloseMenu( pFlyTo );
		}		
	}
}

int FlyoutMenu::GetOriginalTall() const
{
	return m_FromOriginalTall;
}

void FlyoutMenu::SetOriginalTall( int t )
{
	m_FromOriginalTall = t;
}

FlyoutMenu *FlyoutMenu::sm_pActiveMenu = NULL;
