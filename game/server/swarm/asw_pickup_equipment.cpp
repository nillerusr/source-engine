#include "cbase.h"
#include "asw_pickup_equipment.h"
#include "gamerules.h"
#include "items.h"
#include "ammodef.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_weapon_ammo_satchel_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------
// Medkit
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Medkit, DT_ASW_Pickup_Weapon_Medkit)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Medkit )
END_DATADESC()

void CASW_Pickup_Weapon_Medkit::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/personalMedkit/personalMedkit.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Medkit::Precache( void )
{
	PrecacheModel ("models/items/personalMedkit/personalMedkit.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_medkit, CASW_Pickup_Weapon_Medkit);
PRECACHE_REGISTER(asw_pickup_medkit);

//---------------------
// Sentry Gun Case
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Sentry, DT_ASW_Pickup_Weapon_Sentry)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Sentry )
END_DATADESC()

void CASW_Pickup_Weapon_Sentry::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 2;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Sentry::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_sentry, CASW_Pickup_Weapon_Sentry);
PRECACHE_REGISTER(asw_pickup_sentry);

//---------------------
// Flamer Sentry Gun Case
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Sentry_Flamer, DT_ASW_Pickup_Weapon_Sentry_Flamer)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Sentry_Flamer )
END_DATADESC()

void CASW_Pickup_Weapon_Sentry_Flamer::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 4;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Sentry_Flamer::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_sentry_flamer, CASW_Pickup_Weapon_Sentry_Flamer);
PRECACHE_REGISTER(asw_pickup_sentry_flamer);

//---------------------
// Cannon Sentry Gun Case
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Sentry_Cannon, DT_ASW_Pickup_Weapon_Sentry_Cannon)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Sentry_Cannon )
END_DATADESC()

void CASW_Pickup_Weapon_Sentry_Cannon::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 5;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Sentry_Cannon::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_sentry_cannon, CASW_Pickup_Weapon_Sentry_Cannon);
PRECACHE_REGISTER(asw_pickup_sentry_cannon);

//---------------------
// Freeze Sentry Gun Case
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Sentry_Freeze, DT_ASW_Pickup_Weapon_Sentry_Freeze)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Sentry_Freeze )
END_DATADESC()

void CASW_Pickup_Weapon_Sentry_Freeze::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 3;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Sentry_Freeze::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS( asw_pickup_sentry_freeze, CASW_Pickup_Weapon_Sentry_Freeze );
PRECACHE_REGISTER( asw_pickup_sentry_freeze );

//---------------------
// Tesla Trap
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Tesla_Trap, DT_ASW_Pickup_Weapon_Tesla_Trap)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Tesla_Trap )
END_DATADESC()

void CASW_Pickup_Weapon_Tesla_Trap::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/Mine/mine.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Tesla_Trap::Precache( void )
{
	PrecacheModel("models/items/Mine/mine.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_tesla_trap, CASW_Pickup_Weapon_Tesla_Trap);
PRECACHE_REGISTER(asw_pickup_tesla_trap);

//---------------------
// Ammo Bag
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Ammo_Bag, DT_ASW_Pickup_Weapon_Ammo_Bag)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Ammo_Bag )
END_DATADESC()

CASW_Pickup_Weapon_Ammo_Bag::CASW_Pickup_Weapon_Ammo_Bag()
{
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_RIFLE]=5;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_AUTOGUN]=1;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_SHOTGUN]=10;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_ASSAULT_SHOTGUN]=5;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_FLAMER]=5;
	//m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_RAILGUN]=5;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_PDW]=5;
	m_iAmmoCount[CASW_Weapon_Ammo_Bag::ASW_BAG_SLOT_PISTOL]=5;
}

void CASW_Pickup_Weapon_Ammo_Bag::Spawn( void )
{ 	
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 0;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Ammo_Bag::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

void CASW_Pickup_Weapon_Ammo_Bag::InitFrom(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pWeapon);
	if (!pBag)
		return;
	
	// fill in the properties of the pickup
	m_iBulletsInGun = 0;
	m_iClips = 0;
	m_iSecondary = 0;

	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		m_iAmmoCount[i] = pBag->m_AmmoCount[i];
		pBag->m_AmmoCount.Set(i, 0);
	}
}

void CASW_Pickup_Weapon_Ammo_Bag::InitWeapon(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	if (!pMarine || !pWeapon)
		return;
	CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pWeapon);
	if (!pBag)
		return;

	// ammo bag doesn't have normal ammo
	pWeapon->SetClip1( 0 );
	pWeapon->SetClip2( 0 );

	// equip the weapon
	pMarine->Weapon_Equip_In_Index( pWeapon, pMarine->GetWeaponPositionForPickup(GetWeaponClass()) );
	
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		pBag->m_AmmoCount.Set(i, m_iAmmoCount[i]);
		m_iAmmoCount[i] = 0;
	}
}

LINK_ENTITY_TO_CLASS(asw_pickup_ammo_bag, CASW_Pickup_Weapon_Ammo_Bag);
PRECACHE_REGISTER(asw_pickup_ammo_bag);

//---------------------
// Ammo Satchel
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Ammo_Satchel, DT_ASW_Pickup_Weapon_Ammo_Satchel)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Ammo_Satchel )
END_DATADESC()

CASW_Pickup_Weapon_Ammo_Satchel::CASW_Pickup_Weapon_Ammo_Satchel()
{
	//m_nAmmoDrops = AMMO_SATCHEL_DEFAULT_DROP_COUNT;
}

void CASW_Pickup_Weapon_Ammo_Satchel::Spawn( void )
{ 	
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 0;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Ammo_Satchel::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

void CASW_Pickup_Weapon_Ammo_Satchel::InitFrom(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	CASW_Weapon_Ammo_Satchel *pSatchel = dynamic_cast<CASW_Weapon_Ammo_Satchel*>(pWeapon);
	if (!pSatchel)
		return;

	// fill in the properties of the pickup
	//m_iBulletsInGun = 0;
	//m_iClips = 0;
	//m_iSecondary = 0;

	//m_nAmmoDrops = pSatchel->m_nAmmoDrops;
}

void CASW_Pickup_Weapon_Ammo_Satchel::InitWeapon(CASW_Marine* pMarine, CASW_Weapon* pWeapon)
{
	if (!pMarine || !pWeapon)
		return;
	CASW_Weapon_Ammo_Satchel *pSatchel = dynamic_cast<CASW_Weapon_Ammo_Satchel*>(pWeapon);
	if (!pSatchel)
		return;

	// ammo satchel doesn't have normal ammo
	//pWeapon->SetClip1( 0 );
	//pWeapon->SetClip2( 0 );

	// equip the weapon
	pMarine->Weapon_Equip_In_Index( pWeapon, pMarine->GetWeaponPositionForPickup(GetWeaponClass()) );

	//pSatchel->m_nAmmoDrops = m_nAmmoDrops;
}

LINK_ENTITY_TO_CLASS( asw_pickup_ammo_satchel, CASW_Pickup_Weapon_Ammo_Satchel );
PRECACHE_REGISTER( asw_pickup_ammo_satchel );

//---------------------
// Medical Satchel
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Medical_Satchel, DT_ASW_Pickup_Weapon_Medical_Satchel)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Medical_Satchel )
END_DATADESC()

void CASW_Pickup_Weapon_Medical_Satchel::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 1;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Medical_Satchel::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_medical_satchel, CASW_Pickup_Weapon_Medical_Satchel);
PRECACHE_REGISTER(asw_pickup_medical_satchel);

//---------------------
// Stim Pack
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Stim, DT_ASW_Pickup_Weapon_Stim)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Stim )
END_DATADESC()

void CASW_Pickup_Weapon_Stim::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/itembox/itemboxsmall.mdl");
	m_nSkin = 6;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Stim::Precache( void )
{
	PrecacheModel ("models/items/itembox/itemboxsmall.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_stim, CASW_Pickup_Weapon_Stim);
PRECACHE_REGISTER(asw_pickup_stim);

//---------------------
// Flares
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Flares, DT_ASW_Pickup_Weapon_Flares)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Flares )
END_DATADESC()

void CASW_Pickup_Weapon_Flares::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/itembox/itemboxsmall.mdl" );
	m_nSkin = 1;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Flares::Precache( void )
{
	PrecacheModel ("models/items/itembox/itemboxsmall.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_flares, CASW_Pickup_Weapon_Flares);
PRECACHE_REGISTER(asw_pickup_flares);

//---------------------
// Mines
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Mines, DT_ASW_Pickup_Weapon_Mines)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Mines )
END_DATADESC()

void CASW_Pickup_Weapon_Mines::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/itembox/itemboxsmall.mdl");
	m_nSkin = 0;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Mines::Precache( void )
{
	PrecacheModel ("models/items/itembox/itemboxsmall.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_mines, CASW_Pickup_Weapon_Mines);
PRECACHE_REGISTER(asw_pickup_mines);

//---------------------
// T75
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_T75, DT_ASW_Pickup_Weapon_T75)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_T75 )
END_DATADESC()

void CASW_Pickup_Weapon_T75::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/Mine/mine.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_T75::Precache( void )
{
	PrecacheModel ("models/items/Mine/mine.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_t75, CASW_Pickup_Weapon_T75);
PRECACHE_REGISTER(asw_pickup_t75);

//---------------------
// Heal grenade
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Heal_Grenade, DT_ASW_Pickup_Weapon_Heal_Grenade)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Heal_Grenade )
END_DATADESC()

void CASW_Pickup_Weapon_Heal_Grenade::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/ItemBox/ItemBoxLarge.mdl");
	m_nSkin = 1;
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Heal_Grenade::Precache( void )
{
	PrecacheModel ("models/items/ItemBox/ItemBoxLarge.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_heal_grenade, CASW_Pickup_Weapon_Heal_Grenade);
PRECACHE_REGISTER(asw_pickup_heal_grenade);

//---------------------
// Buff grenade
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Buff_Grenade, DT_ASW_Pickup_Weapon_Buff_Grenade)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Buff_Grenade )
END_DATADESC()

void CASW_Pickup_Weapon_Buff_Grenade::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/Mine/mine.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Buff_Grenade::Precache( void )
{
	PrecacheModel ("models/items/Mine/mine.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_buff_grenade, CASW_Pickup_Weapon_Heal_Grenade);
PRECACHE_REGISTER(asw_pickup_buff_grenade);

//---------------------
// Hornet Barrage
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Hornet_Barrage, DT_ASW_Pickup_Hornet_Barrage)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Hornet_Barrage )
END_DATADESC()

void CASW_Pickup_Hornet_Barrage::Spawn( void )
{ 
	Precache( );
	SetModel( "models/items/Mine/mine.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Hornet_Barrage::Precache( void )
{
	PrecacheModel ("models/items/Mine/mine.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_hornet_barrage, CASW_Pickup_Hornet_Barrage);
PRECACHE_REGISTER(asw_pickup_hornet_barrage);

//---------------------
// Flashlight
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Flashlight, DT_ASW_Pickup_Weapon_Flashlight)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Flashlight )
END_DATADESC()

void CASW_Pickup_Weapon_Flashlight::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/flashlight/flashlightpickup.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Flashlight::Precache( void )
{
	PrecacheModel ("models/swarm/flashlight/flashlightpickup.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_flashlight, CASW_Pickup_Weapon_Flashlight);
PRECACHE_REGISTER(asw_pickup_flashlight);

//---------------------
// Welder
//---------------------

IMPLEMENT_SERVERCLASS_ST(CASW_Pickup_Weapon_Welder, DT_ASW_Pickup_Weapon_Welder)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup_Weapon_Welder )
END_DATADESC()

void CASW_Pickup_Weapon_Welder::Spawn( void )
{ 
	Precache( );
	SetModel( "models/swarm/Welder/Welder.mdl");
	BaseClass::Spawn( );
}

void CASW_Pickup_Weapon_Welder::Precache( void )
{
	PrecacheModel ("models/swarm/Welder/Welder.mdl");
}

LINK_ENTITY_TO_CLASS(asw_pickup_welder, CASW_Pickup_Weapon_Welder);
PRECACHE_REGISTER(asw_pickup_welder);