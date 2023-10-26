#include "cbase.h"
#include "asw_hud_master.h"
#include "clientmode_asw.h"
#include "vgui/ISurface.h"
#include "bitmap/psheet.h"
#include "vgui/ILocalize.h"
#include "cdll_bounded_cvars.h"
#include "VGUIMatSurface/IMatSystemSurface.h"
#include "hud_macros.h"
#include "ammodef.h"
#include "asw_input.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_weapon.h"
#include "asw_weapon_parse.h"
#include "asw_equipment_list.h"
#include "asw_circularprogressbar.h"
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_hud_alpha;

using namespace vgui;

DECLARE_HUDELEMENT( CASW_Hud_Master );
DECLARE_HUD_MESSAGE( CASW_Hud_Master, ASWOrderUseItemFX );
DECLARE_HUD_MESSAGE( CASW_Hud_Master, ASWOrderStopItemFX );
DECLARE_HUD_MESSAGE( CASW_Hud_Master, ASWInvalidDesination );

/////////////////////////////////////////////
// DO NOT CHANGE THE ORDER OF THESE UNLESS YOU
// CHANGE THE ORDER IN THE PARTICLE SHEET AS WELL
// "materialsrc\vgui\swarm\EquipIcons\particles\EquipUseIcons_particle.mks"
// They need to match exactly!
/////////////////////////////////////////////
#define	NUM_USE_ITEM_ORDER_CLASSES	19
const char *pszUseItemOrderClasses[NUM_USE_ITEM_ORDER_CLASSES] = {
	"cancel",
	"asw_weapon_buff_grenade",
	"asw_weapon_freeze_grenades",
	"asw_weapon_grenades",
	"asw_weapon_laser_mines",
	"asw_weapon_mines",
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
	"asw_weapon_ammo_satchel",
	"hacking_icon",
};

CASW_Hud_Master::CASW_Hud_Master( const char *pElementName ) :
  CASW_HudElement( pElementName ), BaseClass( NULL, "ASW_Hud_Master" )
{
	SetParent( GetClientMode()->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SwarmSchemeNew.res", "SwarmSchemeNew" ) );

	ADD_HUD_SHEET( Sheet1, "vgui/hud/sheet1/sheet1" );
	ADD_HUD_SHEET( Sheet_Stencil, "vgui/hud/sheet_stencil/sheet_stencil" );

	for ( int i = 0; i < m_HudSheets.Count(); i++ )
	{
		*( m_HudSheets[i].m_pSheetID ) = -1;
	}

	m_pLocalMarine = NULL;
	m_hLocalMarine = NULL;
	m_pLocalMarineResource = NULL;
	m_pLocalMarineProfile = NULL;
	m_pLocalMarineActiveWeapon = NULL;
	m_flLastReloadProgress = 0;
	m_flLastNextAttack = 0;
	m_flLastFastReloadStart = 0;
	m_flLastFastReloadEnd = 0;

	m_flLastHealthChangeTime = 0.0f;
	m_flCurHealthPercent = 1.0f;
	m_flPrevHealthPercent = 1.0f;
	m_bFading = false;
}

CASW_Hud_Master::~CASW_Hud_Master()
{
}

void CASW_Hud_Master::Init()
{
	CASW_HudElement::Init();

	HOOK_HUD_MESSAGE( CASW_Hud_Master, ASWOrderUseItemFX );
	HOOK_HUD_MESSAGE( CASW_Hud_Master, ASWOrderStopItemFX );
	HOOK_HUD_MESSAGE( CASW_Hud_Master, ASWInvalidDesination );
}

void CASW_Hud_Master::VidInit()
{
	CASW_HudElement::VidInit();

	for ( int i = 0; i < m_HudSheets.Count(); i++ )
	{
		if ( *( m_HudSheets[i].m_pSheetID ) == -1 )
		{
			*( m_HudSheets[i].m_pSheetID ) = surface()->CreateNewTextureID();
			surface()->DrawSetTextureFile( *( m_HudSheets[i].m_pSheetID ), m_HudSheets[i].m_pszTextureFile, true, false );

			// pull out UV coords for this sheet
			ITexture *pTexture = materials->FindTexture( m_HudSheets[i].m_pszTextureFile, TEXTURE_GROUP_VGUI );
			if ( pTexture )
			{
				m_HudSheets[i].m_pSize->x = pTexture->GetActualWidth();
				m_HudSheets[i].m_pSize->y = pTexture->GetActualHeight();
				size_t numBytes;
				void const *pSheet =  pTexture->GetResourceData( VTF_RSRC_SHEET, &numBytes );
				if ( pSheet )
				{				
					CUtlBuffer bufLoad( pSheet, numBytes, CUtlBuffer::READ_ONLY );
					CSheet *pSheet = new CSheet( bufLoad );
					for ( int k = 0; k < pSheet->m_SheetInfo.Count(); k++ )
					{
						if ( k >= m_HudSheets[i].m_nNumSubTextures )
						{
							break;
						}
						SequenceSampleTextureCoords_t &Coords = pSheet->m_SheetInfo[ k ].m_pSamples->m_TextureCoordData[ 0 ];
						m_HudSheets[i].m_pTextureData[ k ].u = Coords.m_fLeft_U0;
						m_HudSheets[i].m_pTextureData[ k ].v = Coords.m_fTop_V0;
						m_HudSheets[i].m_pTextureData[ k ].s = Coords.m_fRight_U0;
						m_HudSheets[i].m_pTextureData[ k ].t = Coords.m_fBottom_V0;
					}
				}
				else
				{
					Warning( "Error finding VTF_RSRC_SHEET for %s\n", m_HudSheets[i].m_pszTextureFile );
				}
			}
			else
			{
				Warning( "Error finding %s\n", m_HudSheets[i].m_pszTextureFile );
			}
		}
	}

	m_LocalMarineVideo.SetLoopVideo( ASW_VIDEO_FACE_HEALTHY, 1, 0.3f );
	m_nLocalMarineVideoTextureID = surface()->CreateNewTextureID( true );
	g_pMatSystemSurface->DrawSetTextureMaterial( m_nLocalMarineVideoTextureID, m_LocalMarineVideo.GetMaterial() );
}

void CASW_Hud_Master::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );


}

void CASW_Hud_Master::UpdatePortraitVideo()
{
	if ( m_pLocalMarineResource )
	{
		// TODO: Play an infested video
// 		if ( m_pLocalMarineResource->IsInfested() )
// 		{
// 			m_LocalMarineVideo.SetLoopVideo( "test_infested" );
// 		}
// 		else
		{
			bool bHealthChanged = ( m_nLastLocalMarineHealth != m_nLocalMarineHealth );
			if ( bHealthChanged )
			{
				if ( m_nLocalMarineHealth < m_nLastLocalMarineHealth )		// marine has taken damage
				{
					// TODO: Do we need to add a timer so we don't restart this too often?
					m_LocalMarineVideo.PlayTempVideo( ASW_VIDEO_FACE_PAIN, ASW_VIDEO_FACE_STATIC );
				}

				float flHealthPercent = m_pLocalMarineResource->GetHealthPercent();
				if ( flHealthPercent < 0.5f )
				{
					m_LocalMarineVideo.SetLoopVideo( ASW_VIDEO_FACE_NEEDHEALTH );
				}
				else
				{
					m_LocalMarineVideo.SetLoopVideo( ASW_VIDEO_FACE_HEALTHY, 1, 0.3f );
				}
			}
		}
	}

	m_nLastLocalMarineHealth = m_nLocalMarineHealth;
}

void CASW_Hud_Master::OnThink()
{
	BaseClass::OnThink();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	if ( !pPlayer || !ASWGameResource() || !ASWEquipmentList() )
		return;
	
	// gather squad mate data
	int nMaxResources = ASWGameResource() ? ASWGameResource()->GetMaxMarineResources() : 0;
	int nPosition = 0;
	for ( int i = 0; i < nMaxResources && nPosition < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		
		if ( pMR && !( pMR->IsLocal() && pMR->IsInhabited() ) )
		{
			CASW_Marine_Profile *pProfile = pMR->GetProfile();
			C_ASW_Marine *pMarine = pMR->GetMarineEntity();

			m_SquadMateInfo[ nPosition ].hMarine = pMarine;
			m_SquadMateInfo[ nPosition ].bPositionActive = true;
			m_SquadMateInfo[ nPosition ].MarineClass = pProfile->GetMarineClass();
			m_SquadMateInfo[ nPosition ].nClassIcon = GetClassIcon( pMR );
			m_SquadMateInfo[ nPosition ].flHealthFraction = pMR->GetHealthPercent();
			m_SquadMateInfo[ nPosition ].flInfestation = pMR->GetInfestedPercent();
			m_SquadMateInfo[ nPosition ].bInhabited = pMR->IsInhabited();

			pMR->GetDisplayName( m_SquadMateInfo[ nPosition ].wszMarineName, sizeof( m_SquadMateInfo[ nPosition ].wszMarineName ) );

			if ( pMarine )			
			{
				m_SquadMateInfo[ nPosition ].pWeapon = pMarine->GetActiveASWWeapon();
				if ( m_SquadMateInfo[ nPosition ].pWeapon )
				{
					int nTotalBullets = pMarine->GetAmmoCount( m_SquadMateInfo[ nPosition ].pWeapon->GetPrimaryAmmoType() );
					int nMaxClip = m_SquadMateInfo[ nPosition ].pWeapon->GetMaxClip1();
					m_SquadMateInfo[ nPosition ].nClips = nTotalBullets / nMaxClip;		// TODO: Ammo bag?

					int nAmmoIndex = m_SquadMateInfo[ nPosition ].pWeapon->GetPrimaryAmmoType();
					int nGuns = pMarine->GetNumberOfWeaponsUsingAmmo( nAmmoIndex );
					int nMaxBullets = GetAmmoDef()->MaxCarry( nAmmoIndex, pMarine ) * nGuns;
					m_SquadMateInfo[ nPosition ].nMaxClips = nMaxBullets / nMaxClip;

					if ( m_SquadMateInfo[ nPosition ].pWeapon->DisplayClipsDoubled() )
					{
						m_SquadMateInfo[ nPosition ].nClips *= 2;
						m_SquadMateInfo[ nPosition ].nMaxClips *= 2;
					}
					m_SquadMateInfo[ nPosition ].flAmmoFraction = pMR->GetAmmoPercent();
					//Msg( "Setting squadmate %d ammo to %f.  clip1 is %d max is %d\n", nPosition, pMR->GetAmmoPercent(),
						//m_SquadMateInfo[ nPosition ].pWeapon->Clip1(), m_SquadMateInfo[ nPosition ].pWeapon->GetMaxClip1() );
				}
				else
				{
					m_SquadMateInfo[ nPosition ].nClips = 0;
					m_SquadMateInfo[ nPosition ].nMaxClips = 0;
					m_SquadMateInfo[ nPosition ].flAmmoFraction = 0;
					//Msg( "Setting squadmate %d ammo to zero 1\n", nPosition );
				}

				

				// Search primary and secondary slots for a usable item first
				C_ASW_Weapon *( pExtraItem[ 2 ] ) = { NULL, NULL };
				int nInventoryIndex[ 2 ];

				int nItemCount = 0;
				for ( int k = 0; k < ASW_NUM_INVENTORY_SLOTS && nItemCount < 2; k++ )
				{
					C_ASW_Weapon *pWeapon = pMarine->GetASWWeapon( k );
					if ( !pWeapon )
						continue;

					const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
					if ( !pInfo || !( pInfo->m_bOffhandActivate || ( pInfo->m_bHUDPlayerActivate && pMR->IsInhabited() ) ) )
						continue;

					pExtraItem[ nItemCount ] = pWeapon;
					nInventoryIndex[ nItemCount ] = k;
					nItemCount++;
				}

				bool bWalking = ( pPlayer->GetMarine() && pPlayer->GetMarine()->m_bWalking );
				if ( bWalking )
				{
					int poop = 1;
					poop = 2;
				}

				int nItemSelect = bWalking ? 1 : 0;
				int nItemShift = nItemSelect == 0 ? 1 : 0;

 				if ( pExtraItem[ nItemSelect ] )
				{
					m_SquadMateInfo[ nPosition ].pExtraItemInfo = pExtraItem[ nItemSelect ]->GetWeaponInfo();
					m_SquadMateInfo[ nPosition ].nExtraItemInventoryIndex = nInventoryIndex[ nItemSelect ];
					m_SquadMateInfo[ nPosition ].nEquipmentListIndex = pExtraItem[ nItemSelect ]->GetEquipmentListIndex();
					m_SquadMateInfo[ nPosition ].nExtraItemQuantity = pExtraItem[ nItemSelect ]->Clip1();
				}
				else
				{
					m_SquadMateInfo[ nPosition ].pExtraItemInfo = NULL;
				}

				if ( pExtraItem[ nItemShift ] )
				{
					m_SquadMateInfo[ nPosition ].pExtraItemInfoShift = pExtraItem[ nItemShift ]->GetWeaponInfo();
				}
				else
				{
					m_SquadMateInfo[ nPosition ].pExtraItemInfoShift = NULL;
				}
			}
			else
			{
				m_SquadMateInfo[ nPosition ].pExtraItemInfo = NULL;
				m_SquadMateInfo[ nPosition ].pExtraItemInfoShift = NULL;
				m_SquadMateInfo[ nPosition ].pWeapon = NULL;
				m_SquadMateInfo[ nPosition ].nClips = 0;
				m_SquadMateInfo[ nPosition ].flAmmoFraction = 0;
				//Msg( "Setting squadmate %d ammo to zero 2\n", nPosition );
			}

			nPosition++;
		}
	}

	for ( int i = nPosition; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		m_SquadMateInfo[ i ].bPositionActive = false;
	}

	//UpdatePortraitVideo();

	// loops through to see if we have effects that are flagged to die after a certain time
	HotbarOrderEffectsList_t::IndexLocalType_t f = m_hHotbarOrderEffects.Head();
	while ( m_hHotbarOrderEffects.IsValidIndex( f ) )
	{
		// advance before deleting the pointer out from under us
		const HotbarOrderEffectsList_t::IndexLocalType_t oldf = f;
		f = m_hHotbarOrderEffects.Next( f );

		if ( m_hHotbarOrderEffects[oldf].flRemoveAtTime > 0 && m_hHotbarOrderEffects[oldf].flRemoveAtTime < gpGlobals->curtime )
		{
			int iMarine = m_hHotbarOrderEffects[oldf].iEffectID;	
			C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
			if ( !pMarine )
				return;

			StopItemFX( pMarine, -1 );
		}
	}
}

void CASW_Hud_Master::Paint( void )
{
	VPROF_BUDGET( "CASW_Hud_Master::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;
 
	// gather data for local player
	m_pLocalMarine = C_ASW_Marine::GetLocalMarine();		// only valid inside OnThink and Paint
	m_hLocalMarine = m_pLocalMarine;		// take a handle copy for things like the CLocatorPanel which want to find this outside of our Paint
	m_pLocalMarineResource = m_pLocalMarine ? m_pLocalMarine->GetMarineResource() : ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	m_pLocalMarineProfile = m_pLocalMarineResource ? m_pLocalMarineResource->GetProfile() : NULL;
	m_pLocalMarineActiveWeapon = m_pLocalMarine ? m_pLocalMarine->GetActiveASWWeapon() : NULL;
	if ( m_pLocalMarineActiveWeapon )
	{
		// clips
		int nTotalBullets = m_pLocalMarine->GetAmmoCount( m_pLocalMarineActiveWeapon->GetPrimaryAmmoType() );
		int nMaxClip = m_pLocalMarineActiveWeapon->GetMaxClip1();
		m_nLocalMarineClips = nMaxClip == 0 ? 1 : nTotalBullets / nMaxClip;		// TODO: Ammo bag?

		int nAmmoIndex = m_pLocalMarineActiveWeapon->GetPrimaryAmmoType();
		int nGuns = m_pLocalMarine->GetNumberOfWeaponsUsingAmmo( nAmmoIndex );
		int nMaxBullets = GetAmmoDef()->MaxCarry( nAmmoIndex, m_pLocalMarine ) * nGuns;
		m_nLocalMarineMaxClips = nMaxClip == 0 ? 1 : nMaxBullets / nMaxClip;

		if ( m_pLocalMarineActiveWeapon->DisplayClipsDoubled() )
		{
			m_nLocalMarineClips *= 2;
		}
		m_nLocalMarineBullets = m_pLocalMarineActiveWeapon->Clip1();
		m_nLocalMarineGrenades = m_pLocalMarineActiveWeapon->Clip2();
		m_nLocalMarineGrenades = MIN( m_nLocalMarineGrenades, 9 );
	}
	else
	{
		m_nLocalMarineClips = 0;
		m_nLocalMarineMaxClips = 0;
	}
	if ( m_pLocalMarine )
	{
		m_nLocalMarineHealth = m_pLocalMarine->GetHealth();
	}
	else
	{
		m_nLocalMarineHealth = 0;
	}

	if ( m_pLocalMarineResource )
	{
		C_ASW_Marine_Resource *pMR = m_pLocalMarineResource;

		float flTimeToFade = 2.0f;
		float flHealthPercent = pMR->GetHealthPercent();
		m_flMainProgress = 0.0f;
		m_flDelayProgress = 0.0f;

		float flAlphaDelay = 225.0f;

		m_colorMain =	Color( 255, 255, 255, 255 );
		m_colorBG =		Color( 255, 255, 255, 255 );
		m_colorDelay =	Color( 255, 255, 255, 255 );

		// the Delay bar is fading or animating
		if ( (m_flLastHealthChangeTime + flTimeToFade) >= gpGlobals->curtime && m_bFading )
		{
			if ( m_flCurHealthPercent < m_flPrevHealthPercent )
			{	
				float flAlpha = 225.0f;
				flAlpha *= clamp( ((m_flLastHealthChangeTime+flTimeToFade) - gpGlobals->curtime)/flTimeToFade, 0.0f, 225.0f );
				flAlphaDelay = flAlpha;
				m_flMainProgress = flHealthPercent;
				m_flDelayProgress = m_flPrevHealthPercent;
			}
			else
			{
				float flCurProg = m_flPrevHealthPercent;
				flCurProg = Approach( m_flCurHealthPercent, flCurProg, (gpGlobals->curtime - m_flLastHealthChangeTime) * (m_flCurHealthPercent - m_flPrevHealthPercent) / 20.0f );
				m_flMainProgress = flCurProg;
				m_flPrevHealthPercent = flCurProg;
				m_flDelayProgress = m_flCurHealthPercent;

				flAlphaDelay *= clamp( ((m_flLastHealthChangeTime+flTimeToFade) - gpGlobals->curtime)/flTimeToFade, 0.0f, 225.0f );				

				if ( flCurProg >= m_flDelayProgress - 0.01f )
					m_bFading = false;
			}					
		}
		else
		{
			m_bFading = false;
			m_flPrevHealthPercent = m_flCurHealthPercent;
			flAlphaDelay = 0;
			m_flMainProgress = flHealthPercent;
		}

		if ( flHealthPercent < m_flCurHealthPercent )
		{	
			m_flPrevHealthPercent = m_flCurHealthPercent;
			m_flCurHealthPercent = flHealthPercent;
			m_flLastHealthChangeTime = gpGlobals->curtime;

			m_flDelayProgress = m_flPrevHealthPercent;
			m_flMainProgress = m_flCurHealthPercent;

			m_bFading = true;
		}
		else if ( flHealthPercent > m_flCurHealthPercent )
		{
			m_flPrevHealthPercent = m_flCurHealthPercent;
			m_flCurHealthPercent = flHealthPercent;
			m_flLastHealthChangeTime = gpGlobals->curtime;

			m_flDelayProgress = m_flCurHealthPercent;
			m_flMainProgress = m_flPrevHealthPercent;		

			m_bFading = true;
		}

// 		if ( m_bFading && m_flCurHealthPercent > m_flPrevHealthPercent )
// 			m_colorDelay = Color( 255, 240, 240, flAlphaDelay );
// 		else if( m_bFading && m_flCurHealthPercent < m_flPrevHealthPercent )
// 			m_colorDelay = Color( 255, 160, 32, flAlphaDelay );
		m_colorDelay = Color( 255, 255, 255, flAlphaDelay );

		if ( flHealthPercent < 0.6 )
		{
			// pulse BG?
			float flSinePeriod = 2.0f;
			float flValue = ( sin( 2.0f * M_PI * gpGlobals->curtime / flSinePeriod ) * 0.5f ) + 0.5f;
			m_colorBG = Color( 255, 155.0f + 100.0f * flValue, 155.0f + 100.0f * flValue, 255);
		}
		if ( flHealthPercent < 0.33 )
		{
			float flSinePeriod = 1.0f;
			float flValue = ( sin( 2.0f * M_PI * gpGlobals->curtime / flSinePeriod ) * 0.5f ) + 0.5f;
			m_colorBG = Color( 255, 155.0f + 100.0f * flValue, 155.0f + 100.0f * flValue, 255);
		}
	}

	// calculate some positions
	int nCurrentMarine = 0;
	if ( pPlayer && ASWGameResource() )
	{
		for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
		{
			C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( pMR && pMR->GetCommander() == pPlayer )
			{
				if ( pMR->IsInhabited() )
					break;
				nCurrentMarine++;
			}
		}
	}

	// shift along depending on which marine we are
	int cursor_x = YRES( 18 );
	int nSquadMate = 0;
	for ( int i = 0; i < 4; i++ )
	{
		if ( i == nCurrentMarine )
		{
			m_nMarinePortrait_x = cursor_x;
			cursor_x += m_nSquadMates_x;
		}
		else if ( nSquadMate < MAX_SQUADMATE_HUD_POSITIONS )
		{
			m_SquadMateInfo[ nSquadMate ].xpos = cursor_x;
			cursor_x += m_nSquadMates_spacing;
			nSquadMate++;
		}
	}
	m_nMarinePortrait_circle_x = m_nMarinePortrait_x + m_nMarinePortrait_circle_bg_x;
	m_nMarinePortrait_circle_x2 = m_nMarinePortrait_circle_x + m_nMarinePortrait_circle_bg_w;
	m_nMarinePortrait_circle_y = m_nMarinePortrait_y + m_nMarinePortrait_circle_bg_y;
	m_nMarinePortrait_circle_y2 = m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t;

	m_nMarinePortrait_bar_x = m_nMarinePortrait_circle_x2;
	m_nMarinePortrait_bar_x2 = m_nMarinePortrait_bar_x + m_nMarinePortrait_bar_bg_w;
	m_nMarinePortrait_bar_y = m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t * 0.5f;
	m_nMarinePortrait_bar_y2 = m_nMarinePortrait_circle_y2;

	if ( m_pLocalMarineProfile && m_pLocalMarineResource )
	{
		PaintLocalMarinePortrait();
		PaintLocalMarineInventory();
		PaintFastReload();
	}

	for ( int i = 0; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		if ( m_SquadMateInfo[ i ].bPositionActive )
		{
			PaintSquadMemberStatus( i );
		}
	}

	PaintSquadMatesInventory();

	// paint text
	PaintText();
	for ( int i = 0; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		if ( m_SquadMateInfo[ i ].bPositionActive )
		{
			PaintSquadMemberText( i );
		}
	}
}

bool CASW_Hud_Master::LookupElementBounds( const char *elementName, int &x, int &y, int &wide, int &tall )
{
	if ( !elementName || !m_hLocalMarine.Get() )
		return false;

	if ( V_strcmp( elementName, "weapon1" ) == 0 )
	{
		Assert( ASWEquipmentList() );

		CASW_Weapon *pWeapon = m_hLocalMarine->GetASWWeapon( ASW_INVENTORY_SLOT_SECONDARY );
		if ( pWeapon )
		{
			const CASW_WeaponInfo *pInfo = pWeapon->GetWeaponInfo();
			x = YRES( pInfo->m_iHUDIconOffsetX ) + m_nSecondaryWeapon_x;
			y = YRES( pInfo->m_iHUDIconOffsetY ) + m_nSecondaryWeapon_y;
			wide = m_nWeapon_w;
			tall = m_nWeapon_t;
		}
	}
	else if ( V_strcmp( elementName, "squadpanel0" ) == 0 )
	{
		Assert( ASWEquipmentList() );

		CASW_Weapon *pWeapon = m_hLocalMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
		if ( pWeapon )
		{
			x = m_nExtraItem_x + m_nMarinePortrait_x;
			y = m_nExtraItem_y + m_nMarinePortrait_y;
			wide = m_nWeapon_w;
			tall = m_nWeapon_t;
		}
	}
	else if ( StringHasPrefix( elementName, "squadpanel" ) )
	{
		int nPanel = atoi( elementName + 10 ) - 1;

		if ( nPanel < 0 && nPanel >= MAX_SQUADMATE_HUD_POSITIONS )
		{
			return false;
		}

		if ( m_SquadMateInfo[ nPanel ].hMarine.Get() == NULL )
		{
			return false;
		}

		CASW_Weapon *pWeapon = m_SquadMateInfo[ nPanel ].hMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
		if ( !pWeapon )
		{
			return false;
		}

		if ( pWeapon )
		{
			x = m_SquadMateInfo[ nPanel ].xpos;
			y = m_nSquadMates_y;
			x += m_nSquadMate_ExtraItem_x;
			y += m_nSquadMate_ExtraItem_y;
			wide = m_nSquadMate_ExtraItem_w;
			tall = m_nSquadMate_ExtraItem_t;
		}
	}
	else if ( V_strcmp( elementName, "altammo" ) == 0 )
	{
		x = m_nMarinePortrait_x + m_nMarinePortrait_grenades_icon_x;
		y = m_nMarinePortrait_y + m_nMarinePortrait_grenades_icon_y;
		wide = m_nMarinePortrait_grenades_icon_w;
		tall = m_nMarinePortrait_grenades_icon_t;
	}
	else
	{
		return false;
	}

	return true;
}

int CASW_Hud_Master::GetClassIcon( C_ASW_Marine_Resource *pMR )
{
	int nUV = UV_NCOClassIcon;
	if ( pMR->GetHealthPercent() <= 0.0f )
	{
		nUV = UV_DeadIcon;
	}
	else if ( pMR->IsInfested() )
	{
		nUV = UV_InfestedIcon;
	}
	else
	{
		switch ( pMR->GetProfile()->GetMarineClass() )
		{
		case MARINE_CLASS_SPECIAL_WEAPONS: nUV = UV_SpecialWeaponsClassIcon; break;
		case MARINE_CLASS_MEDIC: nUV = UV_MedicClassIcon; break;
		case MARINE_CLASS_TECH: nUV = UV_TechClassIcon; break;
		}
	}
	return nUV;
}

void CASW_Hud_Master::PaintLocalMarinePortrait()
{	
	surface()->DrawSetTexture( m_nSheet1ID );

	// backgrounds
	surface()->DrawSetColor( 255, 255, 255, asw_hud_alpha.GetInt() );
	
 	surface()->DrawTexturedSubRect(
 		m_nMarinePortrait_circle_x, m_nMarinePortrait_circle_y, m_nMarinePortrait_circle_x2, m_nMarinePortrait_circle_y2,
 		HUD_UV_COORDS( Sheet1, UV_marine_portrait_BG_circle )
 	);

	float flSheet1HalfPixel_x = 1.0f / m_vecSheet1Size.x;
	float flSheet1HalfPixel_y = 1.0f / m_vecSheet1Size.y;
		
	vgui::Vertex_t portrait_bar_points[4] = 
	{ 
		vgui::Vertex_t( Vector2D(m_nMarinePortrait_bar_x, m_nMarinePortrait_bar_y),		Vector2D(m_Sheet1[ UV_marine_portrait_square ].u + flSheet1HalfPixel_x, m_Sheet1[ UV_marine_portrait_square ].v + flSheet1HalfPixel_y) ), 
		vgui::Vertex_t( Vector2D(m_nMarinePortrait_bar_x2, m_nMarinePortrait_bar_y),	Vector2D(m_Sheet1[ UV_marine_portrait_square ].s - flSheet1HalfPixel_x, m_Sheet1[ UV_marine_portrait_square ].v + flSheet1HalfPixel_y) ), 
		vgui::Vertex_t( Vector2D(m_nMarinePortrait_bar_x2, m_nMarinePortrait_bar_y2),	Vector2D(m_Sheet1[ UV_marine_portrait_square ].s - flSheet1HalfPixel_x, m_Sheet1[ UV_marine_portrait_square ].t - flSheet1HalfPixel_y) ), 
		vgui::Vertex_t( Vector2D(m_nMarinePortrait_bar_x, m_nMarinePortrait_bar_y2),	Vector2D(m_Sheet1[ UV_marine_portrait_square ].u + flSheet1HalfPixel_x, m_Sheet1[ UV_marine_portrait_square ].t - flSheet1HalfPixel_y) )
	}; 	
	vgui::surface()->DrawTexturedPolygon( 4, portrait_bar_points );

	// need to material switch to draw the video here	
	//m_LocalMarineVideo.Update();
	//g_pMatSystemSurface->DrawSetTextureMaterial( m_nLocalMarineVideoTextureID, m_LocalMarineVideo.GetMaterial() );
	surface()->DrawSetTexture( m_pLocalMarineProfile->m_nPortraitTextureID );
	surface()->DrawSetColor( 255, 255, 255, 255 );
	const int NUM_CIRCLE_POINTS = 32;

	vgui::Vertex_t portrait_points[ NUM_CIRCLE_POINTS ];
	for ( int i = 0; i < NUM_CIRCLE_POINTS; i++ )
	{
		float flAngle = 2.0f * M_PI * i / NUM_CIRCLE_POINTS;
		portrait_points[i].m_Position.x = m_nMarinePortrait_x + m_nMarinePortrait_face_x + cos( flAngle ) * m_nMarinePortrait_face_radius;
		portrait_points[i].m_Position.y = m_nMarinePortrait_y + m_nMarinePortrait_face_y + sin( flAngle ) * m_nMarinePortrait_face_radius;
		portrait_points[i].m_TexCoord.x = 0.5f + cos( flAngle ) * 0.5f;
		portrait_points[i].m_TexCoord.y = 0.5f + sin( flAngle ) * 0.5f;
	}
	vgui::surface()->DrawTexturedPolygon( NUM_CIRCLE_POINTS, portrait_points );

	surface()->DrawSetTexture( m_nSheet1ID );

	// health bar background
	surface()->DrawSetColor( m_colorBG );
	surface()->DrawTexturedSubRect(
		m_nMarinePortrait_circle_x, m_nMarinePortrait_circle_y, m_nMarinePortrait_circle_x2, m_nMarinePortrait_circle_y2,
		HUD_UV_COORDS( Sheet1, UV_marine_health_circle_BG )
		);

	float flHealthFraction = m_pLocalMarineResource->GetHealthPercent();
 
	// draw recent damage
	surface()->DrawSetColor( m_colorDelay );
	ASWCircularProgressBar::DrawCircleSegmentAtPosition(
		m_nMarinePortrait_circle_x + m_nMarinePortrait_circle_bg_w * 0.5f,
		m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t * 0.5f,
		m_nMarinePortrait_circle_bg_w * 0.5f,
		m_nMarinePortrait_circle_bg_t * 0.5f,
		0.75f * ( m_flMainProgress ), 0.75f * ( m_flDelayProgress ), true, 180, -180,
		HUD_UV_COORDS( Sheet1, UV_marine_health_circle_FG_subtract ) );

	// draw infestation
	if ( m_pLocalMarineResource->IsInfested() )
	{
		float flInfestation = m_pLocalMarineResource->GetInfestedPercent();
		flInfestation = clamp<float>( flInfestation, 0.0f, flHealthFraction );

		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		ASWCircularProgressBar::DrawCircleSegmentAtPosition(
			m_nMarinePortrait_circle_x + m_nMarinePortrait_circle_bg_w * 0.5f,
			m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t * 0.5f,
			m_nMarinePortrait_circle_bg_w * 0.5f,
			m_nMarinePortrait_circle_bg_t * 0.5f,
			flInfestation  * 0.75f, ( flHealthFraction * 0.75f ), true, 180, -180,
			HUD_UV_COORDS( Sheet1, UV_marine_health_circle_FG_infested ) );

		// draw uninfested health bar
		float flUninfestedHealth = flHealthFraction - flInfestation;
		if ( flUninfestedHealth > 0.0f )
		{
			surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
			ASWCircularProgressBar::DrawCircleSegmentAtPosition(
				m_nMarinePortrait_circle_x + m_nMarinePortrait_circle_bg_w * 0.5f,
				m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t * 0.5f,
				m_nMarinePortrait_circle_bg_w * 0.5f,
				m_nMarinePortrait_circle_bg_t * 0.5f,
				0, flUninfestedHealth * 0.75f, true, 180, -180,
				HUD_UV_COORDS( Sheet1, UV_marine_health_circle_FG ) );
		}
	}
	else
	{
		// draw health bar
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		ASWCircularProgressBar::DrawCircleSegmentAtPosition(
			m_nMarinePortrait_circle_x + m_nMarinePortrait_circle_bg_w * 0.5f,
			m_nMarinePortrait_circle_y + m_nMarinePortrait_circle_bg_t * 0.5f,
			m_nMarinePortrait_circle_bg_w * 0.5f,
			m_nMarinePortrait_circle_bg_t * 0.5f,
			0, flHealthFraction * 0.75f, true, 180, -180,
			HUD_UV_COORDS( Sheet1, UV_marine_health_circle_FG ) );
	}

	

	// class icon/special status
	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawTexturedSubRect(
		m_nMarinePortrait_x + m_nMarinePortrait_class_icon_x,
		m_nMarinePortrait_y + m_nMarinePortrait_class_icon_y,
		m_nMarinePortrait_x + m_nMarinePortrait_class_icon_x + m_nMarinePortrait_class_icon_w,
		m_nMarinePortrait_y + m_nMarinePortrait_class_icon_y + m_nMarinePortrait_class_icon_t,
		HUD_UV_COORDS( Sheet1, GetClassIcon( m_pLocalMarineResource ) )
		);
	
	if ( m_pLocalMarineActiveWeapon )
	{
		const CASW_WeaponInfo *pInfo = m_pLocalMarineActiveWeapon->GetWeaponInfo();
		surface()->DrawSetTexture( m_nSheet_StencilID );

		if ( pInfo->m_iShowBulletsOnHUD )
		{
			surface()->DrawTexturedSubRect(
				m_nMarinePortrait_x + m_nMarinePortrait_bullets_icon_x,
				m_nMarinePortrait_y + m_nMarinePortrait_bullets_icon_y,
				m_nMarinePortrait_x + m_nMarinePortrait_bullets_icon_x + m_nMarinePortrait_bullets_icon_w,
				m_nMarinePortrait_y + m_nMarinePortrait_bullets_icon_y + m_nMarinePortrait_bullets_icon_t,
				HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_bullets )
				);
		}

		if ( pInfo->m_iShowGrenadesOnHUD )
		{
			surface()->DrawTexturedSubRect(
				m_nMarinePortrait_x + m_nMarinePortrait_grenades_icon_x,
				m_nMarinePortrait_y + m_nMarinePortrait_grenades_icon_y,
				m_nMarinePortrait_x + m_nMarinePortrait_grenades_icon_x + m_nMarinePortrait_grenades_icon_w,
				m_nMarinePortrait_y + m_nMarinePortrait_grenades_icon_y + m_nMarinePortrait_grenades_icon_t,
				HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_grenade )
				);
		}

		if ( pInfo->m_iShowClipsOnHUD )
		{
			if ( m_nLocalMarineClips > 5 )
			{
				// just draw one clip icon, the quantity will be drawn as text
				surface()->DrawTexturedSubRect(
					m_nMarinePortrait_x + m_nMarinePortrait_clips_x,
					m_nMarinePortrait_y + m_nMarinePortrait_clips_y,
					m_nMarinePortrait_x + m_nMarinePortrait_clips_x + m_nMarinePortrait_clips_w,
					m_nMarinePortrait_y + m_nMarinePortrait_clips_y + m_nMarinePortrait_clips_t,
					HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_full )
					);
			}
			else
			{
				// draw up to 5 clip icons, filling them in if the player has that many clips
				for ( int i = 0; i < 5; i++ )
				{
					int x = m_nMarinePortrait_x + m_nMarinePortrait_clips_x + m_nMarinePortrait_spacing * i;
					if ( m_nLocalMarineClips > i )
					{
						surface()->DrawSetColor( 255, 255, 255, 255 );
						surface()->DrawTexturedSubRect(
							x,
							m_nMarinePortrait_y + m_nMarinePortrait_clips_y,
							x + m_nMarinePortrait_clips_w,
							m_nMarinePortrait_y + m_nMarinePortrait_clips_y + m_nMarinePortrait_clips_t,
							HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_full )
							);
					}
					else if ( m_nLocalMarineMaxClips > i )
					{
						surface()->DrawSetColor( 128, 128, 128, 255 );
						surface()->DrawTexturedSubRect(
							x,
							m_nMarinePortrait_y + m_nMarinePortrait_clips_y,
							x + m_nMarinePortrait_clips_w,
							m_nMarinePortrait_y + m_nMarinePortrait_clips_y + m_nMarinePortrait_clips_t,
							HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_empty )
							);
					}
				}
			}
		}
	}
}

void CASW_Hud_Master::PaintSquadMemberStatus( int nPosition )
{
	int x = m_SquadMateInfo[ nPosition].xpos; //m_nSquadMates_x + nPosition * m_nSquadMates_spacing;
	int y = m_nSquadMates_y;

	// background
	surface()->DrawSetColor( 255, 255, 255, asw_hud_alpha.GetInt() );
	surface()->DrawSetTexture( m_nSheet1ID );
	surface()->DrawTexturedSubRect(
		x + m_nSquadMate_bg_x, y + m_nSquadMate_bg_y,
		x + m_nSquadMate_bg_x + m_nSquadMate_bg_w, y + m_nSquadMate_bg_y + m_nSquadMate_bg_t,
		HUD_UV_COORDS( Sheet1, UV_team_portrait_BG )
		);

	// class icon/special status
	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawTexturedSubRect(
		x + m_nSquadMate_class_icon_x,
		y + m_nSquadMate_class_icon_y,
		x + m_nSquadMate_class_icon_x + m_nSquadMate_class_icon_w,
		y + m_nSquadMate_class_icon_y + m_nSquadMate_class_icon_t,
		HUD_UV_COORDS( Sheet1, m_SquadMateInfo[ nPosition ].nClassIcon )
		);

	// TODO: show infested progress
	// TODO: Flash health bar when they get hit
	float health_x = x + m_nSquadMate_health_x;
	float health_y = y + m_nSquadMate_health_y;
	float health_x2 = x + m_nSquadMate_health_x + m_nSquadMate_health_w;
	float health_x2_progress = x + m_nSquadMate_health_x + m_nSquadMate_health_w * m_SquadMateInfo[ nPosition ].flHealthFraction;
	float health_y2 = y + m_nSquadMate_health_y + m_nSquadMate_health_t;

	vgui::Vertex_t health_bg_points[4] = 
	{ 
		vgui::Vertex_t( Vector2D(health_x, health_y),	Vector2D(m_Sheet1[ UV_team_health_bar_BG ].u, m_Sheet1[ UV_team_health_bar_BG ].v) ), 
		vgui::Vertex_t( Vector2D(health_x2, health_y),	Vector2D(m_Sheet1[ UV_team_health_bar_BG ].s, m_Sheet1[ UV_team_health_bar_BG ].v) ), 
		vgui::Vertex_t( Vector2D(health_x2, health_y2),	Vector2D(m_Sheet1[ UV_team_health_bar_BG ].s, m_Sheet1[ UV_team_health_bar_BG ].t) ), 
		vgui::Vertex_t( Vector2D(health_x, health_y2),	Vector2D(m_Sheet1[ UV_team_health_bar_BG ].u, m_Sheet1[ UV_team_health_bar_BG ].t) )
	}; 	
	vgui::surface()->DrawTexturedPolygon( 4, health_bg_points );

	float flHealth_UV_x2 = m_Sheet1[ UV_team_health_bar_FG ].u + (m_Sheet1[ UV_team_health_bar_FG ].s - m_Sheet1[ UV_team_health_bar_FG ].u ) * m_SquadMateInfo[ nPosition ].flHealthFraction;
	vgui::Vertex_t health_fg_points[4] = 
	{ 
		vgui::Vertex_t( Vector2D(health_x, health_y),			Vector2D(m_Sheet1[ UV_team_health_bar_FG ].u, m_Sheet1[ UV_team_health_bar_FG ].v) ), 
		vgui::Vertex_t( Vector2D(health_x2_progress, health_y),	Vector2D(flHealth_UV_x2, m_Sheet1[ UV_team_health_bar_FG ].v) ), 
		vgui::Vertex_t( Vector2D(health_x2_progress, health_y2),Vector2D(flHealth_UV_x2, m_Sheet1[ UV_team_health_bar_FG ].t) ), 
		vgui::Vertex_t( Vector2D(health_x, health_y2),			Vector2D(m_Sheet1[ UV_team_health_bar_FG ].u, m_Sheet1[ UV_team_health_bar_FG ].t) )
	}; 

	vgui::surface()->DrawTexturedPolygon( 4, health_fg_points );

	if ( m_SquadMateInfo[ nPosition ].flInfestation > 0.0f )
	{
		float flInfestStart = m_SquadMateInfo[ nPosition ].flHealthFraction - m_SquadMateInfo[ nPosition ].flInfestation;
		flInfestStart = clamp<float>( flInfestStart, 0.0f, m_SquadMateInfo[ nPosition ].flHealthFraction );

		float infest_start_x = x + m_nSquadMate_health_x + m_nSquadMate_health_w * flInfestStart;
		float infest_start_UV_x = m_Sheet1[ UV_team_health_bar_FG_infested ].u + (m_Sheet1[ UV_team_health_bar_FG_infested ].s - m_Sheet1[ UV_team_health_bar_FG_infested ].u ) * flInfestStart;
		float infest_end_UV_x = m_Sheet1[ UV_team_health_bar_FG_infested ].u + (m_Sheet1[ UV_team_health_bar_FG_infested ].s - m_Sheet1[ UV_team_health_bar_FG_infested ].u ) * m_SquadMateInfo[ nPosition ].flHealthFraction;
		vgui::Vertex_t infest_fg_points[4] = 
		{ 
			vgui::Vertex_t( Vector2D(infest_start_x, health_y),			Vector2D(infest_start_UV_x, m_Sheet1[ UV_team_health_bar_FG_infested ].v) ), 
			vgui::Vertex_t( Vector2D(health_x2_progress, health_y),	Vector2D(infest_end_UV_x, m_Sheet1[ UV_team_health_bar_FG_infested ].v) ), 
			vgui::Vertex_t( Vector2D(health_x2_progress, health_y2),Vector2D(infest_end_UV_x, m_Sheet1[ UV_team_health_bar_FG_infested ].t) ), 
			vgui::Vertex_t( Vector2D(infest_start_x, health_y2),			Vector2D(infest_start_UV_x, m_Sheet1[ UV_team_health_bar_FG_infested ].t) )
		}; 
		vgui::surface()->DrawTexturedPolygon( 4, infest_fg_points );
	}

	C_ASW_Weapon *pWeapon = m_SquadMateInfo[ nPosition ].pWeapon;
	if ( pWeapon )
	{
		surface()->DrawSetTexture( m_nSheet_StencilID );
		surface()->DrawSetColor( m_SquadMate_bullets_bg_color );

		float bullets_x = x + m_nSquadMate_bullets_x;
		float bullets_y = y + m_nSquadMate_bullets_y;
		float bullets_x2 = x + m_nSquadMate_bullets_x + m_nSquadMate_bullets_w;
		float bullets_x2_progress = x + m_nSquadMate_bullets_x + m_nSquadMate_bullets_w * m_SquadMateInfo[ nPosition ].flAmmoFraction;
		float bullets_y2 = y + m_nSquadMate_bullets_y + m_nSquadMate_bullets_t;

		vgui::Vertex_t bullets_bg_points[4] = 
		{ 
			vgui::Vertex_t( Vector2D(bullets_x, bullets_y),		Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].u, m_Sheet_Stencil[ UV_team_ammo_bar ].v) ), 
			vgui::Vertex_t( Vector2D(bullets_x2, bullets_y),	Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].s, m_Sheet_Stencil[ UV_team_ammo_bar ].v) ), 
			vgui::Vertex_t( Vector2D(bullets_x2, bullets_y2),	Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].s, m_Sheet_Stencil[ UV_team_ammo_bar ].t) ), 
			vgui::Vertex_t( Vector2D(bullets_x, bullets_y2),	Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].u, m_Sheet_Stencil[ UV_team_ammo_bar ].t) )
		}; 
		vgui::surface()->DrawTexturedPolygon( 4, bullets_bg_points );

		surface()->DrawSetColor( m_SquadMate_bullets_fg_color );

		// TODO: Flash this after they fire
		
		float flUV_x2 = m_Sheet_Stencil[ UV_team_ammo_bar ].u + (m_Sheet_Stencil[ UV_team_ammo_bar ].s - m_Sheet_Stencil[ UV_team_ammo_bar ].u ) * m_SquadMateInfo[ nPosition ].flAmmoFraction;
		vgui::Vertex_t bullets_points[4] = 
		{ 
			vgui::Vertex_t( Vector2D(bullets_x, bullets_y),		Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].u, m_Sheet_Stencil[ UV_team_ammo_bar ].v) ), 
			vgui::Vertex_t( Vector2D(bullets_x2_progress, bullets_y),	Vector2D(flUV_x2, m_Sheet_Stencil[ UV_team_ammo_bar ].v) ), 
			vgui::Vertex_t( Vector2D(bullets_x2_progress, bullets_y2),	Vector2D(flUV_x2, m_Sheet_Stencil[ UV_team_ammo_bar ].t) ), 
			vgui::Vertex_t( Vector2D(bullets_x, bullets_y2),	Vector2D(m_Sheet_Stencil[ UV_team_ammo_bar ].u, m_Sheet_Stencil[ UV_team_ammo_bar ].t) )
		}; 

		vgui::surface()->DrawTexturedPolygon( 4, bullets_points );

		if ( m_SquadMateInfo[ nPosition ].nClips > 5 )
		{
			// just draw one clip icon, the quantity will be drawn as text
			surface()->DrawSetColor( m_SquadMate_clips_full_color );
			surface()->DrawTexturedSubRect(
				x + m_nSquadMate_clips_x,
				y + m_nSquadMate_clips_y,
				x + m_nSquadMate_clips_x + m_nSquadMate_clips_w,
				y + m_nSquadMate_clips_y + m_nSquadMate_clips_t,
				HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_full )
				);
		}
		else
		{
			// draw up to 5 clip icons, filling them in if the player has that many clips
			for ( int i = 0; i < 5; i++ )
			{
				int clip_x = x + m_nSquadMate_clips_x + m_nSquadMate_clips_spacing * i;
				if ( m_SquadMateInfo[ nPosition ].nClips > i )
				{
					surface()->DrawSetColor( m_SquadMate_clips_full_color );
					surface()->DrawTexturedSubRect(
						clip_x,
						y + m_nSquadMate_clips_y,
						clip_x + m_nSquadMate_clips_w,
						y + m_nSquadMate_clips_y + m_nSquadMate_clips_t,
						HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_full )
					);
				}
				else if ( m_SquadMateInfo[ nPosition ].nMaxClips > i )
				{
					surface()->DrawSetColor( m_SquadMate_clips_empty_color );
					surface()->DrawTexturedSubRect(
						clip_x,
						y + m_nSquadMate_clips_y,
						clip_x + m_nSquadMate_clips_w,
						y + m_nSquadMate_clips_y + m_nSquadMate_clips_t,
						HUD_UV_COORDS( Sheet_Stencil, UV_hud_ammo_clip_empty )
						);
				}
			}
		}
	}
}

void CASW_Hud_Master::PaintLocalMarineInventory()
{
	Assert( ASWEquipmentList() );

	if ( !m_pLocalMarine )
		return;

	for ( int i = ASW_INVENTORY_SLOT_PRIMARY; i <= ASW_INVENTORY_SLOT_EXTRA; i++ )
	{
		CASW_Weapon *pWeapon = m_pLocalMarine->GetASWWeapon( i );
		if ( pWeapon )
		{
			const CASW_WeaponInfo *pInfo = pWeapon->GetWeaponInfo();
			if ( pWeapon == m_pLocalMarineActiveWeapon )
			{
				surface()->DrawSetColor( 255, 255, 255, 255 );
			}
			else
			{
				surface()->DrawSetColor( 66, 142, 192, 255 );		// light blue
			}
			surface()->DrawSetTexture( ASWEquipmentList()->GetEquipIconTexture( !pInfo->m_bExtra, pWeapon->GetEquipmentListIndex() ) );
			int x = YRES( pInfo->m_iHUDIconOffsetX );
			int y = YRES( pInfo->m_iHUDIconOffsetY );
			int w = m_nWeapon_w;
			int t = m_nWeapon_t;
			switch( i )
			{
				case ASW_INVENTORY_SLOT_PRIMARY:
				{
					x += m_nPrimaryWeapon_x;
					y += m_nPrimaryWeapon_y;
					break;
				}
				case ASW_INVENTORY_SLOT_SECONDARY:
				{
					x += m_nSecondaryWeapon_x;
					y += m_nSecondaryWeapon_y;
					break;
				}
				case ASW_INVENTORY_SLOT_EXTRA:
				{
					x = m_nExtraItem_x + m_nMarinePortrait_x;
					y = m_nExtraItem_y + m_nMarinePortrait_y;
					w = m_nExtraItem_w;
					t = m_nExtraItem_t;
					break;
				}
			}

			// darken icon if we can't use the item right now
			if ( pInfo->m_bShowBatteryCharge && pWeapon->GetBatteryCharge() < pWeapon->GetMinBatteryChargeToActivate() )
			{
				surface()->DrawSetColor( 33, 71, 96, 192 );
			}

			surface()->DrawTexturedRect( x, y, x + w, y + t );

			if ( pInfo->m_bShowBatteryCharge )
			{
				x += m_nExtraItem_battery_x;
				y += m_nExtraItem_battery_y;
				w = m_nExtraItem_battery_w;
				t = m_nExtraItem_battery_t;

				float flCharge = pWeapon->GetBatteryCharge();
				
				surface()->DrawOutlinedRect( x, y, x + w, y + t );
				// inset by 2 pixels
				x += 2;
				y += 2;
				w = ( w - 4 ) * flCharge;
				t -= 4;
				surface()->DrawFilledRect( x, y, x + w, y + t );
			}
			// TODO: Powered up
			// TODO: Weapon swaps
		}
	}
}

void CASW_Hud_Master::PaintSquadMatesInventory()
{	
	for ( int i = 0; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		if ( m_SquadMateInfo[ i ].bPositionActive && m_SquadMateInfo[ i ].pExtraItemInfo )
		{
			int x = m_SquadMateInfo[ i ].xpos; //m_nSquadMates_x + i * m_nSquadMates_spacing;
			int y = m_nSquadMates_y;

			surface()->DrawSetColor( 66, 142, 192, 255 );
			x += m_nSquadMate_ExtraItem_x;// + YRES( m_SquadMateInfo[ i ].pExtraItemInfo->m_iHUDIconOffsetX );
			y += m_nSquadMate_ExtraItem_y;// + YRES( m_SquadMateInfo[ i ].pExtraItemInfo->m_iHUDIconOffsetY );

			int w = m_nSquadMate_ExtraItem_w;
			int h = m_nSquadMate_ExtraItem_t;
			
			surface()->DrawSetTexture( ASWEquipmentList()->GetEquipIconTexture( !m_SquadMateInfo[ i ].pExtraItemInfo->m_bExtra, m_SquadMateInfo[ i ].nEquipmentListIndex ) );
			if ( m_SquadMateInfo[ i ].pExtraItemInfo->m_bZoomHotbarIcon )
			{
				vgui::Vertex_t zoomed_hotbar_icon_points[4] = 
				{ 
					vgui::Vertex_t( Vector2D(x,		y),		Vector2D( 0.3f, 0.1f ) ), 
					vgui::Vertex_t( Vector2D(x + w, y),		Vector2D( 0.7f, 0.1f ) ), 
					vgui::Vertex_t( Vector2D(x + w, y + h),	Vector2D( 0.7f, 0.9f ) ), 
					vgui::Vertex_t( Vector2D(x,		y + h),		Vector2D( 0.3f, 0.9f ) )
				}; 	
				vgui::surface()->DrawTexturedPolygon( 4, zoomed_hotbar_icon_points );
			}
			else
			{
				if ( !m_SquadMateInfo[ i ].pExtraItemInfo->m_bExtra )
				{
					x -= w * 0.5f;
					w *= 2;

					w *= 0.7f;
					h *= 0.7f;

					x += w * 0.2f;
					y += w * 0.1f;
				}

				surface()->DrawTexturedRect( x, y, x + w, y + h );
			}
			// TODO: Powered up
			// TODO: Weapon swaps
		}
	}
}

void CASW_Hud_Master::PaintFastReload()
{
	if ( !m_pLocalMarineActiveWeapon || !m_pLocalMarineActiveWeapon->IsReloading() )
		return;

	// calc fast reload amounts
	C_ASW_Weapon *pWeapon = m_pLocalMarineActiveWeapon;
		
	float flBGAlpha = 255.0f;
	Color colorBG =		Color( 32, 32, 32, flBGAlpha );
	Color colorWindow =	Color( 170, 170, 170, 255 );
	Color colorBar =	Color( 255, 255, 255, 255 );
	float flAlphaFade = 1.0f;
	bool bFailure = pWeapon->m_bFastReloadFailure;
	bool bSuccess = pWeapon->m_bFastReloadSuccess;
	float fStart = pWeapon->m_fReloadStart;
	if ( !bFailure && !bSuccess )
	{
		m_flLastNextAttack = pWeapon->m_flNextPrimaryAttack;
	}
	float fTotalTime = m_flLastNextAttack - fStart;
	if (fTotalTime <= 0)
		fTotalTime = 0.1f;

	float flProgress = 0.0f;
	// if we're in single player, the progress code in the weapon doesn't run on the client because we aren't predicting
	if ( !cl_predict->GetInt() )
		flProgress = (gpGlobals->curtime - fStart) / fTotalTime;
	else
		flProgress = pWeapon->m_fReloadProgress;
	flProgress = MIN( flProgress, 1.0f );

	if ( !bFailure && !bSuccess )
	{
		m_flLastReloadProgress = flProgress;
		m_flLastFastReloadStart = ((pWeapon->m_fFastReloadStart - fStart) / fTotalTime)+0.015f;
		m_flLastFastReloadEnd = ((pWeapon->m_fFastReloadEnd - fStart) / fTotalTime)-0.015f;
	}
	//Msg( "C: %f - %f - %f Reload Progress = %f\n", gpGlobals->curtime, fFastStart, fFastEnd, flProgress );

	if ( bFailure )
	{
		flBGAlpha = 255.0f;
		if ( flProgress > 0.75f )
			flAlphaFade = (1.0f - flProgress) / 0.25f;

		colorBG = Color( 128, 32, 16, flBGAlpha * flAlphaFade );
		colorWindow = Color( 200, 128, 128, 250 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 255 * flAlphaFade  );
	}
	else if ( bSuccess )
	{
		flAlphaFade = 1.0f - flProgress;
		colorBG = Color( 170, 170, 170, flBGAlpha * flAlphaFade );
		colorWindow = Color( 190, 220, 190, 255 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 255 * flAlphaFade  );
	}

	int alpha = flBGAlpha * flAlphaFade;

	// for now, use the white texture

	// draw outline	
	int x = m_nFastReload_x;
	int y = m_nFastReload_y;
	int x2 = m_nFastReload_x + m_nFastReload_w;
	int y2 = m_nFastReload_y + m_nFastReload_t;
	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawOutlinedRect( x - 1, y - 1, x2 + 1, y2 + 1 );

	// draw background
	surface()->DrawSetColor( colorBG );
	surface()->DrawFilledRect( x, y, x2, y2 );

	// draw fast section
	surface()->DrawSetColor( colorWindow );
	surface()->DrawFilledRect( x + m_nFastReload_w * m_flLastFastReloadStart,
								y + 2,
								x + m_nFastReload_w * m_flLastFastReloadEnd,
								y2 - 2 );
	
	// draw progress
	float flDrawProgress = MAX( m_flLastReloadProgress - 0.01f, 0 );
	surface()->DrawSetColor( colorBar );
	surface()->DrawFilledRect( x + m_nFastReload_w * flDrawProgress,
		y + 1,
		x + m_nFastReload_w * flDrawProgress + m_nFastReload_w * 0.01f,
		y2 - 1 );
}

void CASW_Hud_Master::PaintText()
{
	// local player portrait text:

	if ( m_pLocalMarineActiveWeapon && m_pLocalMarineResource )
	{
		const CASW_WeaponInfo *pInfo = m_pLocalMarineActiveWeapon->GetWeaponInfo();

		// weapon name
		surface()->DrawSetTextFont( m_hDefaultSmallFont );
		surface()->DrawSetTextColor( 255, 255, 255, 255 );
		surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_weapon_name_x,
										 m_nMarinePortrait_y + m_nMarinePortrait_weapon_name_y );
		surface()->DrawUnicodeString( g_pVGuiLocalize->FindSafe( pInfo->szPrintName ) );

		// > 5 clips
		static wchar_t wszBullets[ 6 ];
		if ( pInfo->m_iShowClipsOnHUD && m_nLocalMarineClips > 5 )
		{
			_snwprintf( wszBullets, sizeof( wszBullets ), L"x%d", m_nLocalMarineClips );
			surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_clips_x + m_nMarinePortrait_clips_w,
				m_nMarinePortrait_y + m_nMarinePortrait_clips_y );
			surface()->DrawUnicodeString( wszBullets );
		}

		// low ammo
		if ( ( int( gpGlobals->curtime*4 ) % 2 ) == 0 )
		{
			bool bReloading = ( m_pLocalMarineActiveWeapon && m_pLocalMarineActiveWeapon->IsReloading() );
			if ( !bReloading )
			{
				if ( m_pLocalMarineResource->GetAmmoPercent() <= 0 )
				{
					surface()->DrawSetTextColor( 200, 25, 25, 255 );
					surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_low_ammo_x,
						m_nMarinePortrait_y + m_nMarinePortrait_low_ammo_y );
					surface()->DrawUnicodeString( g_pVGuiLocalize->FindSafe( "#asw_no_ammo" ) );
				}
				else if ( m_pLocalMarineResource->IsLowAmmo() )
				{
					surface()->DrawSetTextColor( 200, 25, 25, 255 );
					surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_low_ammo_x,
						m_nMarinePortrait_y + m_nMarinePortrait_low_ammo_y );
					surface()->DrawUnicodeString( g_pVGuiLocalize->FindSafe( "#asw_low_ammo" ) );
				}
			}
		}

		// Bullet count also blinks the low ammo color if set above
		surface()->DrawSetTextFont( m_hDefaultLargeFont );

		if ( pInfo->m_iShowBulletsOnHUD )
		{
			_snwprintf( wszBullets, sizeof( wszBullets ), L"%03d", m_nLocalMarineBullets );
			surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_bullets_x,
				m_nMarinePortrait_y + m_nMarinePortrait_bullets_y );
			surface()->DrawUnicodeString( wszBullets );
		}

		// Back to white
		surface()->DrawSetTextColor( 255, 255, 255, 255 );

		if ( pInfo->m_iShowGrenadesOnHUD )
		{
			_snwprintf( wszBullets, sizeof( wszBullets ), L"%d", m_nLocalMarineGrenades );
			surface()->DrawSetTextPos( m_nMarinePortrait_x + m_nMarinePortrait_grenades_x,
				m_nMarinePortrait_y + m_nMarinePortrait_grenades_y );
			surface()->DrawUnicodeString( wszBullets );
		}
		
	}

	CASW_Weapon *pExtraItem = m_pLocalMarine ? m_pLocalMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA ) : NULL;
	if ( pExtraItem )
	{
		int w, t;
		// quantity for extra item
		if ( pExtraItem->GetWeaponInfo() )
		{
			if ( pExtraItem->GetWeaponInfo()->m_bShowCharges )
			{
				wchar_t wszQuantity[ 12 ];
				_snwprintf( wszQuantity, sizeof( wszQuantity ), L"x%d", pExtraItem->Clip1() );

				surface()->DrawSetTextColor( m_SquadMate_ExtraItem_quantity_color );		
				surface()->DrawSetTextFont( m_hDefaultSmallFont );	
				surface()->GetTextSize( m_hDefaultSmallFont, wszQuantity, w, t );
				surface()->DrawSetTextPos( m_nExtraItem_quantity_x + m_nMarinePortrait_x - w,
					m_nExtraItem_quantity_y + m_nMarinePortrait_y - t );
				surface()->DrawUnicodeString( wszQuantity );
			}		

			if ( pExtraItem->GetWeaponInfo()->m_bShowLocalPlayerHotkey )
			{
				// show hotkey for this marine's extra item
				const char *pszKey = ASW_FindKeyBoundTo( "+grenade1" );
				
				if ( pszKey )
				{
					wchar_t wszKey[ 12 ];

					wchar_t *pwchKeyName = g_pVGuiLocalize->Find( pszKey );
					if ( !pwchKeyName || !pwchKeyName[ 0 ] )
					{
						char szKey[ 12 ];
						Q_snprintf( szKey, sizeof(szKey), "%s", pszKey );
						Q_strupr( szKey );

						g_pVGuiLocalize->ConvertANSIToUnicode( pszKey, wszKey, sizeof( wszKey )  );
						pwchKeyName = wszKey;
					}

					surface()->DrawSetTextColor( m_SquadMate_ExtraItem_hotkey_color );
					surface()->DrawSetTextFont( m_hDefaultFont );	
					surface()->GetTextSize( m_hDefaultFont, pwchKeyName, w, t );
					surface()->DrawSetTextPos( m_nExtraItem_hotkey_x + m_nMarinePortrait_x - w,
						m_nExtraItem_hotkey_y + m_nMarinePortrait_y );
					surface()->DrawUnicodeString( pwchKeyName );
				}
			}
		}
	}

	// Charges for primary/secondary items
	for ( int i = ASW_INVENTORY_SLOT_PRIMARY; i < ASW_INVENTORY_SLOT_EXTRA; i++ )
	{
		if ( !m_pLocalMarine )
			continue;

		CASW_Weapon *pWeapon = m_pLocalMarine->GetASWWeapon( i );
		if ( pWeapon )
		{
			const CASW_WeaponInfo *pInfo = pWeapon->GetWeaponInfo();
			if ( !pInfo->m_bShowCharges )
				continue;

			int x = m_nWeapon_w - YRES( 20 );
			int y = m_nWeapon_t / 4;

			switch( i )
			{
				case ASW_INVENTORY_SLOT_PRIMARY:
				{
					x += m_nPrimaryWeapon_x;
					y += m_nPrimaryWeapon_y;
					break;
				}
				case ASW_INVENTORY_SLOT_SECONDARY:
				{
					x += m_nSecondaryWeapon_x;
					y += m_nSecondaryWeapon_y;
					break;
				}
			}

			int w, t;

			wchar_t wszQuantity[ 12 ];
			_snwprintf( wszQuantity, sizeof( wszQuantity ), L"x%d", pWeapon->Clip1() );
	
			surface()->DrawSetTextColor( ( pWeapon == m_pLocalMarineActiveWeapon ) ? Color( 255, 255, 255, 255 ) : Color( 66, 142, 192, 255 ) );
			surface()->DrawSetTextFont( m_hDefaultSmallFont );	
			surface()->GetTextSize( m_hDefaultSmallFont, wszQuantity, w, t );
			surface()->DrawSetTextPos( x, y );
			surface()->DrawUnicodeString( wszQuantity );
		}
	}
}

void CASW_Hud_Master::PaintSquadMemberText( int nPosition )
{
	int x = m_SquadMateInfo[ nPosition ].xpos; //m_nSquadMates_x + nPosition * m_nSquadMates_spacing;
	int y = m_nSquadMates_y;

	surface()->DrawSetTextFont( m_hDefaultSmallFont );	
	if ( m_SquadMateInfo[ nPosition ].flHealthFraction <= 0 )
	{
		surface()->DrawSetTextColor( m_SquadMate_name_dead_color );
	}
	else
	{
		surface()->DrawSetTextColor( m_SquadMate_name_color );
	}
	
	surface()->DrawSetTextPos( x + m_nSquadMate_name_x,
		y + m_nSquadMate_name_y );
	surface()->DrawUnicodeString( m_SquadMateInfo[ nPosition ].wszMarineName );

	if ( m_SquadMateInfo[ nPosition ].flHealthFraction <= 0 )
		return;

	if ( m_SquadMateInfo[ nPosition ].nClips > 5 )
	{
		static wchar_t wszBullets[ 6 ];
		_snwprintf( wszBullets, sizeof( wszBullets ), L"x%d", m_SquadMateInfo[ nPosition ].nClips );
		surface()->DrawSetTextPos( x + m_nSquadMate_clips_x + m_nSquadMate_clips_w,
			y + m_nSquadMate_clips_y );
		surface()->DrawUnicodeString( wszBullets );
	}

	if ( m_SquadMateInfo[ nPosition ].pExtraItemInfo )
	{
		int w, t;
		// quantity for extra item
		if ( m_SquadMateInfo[ nPosition ].pExtraItemInfo->m_bShowCharges )
		{
			surface()->DrawSetTextColor( m_SquadMate_ExtraItem_quantity_color );
			wchar_t wszQuantity[ 12 ];
			_snwprintf( wszQuantity, sizeof( wszQuantity ), L"x%d", m_SquadMateInfo[ nPosition ].nExtraItemQuantity );
			surface()->GetTextSize( m_hDefaultSmallFont, wszQuantity, w, t );
			surface()->DrawSetTextPos( x + m_nSquadMate_ExtraItem_quantity_x - w,
				y + m_nSquadMate_ExtraItem_quantity_y - t );
			surface()->DrawUnicodeString( wszQuantity );
		}

		if ( ASWGameResource()->IsOfflineGame() || m_SquadMateInfo[ nPosition ].pExtraItemInfo->m_bShowMultiplayerHotkey )
		{
			// show hotkey for this marine's extra item
			char szBinding[ 128 ];
			Q_snprintf( szBinding, sizeof( szBinding ), "asw_squad_hotbar %d", nPosition + 1 );
			const char *pszKey = ASW_FindKeyBoundTo( szBinding );
			
 			char szKey[ 12 ];
			Q_snprintf( szKey, sizeof(szKey), "%s", pszKey );
			Q_strupr( szKey );

			wchar_t wszKey[ 12 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( pszKey, wszKey, sizeof( wszKey )  );

			surface()->DrawSetTextColor( m_SquadMate_ExtraItem_hotkey_color );	
			surface()->GetTextSize( m_hDefaultFont, wszKey, w, t );
			surface()->DrawSetTextPos( x + m_nSquadMate_ExtraItem_hotkey_x - w,
				y + m_nSquadMate_ExtraItem_hotkey_y );
			surface()->DrawUnicodeString( wszKey );
		}
	}
}

void CASW_Hud_Master::ActivateHotbarItem( int nSlot )
{
	if ( nSlot < 0 || nSlot >= MAX_SQUADMATE_HUD_POSITIONS )
		return;
	
	if ( !m_SquadMateInfo[ nSlot ].bPositionActive || !m_SquadMateInfo[ nSlot ].hMarine.Get() )
		return;

	if ( !m_SquadMateInfo[ nSlot ].pExtraItemInfo || !(m_SquadMateInfo[ nSlot ].pExtraItemInfo->m_bOffhandActivate || ( m_SquadMateInfo[ nSlot ].bInhabited && m_SquadMateInfo[ nSlot ].pExtraItemInfo->m_bHUDPlayerActivate ) ) )
		return;

	if ( m_SquadMateInfo[ nSlot ].hMarine->GetCommander() != C_ASW_Player::GetLocalASWPlayer() )
	{
		// We don't own this guy, so let's do an appropriate emote!
		if ( m_SquadMateInfo[ nSlot ].pExtraItemInfo->m_iSquadEmote != -1 )
		{
			char buffer[ 64 ];
			Q_snprintf(buffer, sizeof(buffer), "cl_emote %d", m_SquadMateInfo[ nSlot ].pExtraItemInfo->m_iSquadEmote );
			engine->ClientCmd( buffer );
		}
	}
	else
	{
		char buffer[ 64 ];
		Q_snprintf(buffer, sizeof(buffer), "cl_ai_offhand %d %d", m_SquadMateInfo[ nSlot ].nExtraItemInventoryIndex, m_SquadMateInfo[ nSlot ].hMarine->entindex() );
		engine->ClientCmd( buffer );

		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
	}
}

int CASW_Hud_Master::GetHotBarSlot( const char *szWeaponName )
{
	for ( int i = 0; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		if ( m_SquadMateInfo[ i ].bPositionActive && m_SquadMateInfo[ i ].pExtraItemInfo )
		{
			if ( V_strcmp( m_SquadMateInfo[ i ].pExtraItemInfo->szClassName, szWeaponName ) == 0 )
			{
				return i;
			}
		}
	}

	for ( int i = 0; i < MAX_SQUADMATE_HUD_POSITIONS; i++ )
	{
		if ( m_SquadMateInfo[ i ].bPositionActive && m_SquadMateInfo[ i ].pExtraItemInfoShift )
		{
			if ( V_strcmp( m_SquadMateInfo[ i ].pExtraItemInfoShift->szClassName, szWeaponName ) == 0 )
			{
				// Special value for SHIFT!
				return 100 + i;
			}
		}
	}

	return -1;
}

bool CASW_Hud_Master::OwnsHotBarSlot( C_ASW_Player *pPlayer, int nSlot )
{
	if ( nSlot >= 100 )
	{
		nSlot -= 100;
	}

	C_ASW_Marine *pMarine = m_SquadMateInfo[ nSlot ].hMarine;
	if ( !pMarine )
		return false;

	return ( pMarine->GetCommander() == pPlayer );
}


void SquadHotbar( const CCommand &args )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if ( args.ArgC() < 2 )
		return;

	CASW_Hud_Master *pHUD = GET_HUDELEMENT( CASW_Hud_Master );
	if ( !pHUD )
		return;

	pHUD->ActivateHotbarItem( atoi( args[1] ) - 1 );
}

static ConCommand asw_squad_hotbar( "asw_squad_hotbar", SquadHotbar, "Activate a squad hotbar button", 0 );

void CASW_Hud_Master::StopItemFX( C_ASW_Marine *pMarine, float flDeleteTime, bool bFailed )
{
	if ( !pMarine )
		return;

	// loops through to see if we already have an order effect for this marine
	HotbarOrderEffectsList_t::IndexLocalType_t f = m_hHotbarOrderEffects.Head();
	while ( m_hHotbarOrderEffects.IsValidIndex( f ) )
	{
		// advance before deleting the pointer out from under us
		const HotbarOrderEffectsList_t::IndexLocalType_t i = f;
		f = m_hHotbarOrderEffects.Next( f );
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
void CASW_Hud_Master::MsgFunc_ASWOrderUseItemFX( bf_read &msg )
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
			pszClassName = "hacking_icon";	// no weapon class associated with this
		}
		break;

	default:
		{
			Assert( false ); // unspecified order type
			return;
		}
		break;
	}

	CNewParticleEffect *pEffect = pMarine->ParticleProp()->Create( "order_use_item", PATTACH_ABSORIGIN );
	if ( pEffect )
	{
		pMarine->ParticleProp()->AddControlPoint( pEffect, 1, pMarine, PATTACH_CUSTOMORIGIN );
		pEffect->SetControlPoint( 1, vecPosition + Vector( 0.0f, 0.0f, 5.0f ) );
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

void CASW_Hud_Master::MsgFunc_ASWOrderStopItemFX( bf_read &msg )
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

// used by blink and assault jets when player tries to jump to a bad spot
void CASW_Hud_Master::MsgFunc_ASWInvalidDesination( bf_read &msg )
{
	int iMarine = msg.ReadShort();	
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	Vector vecPosition;
	vecPosition.x = msg.ReadFloat();
	vecPosition.y = msg.ReadFloat();
	vecPosition.z = msg.ReadFloat();
	CNewParticleEffect *pEffect = pMarine->ParticleProp()->Create( "invalid_destination", PATTACH_ABSORIGIN );
	if ( pEffect )
	{
		pMarine->ParticleProp()->AddControlPoint( pEffect, 1, pMarine, PATTACH_CUSTOMORIGIN );
		pEffect->SetControlPoint( 1, vecPosition );
	}
	pMarine->EmitSound( "ASW_Weapon.InvalidDestination" );
}

extern int g_asw_iPlayerListOpen;

bool CASW_Hud_Master::ShouldDraw()
{
	if ( g_asw_iPlayerListOpen > 0 )
		return false;

	return CASW_HudElement::ShouldDraw();
}

const HudSheetTexture_t* CASW_Hud_Master::GetDeadIconUVs()
{
	return &m_Sheet1[ UV_DeadIcon ];
}

int CASW_Hud_Master::GetDeadIconTextureID()
{
	return m_nSheet1ID;
}