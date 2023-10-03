#include "cbase.h"
#include "weapon_unlock_panel.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "asw_model_panel.h"
#include "c_asw_player.h"
#include "asw_weapon_parse.h"
#include "asw_equipment_list.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


WeaponUnlockPanel::WeaponUnlockPanel(vgui::Panel *pParent, const char *szPanelName ) : BaseClass( pParent, szPanelName )
{
	m_pUnlockLabel = new vgui::Label( this, "UnlockLabel", "" );
	m_pWeaponLabel = new vgui::Label( this, "WeaponLabel", "" );
	m_pLevelRequirementLabel = new vgui::Label( this, "LevelRequirementLabel", "" );

	m_pItemModelPanel = new CASW_Model_Panel( this, "ItemModelPanel" );
	m_pItemModelPanel->SetMDL( "models/weapons/autogun/autogun.mdl" );
	m_pItemModelPanel->m_bShouldPaint = false;
	m_pItemModelPanel->SetMouseInputEnabled( false );

	m_pLockedBG = new vgui::Panel( this, "LockedBG" );

	m_pLockedIcon = new vgui::ImagePanel( this, "LockedIcon" );
	m_pLockedIcon->SetShouldScaleImage( true );
	m_pLockedIcon->SetVisible( true );
}

WeaponUnlockPanel::~WeaponUnlockPanel()
{

}

void WeaponUnlockPanel::PerformLayout()
{
	KeyValues *pKV = new KeyValues( "ItemModelPanel" );
	pKV->SetInt( "fov", 20 );
	pKV->SetInt( "start_framed", 0 );
	pKV->SetInt( "disable_manipulation", 1 );
	m_pItemModelPanel->ApplySettings( pKV );

	BaseClass::PerformLayout();
}

void WeaponUnlockPanel::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	LoadControlSettings( "resource/UI/WeaponUnlockPanel.res" );
}

void WeaponUnlockPanel::OnThink()
{
	BaseClass::OnThink();

	if ( m_pItemModelPanel )
	{
		Vector vecPos = Vector( -275.0, 0.0, 170.0 );
		QAngle angRot = QAngle( 32.0, 0.0, 0.0 );

		CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor( m_szWeaponClass );
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

void WeaponUnlockPanel::SetDetails( const char *szWeaponClass, int iPlayerLevel )
{
	Q_snprintf( m_szWeaponClass, sizeof( m_szWeaponClass ), "%s", szWeaponClass );
	m_iPlayerLevel = iPlayerLevel;
	UpdateWeaponDetails();
}

void WeaponUnlockPanel::UpdateWeaponDetails()
{
	m_pItemModelPanel->ClearMergeMDLs();

	CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor( m_szWeaponClass );
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pWeaponData || !pPlayer || !m_szWeaponClass[ 0 ] )
	{
		m_pItemModelPanel->m_bShouldPaint = false;
		m_pItemModelPanel->SetVisible( false );

		m_pUnlockLabel->SetVisible( false );
		m_pWeaponLabel->SetVisible( false );
		m_pLevelRequirementLabel->SetVisible( false );
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

	m_pUnlockLabel->SetVisible( true );
	m_pWeaponLabel->SetVisible( true );

	
	int iRequiredLevel = pPlayer->GetWeaponLevelRequirement( m_szWeaponClass );

	if ( m_iPlayerLevel >= iRequiredLevel )
	{
		m_pUnlockLabel->SetText( "#asw_weapon_unlocked" );
		m_pLockedIcon->SetVisible( false );
		m_pLockedBG->SetVisible( true );
		m_pLevelRequirementLabel->SetVisible( false );
	}
	else
	{
		m_pUnlockLabel->SetText( "#asw_next_weapon_unlock" );
		m_pLockedIcon->SetVisible( true );
		m_pLockedBG->SetVisible( true );
		m_pLevelRequirementLabel->SetVisible( true );
	}
	m_pWeaponLabel->SetText( pWeaponData->szEquipLongName );
	

	wchar_t wnumber[8];
	_snwprintf( wnumber, ARRAYSIZE( wnumber ), L"%d", iRequiredLevel + 1 ); /// levels start from 0 in code, but show from 1 in the UI

	wchar_t wbuffer[96];
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find( "#asw_level_requirement" ), 1,
		wnumber );
	m_pLevelRequirementLabel->SetText( wbuffer );
}