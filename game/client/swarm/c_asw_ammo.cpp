#include "cbase.h"
#include "c_asw_ammo.h"
#include "c_asw_marine.h"
#include "asw_gamerules.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//------------
// Rifle Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Rifle, DT_ASW_Ammo_Rifle, CASW_Ammo_Rifle )
END_RECV_TABLE()

C_ASW_Ammo_Rifle::C_ASW_Ammo_Rifle()
{	
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_rifle");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_rifle");	
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_rifle_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_R");
}

//------------
// Autogun Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Autogun, DT_ASW_Ammo_Autogun, CASW_Ammo_Autogun )
END_RECV_TABLE()

C_ASW_Ammo_Autogun::C_ASW_Ammo_Autogun()
{	
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_autogun");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_autogun");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_autogun_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_AG");
}

//------------
// Shotgun Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Shotgun, DT_ASW_Ammo_Shotgun, CASW_Ammo_Shotgun )
END_RECV_TABLE()

C_ASW_Ammo_Shotgun::C_ASW_Ammo_Shotgun()
{	
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_shotgun");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_shotgun");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_shotgun_full");
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_SG");
}

//------------
// Vindicator (Assault Shotgun) Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Assault_Shotgun, DT_ASW_Ammo_Assault_Shotgun, CASW_Ammo_Assault_Shotgun )
END_RECV_TABLE()

C_ASW_Ammo_Assault_Shotgun::C_ASW_Ammo_Assault_Shotgun()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_vindicator");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_vindicator");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_vindicator_full");
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_ASG");
}

//------------
// Flamer Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Flamer, DT_ASW_Ammo_Flamer, CASW_Ammo_Flamer )
END_RECV_TABLE()

C_ASW_Ammo_Flamer::C_ASW_Ammo_Flamer()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_flamer");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_flamer");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_flamer_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_F");
}

//------------
// Pistol Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Pistol, DT_ASW_Ammo_Pistol, CASW_Ammo_Pistol )
END_RECV_TABLE()

C_ASW_Ammo_Pistol::C_ASW_Ammo_Pistol()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_pistol");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_pistol");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_pistol_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_P");
}

//------------
// Mining Laser Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Mining_Laser, DT_ASW_Ammo_Mining_Laser, CASW_Ammo_Mining_Laser )
END_RECV_TABLE()

C_ASW_Ammo_Mining_Laser::C_ASW_Ammo_Mining_Laser()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_mining_laser");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_mining_laser");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_mining_laser_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_ML");
}

//------------
// Railgun Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Railgun, DT_ASW_Ammo_Railgun, CASW_Ammo_Railgun )
END_RECV_TABLE()

C_ASW_Ammo_Railgun::C_ASW_Ammo_Railgun()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_railgun");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_railgun");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_railgun_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_RG");
}

//------------
// Chainsaw Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_Chainsaw, DT_ASW_Ammo_Chainsaw, CASW_Ammo_Chainsaw )
END_RECV_TABLE()

C_ASW_Ammo_Chainsaw::C_ASW_Ammo_Chainsaw()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_chainsaw");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_chainsaw");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_chainsaw_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_CS");
}

//------------
// PDW Ammo
//------------

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Ammo_PDW, DT_ASW_Ammo_PDW, CASW_Ammo_PDW )
END_RECV_TABLE()

C_ASW_Ammo_PDW::C_ASW_Ammo_PDW()
{
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_ammo_pdw");
	Q_snprintf(m_szNoGunText, sizeof(m_szNoGunText), "#asw_ammo_pdw");
	Q_snprintf(m_szAmmoFullText, sizeof(m_szAmmoFullText), "#asw_ammo_pdw_full");	
	m_iAmmoIndex = GetAmmoDef()->Index("ASW_PDW");
}