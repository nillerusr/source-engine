#include "cbase.h"
#include "nb_select_weapon_entry.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "asw_briefing.h"
#include "asw_marine_profile.h"
#include "nb_horiz_list.h"
#include "vgui_bitmapbutton.h"
#include "nb_select_weapon_panel.h"
#include "asw_medal_store.h"

#include "vgui_controls/Panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Weapon_Entry::CNB_Select_Weapon_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pClassImage = new vgui::ImagePanel( this, "ClassImage" );
	m_pClassLabel = new vgui::Label( this, "ClassLabel", "" );	
	m_pNewLabel = new vgui::Label( this, "NewLabel", "" );
	m_pLockedImage = new vgui::ImagePanel( this, "LockedImage" );
	m_pLockedBG = new vgui::Panel( this, "LockedBG" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pNewLabel->SetMouseInputEnabled( false );
	m_pLockedImage->SetMouseInputEnabled( false );
	m_pLockedBG->SetMouseInputEnabled( false );

	m_pWeaponImage = new CBitmapButton( this, "WeaponImage", "" );
	m_pWeaponImage->AddActionSignalTarget( this );
	m_pWeaponImage->SetCommand( "AcceptButton" );

	m_nInventorySlot = -1;
	m_nEquipIndex = -1;
	m_bCanEquip = true;
}

CNB_Select_Weapon_Entry::~CNB_Select_Weapon_Entry()
{

}

void CNB_Select_Weapon_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_weapon_entry.res" );
}

void CNB_Select_Weapon_Entry::PerformLayout()
{
	

	BaseClass::PerformLayout();

	if ( m_nInventorySlot == ASW_INVENTORY_SLOT_EXTRA )
	{
		SetSize( YRES( 60 ), YRES( 113 ) );
		m_pWeaponImage->SetSize( YRES( 50 ), YRES( 50 ) );

		int x, y, w, t;
		m_pWeaponImage->GetBounds( x, y, w, t );
		m_pLockedImage->SetBounds( x, y, w, t );
		m_pLockedBG->SetBounds( x, y, w, t );
		//m_pLockedImage->SetBounds( YRES( -14 ), YRES( 37 ), YRES( 100 ), YRES( 50 ) );
		//m_pLockedBG->SetBounds( YRES( 9 ), YRES( 37 ), YRES( 100 ), YRES( 50 ) );
		m_pNewLabel->SetBounds( YRES( 0 ), YRES( 85 ), YRES( 50 ), YRES( 11 ) );
	}
	else
	{
		SetSize( YRES( 118 ), YRES( 113 ) );
		m_pWeaponImage->SetSize( YRES( 100 ), YRES( 50 ) );
		int x, y, w, t;
		m_pWeaponImage->GetBounds( x, y, w, t );
		m_pLockedImage->SetBounds( x, y, w, t );
		m_pLockedBG->SetBounds( x, y, w, t );
		//m_pLockedImage->SetBounds( YRES( 9 ), YRES( 37 ), YRES( 100 ), YRES( 50 ) );
		//m_pLockedBG->SetBounds( YRES( 9 ), YRES( 37 ), YRES( 100 ), YRES( 50 ) );
		m_pNewLabel->SetBounds( YRES( 0 ), YRES( 85 ), YRES( 100 ), YRES( 11 ) );
	}
}

void CNB_Select_Weapon_Entry::OnThink()
{
	BaseClass::OnThink();

	if ( m_nEquipIndex == -1 )
		return;

	CASW_EquipItem *pItem = ASWEquipmentList()->GetItemForSlot( m_nInventorySlot, m_nEquipIndex );
	if ( !pItem )
		return;

	CASW_WeaponInfo* pWeaponInfo = ASWEquipmentList()->GetWeaponDataFor( STRING( pItem->m_EquipClass ) );
	if ( !pWeaponInfo )
		return;

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( m_nProfileIndex );
	if ( !pProfile )
		return;

	bool bUnlocked = Briefing()->IsWeaponUnlocked( STRING( pItem->m_EquipClass ) );

	if ( bUnlocked )
	{
		m_pLockedImage->SetVisible( false );
		m_pLockedBG->SetVisible( false );
	}
	else
	{
		m_pLockedImage->SetVisible( true );
		m_pLockedBG->SetVisible( true );

		if ( m_nInventorySlot == ASW_INVENTORY_SLOT_EXTRA )
		{
			m_pLockedImage->SetImage( "swarm/EquipIcons/Locked" );
		}
		else
		{
			m_pLockedImage->SetImage( "swarm/EquipIcons/LockedWide" );
		}
	}

	if ( GetMedalStore() )
	{
		m_pNewLabel->SetVisible( GetMedalStore()->IsWeaponNew( m_nInventorySlot == ASW_INVENTORY_SLOT_EXTRA, m_nEquipIndex ) );
	}

	m_bCanEquip = true;
	if ( pWeaponInfo->m_bTech )
	{
		m_pClassImage->SetImage( "swarm/ClassIcons/TechClassIcon" );
		m_pClassLabel->SetText( "#asw_requires_tech" );
		m_pClassImage->SetVisible( true );
		m_pClassLabel->SetVisible( true );
		if ( pProfile->GetMarineClass() != MARINE_CLASS_TECH )
			m_bCanEquip = false;
	}
	else if ( pWeaponInfo->m_bFirstAid )
	{
		m_pClassImage->SetImage( "swarm/ClassIcons/MedicClassIcon" );
		m_pClassLabel->SetText( "#asw_requires_medic" );
		m_pClassImage->SetVisible( true );
		m_pClassLabel->SetVisible( true );
		if ( pProfile->GetMarineClass() != MARINE_CLASS_MEDIC )
			m_bCanEquip = false;
	}
	else if ( pWeaponInfo->m_bSpecialWeapons )
	{
		m_pClassImage->SetImage( "swarm/ClassIcons/SpecialWeaponsClassIcon" );
		m_pClassLabel->SetText( "#asw_requires_sw" );
		m_pClassImage->SetVisible( true );
		m_pClassLabel->SetVisible( true );
		if ( pProfile->GetMarineClass() != MARINE_CLASS_SPECIAL_WEAPONS )
			m_bCanEquip = false;
	}
	else if ( pWeaponInfo->m_bSapper )
	{
		m_pClassImage->SetImage( "swarm/ClassIcons/NCOClassIcon" );
		m_pClassLabel->SetText( "#asw_requires_nco" );
		m_pClassImage->SetVisible( true );
		m_pClassLabel->SetVisible( true );
		if ( pProfile->GetMarineClass() != MARINE_CLASS_NCO )
			m_bCanEquip = false;
	}
	else
	{
		m_pClassImage->SetVisible( false );
		m_pClassLabel->SetVisible( false );
	}

	if ( !bUnlocked )
	{
		m_bCanEquip = false;
	}

	// TODO: Already equipped ("big" items)
	// bool		m_bUnique;

	// assumes parent's parent is the horiz list!
	//CNB_Horiz_List *pList = dynamic_cast<CNB_Horiz_List*>( GetParent()->GetParent() );
	//bool bHighlight = ( pList && pList->GetHighlightedEntry() == this );

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	color32 lightblue;
	lightblue.r = 66;
	lightblue.g = 142;
	lightblue.b = 192;
	lightblue.a = 255;

	color32 disabled;
// 	disabled.r = 66;
// 	disabled.g = 142;
// 	disabled.b = 192;
	disabled.r = 0;
	disabled.g = 0;
	disabled.b = 0;
	disabled.a = 255;
		
	if ( !m_bCanEquip )
	{
		m_pWeaponImage->SetEnabled( false );
	}
	else
	{
		m_pWeaponImage->SetEnabled( true );
	}

	char buffer[ 256 ];
	Q_snprintf( buffer, sizeof( buffer ), "vgui/%s", pWeaponInfo->szEquipIcon );
	if ( Q_strcmp( buffer, m_szLastImage ) ||
		!m_pWeaponImage->IsImageLoaded( CBitmapButton::BUTTON_PRESSED ) ||
		!m_pWeaponImage->IsImageLoaded( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER ) ||
		!m_pWeaponImage->IsImageLoaded( CBitmapButton::BUTTON_ENABLED ) ||
		!m_pWeaponImage->IsImageLoaded( CBitmapButton::BUTTON_DISABLED )
		)
	{
		Q_snprintf( m_szLastImage, sizeof( m_szLastImage ), "%s", buffer );

		m_pWeaponImage->SetImage( CBitmapButton::BUTTON_PRESSED, buffer, white );
		m_pWeaponImage->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, buffer, ( bUnlocked ? white : disabled ) );
		m_pWeaponImage->SetImage( CBitmapButton::BUTTON_ENABLED, buffer, lightblue );
		m_pWeaponImage->SetImage( CBitmapButton::BUTTON_DISABLED, buffer, disabled );
	}
}

bool CNB_Select_Weapon_Entry::IsCursorOver()
{
	return m_pWeaponImage->IsCursorOver();
}

void CNB_Select_Weapon_Entry::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "AcceptButton" ) )
	{
		CNB_Select_Weapon_Panel* pParent = dynamic_cast<CNB_Select_Weapon_Panel*>( GetParent()->GetParent()->GetParent() );
		pParent->OnCommand( command );
		return;
	}
	BaseClass::OnCommand( command );
}