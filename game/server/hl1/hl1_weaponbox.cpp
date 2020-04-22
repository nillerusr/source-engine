//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "hl1_items.h"
#include "ammodef.h"


#define WEAPONBOX_MODEL "models/w_weaponbox.mdl"


class CWeaponBox : public CHL1Item
{
public:
	DECLARE_CLASS( CWeaponBox, CHL1Item );

	void Spawn( void );
	void Precache( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	void BoxTouch( CBaseEntity *pPlayer );

	DECLARE_DATADESC();

private:
	bool		PackAmmo( char *szName, int iCount );
	int			GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	
	int			m_cAmmoTypes;	// how many ammo types packed into this box (if packed by a level designer)
	string_t	m_rgiszAmmo[MAX_AMMO_SLOTS];	// ammo names
	int			m_rgAmmo[MAX_AMMO_SLOTS];		// ammo quantities
};
LINK_ENTITY_TO_CLASS(weaponbox, CWeaponBox);
PRECACHE_REGISTER(weaponbox);

BEGIN_DATADESC( CWeaponBox )
	DEFINE_ARRAY( m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( m_cAmmoTypes, FIELD_INTEGER ),

	DEFINE_ENTITYFUNC( BoxTouch ),
END_DATADESC()


bool CWeaponBox::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		if ( PackAmmo( (char *)szKeyName, atoi( szValue ) ) )
		{
			m_cAmmoTypes++;// count this new ammo type.

			return true;
		}
	}
	else
	{
		Warning( "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

void CWeaponBox::Spawn( void )
{ 
	Precache();
	SetModel( WEAPONBOX_MODEL );
	BaseClass::Spawn();

	PrecacheScriptSound( "Item.Pickup" );

	SetTouch( &CWeaponBox::BoxTouch );
}


void CWeaponBox::Precache( void )
{
	PrecacheModel( WEAPONBOX_MODEL );
}


void CWeaponBox::BoxTouch( CBaseEntity *pOther )
{
	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( m_rgiszAmmo[ i ] != NULL_STRING )
		{
			// there's some ammo of this type. 
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ]	= NULL_STRING;
			m_rgAmmo[ i ]		= 0;
		}
	}

	CPASAttenuationFilter filter( pOther, "Item.Pickup" );
	EmitSound( filter, pOther->entindex(), "Item.Pickup" );

	SetTouch(NULL);
	if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
	{
		UTIL_Remove( this );
	}
}


bool CWeaponBox::PackAmmo( char *szName, int iCount )
{
	char szConvertedName[ 32 ];

	if ( FStrEq( szName, "" ) )
	{
		// error here
		Warning( "NULL String in PackAmmo!\n" );
		return false;
	}
	
	Q_snprintf( szConvertedName, sizeof( szConvertedName ), "%s", szName );
	if ( !stricmp( szName, "bolts" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "XBowBolt" );
	}
	if ( !stricmp( szName, "uranium" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Uranium" );
	}
	if ( !stricmp( szName, "9mm" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "9mmRound" );
	}
	if ( !stricmp( szName, "Hand Grenade" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Grenade" );
	}
	if ( !stricmp( szName, "Hornets" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Hornet" );
	}
	if ( !stricmp( szName, "ARgrenades" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "MP5_Grenade" );
	}
	if ( !stricmp( szName, "357" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "357Round" );
	}
	if ( !stricmp( szName, "rockets" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "RPG_Rocket" );
	}
	if ( !stricmp( szName, "Satchel Charge" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Satchel" );
	}
	if ( !stricmp( szName, "buckshot" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Buckshot" );
	}
	if ( !stricmp( szName, "Snarks" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "Snark" );
	}
	if ( !stricmp( szName, "Trip Mine" ) )
	{
		Q_snprintf( szConvertedName, sizeof( szConvertedName ), "TripMine" );
	}

	int iMaxCarry = GetAmmoDef()->MaxCarry( GetAmmoDef()->Index( szConvertedName ) );

	if ( iMaxCarry > 0 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, szConvertedName, iMaxCarry );
		return true;
	}

	return false;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex )
{
	int i;

	for ( i = 1; ( i < MAX_AMMO_SLOTS ) && ( m_rgiszAmmo[i] != NULL_STRING ); i++ )
	{
		if ( stricmp( szName, STRING( m_rgiszAmmo[i] ) ) == 0 )
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = MIN( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}

	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i]	= AllocPooledString( szName );
		m_rgAmmo[i]		= iCount;

		return i;
	}
	Warning( "out of named ammo slots\n");
	return i;
}
