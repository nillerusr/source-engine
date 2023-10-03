#include "cbase.h"
#include "nb_weapon_unlocked.h"
#include "nb_button.h"
#include "asw_model_panel.h"
#include <vgui/ISurface.h>
#include "c_asw_player.h"
#include "asw_weapon_parse.h"
#include "asw_equipment_list.h"
#include "vgui_controls/AnimationController.h"
#include "nb_header_footer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Weapon_Unlocked::CNB_Weapon_Unlocked( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pBanner = new CNB_Gradient_Bar( this, "Banner" );
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pWeaponLabel = new vgui::Label( this, "WeaponLabel", "" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );

	m_pItemModelPanel = new CASW_Model_Panel( this, "ItemModelPanel" );
	m_pItemModelPanel->SetMDL( "models/weapons/autogun/autogun.mdl" );
	m_pItemModelPanel->m_bShouldPaint = false;
	m_pItemModelPanel->SetMouseInputEnabled( false );
}

CNB_Weapon_Unlocked::~CNB_Weapon_Unlocked()
{

}

void CNB_Weapon_Unlocked::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_weapon_unlocked.res" );

	SetAlpha( 0 );
	vgui::GetAnimationController()->RunAnimationCommand( this, "alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.SelectWeapon" );
}

void CNB_Weapon_Unlocked::PerformLayout()
{
	KeyValues *pKV = new KeyValues( "ItemModelPanel" );
	pKV->SetInt( "fov", 20 );
	pKV->SetInt( "start_framed", 0 );
	pKV->SetInt( "disable_manipulation", 1 );
	m_pItemModelPanel->ApplySettings( pKV );

	BaseClass::PerformLayout();
}

void CNB_Weapon_Unlocked::OnThink()
{
	BaseClass::OnThink();

	if ( m_pItemModelPanel )
	{
		Vector vecPos = Vector( -275.0, 0.0, 170.0 );
		QAngle angRot = QAngle( 32.0, 0.0, 0.0 );
		CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor( m_pszWeaponUnlockClass );
		if ( pWeaponData )
		{
			vecPos.z += pWeaponData->m_flModelPanelZOffset;
		}

		Vector vecBoundsMins, vecBoundsMax;
		m_pItemModelPanel->GetBoundingBox( vecBoundsMins, vecBoundsMax );
		int iMaxBounds = -vecBoundsMins.x + vecBoundsMax.x;
		iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.y + vecBoundsMax.y );
		iMaxBounds = MAX( iMaxBounds, -vecBoundsMins.z + vecBoundsMax.z );
		vecPos *= (float)iMaxBounds/64.0f;

		m_pItemModelPanel->SetCameraPositionAndAngles( vecPos, angRot );
		m_pItemModelPanel->SetModelAnglesAndPosition( QAngle( 0.0f, gpGlobals->curtime * 45.0f , 0.0f ), vec3_origin );
	}
}

void CNB_Weapon_Unlocked::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		MarkForDeletion();
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Weapon_Unlocked::PaintBackground()
{
	// fill background
	float flAlpha = 200.0f / 255.0f;
	vgui::surface()->DrawSetColor( Color( 16, 32, 46, 230 * flAlpha ) );
	vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
}

void CNB_Weapon_Unlocked::SetWeaponClass( const char *pszWeaponUnlockClass )
{
	m_pszWeaponUnlockClass = pszWeaponUnlockClass;
	UpdateWeaponDetails();
}

void CNB_Weapon_Unlocked::UpdateWeaponDetails()
{
	m_pItemModelPanel->ClearMergeMDLs();

	CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor( m_pszWeaponUnlockClass );
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pWeaponData || !pPlayer || !m_pszWeaponUnlockClass[ 0 ] )
	{
		m_pItemModelPanel->m_bShouldPaint = false;
		m_pItemModelPanel->SetVisible( false );

		m_pWeaponLabel->SetVisible( false );
		return;
	}

	if ( Q_stricmp( pWeaponData->szDisplayModel, "" ) )
	{
		m_pItemModelPanel->SetMDL( pWeaponData->szDisplayModel );
		if ( Q_stricmp( pWeaponData->szDisplayModel2, "" ) )
		{
			m_pItemModelPanel->SetMergeMDL( pWeaponData->szDisplayModel2 );
		}
	}
	else
	{
		m_pItemModelPanel->SetMDL( pWeaponData->szWorldModel );
	}

	int nSkin = 0;
	if ( pWeaponData->m_iDisplayModelSkin > 0 )
		nSkin = pWeaponData->m_iDisplayModelSkin;
	else
		nSkin = pWeaponData->m_iPlayerModelSkin;

	m_pItemModelPanel->SetSkin( nSkin );
	m_pItemModelPanel->SetModelAnim( m_pItemModelPanel->FindAnimByName( "idle" ) );
	m_pItemModelPanel->m_bShouldPaint = true;
	m_pItemModelPanel->SetVisible( true );

	m_pWeaponLabel->SetVisible( true );
	m_pWeaponLabel->SetText( pWeaponData->szEquipLongName );
}