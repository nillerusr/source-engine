#include "cbase.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_shareddefs.h"
#include "asw_gamerules.h"
#undef CASW_Pickup
#include "c_asw_pickup.h"
#include "asw_util_shared.h"
#include "functionproxy.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool C_ASW_Pickup::s_bLoadedUseIconTextures = false;
int C_ASW_Pickup::s_nUseIconTake = -1;
int C_ASW_Pickup::s_nUseIconTakeRifleAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeAutogunAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeShotgunAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeVindicatorAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeFlamerAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakePistolAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeMiningLaserAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakePDWAmmo = -1;
int C_ASW_Pickup::s_nUseIconTakeRailgunAmmo = -1;

int C_ASW_Pickup::s_nUseIconTakePowerup_FireB = -1;
int C_ASW_Pickup::s_nUseIconTakePowerup_FreezeB = -1;
int C_ASW_Pickup::s_nUseIconTakePowerup_ExplodeB = -1;
int C_ASW_Pickup::s_nUseIconTakePowerup_ElectricB = -1;
int C_ASW_Pickup::s_nUseIconTakePowerup_ChemicalB = -1;
int C_ASW_Pickup::s_nUseIconTakePowerup_Speed = -1;

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup, DT_ASW_Pickup, CASW_Pickup )
END_RECV_TABLE()

C_ASW_Pickup::C_ASW_Pickup() :
m_GlowObject( this, Vector( 0.0f, 0.4f, 0.75f ), 1.0f, false, true )
{	
	m_szUseIconText[0] = '\0';
	//m_fAmbientLight = 0.02f;
}

void C_ASW_Pickup::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitPickup();
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void C_ASW_Pickup::ClientThink()
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

void C_ASW_Pickup::GetUseIconText( wchar_t *unicode, int unicodeBufferSizeInBytes )
{
	TryLocalize( m_szUseIconText, unicode, unicodeBufferSizeInBytes );
}

int C_ASW_Pickup::GetUseIconTextureID()
{
	if (!s_bLoadedUseIconTextures)
	{
		// load in each of the textures used by pickups
		LoadUseIconTextures();				
	}

	return s_nUseIconTake;
}

bool C_ASW_Pickup::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_Pickup::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.iUseIconTexture = GetUseIconTextureID();
	GetUseIconText( action.wszText, sizeof( action.wszText ) );
	action.UseTarget = this;
	action.fProgress = -1;
	action.iInventorySlot = -1;
	
	if (AllowedToPickup(pUser))
	{
		action.UseIconRed = 66;
		action.UseIconGreen = 142;
		action.UseIconBlue = 192;
		action.bShowUseKey = true;	
	}
	else
	{
		action.UseIconRed = 255;
		action.UseIconGreen = 0;
		action.UseIconBlue = 0;
		action.TextRed = 164;
		action.TextGreen = 164;
		action.TextBlue = 164;
		action.bTextGlow = false;
		if (ASWGameRules())
			TryLocalize( ASWGameRules()->GetPickupDenial(), action.wszText, sizeof( action.wszText ) );
		action.bShowUseKey = false;
	}

	return true;
}

void C_ASW_Pickup::LoadUseIconTexture(const char* szTextureName, int &textureID)
{
	textureID = vgui::surface()->CreateNewTextureID();		
	vgui::surface()->DrawSetTextureFile( textureID, szTextureName, true, false);
}

void C_ASW_Pickup::LoadUseIconTextures()
{
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeRifleAmmo", s_nUseIconTake);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeRifleAmmo", s_nUseIconTakeRifleAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeAutogunAmmo", s_nUseIconTakeAutogunAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeShotgunAmmo", s_nUseIconTakeShotgunAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeVindicatorAmmo", s_nUseIconTakeVindicatorAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeFlamerAmmo", s_nUseIconTakeFlamerAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakePistolAmmo", s_nUseIconTakePistolAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeMiningLaserAmmo", s_nUseIconTakeMiningLaserAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakeRailgunAmmo", s_nUseIconTakeRailgunAmmo);
	LoadUseIconTexture("vgui/swarm/UseIcons/UseIconTakePDWAmmo", s_nUseIconTakePDWAmmo);

	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_fire_bullets", s_nUseIconTakePowerup_FireB);
	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_freeze_bullets", s_nUseIconTakePowerup_FreezeB);
	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_explosive_bullets", s_nUseIconTakePowerup_ExplodeB);
	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_electric_bullets", s_nUseIconTakePowerup_ElectricB);
	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_chemical_bullets", s_nUseIconTakePowerup_ChemicalB);
	LoadUseIconTexture("vgui/hud/PowerupIcons/powerup_increased_speed", s_nUseIconTakePowerup_Speed);
	
	s_bLoadedUseIconTextures = true;
}

//-----------------------------------------------------------------------------
// Material proxy for making pickups red when you can't pick them up
//-----------------------------------------------------------------------------
class CASW_Deny_Pickup_Proxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
};


bool CASW_Deny_Pickup_Proxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CASW_Deny_Pickup_Proxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer || !pC_BaseEntity )
	{
		SetFloatResult( 0.0f );
		return;
	}

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
	{
		SetFloatResult( 0.0f );
		return;
	}

	C_ASW_Pickup *pPickup = dynamic_cast<C_ASW_Pickup*>( BindArgToEntity( pC_BaseEntity ) );
	if ( pPickup && !pPickup->AllowedToPickup( pMarine ) )
	{
		SetFloatResult( 1.0f );
	}
	else
	{
		SetFloatResult( 0.0f );
	}
}

EXPOSE_INTERFACE( CASW_Deny_Pickup_Proxy, IMaterialProxy, "DenyPickup" IMATERIAL_PROXY_INTERFACE_VERSION );