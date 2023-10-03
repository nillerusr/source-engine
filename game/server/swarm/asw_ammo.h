#ifndef _DEFINED_ASW_AMMO_H
#define _DEFINED_ASW_AMMO_H

#include "asw_pickup.h"

class CASW_Player;
class CASW_Marine;

DECLARE_AUTO_LIST( IAmmoPickupAutoList );

class CASW_Ammo : public CASW_Pickup, public IAmmoPickupAutoList
{
public:	
	DECLARE_CLASS( CASW_Ammo, CASW_Pickup );

	virtual void Spawn( void );
	
	virtual bool AllowedToPickup(CASW_Marine *pMarine);
	int GetAmmoType() { return m_iAmmoIndex; }
	
	static bool VismonCallback( CBaseEntity *pPickup, CBasePlayer *pViewingPlayer );

	IMPLEMENT_AUTO_LIST_GET();

	int m_iAmmoIndex;
};

class CASW_Ammo_Rifle : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Rifle, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_RIFLE; }
};

class CASW_Ammo_Autogun : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Autogun, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_AUTOGUN; }
};

class CASW_Ammo_Shotgun : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Shotgun, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_SHOTGUN; }
};

class CASW_Ammo_Assault_Shotgun : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Assault_Shotgun, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_ASSAULT_SHOTGUN; }
};

class CASW_Ammo_Flamer : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Flamer, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_PISTOL; }
};

class CASW_Ammo_Pistol : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Pistol, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_PISTOL; }
};

class CASW_Ammo_Mining_Laser : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Mining_Laser, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_MINING_LASER; }
};

class CASW_Ammo_Railgun : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Railgun, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_RAILGUN; }
};

class CASW_Ammo_Chainsaw : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_Chainsaw, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_CHAINSAW; }
};

class CASW_Ammo_PDW : public CASW_Ammo
{
public:
	DECLARE_CLASS( CASW_Ammo_PDW, CASW_Ammo );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );	// player has used this item

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_PDW; }
};

#endif /* _DEFINED_ASW_AMMO_H */