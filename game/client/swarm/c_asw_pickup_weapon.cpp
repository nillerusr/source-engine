#include "cbase.h"
#include "c_asw_pickup_weapon.h"
#include "asw_gamerules.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "c_asw_weapon.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_util_shared.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------
// Base weapon pickup
//--------------------

CUtlDict< int, int > g_WeaponUseIcons;

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon, DT_ASW_Pickup_Weapon, CASW_Pickup_Weapon )
	RecvPropInt		(RECVINFO(m_iBulletsInGun)),
	RecvPropInt		(RECVINFO(m_iClips)),
	RecvPropInt		(RECVINFO(m_iSecondary)),
END_RECV_TABLE()

C_ASW_Pickup_Weapon::C_ASW_Pickup_Weapon()
{
	m_nUseIconTextureID = -1;
	m_bWideIcon = false;
}

void C_ASW_Pickup_Weapon::InitPickup()
{
	if ( !ASWEquipmentList() )
		return;

	CASW_WeaponInfo* pInfo = ASWEquipmentList()->GetWeaponDataFor( GetWeaponClass() );
	if ( !pInfo )
		return;

	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "%s", pInfo->szPrintName );

	int nIndex = g_WeaponUseIcons.Find( GetWeaponClass() );
	if ( nIndex == g_WeaponUseIcons.InvalidIndex() )
	{
		m_nUseIconTextureID = vgui::surface()->CreateNewTextureID();
		char buffer[ 256 ];
		Q_snprintf( buffer, sizeof( buffer ), "vgui/%s", pInfo->szEquipIcon );
		vgui::surface()->DrawSetTextureFile( m_nUseIconTextureID, buffer, true, false);

		nIndex = g_WeaponUseIcons.Insert( GetWeaponClass(), m_nUseIconTextureID );
	}
	else
	{
		m_nUseIconTextureID = g_WeaponUseIcons.Element( nIndex );
	}
	m_bWideIcon = !pInfo->m_bExtra;
}

void C_ASW_Pickup_Weapon::GetUseIconText( wchar_t *unicode, int unicodeBufferSizeInBytes )
{
	wchar_t wszWeaponName[ 128 ];
	TryLocalize( m_szUseIconText, wszWeaponName, sizeof( wszWeaponName ) );

	if ( m_bSwappingWeapon )
	{
		g_pVGuiLocalize->ConstructString( unicode, unicodeBufferSizeInBytes, g_pVGuiLocalize->Find("#asw_swap_weapon_format"), 1, wszWeaponName );
	}
	else
	{
		g_pVGuiLocalize->ConstructString( unicode, unicodeBufferSizeInBytes, g_pVGuiLocalize->Find("#asw_take_weapon_format"), 1, wszWeaponName );
	}
}

bool C_ASW_Pickup_Weapon::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	if ( BaseClass::GetUseAction( action, pUser ) )
	{
		if ( action.bShowUseKey )
		{
			action.iInventorySlot = pUser->GetWeaponPositionForPickup(GetWeaponClass());
		}
// 		if ( action.UseIconRed == 255 && action.UseIconGreen == 255 && action.UseIconBlue == 255 )
// 		{
// 			action.UseIconRed = 66;
// 			action.UseIconGreen = 142;
// 			action.UseIconBlue = 192;
// 		}
		action.bWideIcon = m_bWideIcon;
		return true;
	}
	return false;
}


//---------
// Rifle
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Rifle, DT_ASW_Pickup_Weapon_Rifle, CASW_Pickup_Weapon_Rifle )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Rifle::C_ASW_Pickup_Weapon_Rifle()
{
}

//---------
// Prototype Rifle
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_PRifle, DT_ASW_Pickup_Weapon_PRifle, CASW_Pickup_Weapon_PRifle )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_PRifle::C_ASW_Pickup_Weapon_PRifle()
{
}

//---------
// Autogun
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Autogun, DT_ASW_Pickup_Weapon_Autogun, CASW_Pickup_Weapon_Autogun )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Autogun::C_ASW_Pickup_Weapon_Autogun()
{
}

//---------
// Shotgun
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Shotgun, DT_ASW_Pickup_Weapon_Shotgun, CASW_Pickup_Weapon_Shotgun )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Shotgun::C_ASW_Pickup_Weapon_Shotgun()
{
}

//---------
// Assault Shotgun
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Assault_Shotgun, DT_ASW_Pickup_Weapon_Assault_Shotgun, CASW_Pickup_Weapon_Assault_Shotgun )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Assault_Shotgun::C_ASW_Pickup_Weapon_Assault_Shotgun()
{
}

//---------
// Flamer
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Flamer, DT_ASW_Pickup_Weapon_Flamer, CASW_Pickup_Weapon_Flamer )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Flamer::C_ASW_Pickup_Weapon_Flamer()
{
}

//---------
// Railgun
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Railgun, DT_ASW_Pickup_Weapon_Railgun, CASW_Pickup_Weapon_Railgun )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Railgun::C_ASW_Pickup_Weapon_Railgun()
{
}

//---------
// Ricochet
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Ricochet, DT_ASW_Pickup_Weapon_Ricochet, CASW_Pickup_Weapon_Ricochet )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Ricochet::C_ASW_Pickup_Weapon_Ricochet()
{
}

//---------
// Flechette
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Flechette, DT_ASW_Pickup_Weapon_Flechette, CASW_Pickup_Weapon_Flechette )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Flechette::C_ASW_Pickup_Weapon_Flechette()
{
}


//---------
// Fire Extinguisher
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_FireExtinguisher, DT_ASW_Pickup_Weapon_FireExtinguisher, CASW_Pickup_Weapon_FireExtinguisher )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_FireExtinguisher::C_ASW_Pickup_Weapon_FireExtinguisher()
{
}

//---------
// Pistol
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Pistol, DT_ASW_Pickup_Weapon_Pistol, CASW_Pickup_Weapon_Pistol )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Pistol::C_ASW_Pickup_Weapon_Pistol()
{
}


//---------
// Mining Laser
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Mining_Laser, DT_ASW_Pickup_Weapon_Mining_Laser, CASW_Pickup_Weapon_Mining_Laser )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Mining_Laser::C_ASW_Pickup_Weapon_Mining_Laser()
{
}


//---------
// Chainsaw
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Chainsaw, DT_ASW_Pickup_Weapon_Chainsaw, CASW_Pickup_Weapon_Chainsaw )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Chainsaw::C_ASW_Pickup_Weapon_Chainsaw()
{
}

//---------
// PDW
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_PDW, DT_ASW_Pickup_Weapon_PDW, CASW_Pickup_Weapon_PDW )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_PDW::C_ASW_Pickup_Weapon_PDW()
{
}

//---------
// Grenades
//---------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Pickup_Weapon_Grenades, DT_ASW_Pickup_Weapon_Grenades, CASW_Pickup_Weapon_Grenades )
END_RECV_TABLE()

C_ASW_Pickup_Weapon_Grenades::C_ASW_Pickup_Weapon_Grenades()
{
}