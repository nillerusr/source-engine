//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_tfcbase.h"
#include "ammodef.h"
#include "tfc_gamerules.h"

extern IVModelInfo* modelinfo;


#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_tfc_player.h"
	#include "hud_crosshair.h"

#else

	#include "tfc_player.h"

#endif


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}


// ----------------------------------------------------------------------------- //
// CWeaponTFCBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTFCBase, DT_WeaponTFCBase )

BEGIN_NETWORK_TABLE( CWeaponTFCBase, DT_WeaponTFCBase )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTFCBase ) 
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_tfc_base, CWeaponTFCBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponTFCBase )

		DEFINE_FUNCTION( FallThink )

	END_DATADESC()

#endif

#ifdef CLIENT_DLL
	ConVar cl_crosshaircolor( "cl_crosshaircolor", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_dynamiccrosshair( "cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_scalecrosshair( "cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	ConVar cl_crosshairalpha( "cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

	int g_iScopeTextureID = 0;
	int g_iScopeDustTextureID = 0;
#endif

// ----------------------------------------------------------------------------- //
// CWeaponTFCBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponTFCBase::CWeaponTFCBase()
{
	SetPredictionEligible( true );
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}


bool CWeaponTFCBase::IsPredicted() const
{ 
	return true;
}

CTFCPlayer* CWeaponTFCBase::GetPlayerOwner() const
{
	return dynamic_cast< CTFCPlayer* >( GetOwner() );
}

const CTFCWeaponInfo &CWeaponTFCBase::GetTFCWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CTFCWeaponInfo *pTFCInfo;

	#ifdef _DEBUG
		pTFCInfo = dynamic_cast< const CTFCWeaponInfo* >( pWeaponInfo );
		Assert( pTFCInfo );
	#else
		pTFCInfo = static_cast< const CTFCWeaponInfo* >( pWeaponInfo );
	#endif

	return *pTFCInfo;
}


TFCWeaponID CWeaponTFCBase::GetWeaponID( void ) const
{
	Assert( false ); return WEAPON_NONE; 
}


bool CWeaponTFCBase::IsA( TFCWeaponID id ) const
{ 
	return GetWeaponID() == id; 
}


bool CWeaponTFCBase::IsSilenced( void ) const
{
	return false; 
}


void CWeaponTFCBase::Precache( void )
{
	BaseClass::Precache();
}


#ifdef CLIENT_DLL

#else // CLIENT_DLL

	void CWeaponTFCBase::Spawn()
	{
		BaseClass::Spawn();

		// Set this here to allow players to shoot dropped weapons
		SetCollisionGroup( COLLISION_GROUP_WEAPON );
		
		// Move it up a little bit, otherwise it'll be at the guy's feet, and its sound origin 
		// will be in the ground so its EmitSound calls won't do anything.
		SetLocalOrigin( Vector( 0, 0, 5 ) );
	}
	
	bool CWeaponTFCBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
	{
		if ( BaseClass::DefaultReload( iClipSize1, iClipSize2, iActivity ) )
		{
			SendReloadSoundEvent();
			return true;
		}
		else
		{
			return false;
		}
	}

	void CWeaponTFCBase::SendReloadSoundEvent()
	{
		CBasePlayer *pPlayer = GetPlayerOwner();

		assert( pPlayer );

		if ( !pPlayer )
			return;

		// Send a message to any clients that have this entity to play the reload.
		CPASFilter filter( pPlayer->GetAbsOrigin() );
		filter.RemoveRecipient( pPlayer );

		UserMessageBegin( filter, "ReloadEffect" );
			WRITE_SHORT( pPlayer->entindex() );
		MessageEnd();
	}

	Vector CWeaponTFCBase::GetSoundEmissionOrigin() const
	{
		CBasePlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
			return pPlayer->WorldSpaceCenter();
		else
			return WorldSpaceCenter();
	}

#endif
