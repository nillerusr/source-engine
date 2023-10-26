#include "cbase.h"
#include "asw_pickup_weapon.h"
#include "gamerules.h"
#include "items.h"
#include "ammodef.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon, DT_ASW_Pickup_Weapon)	
	SendPropInt		(SENDINFO(m_iBulletsInGun), 16 ),
	SendPropInt		(SENDINFO(m_iClips), 10 ),
	SendPropInt		(SENDINFO(m_iSecondary), 10 ),	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon )
	DEFINE_KEYFIELD( m_iBulletsInGun, FIELD_INTEGER, "BulletsInGun"),
	DEFINE_KEYFIELD( m_iClips, FIELD_INTEGER, "Clips"),
	DEFINE_KEYFIELD( m_iSecondary, FIELD_INTEGER, "SecondaryBullets"),
END_DATADESC()

//---------------------------------------------------------
// Callback for the visibility monitor.
//---------------------------------------------------------
bool CASW_Pickup_Weapon::VismonCallback( CBaseEntity *pPickup, CBasePlayer *pViewingPlayer )
{
	CASW_Pickup_Weapon *pPickupPtr = dynamic_cast < CASW_Pickup_Weapon* >( pPickup );

	if ( !pPickupPtr )
		return true;

	IGameEvent * event = gameeventmanager->CreateEvent( "entity_visible" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "subject", pPickupPtr->entindex() );
		event->SetString( "classname", pPickupPtr->GetWeaponClass() );
		event->SetString( "entityname", STRING( pPickupPtr->GetEntityName() ) );
		gameeventmanager->FireEvent( event );
	}

	return false;
}


void CASW_Pickup_Weapon::Spawn( void )
{
	BaseClass::Spawn();

	VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, &CASW_Pickup_Weapon::VismonCallback, NULL );
}

//---------------------
// Weapon pickup class
//---------------------

// init a pickup created from a weapon carried by the marine
void CASW_Pickup_Weapon::InitFrom(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	if (!pWeapon)
		return;

	int iAmmoIndex = pWeapon->GetPrimaryAmmoType();
	int bullets_on_player = pMarine->GetAmmoCount(iAmmoIndex);
	// fill in the properties of the pickup
	m_iBulletsInGun = pWeapon->Clip1();	
	m_iClips = bullets_on_player / pWeapon->GetMaxClip1();
	m_iSecondary = pWeapon->Clip2();

	if (pMarine->GetNumberOfWeaponsUsingAmmo(iAmmoIndex) > 1)
	{
		// need to leave at least X clips with the marine, since he has a gun using this ammo
		int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex, pMarine);
		int iKeep = MAX(0, (bullets_on_player - iMax));
		m_iClips = iKeep / pWeapon->GetMaxClip1();
	}
}

// init a weapon given to a marine from this pickup
void CASW_Pickup_Weapon::InitWeapon(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	if (!pMarine || !pWeapon)
		return;

	// set the amount of bullets in the gun
	pWeapon->SetClip1( GetNumBulletsInGun() );
	// set secondary bullets in the gun
	pWeapon->SetClip2( GetSecondary() );

	// equip the weapon
	pMarine->Weapon_Equip_In_Index( pWeapon, pMarine->GetWeaponPositionForPickup(GetWeaponClass()) );
	// set the number of clips
	if (pWeapon->GetPrimaryAmmoType()!=-1)
		pMarine->GiveAmmo(GetNumClips() * pWeapon->GetMaxClip1(), pWeapon->GetPrimaryAmmoType());
}

// player has used this item
void CASW_Pickup_Weapon::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	//if (UseIconType==USE_ICON_TAKE)
	{
		if (pMarine->TakeWeaponPickup(this))
		{
			// destroy self
		}
		else
		{
			//todo: print a message about why he couldn't take it
		}
	}
}

//---------------------
// Assault Rifle
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Rifle, DT_ASW_Pickup_Weapon_Rifle)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Rifle )
END_DATADESC()

LINK_ENTITY_TO_CLASS(asw_pickup_rifle, CASW_Pickup_Weapon_Rifle);

void CASW_Pickup_Weapon_Rifle::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/assaultrifle/assaultrifle.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Rifle::Precache( void )
{
	PrecacheModel ("models/weapons/assaultrifle/assaultrifle.mdl");
}

//---------------------
// Prototype Rifle
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_PRifle, DT_ASW_Pickup_Weapon_PRifle)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_PRifle )
END_DATADESC()

LINK_ENTITY_TO_CLASS(asw_pickup_prifle, CASW_Pickup_Weapon_PRifle);

void CASW_Pickup_Weapon_PRifle::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/Prototype/prototyperifle.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_PRifle::Precache( void )
{
	PrecacheModel ("models/weapons/Prototype/prototyperifle.mdl");
}

//---------------------
// Autogun
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Autogun, DT_ASW_Pickup_Weapon_Autogun)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Autogun )
END_DATADESC()

LINK_ENTITY_TO_CLASS(asw_pickup_autogun, CASW_Pickup_Weapon_Autogun);

void CASW_Pickup_Weapon_Autogun::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/autogun/autogun.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Autogun::Precache( void )
{
	PrecacheModel ("models/weapons/autogun/autogun.mdl");
}

//---------------------
// Shotgun
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Shotgun, DT_ASW_Pickup_Weapon_Shotgun)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Shotgun )
END_DATADESC()

void CASW_Pickup_Weapon_Shotgun::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/Shotgun/Shotgun.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Shotgun::Precache( void )
{
	PrecacheModel ("models/weapons/Shotgun/Shotgun.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_shotgun, CASW_Pickup_Weapon_Shotgun);

//---------------------
// Vindicator (Assault Shotgun)
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Assault_Shotgun, DT_ASW_Pickup_Weapon_Assault_Shotgun)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Assault_Shotgun )
END_DATADESC()

void CASW_Pickup_Weapon_Assault_Shotgun::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/vindicator/vindicator.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Assault_Shotgun::Precache( void )
{
	PrecacheModel ("models/weapons/vindicator/vindicator.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_vindicator, CASW_Pickup_Weapon_Assault_Shotgun);

//---------------------
// Flamer
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Flamer, DT_ASW_Pickup_Weapon_Flamer)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Flamer )
END_DATADESC()

void CASW_Pickup_Weapon_Flamer::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/flamethrower/flamethrower.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Flamer::Precache( void )
{
	PrecacheModel ("models/weapons/flamethrower/flamethrower.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_flamer, CASW_Pickup_Weapon_Flamer);

//---------------------
// Railgun
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Railgun, DT_ASW_Pickup_Weapon_Railgun)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Railgun )
END_DATADESC()

void CASW_Pickup_Weapon_Railgun::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/Railgun/Railgun.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Railgun::Precache( void )
{
	PrecacheModel ("models/weapons/Railgun/Railgun.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_railgun, CASW_Pickup_Weapon_Railgun);

//---------------------
// Ricochet
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Ricochet, DT_ASW_Pickup_Weapon_Ricochet)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Ricochet )
END_DATADESC()

void CASW_Pickup_Weapon_Ricochet::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/Railgun/Railgun.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Ricochet::Precache( void )
{
	PrecacheModel ("models/swarm/Railgun/Railgun.mdl");
}

//LINK_ENTITY_TO_CLASS(asw_pickup_ricochet, CASW_Pickup_Weapon_Ricochet);

//---------------------
// Rocket
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Flechette, DT_ASW_Pickup_Weapon_Flechette)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Flechette )
END_DATADESC()

void CASW_Pickup_Weapon_Flechette::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/Railgun/Railgun.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Flechette::Precache( void )
{
	PrecacheModel ("models/swarm/Railgun/Railgun.mdl");
}

//LINK_ENTITY_TO_CLASS(asw_pickup_flechette, CASW_Pickup_Weapon_Flechette);

//---------------------
// Fire Extinguisher
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_FireExtinguisher, DT_ASW_Pickup_Weapon_FireExtinguisher)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_FireExtinguisher )
END_DATADESC()

void CASW_Pickup_Weapon_FireExtinguisher::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/FireExt/fireextpickup.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_FireExtinguisher::Precache( void )
{
	PrecacheModel ("models/swarm/FireExt/fireextpickup.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_fire_extinguisher, CASW_Pickup_Weapon_FireExtinguisher);


//---------------------
// Pistol
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Pistol, DT_ASW_Pickup_Weapon_Pistol)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Pistol )
END_DATADESC()

void CASW_Pickup_Weapon_Pistol::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/pistol/Pistol.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Pistol::Precache( void )
{
	PrecacheModel("models/weapons/pistol/Pistol.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_pistol, CASW_Pickup_Weapon_Pistol);


//---------------------
// Mining Laser
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Mining_Laser, DT_ASW_Pickup_Weapon_Mining_Laser)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Mining_Laser )
END_DATADESC()

void CASW_Pickup_Weapon_Mining_Laser::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/MiningLaser/MiningLaser.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Mining_Laser::Precache( void )
{
	PrecacheModel ("models/weapons/MiningLaser/MiningLaser.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_mining_laser, CASW_Pickup_Weapon_Mining_Laser);

//---------------------
// Chainsaw
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Chainsaw, DT_ASW_Pickup_Weapon_Chainsaw)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Chainsaw )
END_DATADESC()

void CASW_Pickup_Weapon_Chainsaw::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/Chainsaw/Chainsaw.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Chainsaw::Precache( void )
{
	PrecacheModel ("models/weapons/Chainsaw/Chainsaw.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_chainsaw, CASW_Pickup_Weapon_Chainsaw);

//---------------------
// PDW
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_PDW, DT_ASW_Pickup_Weapon_PDW)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_PDW )
END_DATADESC()

void CASW_Pickup_Weapon_PDW::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/pdw/pdw.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_PDW::Precache( void )
{
	PrecacheModel ("models/weapons/pdw/pdw.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_pdw, CASW_Pickup_Weapon_PDW);
PRECACHE_REGISTER(asw_pickup_pdw);

//---------------------
// Cluster Grenades
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Grenades, DT_ASW_Pickup_Weapon_Grenades)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Grenades )
END_DATADESC()

void CASW_Pickup_Weapon_Grenades::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/grenades/grenadebox.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Grenades::Precache( void )
{
	PrecacheModel ("models/swarm/grenades/grenadebox.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_grenades, CASW_Pickup_Weapon_Grenades);
PRECACHE_REGISTER(asw_pickup_grenades);

