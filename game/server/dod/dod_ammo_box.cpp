//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_ammo_box.h"
#include "dod_player.h"
#include "dod_gamerules.h"

BEGIN_DATADESC( CAmmoBox )
	DEFINE_THINKFUNC( FlyThink ),
	DEFINE_ENTITYFUNC( BoxTouch ),
END_DATADESC();

LINK_ENTITY_TO_CLASS( dod_ammo_box, CAmmoBox );

void CAmmoBox::Spawn( void )
{
	Precache( );
	SetModel( "models/ammo/ammo_us.mdl" );
	BaseClass::Spawn();

	SetNextThink( gpGlobals->curtime + 0.75f );
	SetThink( &CAmmoBox::FlyThink );

	SetTouch( &CAmmoBox::BoxTouch );

	m_hOldOwner = GetOwnerEntity();
}

void CAmmoBox::Precache( void )
{
	PrecacheModel( "models/ammo/ammo_axis.mdl" );
	PrecacheModel( "models/ammo/ammo_us.mdl" );
}

CAmmoBox *CAmmoBox::Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, int team )
{
	CAmmoBox *p = static_cast<CAmmoBox *> ( CBaseAnimating::Create( "dod_ammo_box", vecOrigin, vecAngles, pOwner ) );

	p->SetAmmoTeam( team );

	return p;
}

void CAmmoBox::SetAmmoTeam( int team )
{
	switch( team )
	{
	case TEAM_ALLIES:
		{
			SetModel( "models/ammo/ammo_us.mdl" );
		}
		break;
	case TEAM_AXIS:
		{
			SetModel( "models/ammo/ammo_axis.mdl" );
		}
		break;
	default:
		Assert(0);
		break;
	}

	m_iAmmoTeam = team;
}

void CAmmoBox::FlyThink( void )
{
	SetOwnerEntity( NULL );	//so our owner can pick it back up
}

void CAmmoBox::BoxTouch( CBaseEntity *pOther )
{
	Assert( pOther );

	if( !pOther->IsPlayer() )
		return;

	if( !pOther->IsAlive() )
		return;

	//Don't let the person who threw this ammo pick it up until it hits the ground.
	//This way we can throw ammo to people, but not touch it as soon as we throw it ourselves
	if( GetOwnerEntity() == pOther )
		return;

	CDODPlayer *pPlayer = ToDODPlayer( pOther );

	Assert( pPlayer );

	if( pPlayer->GetTeamNumber() != m_iAmmoTeam )
		return;

	if( pPlayer == m_hOldOwner )
	{
		//don't give ammo, just give him his drop again
		pPlayer->ReturnGenericAmmo();	
		UTIL_Remove(this);
	}
	else
	{
		//See if they can use some ammo, if so, remove the box
		if( pPlayer->GiveGenericAmmo() )
			UTIL_Remove(this);
	}	
}

bool CAmmoBox::MyTouch( CBasePlayer *pBasePlayer )
{
	if ( !pBasePlayer )
	{
		Assert( false );
		return false;
	}

	if( !pBasePlayer->IsAlive() )
		return false;

	if( pBasePlayer->GetTeamNumber() != m_iAmmoTeam )
		return false;

	CDODPlayer *pPlayer = ToDODPlayer( pBasePlayer );

	if( pPlayer == m_hOldOwner )
	{
		//don't give ammo, just give him his drop again
		pPlayer->ReturnGenericAmmo();	
		UTIL_Remove(this);
	}
	else
	{
		//See if they can use some ammo, if so, remove the box
		if( pPlayer->GiveGenericAmmo() )
			UTIL_Remove(this);
	}

	return true;
}