#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui/ILocalize.h>
#include <filesystem.h>
#include <keyvalues.h>
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "c_asw_pickup.h"
#include "c_asw_pickup_weapon.h"
#include "c_asw_door.h"
#include "c_asw_door_area.h"
#include "c_asw_button_area.h"
#include "asw_shareddefs.h"
#include "asw_equipment_list.h"
#include "c_asw_weapon.h"
#include "ConVar.h"
#include "asw_weapon_parse.h"
#include "tier0/vprof.h"
#include "asw_hud_objective.h"
#include "asw_hudelement.h"
#include "asw_hud_use_area.h"
#include "asw_hud_use_icon.h"
#include <vgui_controls/PHandle.h>
#include <vgui_controls/Label.h>
#include "asw_hud_powerups.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_hud_scale;
extern ConVar asw_hotbar_self;
ConVar asw_debug_hud( "asw_debug_hud", "0", FCVAR_ARCHIVE, "Draw debug info for the HUD" );
ConVar asw_hud_swaps( "asw_hud_swaps", "1", FCVAR_NONE, "Show weapon swap icons on the HUD" );

using namespace vgui;

// shows weapons held by the current marine
class CASWHudWeapons : public CASW_HudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CASWHudWeapons, CHudNumericDisplay );

public:
	CASWHudWeapons( const char *pElementName );
	~CASWHudWeapons();
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void Paint();	
	virtual void ApplySchemeSettings(IScheme *pScheme);
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }
	
	vgui::HFont m_hWeaponHUDFont;
	vgui::DHANDLE<CASWHudUseArea> m_hUseArea;

	vgui::Panel *(pWeaponPos[ ASW_NUM_INVENTORY_SLOTS ]);

	CASW_Hud_Powerups* m_pPowerups;

	CPanelAnimationVarAliasType( int, m_nSwapArrowTexture, "EquipSwapArrow", "vgui/swarm/EquipIcons/EquipSwapArrow", "textureid" );
		
	int m_iFrameWidth, m_iFrameHeight;
private:
};	

// Disabled - merged into ASW_Hud_Master
//DECLARE_HUDELEMENT( CASWHudWeapons );

CASWHudWeapons::CASWHudWeapons( const char *pElementName ) : CASW_HudElement( pElementName ), CHudNumericDisplay(NULL, "ASWHudWeapons")
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_REMOTE_TURRET);
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	m_hWeaponHUDFont = vgui::INVALID_FONT;

	pWeaponPos[ ASW_INVENTORY_SLOT_PRIMARY ] = new vgui::Panel( this, "Weapon1" );
	pWeaponPos[ ASW_INVENTORY_SLOT_SECONDARY ] = new vgui::Panel( this, "Weapon2" );
	pWeaponPos[ ASW_INVENTORY_SLOT_EXTRA ] = new vgui::Panel( this, "Weapon3" );

	m_pPowerups = new CASW_Hud_Powerups(this, "HudPowerups");
}

CASWHudWeapons::~CASWHudWeapons()
{
	
}

void CASWHudWeapons::Init()
{
	Reset();
}

void CASWHudWeapons::Reset()
{
	SetLabelText(L" ");

	SetShouldDisplayValue(false);
}

void CASWHudWeapons::VidInit()
{
	Reset();
}

void CASWHudWeapons::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,0));
	m_hWeaponHUDFont = ASW_GetDefaultFont(false);

	for ( int i = 0; i < ASW_NUM_INVENTORY_SLOTS; i++ )
	{
		pWeaponPos[ i ]->SetAlpha( 0 );
	}
}

void CASWHudWeapons::Paint()
{
	VPROF_BUDGET( "CASWHudWeapons::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	GetSize(m_iFrameWidth,m_iFrameHeight);
	BaseClass::Paint();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;		
	char cbuffer[64];

	if ( !m_hUseArea.Get() )
	{
		m_hUseArea = ( CASWHudUseArea * )GET_HUDELEMENT( CASWHudUseArea );
	}

	ASWUseAction* pUseAction = NULL;
	if ( m_hUseArea.Get() && m_hUseArea->m_pUseIcon )
	{
		pUseAction = m_hUseArea->m_pUseIcon->GetCurrentUseAction();
	}
	
	int r, g, b;
	
	int tall = m_iFrameHeight * 0.25f * asw_hud_scale.GetFloat();	// how tall to draw the weapon icons
	int spacing_y = tall * 0.7f;
	int nWeaponsToShow = asw_hotbar_self.GetBool() ? ASW_NUM_INVENTORY_SLOTS - 1 : ASW_NUM_INVENTORY_SLOTS;
	bool bPowOnWeapon = false;
	for ( int k = 0; k < nWeaponsToShow; k++ )
	{
		C_ASW_Weapon* pWeapon = pMarine->GetASWWeapon(k);
		// set draw color
		bool bActiveWeapon = (pWeapon && pWeapon == pMarine->GetActiveWeapon());
		if ( bActiveWeapon )
		{
			r = 255;
			g = 255;
			b = 255;
		}
		else
		{
			r = 66;	// light blue
			g = 142;
			b = 192;
		}
		
		if (asw_debug_hud.GetInt()!=0)
		{
			if (pWeapon)
			{
				Q_snprintf(cbuffer, sizeof(cbuffer), "%d:%s %d(%d)", k, pWeapon->GetClassname(),
					pWeapon->GetEffects(), pWeapon->IsEffectActive(EF_NODRAW));
			}
			else
			{
				b = 255;
				Q_snprintf(cbuffer, sizeof(cbuffer), "%d:Empty", k);
			}		
			g_pMatSystemSurface->DrawColoredText(m_hWeaponHUDFont, 0, k*tall, r, g, 
				b, 200, &cbuffer[0]);
		}

		// draw icon
		if (pWeapon && ASWEquipmentList())
		{
			int list_index = pWeapon->GetEquipmentListIndex();
			int iTexture = ASWEquipmentList()->GetEquipIconTexture(!pWeapon->GetWeaponInfo()->m_bExtra, list_index);
			if (iTexture != -1)
			{				
				surface()->DrawSetColor(Color(r,g,b,255));
				surface()->DrawSetTexture(iTexture);

				int icon_x_offset = pWeapon->GetWeaponInfo()->m_iHUDIconOffsetX * (m_iFrameWidth / 160.0f) * asw_hud_scale.GetFloat();
				int icon_y_offset = pWeapon->GetWeaponInfo()->m_iHUDIconOffsetY * (m_iFrameHeight / 160.0f) * asw_hud_scale.GetFloat();
				int text_x_offset = pWeapon->GetWeaponInfo()->m_iHUDNumberOffsetX * (m_iFrameWidth / 160.0f) * asw_hud_scale.GetFloat();
				int text_y_offset = pWeapon->GetWeaponInfo()->m_iHUDNumberOffsetY * (m_iFrameHeight / 160.0f) * asw_hud_scale.GetFloat();

				// drop shadow
				/*
				surface()->DrawSetColor(Color(0,0,0,255));
				if (pWeapon->GetWeaponInfo()->m_bExtra)
					surface()->DrawTexturedRect(icon_x_offset + 1, k*spacing_y + 1 + icon_y_offset,
												icon_x_offset + 1 + tall, k*spacing_y + tall + 1 + icon_y_offset);
				else
					surface()->DrawTexturedRect(icon_x_offset + 1, k*spacing_y + 1 + icon_y_offset,
												icon_x_offset + 1 + tall * 2, k*spacing_y + tall + 1 + icon_y_offset);
				*/

				//todo: set glow vars to make the weapon glow when it's currently selected
				/*
				bool foundVar;
				MaterialVar* pTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
				if (foundVar)
				{
					if ( bActiveWeapon )
					{

					}
					else
					{

					}
				}
				*/

				surface()->DrawSetColor(Color(r,g,b,255));
				if ( pWeapon->GetWeaponInfo()->m_bExtra )
				{
					surface()->DrawTexturedRect(icon_x_offset, k*spacing_y + icon_y_offset,
												icon_x_offset + tall, k*spacing_y + tall + icon_y_offset);
				}
				else
				{
					surface()->DrawTexturedRect(icon_x_offset, k*spacing_y + icon_y_offset,
												icon_x_offset + tall * 2, k*spacing_y + tall + icon_y_offset);
				}

				pWeaponPos[ k ]->SetBounds( icon_x_offset, k * spacing_y + icon_y_offset, tall * 2, tall );

				if ( pWeapon->m_bPoweredUp )
				{
					int iPowBuffer = m_iFrameHeight/20;
					m_pPowerups->SetBounds(icon_x_offset + (tall*2) + iPowBuffer, (k * spacing_y + icon_y_offset) + iPowBuffer, (tall*2), tall - (iPowBuffer*2));
					bPowOnWeapon = true;
				}

				// show charges?
				if (pWeapon->GetWeaponInfo()->m_bShowCharges)
				{
					Q_snprintf(cbuffer, sizeof(cbuffer), "%d", pWeapon->GetChargesForHUD() );
					g_pMatSystemSurface->DrawColoredText(m_hWeaponHUDFont, text_x_offset, 
						k*spacing_y + text_y_offset, r, g, 	b, 200, &cbuffer[0]); 
				}
				// show weapon swaps
				if ( asw_hud_swaps.GetBool() && pUseAction && pUseAction->iInventorySlot == k )
				{
					const CASW_WeaponInfo* pWeaponData = NULL;
					const char *szWeaponClass = NULL;
					C_ASW_Pickup_Weapon *pPickup = dynamic_cast<C_ASW_Pickup_Weapon*>( pUseAction->UseTarget.Get());
					if ( pPickup )
					{
						pWeaponData = ASWEquipmentList()->GetWeaponDataFor( pPickup->GetWeaponClass() );
						szWeaponClass = pPickup->GetWeaponClass();
					}
					else
					{
						C_ASW_Weapon *pWeapon = dynamic_cast<C_ASW_Weapon*>( pUseAction->UseTarget.Get() );
						if ( pWeapon )
						{
							pWeaponData = pWeapon->GetWeaponInfo();
							szWeaponClass = pWeapon->GetClassname();
						}
					}
					if ( pWeaponData )
					{
						int iEquipListIndex = pWeaponData->m_bExtra ? ASWEquipmentList()->GetExtraIndex( szWeaponClass )
							: ASWEquipmentList()->GetRegularIndex( szWeaponClass );
						int iSwapWeaponTexture = ASWEquipmentList()->GetEquipIconTexture( !pWeaponData->m_bExtra, iEquipListIndex );
						if ( m_nSwapArrowTexture != -1 && iSwapWeaponTexture != -1 )
						{
							surface()->DrawSetColor( Color( r,g,b,m_hUseArea->m_pUseIcon->m_pUseText->GetAlpha() ) );	// fade in along with the use icon
							icon_x_offset = tall * 2.0f;
							icon_y_offset = 0;
							surface()->DrawSetTexture( m_nSwapArrowTexture );
							surface()->DrawTexturedRect( icon_x_offset, k*spacing_y + icon_y_offset,
								icon_x_offset + tall, k*spacing_y + tall + icon_y_offset );

							icon_x_offset = tall * 3.0f + pWeaponData->m_iHUDIconOffsetX;
							icon_y_offset = pWeaponData->m_iHUDIconOffsetY;
							surface()->DrawSetTexture( iSwapWeaponTexture );
							surface()->DrawTexturedRect( icon_x_offset, k*spacing_y + icon_y_offset,
								icon_x_offset + ( pWeaponData->m_bExtra ? tall : tall * 2.0f ) , k*spacing_y + tall + icon_y_offset );
						}
					}
				}
			}
		}		
	}

	if ( !bPowOnWeapon )
	{
		//int iPowBuffer = m_iFrameHeight/20;
		int xpos = 12 * (m_iFrameWidth / 160.0f) * asw_hud_scale.GetFloat();
		//int ypos = 2 * (m_iFrameWidth / 160.0f) * asw_hud_scale.GetFloat();
		m_pPowerups->SetBounds(xpos, (spacing_y*nWeaponsToShow) + (spacing_y/3), tall, tall*0.75f );
	}
}
