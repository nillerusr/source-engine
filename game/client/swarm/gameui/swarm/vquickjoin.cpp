//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VQuickJoin.h"
#include "VGenericPanelList.h"
#include "VHybridButton.h"
#include "VMainMenu.h"
#include "EngineInterface.h"
#include "gameui_util.h"

#include "tier1/KeyValues.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"
#define PLAYER_DEBUG_NAME_UNICODE L"WWWWWWWWWWWWWWW"

DECLARE_BUILD_FACTORY( QuickJoinPanel );

ConVar cl_quick_join_scroll_start( "cl_quick_join_scroll_start", "1", FCVAR_NONE, "Number of players available to start friend scrolling ticker." );
ConVar cl_quick_join_scroll_max( "cl_quick_join_scroll_max", "4", FCVAR_NONE, "Max players shown in the friend scrolling ticker." );
ConVar cl_quick_join_scroll_rate( "cl_quick_join_scroll_rate", "90", FCVAR_NONE, "Rate of the friend scrolling ticker." );
ConVar cl_quick_join_scroll_offset( "cl_quick_join_scroll_offset", "16", FCVAR_NONE, "Offset of the friend scrolling ticker from the title." );
ConVar cl_quick_join_panel_tall( "cl_quick_join_panel_tall", IsX360() ? "16" : "14", // X360 doesn't show icons, but need extra space on PC for Steam avatars
								 FCVAR_NONE, "The spacing between panels." );
ConVar cl_quick_join_panel_accel( "cl_quick_join_panel_accel", "0.025", FCVAR_NONE, "Acceleration for the y position of the panel when items are added or removed." );
ConVar cl_quick_join_panel_fakecount( "cl_quick_join_panel_fakecount", "-1", FCVAR_NONE );


//=============================================================================
// Quick Join Panel List Item
//=============================================================================
QuickJoinPanelItem::QuickJoinPanelItem( vgui::Panel* parent, const char* panelName ):
	BaseClass( parent, panelName )
{
	SetProportional( true );

	m_tooltip = NULL;

	Q_memset( &m_info, 0, sizeof( m_info ) );
	Q_memset( m_name, 0, sizeof( m_name ) );
}

QuickJoinPanelItem::~QuickJoinPanelItem( )
{
}

#ifdef _X360
void QuickJoinPanelItem::NavigateTo()
{
	BaseClass::NavigateTo();
	SetBgColor( m_FocusBgColor );
	GetTooltip()->SetText( m_tooltip );
}

void QuickJoinPanelItem::NavigateFrom()
{
	BaseClass::NavigateFrom();
	SetBgColor( m_UnfocusBgColor );
}
#endif // _X360

void QuickJoinPanelItem::SetInfo( QuickJoinPanel::QuickInfo const &qi )
{
	m_info = qi;
}

const wchar_t *QuickJoinPanelItem::GetName() const
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return PLAYER_DEBUG_NAME_UNICODE;

	return m_name;
}

void QuickJoinPanelItem::SetName( wchar_t *name )
{
	Q_wcsncpy( m_name, name, ARRAYSIZE( m_name ) );
}

bool QuickJoinPanelItem::GetText(char* outText, int buffLen)
{
	vgui::Label *lblGamerTag = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTag" ) );
	if ( lblGamerTag )
	{
		lblGamerTag->GetText(outText, buffLen);
		return true;
	}
	return false;
}

void QuickJoinPanelItem::Update()
{
	vgui::ImagePanel *imgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	if ( imgAvatar && IsPC() )
	{
		if ( !GetName() || GetName()[ 0 ] == L'\0' )
		{
			imgAvatar->SetVisible( false );
		}
		else
		{
			imgAvatar->SetVisible( true );

			IImage *pImage = NULL;

			if ( m_info.m_eType == m_info.TYPE_PLAYER )
				pImage = CUIGameData::Get()->GetAvatarImage( m_info.m_xuid );

			if ( pImage )
			{
				imgAvatar->SetImage( "" );
				imgAvatar->SetImage( pImage );
			}
			else
			{
				imgAvatar->SetImage( "icon_lan" );
			}
		}
	}

	SetControlString( "LblGamerTag", GetName() );

	SetTall( vgui::scheme()->GetProportionalScaledValue( cl_quick_join_panel_tall.GetInt() ) );
}

void QuickJoinPanelItem::SetItemTooltip( const char* tooltip )
{
	m_tooltip = tooltip;
}

const char* QuickJoinPanelItem::GetItemTooltip()
{
	return m_tooltip;
}

void QuickJoinPanelItem::PaintBackground()
{
}

void QuickJoinPanelItem::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_FocusBgColor = inResourceData->GetColor( "focusColor", Color( 128, 0, 0, 255 ) );
	m_UnfocusBgColor = inResourceData->GetColor( "unfocusColor", Color( 64, 64, 64, 255 ) );
}

void QuickJoinPanelItem::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/BaseModUI/QuickJoinPanelItem.res" );

	Update();
}

//=============================================================================
void QuickJoinPanelItem::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}
}

//=============================================================================
// Quick Join Panel
//=============================================================================
QuickJoinPanel::QuickJoinPanel( vgui::Panel *parent , const char* panelName ):
BaseClass( parent, panelName )
{
	SetProportional( true );

	m_GplQuickJoinList = new GenericPanelList( this, "GplQuickJoinList", GenericPanelList::ISM_PERITEM );
	ClearGames();

	m_resFile = NULL;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 1000 );	// used to bring us back from invisible

	m_iPrevWrap = 0;
	m_flScrollSpeed = 0.0f;
	m_flScrollAccum = 0.0f;

	m_bHasMouseover = false;
}

QuickJoinPanel::~QuickJoinPanel()
{
	delete [] m_resFile;
	delete m_GplQuickJoinList;
}

bool QuickJoinPanel::ShouldBeVisible() const
{
#ifdef _X360
	return !XBX_GetPrimaryUserIsGuest();
#else
	return true;
#endif
}

void QuickJoinPanel::OnTick()
{
	BaseClass::OnTick();

	if ( IsVisible() )
		return;

	if ( ShouldBeVisible() )
		OnThink();
}

static int QuickJoinNameAlphaSort( const QuickJoinPanel::QuickInfo *pFriend1, const QuickJoinPanel::QuickInfo *pFriend2 )
{
	return Q_stricmp( pFriend1->m_szName, pFriend2->m_szName );
}

static void RemoveDuplicateEntries( CUtlVector< QuickJoinPanel::QuickInfo > &arr )
{
	//
	// Assumes list sorted by name "stricmp"
	//
	const char *szLastName = "";

	for ( int k = 0; k < arr.Count(); ++ k )
	{
		QuickJoinPanel::QuickInfo &fi = arr[k];
		if ( !stricmp( fi.m_szName, szLastName ) )
		{
			// Same name entry, must be removed
			arr.Remove( k -- );
		}
		else
		{
			// Next name, remember for next item
			szLastName = fi.m_szName;
		}
	}
}

int QuickJoinPanel::GetSmoothPanelY( int iCurrentY, int iDesiredY )
{
	if( iCurrentY == iDesiredY )
	{
		m_flScrollSpeed = 0;
		m_flScrollAccum = 0;
	}
	else
	{
		m_flScrollSpeed *= 1.0f - cl_quick_join_panel_accel.GetFloat();
		m_flScrollSpeed += cl_quick_join_panel_accel.GetFloat()*(iDesiredY - iCurrentY );

		m_flScrollAccum += m_flScrollSpeed;

		int iSpeed = m_flScrollAccum;
		m_flScrollAccum -= iSpeed;

		if ( iCurrentY > iDesiredY )
		{
			return MAX( iDesiredY, iCurrentY + iSpeed );
		}
		else if ( iCurrentY < iDesiredY )
		{
			return MIN( iDesiredY, iCurrentY + iSpeed );
		}
	}
	return iDesiredY;
}

void QuickJoinPanel::OnThink()
{
	int iNumItems = MIN( m_FriendInfo.Count(), cl_quick_join_scroll_max.GetInt() );

	const int iItemTallMin = 2;
	int iItemTall = iItemTallMin;

	Panel *pItem = m_GplQuickJoinList->GetPanelItem( 0 );
	if ( pItem )
	{
		iItemTall = pItem->GetTall();

		Panel *pItem2 = m_GplQuickJoinList->GetPanelItem( 1 );
		if ( pItem2 )
		{
			int iXPos1, iYPos1;
			int iXPos2, iYPos2;

			pItem->GetPos( iXPos1, iYPos1 );
			pItem2->GetPos( iXPos2, iYPos2 );

			iItemTall = iYPos2 - iYPos1;
		}
	}

	iItemTall = MAX( iItemTall, iItemTallMin );

	long lRateValue = ( system()->GetTimeMillis() / cl_quick_join_scroll_rate.GetInt() );
	int iWrap = ( lRateValue / iItemTall ) + cl_quick_join_scroll_max.GetInt();

	if ( iWrap != m_iPrevWrap )
	{
		RefreshContents( iWrap );
		m_iPrevWrap = iWrap;

		UpdateNumGamesFoundLabel();

		SetVisible( m_FriendInfo.Count() > 0 );

		// Fade out last item completely since we wrapped around and done!
		pItem = m_GplQuickJoinList->GetPanelItem( ( iNumItems > 1 ) ? ( iNumItems - 1 ) : ( 1 ) );
		if ( pItem )
			pItem->SetAlpha( 0.0f );
		return;
	}

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );

	int iYOffset = 110;
	if ( IsPC() )
	{
		iYOffset = 30;
	}

	bool bScrolling = ( iNumItems >= cl_quick_join_scroll_start.GetInt() );

	int iCurrentX, iCurrentY;
	GetPos( iCurrentX, iCurrentY );
	iCurrentY = GetSmoothPanelY( iCurrentY, screenTall
								 - vgui::scheme()->GetProportionalScaledValue( iYOffset )
								 - MAX( iNumItems, ( bScrolling ) ? ( 2 ) : ( 1 ) ) * iItemTall );
	SetPos( iCurrentX, iCurrentY );

	if ( iNumItems > cl_quick_join_scroll_start.GetInt() - 1 )
	{
		// Scrolling list illusion
		int iTimeOffset = lRateValue % iItemTall;

		float fTimeOffset = iTimeOffset;

		m_GplQuickJoinList->GetPos( iCurrentX, iCurrentY );
		m_GplQuickJoinList->SetPos( iCurrentX, fTimeOffset + vgui::scheme()->GetProportionalScaledValue( cl_quick_join_scroll_offset.GetFloat() ) );

		float fListEdgeAlphas = 255.0f * ( fTimeOffset / static_cast<float>( iItemTall ) );

		// Fade in first item
		pItem = m_GplQuickJoinList->GetPanelItem( 0 );
		if ( pItem )
		{
			pItem->SetAlpha( fListEdgeAlphas );
		}

		// Fade out last item
		pItem = m_GplQuickJoinList->GetPanelItem( ( iNumItems > 1 ) ? ( iNumItems - 1 ) : ( 1 ) );

		if ( pItem )
		{
			pItem->SetAlpha( 255.0f - fListEdgeAlphas );
		}
	}
	else
	{
		m_GplQuickJoinList->GetPos( iCurrentX, iCurrentY );
		m_GplQuickJoinList->SetPos( iCurrentX, vgui::scheme()->GetProportionalScaledValue( cl_quick_join_scroll_offset.GetFloat() ) );
	}
}

void QuickJoinPanel::AddServersToList( void )
{
	int numFakeItems = cl_quick_join_panel_fakecount.GetInt();
	if ( numFakeItems >= 0 )
	{
		for ( int k = 0; k < numFakeItems; ++ k )
		{
			QuickInfo qi;
			qi.m_eType = qi.TYPE_PLAYER;
			qi.m_xuid = k + 100;
			Q_snprintf( qi.m_szName, sizeof( qi.m_szName ), "FakeItem#%03d", k + 10 );
			m_FriendInfo.AddToTail( qi );
		}
		return;
	}

	IPlayerManager *mgr = g_pMatchFramework->GetMatchSystem()->GetPlayerManager();
	
	int numFriends = mgr->GetNumFriends();
	for ( int i = 0; i < numFriends; ++ i )
	{
		IPlayerFriend *pFriend = mgr->GetFriendByIndex( i );
		
		QuickInfo qi;
		qi.m_eType = qi.TYPE_PLAYER;
		qi.m_xuid = pFriend->GetXUID();
		Q_strncpy( qi.m_szName, pFriend->GetName(), sizeof( qi.m_szName ) );

		if ( !qi.m_szName[0] || !qi.m_xuid )
			continue;

		m_FriendInfo.AddToTail( qi );
	}
}

void QuickJoinPanel::RefreshContents( int iWrap )
{
	m_FriendInfo.RemoveAll();

	AddServersToList();	

	m_FriendInfo.Sort( QuickJoinNameAlphaSort );
	RemoveDuplicateEntries( m_FriendInfo );

	int iNumItems = MIN( m_FriendInfo.Count(), cl_quick_join_scroll_max.GetInt() );

	ClearGames();

	if ( m_FriendInfo.Count() > 0 )
	{
		if ( m_FriendInfo.Count() < cl_quick_join_scroll_start.GetInt() )
		{
			iWrap = cl_quick_join_scroll_start.GetInt();
		}

		if ( m_FriendInfo.Count() == 1 && cl_quick_join_scroll_start.GetInt() <= 1 )
		{
			AddGame( 0, m_FriendInfo[ 0 ], m_FriendInfo[ 0 ].m_szName );
			AddGame( 1, m_FriendInfo[ 0 ], m_FriendInfo[ 0 ].m_szName );
		}
		else
		{
			for ( int i = 0; i < iNumItems; ++i )
			{
				int iWrappedIndex = ( -1 * ( i - iWrap ) ) % m_FriendInfo.Count();

				AddGame( i, m_FriendInfo[ iWrappedIndex ], m_FriendInfo[ iWrappedIndex ].m_szName );
			}
		}
	}
}

void QuickJoinPanel::UpdateNumGamesFoundLabel( void )
{
	BaseModHybridButton *title = dynamic_cast< BaseModHybridButton* >( FindChildByName( "LblTitle" ) );
	if ( title )
	{
		wchar_t* wFormatString = g_pVGuiLocalize->Find( GetTitle() );
		if( wFormatString )
		{
			char gameCountTxt[ 4 ];
			itoa( m_FriendInfo.Count(), gameCountTxt, 10 );

			wchar_t wGameCountTxt[ 4 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( gameCountTxt, wGameCountTxt, sizeof( wGameCountTxt ) );

			wchar_t wMessage[ 256 ];
			g_pVGuiLocalize->ConstructString( wMessage, sizeof( wMessage ), wFormatString, 1, wGameCountTxt );
			title->SetText( wMessage );
		}
	}
}

void QuickJoinPanel::NavigateTo()
{
	BaseClass::NavigateTo();
	SetHasMouseover( true );

	BaseModHybridButton *title = dynamic_cast< BaseModHybridButton* >( FindChildByName( "LblTitle" ) );
	if ( title )
	{
		title->SetNavUp( GetNavUp() );
		title->SetNavDown( GetNavDown() );
		if ( m_bHasMouseover && title->IsVisible() && ( title->GetCurrentState() == BaseModHybridButton::Enabled || title->GetCurrentState() == BaseModHybridButton::Disabled ) )
		{
			title->NavigateTo();
		}
	}

#ifdef _X360
	m_GplQuickJoinList->NavigateTo();
#endif
}

void QuickJoinPanel::NavigateFrom()
{
	BaseClass::NavigateFrom();
	SetHasMouseover( false );
	for( int i = 0; i != GetChildCount(); ++i )
	{
		Panel *pPanel = GetChild(i);
		if( pPanel )
			pPanel->NavigateFrom();
	}
}

void QuickJoinPanel::NavigateToChild( Panel *pNavigateTo )
{
	GetParent()->NavigateToChild( this );
}

//=============================================================================
void QuickJoinPanel::OnCursorEntered() 
{ 
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
	GetParent()->NavigateToChild( this ); //SetHasMouseover( true ); 
}

QuickJoinPanelItem* QuickJoinPanel::GetGame( int i )
{
	return dynamic_cast< QuickJoinPanelItem* >( m_GplQuickJoinList->GetPanelItem( i ) );
}

QuickJoinPanelItem* QuickJoinPanel::AddGame( int iPanelIndex, QuickInfo const &qi, char *name )
{
	Assert( name );
	wchar_t wszName[ 128 ];

	g_pVGuiLocalize->ConvertANSIToUnicode( name, wszName, sizeof( wszName ) );
	return AddGame( iPanelIndex, qi, wszName );
}

QuickJoinPanelItem* QuickJoinPanel::AddGame( int iPanelIndex, QuickInfo const &qi, wchar_t *name )
{
	QuickJoinPanelItem* panel = dynamic_cast< QuickJoinPanelItem* >( m_GplQuickJoinList->GetPanelItem( iPanelIndex ) );

	if ( panel )
	{
		// Just update name
		panel->SetInfo( qi );
		panel->SetName( name );
		panel->Update();
		return panel;
	}

	// Not found, add a new item
	QuickJoinPanelItem* result = m_GplQuickJoinList->AddPanelItem< QuickJoinPanelItem >( "QuickJoinPanelItem" );
	result->SetInfo( qi );
	result->SetName( name );
	result->SetItemTooltip( "#L4D360UI_MainMenu_Tooltip_QuickJoinItem" );
	result->Update();
	return result;
}

void QuickJoinPanel::RemoveGame( int i )
{
	QuickJoinPanelItem* panel = GetGame( i );
	if( panel )
	{
		m_GplQuickJoinList->RemovePanelItem( i );
	}
}

void QuickJoinPanel::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "SeeAll" ) == 0 )
	{
		// relay the command up to our parent
		if ( GetParent() )
		{
			GetParent()->OnCommand( command );
		}
	}
}

void QuickJoinPanel::OnMousePressed( vgui::MouseCode code )
{
	switch ( code )
	{
	case MOUSE_LEFT:
		OnCommand( "SeeAll" );
		break;
	}
}

void QuickJoinPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
	const char* resFile = inResourceData->GetString( "ResourceFile", NULL );
	if( resFile )
	{
		delete[] m_resFile;
		m_resFile = new char[ Q_strlen( resFile ) + 1 ];
		Q_strcpy( m_resFile, resFile );
		LoadControlSettings( resFile );
	}

	for ( int i = 0; i < cl_quick_join_scroll_max.GetInt(); ++i )
	{
		QuickJoinPanelItem* result = m_GplQuickJoinList->AddPanelItem< QuickJoinPanelItem >( "QuickJoinPanelItem" );
		if ( result )
		{
			result->SetItemTooltip( "#L4D360UI_MainMenu_Tooltip_QuickJoinItem" );
		}
	}
}

//=============================================================================
void QuickJoinPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );

		if ( panel && !dynamic_cast<BaseModHybridButton*>(panel) )
		{
			Assert( panel );
			panel->SetMouseInputEnabled( false );
		}
	}
}

void QuickJoinPanel::ClearGames()
{
	for ( int i = 0; i < m_GplQuickJoinList->GetPanelItemCount(); ++i )
	{
		QuickJoinPanelItem* panel = dynamic_cast< QuickJoinPanelItem* >( m_GplQuickJoinList->GetPanelItem( i ) );
		Assert( panel );
		if( panel )
		{
			QuickInfo qi;
			memset( &qi, 0, sizeof( qi ) );

			panel->SetAlpha( 255 );
			panel->SetName( L"" );
			panel->SetInfo( qi );
			panel->Update();
		}
	}
}
