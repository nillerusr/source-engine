//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "basetfcombatweapon_shared.h"
#include "weapon_objectselection.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>
#include "keydefs.h"

#include <vgui/IVGui.h>

#include "weapon_twohandedcontainer.h"
#ifdef CLIENT_DLL
#include "c_weapon_builder.h"
#include "iinput.h"
#else
#include "weapon_builder.h"
#endif

using namespace vgui;

class CHudWeaponSelection;

typedef enum 
{
	IMAGE_BACKGROUND = 0,
	IMAGE_CURRENT,
	IMAGE_INVALID
} WeaponBoxType_t;

enum
{
	HUMAN_WEAPON_SELECTION = 0,
	ALIEN_WEAPON_SELECTION,
};

struct CSlotInfo
{
	bool	active;
	bool	valid;
	CHudTexture const *icon;
	bool	drawAmmo;
	float	ammoPerc;
	bool	ammoCaution;
	char	printname[ 128 ];
	CHandle< C_BaseCombatWeapon > weapon;
};

struct CWeaponMenuItem
{
	// If true, it's a dummy item
	bool							buildslot;
	int								priority;
	int								hotkey;
	int								openbuildmenu;
	bool							isvalidmenuitem;
	CHandle< C_BaseCombatWeapon >	weapon;
};

class WeaponMenu
{
public:
	WeaponMenu() : items( 0, 0, WeaponLessFunc )
	{
		m_bIsBuildMenu = false;
		m_nActiveItem = 0;
	}

	void	SetActiveItem( int item )
	{
		bool changed = ( m_nActiveItem != item );

		m_nActiveItem = item;

		if ( !changed )
			return;

		const char *prefix = m_bIsBuildMenu ? "WeaponHiliteBuildMenu" : "WeaponHiliteWeaponMenu";

		if ( m_nActiveItem >= 1 )
		{
			char slotSequence[ 128 ];
			Q_snprintf( slotSequence, sizeof( slotSequence ), "%s%i", prefix, m_nActiveItem );

			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( slotSequence );
		}
	}

	int		GetActiveItem()
	{
		return m_nActiveItem;
	}

	static bool WeaponLessFunc( const CWeaponMenuItem& w1, const CWeaponMenuItem& w2 )
	{
		return w1.priority < w2.priority;
	}

	CUtlRBTree< CWeaponMenuItem, int >	items;
	int			m_nActiveItem;
	bool		m_bIsBuildMenu;
};

class CHudWeaponItemPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponItemPanel, vgui::Panel );

public:
	CHudWeaponItemPanel( vgui::Panel *parent, const char *panelName );

	CPanelAnimationVarAliasType( float, m_flIconWidth, "IconWidth", "60", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconHeight, "IconHeight", "30", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconXPos, "IconXPos", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconYPos, "IconYPos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flTextXPos, "TextXPos", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flTextYPos, "TextYPos", "35", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flAmmoBarX, "AmmoBarX", "75", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flAmmoBarWide, "AmmoBarWide", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSelectionNumberXPos, "SelectionNumberXPos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSelectionNumberYPos, "SelectionNumberYPos", "4", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flPriceXEndPos,	"PriceXEndPos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flPriceYEndPos,	"PriceYEndPos", "4", "proportional_float" );

	CPanelAnimationVar( float, m_flGrowFraction, "GrowFraction", "1.0" );

	bool				m_bInUse;

	bool				m_bDrawNumbers;
	int					m_nSlotNumber;
	bool				m_bBuildMenu;

	// Copies of data
	CSlotInfo			info;
	CWeaponMenuItem		menuItem;

	virtual void		Paint();

	virtual void		ApplySchemeSettings( vgui::IScheme *scheme );

	void				SetupIcons();

	enum
	{
		NUM_SLOT_TEAMS = 2,
		NUM_MENU_TYPES = 3,
	};

	CHudTexture			*m_BackgroundIcons[ NUM_SLOT_TEAMS ][ NUM_MENU_TYPES ];

private:
	void TranslateColor( float percent, Color& clr );
	void DrawBox( bool isbuildmenu, int x, int y, int wide, int tall, bool isactive, bool isvalid, float normalizedAlpha, int number );
	int GetTeamIndex();
	void OnWeaponSelectionDrawn( CHudWeaponSelection *selection, C_WeaponObjectSelection *weapon, bool bCurrentlySelected, 
		int wide, int tall, Color& clr );
};

//-----------------------------------------------------------------------------
// Purpose: tf2 weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponSelection, vgui::Panel );

public:

	CHudWeaponSelection(const char *pElementName );

	virtual bool ShouldDraw( void );
	
	virtual void VidInit(void);

	virtual void CycleToNextWeapon( void );
	virtual void CycleToPrevWeapon( void );
	virtual void SwitchToLastWeapon( void );

	virtual C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual void SelectWeaponSlot( int iSlot );

	virtual void OnWeaponPickup( C_BaseCombatWeapon *pWeapon );

	virtual void SelectWeapon( void );
	
	virtual C_BaseCombatWeapon	*GetSelectedWeapon( void );

	virtual void OpenSelection( void );

	virtual int	KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual void HideSelection( void );

protected:
	virtual void OnTick();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void ApplySettings( KeyValues *resourceData );
private:

	// Offensive
	// Defensive
	// General Purpose
	enum
	{
		BUILD_OFFENSIVE = 0,
		BUILD_DEFENSIVE,
		BUILD_GENERAL_PURPOSE,

		NUM_BUILD_MENUS,
	};

	enum
	{
		MAX_WEAPON_MENU_ITEMS = 7,
		MAX_BUILD_MENU_ITEMS = 6,
	};

	void SetupWeaponMenu( WeaponMenu& menu, C_BaseCombatWeapon *activeItem, bool drawNumbers );
	void SetupBuildMenu( WeaponMenu& menu, C_BaseCombatWeapon *activeItem );

	void DrawBox( bool isbuildmenu, int x, int y, int wide, int tall, bool current, bool isvalid, float normalizedAlpha, int number );

	void TranslateColor( float percent, Color& clr );

	void	ShowBuildMenu( int buildmenu );
	void ShowBuildMenu( WeaponMenu& buildMenu );
	void	HideBuildMenu( void );
	WeaponMenu&	GetActiveMenu();
	int GetLowIndex( WeaponMenu& menu );
	void WrapIndexToRange( int& index, int low, int high );
	int FindSlotForHotKey( WeaponMenu& menu, int hotkey );
	int FindNextSelectableWeaponInMenu( WeaponMenu& menu, int startindex, int direction );
	int CountSelectableItems( WeaponMenu& menu );
	CWeaponMenuItem *GetFirstSelectableWeapon( WeaponMenu& menu );
	const char *GetBuildMenuName( int slot );
	const char *GetBuildMenuMaterial( int slot );

	void	SelectBuildMenuSlot( WeaponMenu& buildMenu );

	void	CreateWeaponItemPanels();

	void	ClearBuildMenu();
	void	RebuildMenus();
	void	AssignSlots( WeaponMenu& menu, int firstindex );
	void	CollapseSingleItemBuildMenus();
	
	void	GetSlotInfo( const CWeaponMenuItem *w, C_BaseCombatWeapon *active, CSlotInfo& info );

	int GetTeamIndex();

	void	SetupIcons();

	CPanelAnimationVar( float, m_flAlphaOverride, "Alpha", "255.0" );
	CPanelAnimationVar( float, m_flSelectionAlphaOverride, "SelectionAlpha", "255.0" );

	CPanelAnimationVar( Color, m_TextColor, "TextColor", "SelectionTextFg" );
	CPanelAnimationVar( Color, m_InvalidActiveColor, "InvalidActiveColor", "InvalidActiveSlotFg" );
	CPanelAnimationVar( Color, m_InvalidActiveTextColor, "InvalidActiveTextColor", "InvalidActiveSlotText" );
	CPanelAnimationVar( Color, m_InvalidColor, "InvalidColor", "InvalidSlotFg" );
	CPanelAnimationVar( Color, m_InvalidTextColor, "InvalidTextColor", "InvalidSlotText" );
	CPanelAnimationVar( Color, m_OtherColor, "OtherColor", "OtherSlotFg" );
	CPanelAnimationVar( Color, m_OtherTextColor, "OtherTextColor", "OtherSlotText" );
	CPanelAnimationVar( Color, m_AmmoNormalColor, "AmmoNormalColor", "AmmoNormal" );
	CPanelAnimationVar( Color, m_AmmoCautionColor, "AmmoCautionColor", "AmmoCaution" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudSelectionText" );
	CPanelAnimationVar( vgui::HFont, m_hTextFontSmall, "TextFontSmall", "HudSelectionTextSmall" );
	CPanelAnimationVar( vgui::HFont, m_hPriceFont, "PriceFont", "HudSelectionTextSmall" );

	CPanelAnimationVar( float, m_flGrowFraction, "GrowFraction", "0.0" );

	CHandle<C_BaseCombatWeapon> m_hLastPickedUpWeapon;
	bool		m_bInSelectionMode;
	int			m_nLastBuildTick;
	bool		m_bInBuildMenu;
	int			m_nActiveBuildMenu;

	WeaponMenu	*m_PreviousBuildMenu;

	WeaponMenu m_Weapons;
	WeaponMenu m_BuildObjects[ NUM_BUILD_MENUS ];

	CHudWeaponItemPanel *m_pWeaponPanels[ MAX_WEAPON_MENU_ITEMS ];
	CHudWeaponItemPanel *m_pBuildPanels[ MAX_BUILD_MENU_ITEMS ];

	CWeaponMenuItem *FindWeapon( WeaponMenu& menu, int hotkey );

	friend class CHudWeaponItemPanel;
};

DECLARE_HUDELEMENT( CHudWeaponSelection );

void CHudWeaponSelection::VidInit(void)
{
	CBaseHudWeaponSelection::VidInit();

	SetupIcons();
}

void CHudWeaponSelection::SetupIcons()
{
	int i;

	for ( i = 0; i < MAX_WEAPON_MENU_ITEMS; i++ )
	{
		m_pWeaponPanels[ i ]->SetupIcons();
	}

	for ( i = 0; i < MAX_BUILD_MENU_ITEMS; i++ )
	{
		m_pBuildPanels[ i ]->SetupIcons();
	}
}


void CHudWeaponSelection::AssignSlots( WeaponMenu& menu, int firstindex )
{
	int slot = firstindex;

	for ( int i = menu.items.FirstInorder(); i != menu.items.InvalidIndex(); i = menu.items.NextInorder( i ) )
	{
		CWeaponMenuItem *w = &menu.items[ i ];
		w->hotkey = slot++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CreateWeaponItemPanels()
{
	int i;
	char sz[ 128 ];
	for ( i = 0; i < MAX_WEAPON_MENU_ITEMS; i++ )
	{
		Q_snprintf( sz, sizeof( sz ), "WeaponMenu%i", i + 1 );
		m_pWeaponPanels[ i ] = new CHudWeaponItemPanel( this, sz );
		m_pWeaponPanels[ i ]->m_nSlotNumber = i;
		m_pWeaponPanels[ i ]->m_bBuildMenu = false;
	}

	for ( i = 0; i < MAX_BUILD_MENU_ITEMS; i++ )
	{
		Q_snprintf( sz, sizeof( sz ), "BuildMenu%i", i + 1 );
		m_pBuildPanels[ i ] = new CHudWeaponItemPanel( this, sz );
		m_pBuildPanels[ i ]->m_nSlotNumber = i;
		m_pBuildPanels[ i ]->m_bBuildMenu = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::RebuildMenus()
{
	int i;
	
	if ( gpGlobals->tickcount == m_nLastBuildTick )
		return;

	m_nLastBuildTick =  gpGlobals->tickcount;


	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	m_Weapons.items.RemoveAll();
	for ( i = 0; i < NUM_BUILD_MENUS; i++ )
	{
		m_BuildObjects[ i ].items.RemoveAll();
	}

	CWeaponMenuItem item;

	int c = MAX_WEAPONS;
	for ( i = 0; i < c; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(  i );
		if ( !pWeapon )
		{
			continue;
		}

		if ( !pWeapon->VisibleInWeaponSelection() )
		{
			continue;
		}

		item.priority = pWeapon->GetSlot() * 10 + pWeapon->GetPosition();
		item.buildslot = false;
		item.openbuildmenu = -1;
		item.weapon = pWeapon;
		item.isvalidmenuitem = true;

		if ( dynamic_cast< C_WeaponObjectSelection * >( pWeapon ) )
		{
			// HACK
			int firstbuildslot = 4;
			int whichBuildMenu = clamp( pWeapon->GetSlot() - firstbuildslot, 0, NUM_BUILD_MENUS - 1 );

			m_BuildObjects[ whichBuildMenu ].items.Insert( item );
		}
		else
		{
			m_Weapons.items.Insert( item );
		}
	}

	while ( m_Weapons.items.Count() < 3 )
	{
		// Add build item menus
		item.priority = 999;
		item.buildslot = false;
		item.openbuildmenu = -1;
		item.weapon = NULL;
		item.isvalidmenuitem = false;
		m_Weapons.items.Insert( item );

		m_Weapons.m_bIsBuildMenu = false;
	}

	for ( i = 0; i < NUM_BUILD_MENUS; i++ )
	{
		// Add build item menus
		item.priority = 999;
		item.buildslot = true;
		item.openbuildmenu = i;
		item.weapon = NULL;
		item.isvalidmenuitem = CountSelectableItems( m_BuildObjects[ i ] ) > 0 ? true : false;
		m_Weapons.items.Insert( item );
	
		// Assign build menu slots
		AssignSlots( m_BuildObjects[ i ], 0 );
	
		m_BuildObjects[ i ].m_bIsBuildMenu = true;
	}

	// Now walk in order and assign hotkeys
	AssignSlots( m_Weapons, 1 );

	// Merge single item build menus into main menu
	CollapseSingleItemBuildMenus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CollapseSingleItemBuildMenus()
{
	for ( int i = 0; i < NUM_BUILD_MENUS; i++ )
	{
		int count = m_BuildObjects[ i ].items.Count();

		// Not an issue
		if ( count > 1 )
			continue;

		CWeaponMenuItem *mainItem = FindWeapon( m_Weapons, 4 + i );
		if ( !mainItem )
			continue;

		// It's empty, need to make main item a disabled slot
		if ( count == 0 )
		{
			// Add build item menus
			mainItem->isvalidmenuitem = false;
		}
		else
		{
			// Only one item in sub menu, copy it over
			CWeaponMenuItem *buildItem = &m_BuildObjects[ i ].items[ 0 ];
			mainItem->buildslot = false;
			mainItem->openbuildmenu = -1;
			mainItem->isvalidmenuitem = true;
			mainItem->weapon = buildItem->weapon;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ClearBuildMenu()
{
	for ( int i = 0; i < MAX_BUILD_MENU_ITEMS; i++ )
	{
		m_pBuildPanels[ i ]->m_bInUse = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection(const char *pElementName ) : 
	CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudWeaponSelection")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_nLastBuildTick = -1;

	m_hNumberFont = NULL;
	m_bInSelectionMode = false;

	CreateWeaponItemPanels();

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

void CHudWeaponSelection::ShowBuildMenu( int buildmenu )
{
	bool wasinbuild = m_bInBuildMenu;

	int mainItem = buildmenu + 4;
	m_Weapons.SetActiveItem( mainItem );

	m_bInBuildMenu = true;
	m_nActiveBuildMenu = buildmenu;
	GetActiveMenu().SetActiveItem( GetLowIndex( GetActiveMenu() ) );

	if ( !wasinbuild )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildSubMenu");

		switch ( buildmenu )
		{
		default:
		case BUILD_OFFENSIVE:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildOffensive");
			break;
		case BUILD_DEFENSIVE:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildDefensive");
			break;
		case BUILD_GENERAL_PURPOSE:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildGeneral");
			break;
		}
	}
}

void CHudWeaponSelection::ShowBuildMenu( WeaponMenu& buildMenu )
{
	int c = m_Weapons.items.Count();
	for ( int i = 0; i < c; i++ )
	{
		CWeaponMenuItem *item = &m_Weapons.items[ i ];
		if ( !item || !item->buildslot )
			continue;

		WeaponMenu &menu = m_BuildObjects[ item->openbuildmenu ];
		if ( &menu == &buildMenu )
		{
			ShowBuildMenu( item->openbuildmenu );
			return;
		}
	}

	Assert( 0 );
}

void CHudWeaponSelection::HideBuildMenu( void )
{
	bool wasinbuild = m_bInBuildMenu;

	GetActiveMenu().SetActiveItem( GetLowIndex( GetActiveMenu() ) );
	m_bInBuildMenu = false;

	if ( wasinbuild )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponCloseBuildSubMenu");
	}

	m_nActiveBuildMenu = -1;
}

WeaponMenu&	CHudWeaponSelection::GetActiveMenu()
{
	return m_bInBuildMenu ? m_BuildObjects[ m_nActiveBuildMenu ] : m_Weapons;
}

int CHudWeaponSelection::GetLowIndex( WeaponMenu& menu )
{
	if ( menu.items.Count() == 0 )
		return 1;

	int lowindex =  menu.items[  menu.items.FirstInorder() ].hotkey;
	return lowindex;
}

void CHudWeaponSelection::WrapIndexToRange( int& index, int low, int high )
{
	int delta = ( high - low ) + 1;
	while ( index < low )
	{
		index += delta;
	}
	
	while ( index > high )
	{
		index -= delta;
	}
}

int CHudWeaponSelection::FindSlotForHotKey( WeaponMenu& menu, int hotkey )
{
	int count		= menu.items.Count();
	for ( int i = 0; i < count;i++ )
	{
		if ( menu.items[ i ].hotkey == hotkey )
			return i;
	}

	return 0;
}

int CHudWeaponSelection::FindNextSelectableWeaponInMenu( WeaponMenu& menu, int startindex, int direction )
{
	direction = direction > 0 ? 1 : -1;

	int count		= menu.items.Count();
	int lowindex	= GetLowIndex( menu );
	int highindex	= lowindex + count - 1;

	for ( int offset = 1; offset <= count; offset++ )
	{
		int current = startindex + offset * direction;
		
		WrapIndexToRange( current, lowindex, highindex );

		//int slot = FindSlotForHotKey( menu, current );

		//C_BaseCombatWeapon *w = menu.items[ slot ].weapon;
		//if ( w && !w->CanBeSelected() )
		//	continue;

		return current;
	}

	return startindex;
}

int CHudWeaponSelection::CountSelectableItems( WeaponMenu& menu )
{
	int count = 0;
	int c = menu.items.Count();
	for ( int i = 0; i < c; i++ )
	{
		C_BaseCombatWeapon *w = menu.items[ i ].weapon;
		if ( w && w->CanBeSelected() )
		{
			count++;
		}
	}
	return count;
}

CWeaponMenuItem *CHudWeaponSelection::GetFirstSelectableWeapon( WeaponMenu& menu )
{
	int c = menu.items.Count();
	for ( int i = 0; i < c; i++ )
	{
		C_BaseCombatWeapon *w = menu.items[ i ].weapon;
		if ( w && w->CanBeSelected() )
		{
			return &menu.items[ i ];
		}
	}
	return NULL;
}

const char *CHudWeaponSelection::GetBuildMenuName( int slot )
{
	switch ( slot )
	{
	default:
	case BUILD_OFFENSIVE:
		return "Offensive Object";
	case BUILD_DEFENSIVE:
		return "Defensive Object";
	case BUILD_GENERAL_PURPOSE:
		return "General Purpose";
	}

	return "General Purpose";
}

const char *CHudWeaponSelection::GetBuildMenuMaterial( int slot )
{
	switch ( slot )
	{
	default:
	case BUILD_OFFENSIVE:
		return "weapon_selection_offensive";
	case BUILD_DEFENSIVE:
		return "weapon_selection_defensive";
	case BUILD_GENERAL_PURPOSE:
		return "weapon_selection_general";
	}

	return "hud/menu/weapon_selection_general";
}

//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon pickup
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponPickup( C_BaseCombatWeapon *pWeapon )
{
	// Nothing in TF2
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::ShouldDraw()
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer || pPlayer->GetPlayerClass() == NULL )
	{
		if ( IsInSelectionMode() )
		{
			HideSelection();
		}
		return false;
	}
	
	bool bret = CBaseHudWeaponSelection::ShouldDraw();
	
	if ( !bret )
		return false;

	return ( m_bSelectionVisible || ( m_flGrowFraction > 0.0f ) ) ? true : false;
}

void CHudWeaponSelection::TranslateColor( float percent, Color& clr )
{
	if ( percent >= 1.0f )
		return;

	if ( percent <= 0.75f )
	{
		clr = Color( 0, 0, 0, 0 );
	}
	else
	{
		float frac = ( percent - 0.75f ) / 0.25f;

		clr = Color( clr[0] * frac,
			clr[1] * frac,
			clr[2] * frac,
			clr[3] * frac );
	}
}

void CHudWeaponSelection::GetSlotInfo( const CWeaponMenuItem *w, C_BaseCombatWeapon *active, CSlotInfo& info )
{
	if ( w->buildslot )
	{
		info.active = !m_bInBuildMenu && ( GetActiveMenu().GetActiveItem() == w->hotkey );
		info.valid = w->isvalidmenuitem;
		info.icon = gHUD.GetIcon( GetBuildMenuMaterial( w->openbuildmenu ) );
		info.drawAmmo = false;
		info.ammoCaution = false;
		Q_snprintf( info.printname, sizeof( info.printname ), GetBuildMenuName( w->openbuildmenu ) );
		info.weapon = NULL;
	}
	else if ( !w->weapon )
	{
		info.active =false;
		info.valid = false;
		info.icon = NULL;
		info.drawAmmo = false;
		info.ammoCaution = false;
		info.printname[0]=0;
		info.weapon = NULL;
	}
	else
	{
		C_BaseCombatWeapon *pWeapon = w->weapon;
		Assert( pWeapon );

		info.active = active == pWeapon;
		info.valid = pWeapon->CanBeSelected();
		info.icon = info.active ? pWeapon->GetSpriteActive() : pWeapon->GetSpriteInactive();

		info.drawAmmo = false;
		info.ammoPerc = 1.0f;
		// Don't know the max ammo, so if I don't use clips, just go red on no ammo left
		if ( pWeapon->UsesClipsForAmmo1() || pWeapon->HasAmmo() )
		{
			info.ammoPerc = 1.0f - ( (float) pWeapon->Clip1() ) / ( (float)pWeapon->GetMaxClip1() );
			info.drawAmmo = true;
		}
		info.ammoCaution = ( info.ammoPerc >= CLIP_PERC_THRESHOLD ) ? true : false;

		Q_snprintf( info.printname, sizeof( info.printname ), "%s", pWeapon->GetPrintName() );
		info.weapon = pWeapon;
	}

	strupr(info.printname);
}

void CHudWeaponSelection::SetupWeaponMenu( WeaponMenu& menu, C_BaseCombatWeapon *activeItem, bool drawNumbers )
{
	int panelPosition = 0;
	for ( int item = menu.items.FirstInorder(); item != menu.items.InvalidIndex() && panelPosition < MAX_WEAPON_MENU_ITEMS; item = menu.items.NextInorder( item ) )
	{
		CWeaponMenuItem *w = &menu.items[ item ];
		Assert( w );

		CSlotInfo info;
		GetSlotInfo( w, activeItem, info );

		CHudWeaponItemPanel *panel = m_pWeaponPanels[ panelPosition++ ];
		if ( !panel )
		{
			Assert( 0 );
			continue;
		}

		panel->m_bInUse		= true;
		panel->info			= info;
		panel->menuItem		= *w;
		panel->m_bDrawNumbers = drawNumbers;
	}

	for ( ; panelPosition < MAX_WEAPON_MENU_ITEMS; )
	{
		CHudWeaponItemPanel *panel = m_pWeaponPanels[ panelPosition++ ];
		panel->m_bInUse = false;
	}
}

void CHudWeaponSelection::SetupBuildMenu( WeaponMenu& menu, C_BaseCombatWeapon *activeItem )
{
	int panelPosition = 0;
	for ( int item = menu.items.FirstInorder(); item != menu.items.InvalidIndex() && panelPosition < MAX_BUILD_MENU_ITEMS; item = menu.items.NextInorder( item ) )
	{
		CWeaponMenuItem *w = &menu.items[ item ];
		Assert( w );

		CSlotInfo info;
		GetSlotInfo( w, activeItem, info );

		CHudWeaponItemPanel *panel = m_pBuildPanels[ panelPosition++ ];
		if ( !panel )
		{
			Assert( 0 );
			continue;
		}

		panel->m_bInUse		= true;
		panel->info			= info;
		panel->menuItem		= *w;
		panel->m_bDrawNumbers = false;
	}

	for ( ; panelPosition < MAX_BUILD_MENU_ITEMS; )
	{
		CHudWeaponItemPanel *panel = m_pBuildPanels[ panelPosition++ ];
		panel->m_bInUse = false;
	}
}

void CHudWeaponSelection::OnTick()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	RebuildMenus();

	int x, y;
	GetPos(x, y);

	C_BaseCombatWeapon *wpn = GetSelectedWeapon();

	// Draw main menu
	SetupWeaponMenu( m_Weapons, wpn, true );
	
	CWeaponMenuItem *item = FindWeapon( GetActiveMenu(), GetActiveMenu().GetActiveItem() );

	WeaponMenu* otherMenu = NULL;
	int buildMenuNumber=  0;

	// Drawing build menu
	if ( m_bInBuildMenu )
	{
		otherMenu = &m_BuildObjects[ m_nActiveBuildMenu ];
		buildMenuNumber = m_nActiveBuildMenu;
	}
	else if ( item && item->buildslot && item->hotkey == GetActiveMenu().GetActiveItem() )
	{
		otherMenu = &m_BuildObjects[ item->openbuildmenu ];
		buildMenuNumber = item->openbuildmenu;
	}
	else if ( m_nActiveBuildMenu >= 0 )
	{
		otherMenu = &m_BuildObjects[ m_nActiveBuildMenu ];
		buildMenuNumber = m_nActiveBuildMenu;
	}

	if ( otherMenu )
	{
		SetupBuildMenu( *otherMenu, wpn );
	}
	else
	{
		ClearBuildMenu();
	}

	if ( otherMenu != m_PreviousBuildMenu )
	{
		m_PreviousBuildMenu = otherMenu;

		if ( otherMenu )
		{
			switch ( buildMenuNumber )
			{
			default:
			case BUILD_OFFENSIVE:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildOffensive");
				break;
			case BUILD_DEFENSIVE:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildDefensive");
				break;
			case BUILD_GENERAL_PURPOSE:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenBuildGeneral");
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: Select the next item in the weapon list
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon( void )
{
	// Get the local player.
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer || !pPlayer->GetPlayerClass() )
		return;

	// Make it active
	bool justopened = false;
	if ( !IsInSelectionMode() )
	{
		OpenSelection();
		justopened = true;
	}

	RebuildMenus();

	WeaponMenu& menu = GetActiveMenu();
	int c = menu.items.Count();
	if ( c != 0 && !justopened )
	{
		GetActiveMenu().SetActiveItem( FindNextSelectableWeaponInMenu( menu, GetActiveMenu().GetActiveItem(), +1 ) );
	}

	// Play the "cycle to next weapon" sound
	pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}

//-----------------------------------------------------------------------------
// Purpose: Selects the previous item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToPrevWeapon( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer || !pPlayer->GetPlayerClass() )
		return;

	// Make it active
	bool justopened = false;
	if ( !IsInSelectionMode() )
	{
		OpenSelection();
		justopened = true;
	}

	RebuildMenus();

	WeaponMenu& menu = GetActiveMenu();
	int c = menu.items.Count();
	if ( c != 0 && !justopened )
	{
		GetActiveMenu().SetActiveItem( FindNextSelectableWeaponInMenu( menu, GetActiveMenu().GetActiveItem(), -1 ) );
	}

	// Play the "cycle to next weapon" sound
	pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}

//-----------------------------------------------------------------------------
// Purpose: Switches the last weapon the player was using
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SwitchToLastWeapon( void )
{
	// Get the player's last weapon
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// If we're currently using the builder weapon, switch to the weapon we were
	// using before we started using it.
	C_BaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
	// Handle the twohanded weapon container
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pActiveWeapon );
	if ( pContainer )
	{
		pActiveWeapon = pContainer->GetLeftWeapon();
	}

	if ( dynamic_cast< C_WeaponBuilder* >( pActiveWeapon ) )
	{
		::input->MakeWeaponSelection( pPlayer->GetLastWeaponBeforeObject() );
		return;
	}

	// Switch to previous weapon normally
	::input->MakeWeaponSelection( pPlayer->GetLastWeapon() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMenuItem *CHudWeaponSelection::FindWeapon( WeaponMenu& menu, int hotkey )
{
	RebuildMenus();

	int c = menu.items.Count();

	for ( int i = 0; i < c; i++ )
	{
		CWeaponMenuItem *item = &menu.items[ i ];
		if ( item->hotkey == hotkey )
			return item;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	CWeaponMenuItem *item = FindWeapon( GetActiveMenu(), iSlot );
	if ( item )
	{
		return item->weapon;
	}

	return NULL;
}

C_BaseCombatWeapon *CHudWeaponSelection::GetSelectedWeapon( void )
{
	RebuildMenus();

	return GetWeaponInSlot( GetActiveMenu().GetActiveItem(), 0 );
}

void CHudWeaponSelection::SelectBuildMenuSlot( WeaponMenu& buildMenu )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int numSelectable = CountSelectableItems( buildMenu );

	if ( numSelectable >= 1 )
	{
		// Single item, just fast switch to it
		CWeaponMenuItem *fastSwitchItem = GetFirstSelectableWeapon( buildMenu );
		C_BaseCombatWeapon *w = fastSwitchItem ? fastSwitchItem->weapon : NULL;

		Assert( w && w->CanBeSelected() );
		if ( 0 && numSelectable == 1 && w && w->CanBeSelected() )
		{
			ShowBuildMenu( buildMenu );

			m_hSelectedWeapon = w;
			GetActiveMenu().SetActiveItem( fastSwitchItem->hotkey );

			// There's only one active item in bucket, so change directly to weapon
			SetWeaponSelected();
			engine->ClientCmd( "cancelselect\n" );
		}
		else
		{
			// Play the "open weapon selection" sound
			pPlayer->EmitSound( "Player.WeaponSelectionOpen" );

			ShowBuildMenu( buildMenu );
		}
	}
	else
	{
		pPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Weapon selection code
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeaponSlot( int iSlot )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
		return;

	// Make it active
	if ( !IsInSelectionMode() )
	{
		OpenSelection();
	}

	RebuildMenus();
	CWeaponMenuItem *item = FindWeapon( GetActiveMenu(), iSlot );

	C_BaseCombatWeapon *weapon = item ? item->weapon : NULL;

	if ( item && item->buildslot )
	{
		SelectBuildMenuSlot( m_BuildObjects[ item->openbuildmenu ] );
		return;
	}

	if ( m_bInBuildMenu )
	{
		CWeaponMenuItem *mainItem = FindWeapon( m_Weapons, iSlot );
		if ( mainItem )
		{
			if ( mainItem->buildslot )
			{
				if ( mainItem->openbuildmenu == m_nActiveBuildMenu )
				{
					CycleToNextWeapon();
				}
				else
				{
					SelectBuildMenuSlot(  m_BuildObjects[ mainItem->openbuildmenu ] );
				}
			}
			else if ( mainItem->weapon )
			{
				// Play the "open weapon selection" sound
				pPlayer->EmitSound( "Player.WeaponSelectionOpen" );

				m_bInBuildMenu = false;
				m_hSelectedWeapon = mainItem->weapon;
				GetActiveMenu().SetActiveItem( mainItem->hotkey );

				// Check for fast weapon switch mode
				if ( GetSelectedWeapon() && GetSelectedWeapon()->CanBeSelected() )
				{	
					// There's only one active item in bucket, so change directly to weapon
					SetWeaponSelected();
					engine->ClientCmd( "cancelselect\n" );
					return;
				}
			}
		}
		return;
	}

	if ( item && weapon )
	{
		// Play the "open weapon selection" sound
		pPlayer->EmitSound( "Player.WeaponSelectionOpen" );

		if ( !weapon->CanBeSelected() )
		{
			if ( GetActiveMenu().GetActiveItem() == item->hotkey )
			{
				pPlayer->EmitSound( "Player.DenyWeaponSelection" );
			}
		}

		m_hSelectedWeapon = weapon;
		GetActiveMenu().SetActiveItem( item->hotkey );

		// Check for fast weapon switch mode
		if ( GetSelectedWeapon() && GetSelectedWeapon()->CanBeSelected() )
		{	
			// There's only one active item in bucket, so change directly to weapon
			SetWeaponSelected();
			engine->ClientCmd( "cancelselect\n" );
			return;
		}
	}
	else
	{
		pPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player has chosen to draw the currently selected weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeapon( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	CWeaponMenuItem *item = FindWeapon( GetActiveMenu(), GetActiveMenu().GetActiveItem() );
	if ( !item )
	{
		engine->ClientCmd( "cancelselect\n" );
		return;
	}

	if ( item->buildslot )
	{
		SelectBuildMenuSlot(  m_BuildObjects[ item->openbuildmenu ] );
		return;
	}

	CBaseHudWeaponSelection::SelectWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : down - 
//			keynum - 
//			*pszCurrentBinding - 
//-----------------------------------------------------------------------------
int	CHudWeaponSelection::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( !down )
		return 1;

	if ( keynum != KEY_ESCAPE )
		return 1;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return 1;

	if ( !IsInSelectionMode() )
		return 1;

	if ( !m_bInBuildMenu )
	{
		engine->ClientCmd( "cancelselect\n" );
		return 0;
	}

	// Play the "open weapon selection" sound
	player->EmitSound( "Player.WeaponSelectionOpen" );

	HideBuildMenu();

	// Swallow the key
	return 0;
}

void CHudWeaponSelection::OpenSelection( void )
{
	HideBuildMenu();
	m_nActiveBuildMenu = -1;
	
	CBaseHudWeaponSelection::OpenSelection();
	
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponOpenWeaponMenu");
}

void CHudWeaponSelection::HideSelection( void )
{
	CBaseHudWeaponSelection::HideSelection();
	
	if ( m_bInBuildMenu )
	{
		HideBuildMenu();
	}

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponCloseWeaponMenu");
}

int CHudWeaponSelection::GetTeamIndex()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return HUMAN_WEAPON_SELECTION;
	int team = pPlayer->GetTeamNumber();
	if ( !team )
		return HUMAN_WEAPON_SELECTION;

	bool human = ( team == TEAM_HUMANS ) ? true : false;
	return human ? HUMAN_WEAPON_SELECTION : ALIEN_WEAPON_SELECTION;
}

//-----------------------------------------------------------------------------
// Purpose: serializes settings from a resource data container
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySettings( KeyValues *resourceData )
{
	BaseClass::ApplySettings( resourceData );

	// loop through all the keys, applying them wherever
	for (KeyValues *controlKeys = resourceData->GetFirstSubKey(); controlKeys != NULL; controlKeys = controlKeys->GetNextKey())
	{
		// Skip keys that are atomic..
		if (controlKeys->GetDataType() != KeyValues::TYPE_NONE)
			continue;

		Panel *panel = FindChildByName( controlKeys->GetName() );
		if ( !panel )
			continue;

		// apply the settings
		panel->ApplySettings(controlKeys);
	}
}









//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHudWeaponItemPanel::CHudWeaponItemPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
	m_bInUse = false;
	m_bDrawNumbers = false;
	m_nSlotNumber = 0;

	// Copies of data
	memset( &info, 0, sizeof( info ) );
	memset( &menuItem, 0, sizeof( menuItem ) );

	memset( m_BackgroundIcons, 0, sizeof( m_BackgroundIcons ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::Paint()
{
	if ( !m_bInUse )
		return;

	CHudWeaponSelection *selection = static_cast< CHudWeaponSelection * >( GetParent() );
	if ( !selection )
		return;

	int wide, tall;
	GetSize( wide, tall );

	float transparencyfrac = m_flGrowFraction;

	float sizefracw = m_flGrowFraction;
	float sizefrach = m_flGrowFraction;
	float sizefrac = m_flGrowFraction;

	bool buildMenu = m_bBuildMenu;

	bool isactive = info.active;
	bool isvalid = 	info.valid;

	Color iconColor = selection->GetFgColor();
	Color textColor  = selection->m_TextColor;
	if ( isactive )
	{
		if ( !isvalid )
		{
			iconColor = selection->m_InvalidActiveColor;
			textColor = selection->m_InvalidActiveTextColor;
		}
	}
	else
	{
		if ( isvalid )
		{
			iconColor = selection->m_OtherColor;
			textColor = selection->m_OtherTextColor;
		}
		else
		{
			iconColor = selection->m_InvalidColor;
			textColor = selection->m_InvalidTextColor;
		}
	}

	Color ammoColor = info.ammoCaution ? selection->m_AmmoCautionColor : selection->m_AmmoNormalColor;

	TranslateColor( transparencyfrac, iconColor );
	TranslateColor( transparencyfrac, textColor );
	TranslateColor( transparencyfrac, ammoColor );

	// Draw box
	DrawBox( buildMenu, 0, 0, wide, tall, isactive, isvalid, m_flGrowFraction * 255, m_bDrawNumbers ? menuItem.hotkey : -1 );

	// icons use old system, drawing in screen space
	int iconXPos = m_flIconXPos;
	int iconYPos = m_flIconYPos;
	int iconWide = m_flIconWidth;
	int iconTall = m_flIconHeight;

	if ( info.icon )
	{
		info.icon->DrawSelf( sizefracw * iconXPos, sizefrach * iconYPos, sizefracw * iconWide, sizefrach * iconTall,
			iconColor ); 
	}

	surface()->DrawSetTextColor( textColor );
	HFont textFont = sizefrac < 1.0 ? selection->m_hTextFontSmall : selection->m_hTextFont;
	surface()->DrawSetTextFont( textFont );
	//int slen = UTIL_ComputeStringWidth( textFont, info.printname ); 
	int charCount = Q_strlen( info.printname );

	int textYPos = m_flTextYPos;
	int textXPos = m_flTextXPos;

	surface()->DrawSetTextPos( sizefracw * textXPos, sizefrach * textYPos );
	for (char *pch = info.printname; charCount > 0; pch++, charCount--)
	{
		surface()->DrawUnicodeChar(*pch);
	}

	if ( info.drawAmmo )
	{
		int ammoBarX = m_flAmmoBarX;
		int ammoBarWide = m_flAmmoBarWide;

		// Draw the clip ratio bar
		gHUD.DrawProgressBar( 
			sizefracw * ammoBarX, sizefrach * iconYPos, 
			sizefracw * ammoBarWide, sizefrach * iconTall, info.ammoPerc, 
			ammoColor, CHud::HUDPB_VERTICAL );
	}

	// Let the weapon draw stuff too
	if ( info.weapon )
	{
		C_WeaponObjectSelection *selectionWeapon = dynamic_cast< C_WeaponObjectSelection * >( info.weapon.Get() );
		if ( selectionWeapon )
		{
			OnWeaponSelectionDrawn( selection, selectionWeapon, isactive, wide, tall, textColor );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: We've just been drawn in the weapon selection
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::OnWeaponSelectionDrawn( CHudWeaponSelection *selection, C_WeaponObjectSelection *weapon, 
	bool bCurrentlySelected, int wide, int tall, Color& clr )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( weapon->GetOwner() );
	if ( !pOwner )
		return;

	// Draw our resource cost
	int	fontHeight = vgui::surface()->GetFontTall( selection->m_hPriceFont );
	int iCost = CalculateObjectCost( weapon->GetSubType(),  pOwner->GetNumObjects(weapon->GetSubType()), pOwner->GetTeamNumber() );

	int x = wide - m_flPriceXEndPos;
	int y = tall - m_flPriceYEndPos - fontHeight;

	char text[ 32 ];
	Q_snprintf( text, sizeof( text ), "%i", iCost );

	// Compute pixels needed so we can right justify it
	int pixels = 0;
	char *pch;
	for (pch = text; *pch != 0; pch++)
	{
		pixels += vgui::surface()->GetCharacterWidth( selection->m_hPriceFont, *pch );
	}

	vgui::surface()->DrawSetTextFont( selection->m_hPriceFont );
	vgui::surface()->DrawSetTextPos( x - pixels, y );
	vgui::surface()->DrawSetTextColor( clr );
	for ( pch = text; *pch; pch++ )
	{
		vgui::surface()->DrawUnicodeChar(*pch);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percent - 
//			clr - 
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::TranslateColor( float percent, Color& clr )
{
	if ( percent >= 1.0f )
		return;

	if ( percent <= 0.75f )
	{
		clr = Color( 0, 0, 0, 0 );
	}
	else
	{
		float frac = ( percent - 0.75f ) / 0.25f;

		clr = Color( clr[0] * frac,
			clr[1] * frac,
			clr[2] * frac,
			clr[3] * frac );
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws a selection box
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::DrawBox( bool isbuildmenu, int x, int y, int wide, int tall, bool isactive, bool isvalid, float normalizedAlpha, int number )
{
	CHudWeaponSelection *selection = static_cast< CHudWeaponSelection * >( GetParent() );
	if ( !selection )
	{
		Assert( 0 );
		return;
	}

	int team = GetTeamIndex();

	Color boxColor = Color( 255, 255, 255, 255 );
	boxColor[3] *= ( normalizedAlpha / 255.0f );

	Color numberColor = selection->m_TextColor;

	CHudTexture *texture;
	
	texture = m_BackgroundIcons[ team ][ (int)IMAGE_BACKGROUND ];
	if ( texture )
	{
		texture->DrawSelf( x, y, wide, tall, boxColor );
	}

	if ( isactive )
	{
		if ( !isvalid )
		{
			texture = m_BackgroundIcons[ team ][ (int)IMAGE_INVALID ];
			if ( texture )
			{
				texture->DrawSelf( x, y, wide, tall, boxColor );
			}

			numberColor = selection->m_InvalidActiveColor;
		}
		else
		{
			texture = m_BackgroundIcons[ team ][ (int)IMAGE_CURRENT ];
			if ( texture )
			{
				texture->DrawSelf( x, y, wide, tall, boxColor );
			}

			numberColor = selection->m_OtherColor;
		}
	}
	else
	{
		if ( !isvalid )
		{
			numberColor = selection->m_InvalidColor;
		}
		else
		{
			numberColor = selection->m_OtherColor;
		}
	}

	// draw the number
	if (number >= 0)
	{
		numberColor[3] *= ( normalizedAlpha / 255.0f );
		surface()->DrawSetTextColor(numberColor);
		surface()->DrawSetTextFont(selection->m_hNumberFont);
		wchar_t wch = '0' + number;
		surface()->DrawSetTextPos(x + m_flSelectionNumberXPos, y + m_flSelectionNumberYPos);
		surface()->DrawUnicodeChar(wch);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CHudWeaponItemPanel::GetTeamIndex()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return HUMAN_WEAPON_SELECTION;
	int team = pPlayer->GetTeamNumber();
	if ( !team )
		return HUMAN_WEAPON_SELECTION;

	bool human = ( team == TEAM_HUMANS ) ? true : false;
	return human ? HUMAN_WEAPON_SELECTION : ALIEN_WEAPON_SELECTION;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponItemPanel::SetupIcons()
{
	memset( m_BackgroundIcons, 0, sizeof( m_BackgroundIcons ) );

	const char *menutypestr = m_bBuildMenu ? "build" : "selection";

	for ( int team = 0; team < NUM_SLOT_TEAMS; team++ )
	{
		const char *teamtype = team == 0 ? "human" : "alien";

		for ( int type = 0; type < NUM_MENU_TYPES; type++ )
		{
			const char *slottype = type == 0 ? "background" : type == 1 ? "current" : "invalid";

			char sz[ 256 ];
			Q_snprintf( sz, sizeof( sz ), "%s_weapon_%s_%s_%i",
				teamtype, menutypestr, slottype, m_nSlotNumber + 1 );

			m_BackgroundIcons[ team ][ type ] = gHUD.GetIcon( sz );;
		}
	}
}
