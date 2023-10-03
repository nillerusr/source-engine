#include "cbase.h"
#include "asw_hud_squad_hotbar.h"
#include "hud_macros.h"
#include "squad_inventory_panel.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "c_asw_marine.h"
#include "asw_shareddefs.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "c_asw_weapon.h"
#include "asw_marine_profile.h"
#include "asw_weapon_parse.h"
#include "clientmode_asw.h"
#include "briefingtooltip.h"
#include "iclientmode.h"
#include "asw_input.h"
#include <vgui_controls/AnimationController.h>
#include "hud_locator_target.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;

ConVar asw_hotbar_simple( "asw_hotbar_simple", "1", FCVAR_NONE, "Show only 1 item per AI marine on the squad hotbar" );
ConVar asw_hotbar_self( "asw_hotbar_self", "1", FCVAR_NONE, "Show your own items on the hotbar" );

/*
DECLARE_HUDELEMENT( CASW_Hud_Squad_Hotbar );
DECLARE_HUD_MESSAGE( CASW_Hud_Squad_Hotbar, ASWOrderUseItemFX );
DECLARE_HUD_MESSAGE( CASW_Hud_Squad_Hotbar, ASWOrderStopItemFX );

#define	NUM_USE_ITEM_ORDER_CLASSES	19
const char *pszUseItemOrderClasses[NUM_USE_ITEM_ORDER_CLASSES] = {
	"cancel",
	"asw_weapon_bait",
	"asw_weapon_buff_grenade",
	"asw_weapon_freeze_grenades",
	"asw_weapon_grenades",
	"asw_weapon_laser_mines",
	"asw_weapon_mines",
	"asw_weapon_t75",
	"asw_weapon_tesla_trap",
	"asw_weapon_heal_grenade",
	"asw_weapon_sentry",
	"asw_weapon_sentry_cannon",
	"asw_weapon_sentry_flamer",
	"asw_weapon_sentry_freeze",
	"asw_weapon_hornet_barrage",
	"asw_weapon_electrified_armor",
	"asw_weapon_flares",
	"asw_weapon_welder",
	"asw_weapon_stim",
};
*/

CASW_Hud_Squad_Hotbar::CASW_Hud_Squad_Hotbar( const char *pElementName ) : CASW_HudElement( pElementName ), vgui::Panel( GetClientMode()->GetViewport(), "ASW_Hud_Squad_Hotbar" )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_REMOTE_TURRET );
	SetScheme( vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew" ) );
	m_iNumMarines = 0;
}

CASW_Hud_Squad_Hotbar::~CASW_Hud_Squad_Hotbar()
{
	if ( g_hBriefingTooltip.Get() )
	{
		g_hBriefingTooltip->MarkForDeletion();
		g_hBriefingTooltip = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Hud_Squad_Hotbar::Init( void )
{
	//HOOK_HUD_MESSAGE( CASW_Hud_Squad_Hotbar, ASWOrderUseItemFX );
	//HOOK_HUD_MESSAGE( CASW_Hud_Squad_Hotbar, ASWOrderStopItemFX );
}

bool CASW_Hud_Squad_Hotbar::ShouldDraw( void )
{
	if ( !ASWGameResource() )
		return false;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return false;

	if ( asw_hotbar_self.GetBool() )
	{
		return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw();
	}

	m_iNumMarines = 0;
	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetCommander() != pPlayer )
			continue;

		C_ASW_Marine *pMarine = pMR->GetMarineEntity();
		if ( !pMarine )
			continue;

		m_iNumMarines++;
	}

	return m_iNumMarines > 1 && asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw();
}

void CASW_Hud_Squad_Hotbar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( Color( 0, 0, 0, 100 ) );
}

void CASW_Hud_Squad_Hotbar::PerformLayout()
{
	BaseClass::PerformLayout();

	// position our child entries in a horizontal list and expand to fit them
	int border = YRES( 2 );
	int gap = YRES( 4 );
	int tw = border, th = 0;
	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		m_pEntries[i]->InvalidateLayout( true );
		m_pEntries[i]->SetPos( tw, border );
		int w, h;
		m_pEntries[i]->GetSize( w, h );
		th = MAX( h, th );
		tw += w + gap;
	}
	th = border + th + border;
	tw = tw - gap + border;
	SetSize( tw, th );
	SetPos( ScreenWidth() * 0.5f - tw * 0.5f, ScreenHeight() * 0.3f - th );	// position in lower center of screen
}

void CASW_Hud_Squad_Hotbar::OnThink()
{
	BaseClass::OnThink();

	UpdateList();

	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		if ( m_pEntries[ i ]->IsCursorOver() )
		{
			// make sure tooltip is created and parented correctly
			if ( !g_hBriefingTooltip.Get() )
			{
				g_hBriefingTooltip = new BriefingTooltip( GetParent(), "SquadInventoryTooltip" );
				g_hBriefingTooltip->SetScheme( vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew") );
			}
			else if ( g_hBriefingTooltip->GetParent() != GetParent() )
			{
				g_hBriefingTooltip->SetParent( GetParent() );
			}

			m_pEntries[ i ]->ShowTooltip();
			break;
		}
	}

	/*
	// loops through to see if we have effects that are flagged to die after a certain time
	for ( HotbarOrderEffectsList_t::IndexLocalType_t i = m_hHotbarOrderEffects.Head() ;
		m_hHotbarOrderEffects.IsValidIndex(i) ;
		i = m_hHotbarOrderEffects.Next(i) )
	{
		if ( m_hHotbarOrderEffects[i].flRemoveAtTime > 0 && m_hHotbarOrderEffects[i].flRemoveAtTime < gpGlobals->curtime )
		{
			int iMarine = m_hHotbarOrderEffects[i].iEffectID;	
			C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
			if ( !pMarine )
				return;

			StopItemFX( pMarine, -1 );
		}
	}
	*/
}

void CASW_Hud_Squad_Hotbar::UpdateList()
{
	if ( !ASWGameResource() )
		return;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	int iEntry = 0;
	bool bHasItem = false;
	if ( asw_hotbar_self.GetBool() )
	{
		if ( iEntry >= m_pEntries.Count() )
		{
			CASW_Hotbar_Entry *pPanel = new CASW_Hotbar_Entry( this, "SquadInventoryPanelEntry" );
			m_pEntries.AddToTail( pPanel );
			InvalidateLayout();
		}

		// add your offhand item to the hotbar first
		CASW_Marine *pPlayerMarine = pPlayer->GetMarine();
		if ( pPlayerMarine )
		{
			C_ASW_Weapon *pWeapon = pPlayerMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
			if ( pWeapon )
			{
				m_pEntries[ iEntry ]->m_iHotKeyIndex = -1;
				m_pEntries[ iEntry ]->SetVisible( true );
				m_pEntries[ iEntry ]->SetDetails( pPlayerMarine, ASW_INVENTORY_SLOT_EXTRA );
				bHasItem = true;
			}
		}

		if ( !bHasItem )	// blank it out if there's no item in that slot
		{
			m_pEntries[ iEntry ]->m_iHotKeyIndex = iEntry;
			m_pEntries[ iEntry ]->SetDetails( NULL, -1 );
			m_pEntries[ iEntry ]->SetVisible( false );
		}

		iEntry++;
	}

	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->GetCommander() != pPlayer )
			continue;

		C_ASW_Marine *pMarine = pMR->GetMarineEntity();
		if ( !pMarine )
			continue;

		if ( pMarine->IsInhabited() )
			continue;

		if ( iEntry >= m_pEntries.Count() )
		{
			CASW_Hotbar_Entry *pPanel = new CASW_Hotbar_Entry( this, "SquadInventoryPanelEntry" );
			m_pEntries.AddToTail( pPanel );
			InvalidateLayout();
		}

		bHasItem = false;
		for ( int k = 0; k < ASW_NUM_INVENTORY_SLOTS; k++ )
		{
			C_ASW_Weapon *pWeapon = pMarine->GetASWWeapon( k );
			if ( !pWeapon )
				continue;

			const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
			if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
				continue;

			m_pEntries[ iEntry ]->m_iHotKeyIndex = iEntry;
			m_pEntries[ iEntry ]->SetVisible( true );
			m_pEntries[ iEntry ]->SetDetails( pMarine, k );
			bHasItem = true;

			if ( asw_hotbar_simple.GetBool() )		// only 1 item per marine
				break;
		}

		if ( !bHasItem )	// blank it out if there's no item in that slot
		{
			m_pEntries[ iEntry ]->m_iHotKeyIndex = iEntry;
			m_pEntries[ iEntry ]->SetDetails( NULL, -1 );
			m_pEntries[ iEntry ]->SetVisible( false );
		}

		iEntry++;
	}

	for ( int i = iEntry; i < m_pEntries.Count(); i++ )
	{
		m_pEntries[ i ]->SetVisible( false );
	}
}

void CASW_Hud_Squad_Hotbar::ActivateHotbarItem( int i )
{
	if ( i < 0 || i >= m_pEntries.Count() )
		return;

	m_pEntries[ i ]->ActivateItem();
}

int CASW_Hud_Squad_Hotbar::GetHotBarSlot( const char *szWeaponName )
{
	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		CASW_Hotbar_Entry *pEntry = m_pEntries[ i ];
		if ( pEntry && pEntry->m_hMarine.Get() )
		{
			C_ASW_Weapon *pWeapon = pEntry->m_hMarine->GetASWWeapon( pEntry->m_iInventoryIndex );
			if ( pWeapon )
			{
				if ( FClassnameIs( pWeapon, szWeaponName ) )
				{
					return i;
				}
			}
		}
	}

	return -1;
}
#if 0
void CASW_Hud_Squad_Hotbar::StopItemFX( C_ASW_Marine *pMarine, float flDeleteTime, bool bFailed )
{
	if ( !pMarine )
		return;

	// loops through to see if we already have an order effect for this marine
	for ( HotbarOrderEffectsList_t::IndexLocalType_t i = m_hHotbarOrderEffects.Head() ;
		m_hHotbarOrderEffects.IsValidIndex(i) ;
		i = m_hHotbarOrderEffects.Next(i) )
	{
		if ( m_hHotbarOrderEffects[i].iEffectID == pMarine->entindex() && m_hHotbarOrderEffects[i].pEffect )
		{
			if ( bFailed )
			{
				m_hHotbarOrderEffects[i].flRemoveAtTime = MAX( flDeleteTime, 0.5f );
				m_hHotbarOrderEffects[i].pEffect->SetControlPoint( 2, Vector( 0, 1, 0 ) );
				break;
			}

			if ( flDeleteTime > 0 )
			{
				m_hHotbarOrderEffects[i].flRemoveAtTime = flDeleteTime;
				break;
			}

			pMarine->ParticleProp()->StopEmission( m_hHotbarOrderEffects[i].pEffect );
			m_hHotbarOrderEffects[i].pEffect = NULL;
			m_hHotbarOrderEffects.Remove(i);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for ASWOrderUseItemFX message
//-----------------------------------------------------------------------------
void CASW_Hud_Squad_Hotbar::MsgFunc_ASWOrderUseItemFX( bf_read &msg )
{
	int iMarine = msg.ReadShort();	
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	int iOrderType = msg.ReadShort();
	int iInventorySlot = msg.ReadShort();

	Vector vecPosition;
	vecPosition.x = msg.ReadFloat();
	vecPosition.y = msg.ReadFloat();
	vecPosition.z = msg.ReadFloat();

	// loops through to see if we already have an order effect for this marine
	StopItemFX( pMarine );

	const char *pszClassName = NULL;

	switch( iOrderType )
	{
	case ASW_USE_ORDER_WITH_ITEM:
		{
			// check we have an item in that slot
			CASW_Weapon* pWeapon = pMarine->GetASWWeapon( iInventorySlot );
			if ( !pWeapon || !pWeapon->GetWeaponInfo() || !pWeapon->GetWeaponInfo()->m_bOffhandActivate )
				return;

			pszClassName = pWeapon->GetClassname();
		}
		break;

	case ASW_USE_ORDER_HACK:
		{
			pszClassName = "asw_weapon_t75";	// for now, we're using the t75 icon for hacking
		}
		break;

	default:
		{
			Assert( false ); // unspecified order type
			return;
		}
		break;
	}

	//CNewParticleEffect *pEffect = pMarine->ParticleProp()->Create( "order_use_item", PATTACH_CUSTOMORIGIN, -1, vecPosition - pMarine->GetAbsOrigin() );
	CNewParticleEffect *pEffect = pMarine->ParticleProp()->Create( "order_use_item", PATTACH_ABSORIGIN );
	if ( pEffect )
	{
		pMarine->ParticleProp()->AddControlPoint( pEffect, 1, pMarine, PATTACH_CUSTOMORIGIN );
		pEffect->SetControlPoint( 1, vecPosition );//vecPosition - pMarine->GetAbsOrigin()
		for ( int i = 0; i < NUM_USE_ITEM_ORDER_CLASSES; i++ )
		{
			if ( pszUseItemOrderClasses[i] && !Q_strcmp (  pszUseItemOrderClasses[i]  ,  pszClassName )  )
			{
				pEffect->SetControlPoint( 2, Vector( i, 0, 0 ) );
				break;
			}
		}

		HotbarOrderEffectsList_t::IndexLocalType_t iIndex = m_hHotbarOrderEffects.AddToTail();
		m_hHotbarOrderEffects[iIndex].iEffectID = iMarine;
		m_hHotbarOrderEffects[iIndex].pEffect = pEffect;
	}
}

void CASW_Hud_Squad_Hotbar::MsgFunc_ASWOrderStopItemFX( bf_read &msg )
{
	int iMarine = msg.ReadShort();	
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	bool bShouldDelay = msg.ReadOneBit() ? true : false;
	bool bFailed = msg.ReadOneBit() ? true : false;

	// loops through to see if we already have an order effect for this marine
	if ( bFailed )
		StopItemFX( pMarine, gpGlobals->curtime + 2.0f, true );
	else if ( bShouldDelay )
		StopItemFX( pMarine, gpGlobals->curtime + 2.0f );
	else
		StopItemFX( pMarine );
}
#endif

/// =========================================

CASW_Hotbar_Entry::CASW_Hotbar_Entry( vgui::Panel *pParent, const char *pElementName ) : BaseClass( pParent, pElementName )
{
	m_pWeaponImage = new vgui::ImagePanel( this, "Image" );
	m_pWeaponImage->SetShouldScaleImage( true );
	m_pMarineNameLabel = new vgui::Label( this, "MarineNameLabel", "" );
	m_pKeybindLabel = new vgui::Label( this, "KeybindLabel", "" );
	m_pQuantityLabel = new vgui::Label( this, "pQuantityLabel", "" );
	m_bMouseOver = false;
	m_iHotKeyIndex = 0;
}

CASW_Hotbar_Entry::~CASW_Hotbar_Entry()
{

}

void CASW_Hotbar_Entry::SetDetails( C_ASW_Marine *pMarine, int iInventoryIndex )
{
	m_hMarine = pMarine;
	m_iInventoryIndex = iInventoryIndex;

	UpdateImage();
}

void CASW_Hotbar_Entry::ClearImage()
{
	m_pWeaponImage->SetVisible( false );
	m_pMarineNameLabel->SetVisible( false );
	m_pKeybindLabel->SetVisible( false );
	m_pQuantityLabel->SetVisible( false );
}

void CASW_Hotbar_Entry::UpdateImage()
{
	if ( !m_hMarine.Get() )
	{
		ClearImage();
		return;
	}

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
	{
		ClearImage();
		return;
	}


	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
	{
		if ( !asw_hotbar_self.GetBool() || m_iHotKeyIndex != -1 )		// allow your own third item to show even if it's not usable
		{
			ClearImage();
			return;
		}
	}

	m_pWeaponImage->SetVisible( true );
	m_pMarineNameLabel->SetVisible( true );
	m_pKeybindLabel->SetVisible( true );
	m_pQuantityLabel->SetVisible( true );

	m_pWeaponImage->SetImage( pInfo->szEquipIcon );

	CASW_Marine_Profile *pProfile = m_hMarine->GetMarineProfile();
	if ( pProfile )
	{
		m_pMarineNameLabel->SetText( pProfile->GetShortName() );
	}

	const char *pszKey = "";
	if ( m_iHotKeyIndex != -1 )
	{
		char szBinding[ 128 ];
		Q_snprintf( szBinding, sizeof( szBinding ), "asw_squad_hotbar %d", m_iHotKeyIndex );
		pszKey = ASW_FindKeyBoundTo( szBinding );
	}
	else
	{
		pszKey = ASW_FindKeyBoundTo( "+grenade1" );
	}
	char szKey[ 12 ];
	Q_snprintf( szKey, sizeof(szKey), "%s", pszKey );
	Q_strupr( szKey );
	m_pKeybindLabel->SetText( szKey );		// TODO: Eventually make this into instructor key style? or a version of that which fits the HUD

	char szQuantity[ 32 ];
	Q_snprintf( szQuantity, sizeof( szQuantity ), "x%d", pWeapon->Clip1() );
	m_pQuantityLabel->SetText( szQuantity );
}

void CASW_Hotbar_Entry::ShowTooltip()
{
	if ( !m_hMarine.Get() )
		return;

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
		return;

	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
		return;

	int x = GetWide() * 0.8f;
	int y = GetTall() * 0.02f;
	LocalToScreen( x, y );
	g_hBriefingTooltip->SetTooltip( this, pInfo->szPrintName, " ", x, y, true );
}

void CASW_Hotbar_Entry::ActivateItem()
{
	if ( !m_hMarine.Get() )
		return;

	if ( m_hMarine->IsInhabited() )
		return;

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
		return;

	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
		return;

	// make it flash
	new CASWSelectOverlayPulse( this );

	//Msg( "Activating item %s (%s) on marine %s\n", pInfo->szPrintName, pWeapon->GetDebugName(), m_hMarine->GetDebugName() );
	//pWeapon->OffhandActivate();

	char buffer[ 64 ];
	Q_snprintf(buffer, sizeof(buffer), "cl_ai_offhand %d %d", m_iInventoryIndex, m_hMarine->entindex() );
	engine->ClientCmd( buffer );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );

	/*  this was replaced with a particle indicator in the world
	// show an icon where they were ordered!
	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( Locator_AddTarget() );
	if ( pLocatorTarget )
	{
		pLocatorTarget->m_vecOrigin = ASWInput()->GetCrosshairTracePos();
		pLocatorTarget->SetCaptionText( "", "" );
		pLocatorTarget->SetOnscreenIconTextureName( pWeapon->GetWeaponInfo()->szClassName );
		pLocatorTarget->SetVisible( true );
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_NONE );
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_NO_OFFSCREEN );
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_FORCE_CAPTION );
	}
	*/
}

void CASW_Hotbar_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pMarineNameLabel->SetFgColor( Color( 168, 168, 192, 255 ) );	
	m_pMarineNameLabel->SetFont( pScheme->GetFont( "VerdanaVerySmall", IsProportional() ) );

	m_pKeybindLabel->SetFgColor( Color( 255, 255, 255, 255 ) );	
	m_pKeybindLabel->SetFont( pScheme->GetFont( "Default", IsProportional() ) );

	m_pQuantityLabel->SetFgColor( Color( 66, 142, 192, 255 ) );	
	m_pQuantityLabel->SetFont( pScheme->GetFont( "DefaultSmall", IsProportional() ) );

	m_pWeaponImage->SetDrawColor( Color( 66, 142, 192, 255 ) );

	SetBgColor( Color( 32, 32, 64, 128 ) );

	UpdateImage();
}

void CASW_Hotbar_Entry::PerformLayout()
{
	BaseClass::PerformLayout();

	int w = YRES( 38); // halfwide, presuming 'extra' sized items
	int h = YRES( 38 );

	SetSize( w, h );

	int border = YRES( 2 );
	m_pMarineNameLabel->SetContentAlignment( vgui::Label::a_northeast );
	m_pMarineNameLabel->SetBounds( border, border, w - border * 2, YRES( 12 ) );
	m_pKeybindLabel->SetContentAlignment( vgui::Label::a_northwest );
	m_pKeybindLabel->SetBounds( border, border, w - border * 2, YRES( 12 ) );
	m_pQuantityLabel->SetContentAlignment( vgui::Label::a_southeast );
	m_pQuantityLabel->SetBounds( border, border, w - border * 2, h - border * 2 );

	border = YRES( 3 );
	m_pWeaponImage->SetBounds( border, border, w - border * 2, h - border * 2 );
}

/*
void SquadHotbar( const CCommand &args )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if ( args.ArgC() < 2 )
		return;
	
	CASW_Hud_Squad_Hotbar *pHUD = GET_HUDELEMENT( CASW_Hud_Squad_Hotbar );
	if ( !pHUD )
		return;

	pHUD->ActivateHotbarItem( atoi( args[1] ) );
}

static ConCommand asw_squad_hotbar( "asw_squad_hotbar", SquadHotbar, "Activate a squad hotbar button", 0 );
*/

// ============================================================


//-----------------------------------------------------------------------------
//	CASWSelectOverlayPulse
// 
//	creates a highlight pulse over the panel of an offhand item that has just been used
//
//-----------------------------------------------------------------------------
CASWSelectOverlayPulse::CASWSelectOverlayPulse( vgui::Panel* pParent )
{
	SetPaintBackgroundEnabled( false );
	m_pParent = NULL;
	m_bStarted = false;
	m_fScale = 1.1;

	if ( pParent )
	{
		m_pParent = pParent;
		SetParent( pParent->GetParent() );
		Initialize();
	}
	else
	{
		MarkForDeletion();
		SetVisible( false );
	}
}

CASWSelectOverlayPulse::~CASWSelectOverlayPulse()
{
}

void CASWSelectOverlayPulse::Initialize()
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/swarm/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);	

	m_pOverlayLabel = new vgui::ImagePanel( this, "SelectOverlayImage" );

	m_fStartTime = gpGlobals->curtime;

	int x, y;
	m_pParent->GetPos( x, y );

	int w, h;
	m_pParent->GetSize( w, h );

	SetSize( w * m_fScale, h * m_fScale );
	SetPos( x - ( GetWide() - w ) / 2, y - ( GetTall() - h ) / 2 );
	SetZPos( 100 );

	if ( m_pOverlayLabel )
	{
		m_pOverlayLabel->SetImage( "inventory/item_select_glow" );
		m_pOverlayLabel->SetShouldScaleImage( true );
		m_pOverlayLabel->SetAlpha( 255 );
		m_pOverlayLabel->SetPos( 0, 0 );
		m_pOverlayLabel->SetSize( w * m_fScale, h * m_fScale );

		m_bStarted = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Delete panel when highlight has faded out
//-----------------------------------------------------------------------------
void CASWSelectOverlayPulse::OnThink()
{
	if ( m_pOverlayLabel && gpGlobals->curtime > m_fStartTime )
	{
		if ( m_bStarted )
		{
			m_bStarted = false;
			vgui::GetAnimationController()->RunAnimationCommand( m_pOverlayLabel, "alpha",	0,		0.21, 1.0f, vgui::AnimationController::INTERPOLATOR_DEACCEL );

		}
		else if ( m_pOverlayLabel->GetAlpha() <= 0 )
		{
			MarkForDeletion();
			SetVisible( false );
		}
	}
}