#include "cbase.h"
#include "nb_select_weapon_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "nb_horiz_list.h"
#include "nb_weapon_detail.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "asw_shareddefs.h"
#include "nb_select_weapon_entry.h"
#include "asw_briefing.h"
#include "asw_model_panel.h"
#include <vgui_controls/AnimationController.h>
#include "nb_header_footer.h"
#include "nb_button.h"
#include "asw_medal_store.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Weapon_Panel::CNB_Select_Weapon_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pHorizList = new CNB_Horiz_List( this, "HorizList" );
	m_pWeaponLabel = new vgui::Label( this, "WeaponLabel", "" );
	m_pDescriptionTitle = new vgui::Label( this, "DescriptionTitle", "" );
	m_pDescriptionLabel = new vgui::Label( this, "DescriptionLabel", "" );
	m_pWeaponDetail0 = new CNB_Weapon_Detail( this, "WeaponDetail0" );
	m_pWeaponDetail1 = new CNB_Weapon_Detail( this, "WeaponDetail1" );
	m_pWeaponDetail2 = new CNB_Weapon_Detail( this, "WeaponDetail2" );
	m_pWeaponDetail3 = new CNB_Weapon_Detail( this, "WeaponDetail3" );
	m_pWeaponDetail4 = new CNB_Weapon_Detail( this, "WeaponDetail4" );
	m_pWeaponDetail5 = new CNB_Weapon_Detail( this, "WeaponDetail5" );
	m_pWeaponDetail6 = new CNB_Weapon_Detail( this, "WeaponDetail6" );
	m_pItemModelPanel = new CASW_Model_Panel( this, "ItemModelPanel" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pAcceptButton = new CNB_Button( this, "AcceptButton", "", this, "AcceptButton" );
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );

	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_BLUE );

	m_pItemModelPanel->m_bShouldPaint = false;
	m_pItemModelPanel->SetVisible( false );

	m_nLastWeaponHash = -1;
}

CNB_Select_Weapon_Panel::~CNB_Select_Weapon_Panel()
{

}

void CNB_Select_Weapon_Panel::InitWeaponList()
{
	if ( m_nInventorySlot != ASW_INVENTORY_SLOT_EXTRA )
	{
		for ( int i = 0; i < ASWEquipmentList()->GetNumRegular( false ); i++ )
		{
			CNB_Select_Weapon_Entry *pEntry = new CNB_Select_Weapon_Entry( NULL, VarArgs( "Entry%d", i ) );
			pEntry->m_nInventorySlot = m_nInventorySlot;
			pEntry->m_nEquipIndex = i;
			pEntry->m_nProfileIndex = m_nProfileIndex;
			m_pHorizList->AddEntry( pEntry );
		}
		if ( m_nInventorySlot == ASW_INVENTORY_SLOT_PRIMARY )
		{
			m_pTitle->SetText( "#nb_select_weapon_one" );
		}
		else
		{
			m_pTitle->SetText( "#nb_select_weapon_two" );
		}	
	}
	else
	{
		for ( int i = 0; i < ASWEquipmentList()->GetNumExtra( false ); i++ )
		{
			CNB_Select_Weapon_Entry *pEntry = new CNB_Select_Weapon_Entry( NULL, VarArgs( "Entry%d", i ) );
			pEntry->m_nInventorySlot = m_nInventorySlot;
			pEntry->m_nEquipIndex = i;
			pEntry->m_nProfileIndex = m_nProfileIndex;
			m_pHorizList->AddEntry( pEntry );
		}
		m_pTitle->SetText( "#nb_select_offhand" );
	}

	int nEquipIndex = Briefing()->GetProfileSelectedWeapon( m_nProfileIndex, m_nInventorySlot );
	m_pHorizList->SetHighlight( nEquipIndex );
}

void CNB_Select_Weapon_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_weapon_panel.res" );

	m_pHorizList->m_pBackgroundImage->SetImage( "briefing/select_marine_list_bg" );
	m_pHorizList->m_pForegroundImage->SetImage( "briefing/horiz_list_fg" );
}

void CNB_Select_Weapon_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	KeyValues *pKV = new KeyValues( "ItemModelPanel" );
	pKV->SetInt( "fov", 20 );
	pKV->SetInt( "start_framed", 0 );
	pKV->SetInt( "disable_manipulation", 1 );
	m_pItemModelPanel->ApplySettings( pKV );
}

void CNB_Select_Weapon_Panel::OnThink()
{
	BaseClass::OnThink();

	CNB_Select_Weapon_Entry *pHighlighted = dynamic_cast<CNB_Select_Weapon_Entry*>( m_pHorizList->GetHighlightedEntry() );
	if ( pHighlighted )
	{
		CASW_EquipItem *pItem = ASWEquipmentList()->GetItemForSlot( pHighlighted->m_nInventorySlot, pHighlighted->m_nEquipIndex );
		if ( !pItem )
			return;

		CASW_WeaponInfo* pWeaponInfo = ASWEquipmentList()->GetWeaponDataFor( STRING( pItem->m_EquipClass ) );
		if ( !pWeaponInfo )
			return;
		
		m_pWeaponLabel->SetText( pWeaponInfo->szEquipLongName );
		m_pDescriptionLabel->SetText( pWeaponInfo->szEquipDescription1 );

		bool bShowDetails = ( (m_nInventorySlot != ASW_INVENTORY_SLOT_EXTRA && pWeaponInfo->m_flBaseDamage > 0 ) || pWeaponInfo->m_iDisplayClipSize >= 0 );

		int nWeaponHash = pItem->m_iItemIndex * ( m_nInventorySlot + 1 );
		bool bWeaponChanged = false;
		if ( nWeaponHash != m_nLastWeaponHash )
		{
			m_nLastWeaponHash = nWeaponHash;
			bWeaponChanged = true;
		}

		bool bShowModelPanel = true; //!bShowDetails;

		if ( bShowDetails )
		{
			m_pWeaponDetail0->m_bHidden = false;
			m_pWeaponDetail1->m_bHidden = false;
			m_pWeaponDetail2->m_bHidden = false;
			m_pWeaponDetail3->m_bHidden = false;
			m_pWeaponDetail4->m_bHidden = false;
			m_pWeaponDetail5->m_bHidden = false;
			m_pWeaponDetail6->m_bHidden = false;
		}
		else
		{
			m_pWeaponDetail0->m_bHidden = true;
			m_pWeaponDetail1->m_bHidden = true;
			m_pWeaponDetail2->m_bHidden = true;
			m_pWeaponDetail3->m_bHidden = true;
			m_pWeaponDetail4->m_bHidden = true;
			m_pWeaponDetail5->m_bHidden = false;
			m_pWeaponDetail6->m_bHidden = false;
		}

		if ( bShowModelPanel )
		{
			Vector vecPos = Vector( -275.0, 0.0, 190.0 );
			QAngle angRot = QAngle( 32.0, 0.0, 0.0 );

			vecPos.z += pWeaponInfo->m_flModelPanelZOffset;

			Vector vecBoundsMins, vecBoundsMax;
			m_pItemModelPanel->GetBoundingBox( vecBoundsMins, vecBoundsMax );
			int iMaxBounds = -vecBoundsMins.x + vecBoundsMax.x;
			iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.y + vecBoundsMax.y );
			iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.z + vecBoundsMax.z );
			vecPos *= (float)iMaxBounds/64.0f;

			m_pItemModelPanel->SetCameraPositionAndAngles( vecPos, angRot );
			m_pItemModelPanel->SetModelAnglesAndPosition( QAngle( 0.0f, gpGlobals->curtime * 45.0f , 0.0f ), vec3_origin );

			if ( bWeaponChanged )
			{
				m_pItemModelPanel->ClearMergeMDLs();
				if ( Q_stricmp( pWeaponInfo->szDisplayModel, "" ) )
				{
					m_pItemModelPanel->SetMDL( pWeaponInfo->szDisplayModel );
					if ( Q_stricmp( pWeaponInfo->szDisplayModel2, "" ) )
						m_pItemModelPanel->SetMergeMDL( pWeaponInfo->szDisplayModel2 );
				}
				else
				{
					m_pItemModelPanel->SetMDL( pWeaponInfo->szWorldModel );
				}

				int nSkin = 0;
				if ( pWeaponInfo->m_iDisplayModelSkin > 0 )
					nSkin = pWeaponInfo->m_iDisplayModelSkin;
				else
					nSkin = pWeaponInfo->m_iPlayerModelSkin;

				m_pItemModelPanel->SetSkin( nSkin );
				m_pItemModelPanel->SetModelAnim( m_pItemModelPanel->FindAnimByName( "idle" ) );
				m_pItemModelPanel->m_bShouldPaint = true;
				m_pItemModelPanel->SetVisible( true );


				// force resetup of various things (this block of code fixes size popping when changing model)
				m_pItemModelPanel->InvalidateLayout( true );

				m_pItemModelPanel->GetBoundingBox( vecBoundsMins, vecBoundsMax );
				int iMaxBounds = -vecBoundsMins.x + vecBoundsMax.x;
				iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.y + vecBoundsMax.y );
				iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.z + vecBoundsMax.z );
				vecPos *= (float)iMaxBounds/64.0f;

				m_pItemModelPanel->SetCameraPositionAndAngles( vecPos, angRot );
				m_pItemModelPanel->SetModelAnglesAndPosition( QAngle( 0.0f, gpGlobals->curtime * 45.0f , 0.0f ), vec3_origin );

				m_pItemModelPanel->InvalidateLayout( true );
				m_pItemModelPanel->SetAlpha( 0 );
				vgui::GetAnimationController()->RunAnimationCommand( m_pItemModelPanel, "Alpha", 255, 0.01f, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
		}
		else
		{
			m_pItemModelPanel->m_bShouldPaint = false;
			m_pItemModelPanel->SetVisible( false );
		}
		
		m_pWeaponDetail0->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 0 );
		m_pWeaponDetail1->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 1 );
		m_pWeaponDetail2->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 2 );
		m_pWeaponDetail3->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 3 );
		m_pWeaponDetail4->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 4 );
		m_pWeaponDetail5->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 5 );
		m_pWeaponDetail6->SetWeaponDetails( pHighlighted->m_nEquipIndex, pHighlighted->m_nInventorySlot, pHighlighted->m_nProfileIndex, 6 );

		m_pAcceptButton->SetEnabled( pHighlighted->m_bCanEquip );
	}
}


void CNB_Select_Weapon_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		MarkForDeletion();
		Briefing()->SetChangingWeaponSlot( 0 );
		return;
	}
	else if ( !Q_stricmp( command, "AcceptButton" ) )
	{
		
		CNB_Select_Weapon_Entry *pHighlighted = dynamic_cast<CNB_Select_Weapon_Entry*>( m_pHorizList->GetHighlightedEntry() );
		if ( pHighlighted )
		{
			if ( !pHighlighted->m_bCanEquip )
				return;

			if ( GetMedalStore() )
			{
				GetMedalStore()->OnSelectedEquipment( m_nInventorySlot == ASW_INVENTORY_SLOT_EXTRA, pHighlighted->m_nEquipIndex );			}

			Briefing()->SelectWeapon( m_nProfileIndex, m_nInventorySlot, pHighlighted->m_nEquipIndex );			

			// TODO: Replace this with "vgui::surface()->PlaySound" (and figure out how to keep the abstraction)
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.SelectWeapon" );

			MarkForDeletion();
			Briefing()->SetChangingWeaponSlot( 0 );
		}
		return;
	}
	BaseClass::OnCommand( command );
}