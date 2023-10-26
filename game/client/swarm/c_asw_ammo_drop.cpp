#include "cbase.h"
#include "c_asw_ammo_drop.h"
#include "asw_ammo_drop_shared.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include <vgui/ILocalize.h>
#include "vguimatsurface/imatsystemsurface.h"
#include "asw_util_shared.h"
#include "ammodef.h"
#include "c_asw_player.h"
#include "asw_input.h"
#include "asw_hud_use_icon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Ammo_Drop, DT_ASW_Ammo_Drop, CASW_Ammo_Drop)
	RecvPropInt( RECVINFO( m_iAmmoUnitsRemaining ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Ammo_Drop )

END_PREDICTION_DATA()

bool C_ASW_Ammo_Drop::s_bLoadedUseActionIcons = false;
int  C_ASW_Ammo_Drop::s_nUseActionIconTextureID = -1;

CUtlVector<C_ASW_Ammo_Drop*>	g_AmmoDrops;

vgui::HFont C_ASW_Ammo_Drop::s_hAmmoFont = vgui::INVALID_FONT;

C_ASW_Ammo_Drop::C_ASW_Ammo_Drop() :
	m_GlowObject( this, Vector( 0.0f, 0.4f, 0.75f ), 1.0f, false, true )
{
	m_iAmmoUnitsRemaining = DEFAULT_AMMO_DROP_UNITS;

	g_AmmoDrops.AddToTail( this );

	m_bEnoughAmmo = false;
}


C_ASW_Ammo_Drop::~C_ASW_Ammo_Drop()
{
	g_AmmoDrops.FindAndRemove( this );
}

bool C_ASW_Ammo_Drop::ShouldDraw()
{
	return true;
}

int C_ASW_Ammo_Drop::DrawModel( int flags, const RenderableInstance_t &instance )
{
	int d = BaseClass::DrawModel(flags, instance);

	return d;
}

int C_ASW_Ammo_Drop::GetAmmoDropIconTextureID()
{
	if (!s_bLoadedUseActionIcons)
	{
		// load the portrait textures
		s_nUseActionIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nUseActionIconTextureID, "vgui/swarm/UseIcons/UseIconTakeAmmoDrop", true, false);
		s_bLoadedUseActionIcons = true;
	}

	return s_nUseActionIconTextureID;
}

bool C_ASW_Ammo_Drop::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_Ammo_Drop::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.iUseIconTexture = GetAmmoDropIconTextureID();
	action.UseTarget = this;

	if ( !AllowedToPickup( pUser ) )
	{
		if ( !m_bEnoughAmmo )
		{
			TryLocalize( "#asw_not_enough_ammo_drop", action.wszText, sizeof( action.wszText ) );
		}
		else
		{
			TryLocalize( "#asw_full_ammo_drop", action.wszText, sizeof( action.wszText ) );
		}

		action.fProgress = -1;

		action.UseIconRed = 255;
		action.UseIconGreen = 0;
		action.UseIconBlue = 0;
		action.TextRed = 164;
		action.TextGreen = 164;
		action.TextBlue = 164;
		action.bTextGlow = false;
		action.bShowUseKey = false;
		action.iInventorySlot = -1;
	}
	else
	{
		TryLocalize( "#asw_use_ammo_drop", action.wszText, sizeof( action.wszText ) );

		action.fProgress = -1;

		action.UseIconRed = 255;
		action.UseIconGreen = 255;
		action.UseIconBlue = 255;
		action.bShowUseKey = true;
		action.iInventorySlot = -1;
	}

	return true;
}

void C_ASW_Ammo_Drop::CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon)
{
	if (s_hAmmoFont == vgui::INVALID_FONT)
	{
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
		vgui::IScheme *pScheme = vgui::scheme()->GetIScheme(scheme);
		if (pScheme)
			s_hAmmoFont = vgui::scheme()->GetIScheme(scheme)->GetFont("DefaultSmall", true);
	}

	if (s_hAmmoFont == vgui::INVALID_FONT || alpha <= 0)
		return;

	Color textColor( 255, 255, 255, 255 );

	if ( pUseIcon )
	{
		CASW_HUD_Use_Icon *pUseIconPanel = static_cast<CASW_HUD_Use_Icon*>(pUseIcon);
		float flProgress = (float) GetAmmoUnitsRemaining() / 100.0f;
		char szCountText[64];
		Q_snprintf( szCountText, sizeof( szCountText ), "%d%%", MAX( GetAmmoUnitsRemaining(), 0 ) );
		pUseIconPanel->CustomPaintProgressBar( ix, iy, alpha / 255.0f, flProgress, szCountText, s_hAmmoFont, textColor, "#asw_ammo_label" );
	}
}

int C_ASW_Ammo_Drop::GetAmmoUnitCost( int iAmmoType )
{
	return CASW_Ammo_Drop_Shared::GetAmmoUnitCost( iAmmoType );
}

C_ASW_Weapon* C_ASW_Ammo_Drop::GetAmmoUseUnits( C_ASW_Marine *pMarine )
{
	if ( pMarine )
	{
		CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		if ( !pWeapon || pWeapon->Classify() == CLASS_ASW_AMMO_SATCHEL )
		{
			//pWeapon
			C_ASW_Weapon *pOtherWeapon = pMarine->GetASWWeapon( 0 );
			if ( pOtherWeapon && pOtherWeapon != pWeapon )
			{
				pWeapon = pOtherWeapon;
			}
			else
			{
				pOtherWeapon = pMarine->GetASWWeapon( 1 );
				if ( pOtherWeapon && pOtherWeapon != pWeapon )
				{
					pWeapon = pOtherWeapon;
				}
			}
		}

		if ( pWeapon && pWeapon->IsOffensiveWeapon() )
		{
			int iAmmoType = pWeapon->GetPrimaryAmmoType();
			int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo( iAmmoType );
			int iMaxAmmoCount = GetAmmoDef()->MaxCarry( iAmmoType, pMarine ) * iGuns;
			int iBullets = pMarine->GetAmmoCount( iAmmoType );
			int iAmmoCost = CASW_Ammo_Drop_Shared::GetAmmoUnitCost( iAmmoType );

			m_bEnoughAmmo = m_iAmmoUnitsRemaining >= iAmmoCost;

			if ( ( iBullets < iMaxAmmoCount ) && m_bEnoughAmmo )
			{
				return pWeapon;
			}
		}
	}

	return NULL;
}

bool C_ASW_Ammo_Drop::AllowedToPickup( C_ASW_Marine *pMarine )
{
	// if the marine can't use it, the use portion is zero
	return ( GetAmmoUseUnits( pMarine ) != NULL );
}

void C_ASW_Ammo_Drop::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void C_ASW_Ammo_Drop::ClientThink()
{
	bool bShouldGlow = false;
	float flDistanceToMarineSqr = 0.0f;
	float flWithinDistSqr = (ASW_MARINE_USE_RADIUS*4)*(ASW_MARINE_USE_RADIUS*4);

	C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetMarine() && ASWInput()->GetUseGlowEntity() != this && AllowedToPickup( pLocalPlayer->GetMarine() ) )
	{
		flDistanceToMarineSqr = (pLocalPlayer->GetMarine()->GetAbsOrigin() - WorldSpaceCenter()).LengthSqr();
		if ( flDistanceToMarineSqr < flWithinDistSqr )
			bShouldGlow = true;
	}

	m_GlowObject.SetRenderFlags( false, bShouldGlow );

	if ( m_GlowObject.IsRendering() )
	{
		m_GlowObject.SetAlpha( MIN( 0.7f, (1.0f - (flDistanceToMarineSqr / flWithinDistSqr)) * 1.0f) );
	}
}