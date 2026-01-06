//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handling for the suit batteries.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "basecombatweapon.h"
#include "gamerules.h"
#include "items.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CItemBattery : public CItem
{
public:
	DECLARE_CLASS( CItemBattery, CItem );

#ifdef EZ
	static const char *pModelNames[];
#endif

	void Spawn( void )
	{ 
		Precache( );
#ifdef EZ
		SetModel( STRING( GetModelName() ) );
#else
		SetModel( "models/items/battery.mdl" );
#endif
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
#ifdef EZ
		SetModelName( AllocPooledString( pModelNames[ GetEZVariant() ] ) );
		PrecacheModel( STRING( GetModelName() ) );

		// Goo-covered is just a separate skin of the Arbeit model
		if (GetEZVariant() == EZ_VARIANT_RAD)
			m_nSkin = 1;
#else
		PrecacheModel ("models/items/battery.mdl");
#endif

		PrecacheScriptSound( "ItemBattery.Touch" );

	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );
#ifdef MAPBASE
		return ( pHL2Player && pHL2Player->ApplyBattery( m_flPowerMultiplier ) );
#else
		return ( pHL2Player && pHL2Player->ApplyBattery() );
#endif
	}

#ifdef MAPBASE
	void	InputSetPowerMultiplier( inputdata_t &inputdata ) { m_flPowerMultiplier = inputdata.value.Float(); }
	float	m_flPowerMultiplier = 1.0f;

	DECLARE_DATADESC();
#endif
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);
PRECACHE_REGISTER(item_battery);

#ifdef MAPBASE
BEGIN_DATADESC( CItemBattery )

	DEFINE_KEYFIELD( m_flPowerMultiplier, FIELD_FLOAT, "PowerMultiplier" ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetPowerMultiplier", InputSetPowerMultiplier ),

END_DATADESC()
#endif

#ifdef EZ
const char *CItemBattery::pModelNames[EZ_VARIANT_COUNT] = {
	"models/items/battery.mdl",
	"models/items/xen/battery.mdl",
	"models/items/arbeit/battery.mdl", // Skin 1
	"models/items/temporal/battery.mdl",
	"models/items/arbeit/battery.mdl",
	"models/items/blood/battery.mdl",
	"models/items/athenaeum/battery.mdl",
	"models/items/ash/battery.mdl",
};
#endif

