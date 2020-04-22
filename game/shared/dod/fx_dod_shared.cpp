//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_dod_shared.h"
#include "weapon_dodbase.h"
#include "engine/ivdebugoverlay.h"

#ifndef CLIENT_DLL
	#include "ilagcompensationmanager.h"
#endif

#ifndef CLIENT_DLL

//=============================================================================
//
// Explosions.
//
class CTEDODExplosion : public CBaseTempEntity
{
public:

	DECLARE_CLASS( CTEDODExplosion, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEDODExplosion( const char *name );

public:

	Vector m_vecOrigin;
	Vector m_vecNormal;
};

// Singleton to fire explosion objects
static CTEDODExplosion g_TEDODExplosion( "DODExplosion" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDODExplosion::CTEDODExplosion( const char *name ) : CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
}

IMPLEMENT_SERVERCLASS_ST( CTEDODExplosion, DT_TEDODExplosion )
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[0] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[1] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[2] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropVector( SENDINFO_NOCHECK( m_vecNormal ), 6, 0, -1.0f, 1.0f ),
END_SEND_TABLE()

void TE_DODExplosion( IRecipientFilter &filter, float flDelay, const Vector &vecOrigin, const Vector &vecNormal )
{
	VectorCopy( vecOrigin, g_TEDODExplosion.m_vecOrigin );
	VectorCopy( vecNormal, g_TEDODExplosion.m_vecNormal );

	// Send it over the wire
	g_TEDODExplosion.Create( filter, flDelay );
}

#endif

#ifdef CLIENT_DLL

	#include "fx_impact.h"

	extern void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType );

	// this is a cheap ripoff from CBaseCombatWeapon::WeaponSound():
	void FX_WeaponSound(
		int iPlayerIndex,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		CDODWeaponInfo *pWeaponInfo )
	{

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = pWeaponInfo->aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only

		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, iPlayerIndex, shootsound, &vOrigin ); 
	}

	class CGroupedSound
	{
	public:
		string_t m_SoundName;
		Vector m_vPos;
	};

	CUtlVector<CGroupedSound> g_GroupedSounds;

	
	// Called by the ImpactSound function.
	void ShotgunImpactSoundGroup( const char *pSoundName, const Vector &vEndPos )
	{
		// Don't play the sound if it's too close to another impact sound.
		for ( int i=0; i < g_GroupedSounds.Count(); i++ )
		{
			CGroupedSound *pSound = &g_GroupedSounds[i];

			if ( vEndPos.DistToSqr( pSound->m_vPos ) < 300*300 )
			{
				if ( Q_stricmp( pSound->m_SoundName, pSoundName ) == 0 )
					return;
			}
		}

		// Ok, play the sound and add it to the list.
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, NULL, pSoundName, &vEndPos );

		int tail = g_GroupedSounds.AddToTail();
		g_GroupedSounds[tail].m_SoundName = pSoundName;
		g_GroupedSounds[tail].m_vPos = vEndPos;
	}


	void StartGroupingSounds()
	{
		Assert( g_GroupedSounds.Count() == 0 );
		SetImpactSoundRoute( ShotgunImpactSoundGroup );
	}


	void EndGroupingSounds()
	{
		g_GroupedSounds.Purge();
		SetImpactSoundRoute( NULL );
	}

#else

	#include "te_firebullets.h"

	// Server doesn't play sounds anyway.
	void StartGroupingSounds() {}
	void EndGroupingSounds() {}
	void FX_WeaponSound ( int iPlayerIndex,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		CDODWeaponInfo *pWeaponInfo ) {};

#endif



// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float flSpread
	)
{
	bool bDoEffects = true;

#ifdef CLIENT_DLL
	C_DODPlayer *pPlayer = ToDODPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );
#else
	CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( iPlayerIndex) );
#endif

	const char * weaponAlias =	WeaponIDToAlias( iWeaponID );

	if ( !weaponAlias )
	{
		DevMsg("FX_FireBullets: weapon alias for ID %i not found\n", iWeaponID );
		return;
	}

	//MATTTODO: Why are we looking up the weapon info again when every weapon
	// stores its own m_pWeaponInfo pointer?

	char wpnName[128];
	Q_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", weaponAlias );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );

	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		DevMsg("FX_FireBullets: LookupWeaponInfoSlot failed for weapon %s\n", wpnName );
		return;
	}

	CDODWeaponInfo *pWeaponInfo = static_cast< CDODWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

#ifdef CLIENT_DLL
	if( pPlayer && !pPlayer->IsDormant() )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#else
	if( pPlayer )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#endif	

#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_FireBullets( 
		iPlayerIndex,
		vOrigin, 
		vAngles, 
		iWeaponID,
		iMode,
		iSeed,
		flSpread
	);

	bDoEffects = false; // no effects on server

	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	pPlayer->NoteWeaponFired();
#endif

	

	WeaponSound_t sound_type = SINGLE;

	if ( bDoEffects)
	{
		FX_WeaponSound( iPlayerIndex, sound_type, vOrigin, pWeaponInfo );
	}

	// Fire bullets, calculate impacts & effects
	if ( !pPlayer )
		return;
	
	StartGroupingSounds();

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	RandomSeed( iSeed );

	float x, y;
	do
	{
		x = random->RandomFloat( -0.5, 0.5 ) + random->RandomFloat( -0.5, 0.5 );
		y = random->RandomFloat( -0.5, 0.5 ) + random->RandomFloat( -0.5, 0.5 );
	} while ( (x * x + y * y) > 1.0f );

	Vector vecForward, vecRight, vecUp;
	AngleVectors( vAngles, &vecForward, &vecRight, &vecUp );

	Vector vecDirShooting = vecForward +
					x * flSpread * vecRight +
					y * flSpread * vecUp;

	vecDirShooting.NormalizeInPlace();

	FireBulletsInfo_t info( 1 /*shots*/, vOrigin, vecDirShooting, Vector( flSpread, flSpread, FLOAT32_NAN), MAX_COORD_RANGE, pWeaponInfo->iAmmoType );
	info.m_flDamage = pWeaponInfo->m_iDamage;
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets( info );

#ifdef CLIENT_DLL
	
	{
		trace_t tr;		
		UTIL_TraceLine( vOrigin, vOrigin + vecDirShooting * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

		// if this is a local player, start at attachment on view model
		// else start on attachment on weapon model

		int iEntIndex = pPlayer->entindex();
		int iAttachment = 1;

		Vector vecStart = tr.startpos; 
		QAngle angAttachment;

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		bool bInToolRecordingMode = clienttools->IsInRecordingMode();

		// try to align tracers to actual weapon barrel if possible
		if ( pPlayer->IsLocalPlayer() && !bInToolRecordingMode )
		{
			C_BaseViewModel *pViewModel = pPlayer->GetViewModel(0);

			if ( pViewModel )
			{
				iEntIndex = pViewModel->entindex();
				pViewModel->GetAttachment( iAttachment, vecStart, angAttachment );
			}
		}
		else if ( pLocalPlayer &&
					pLocalPlayer->GetObserverTarget() == pPlayer &&
					pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
		{	
			// get our observer target's view model

			C_BaseViewModel *pViewModel = pLocalPlayer->GetViewModel(0);

			if ( pViewModel )
			{
				iEntIndex = pViewModel->entindex();
				pViewModel->GetAttachment( iAttachment, vecStart, angAttachment );
			}
		}
		else if ( !pPlayer->IsDormant() )
		{
			// fill in with third person weapon model index
			C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

			if( pWeapon )
			{
            	iEntIndex = pWeapon->entindex();

				int nModelIndex = pWeapon->GetModelIndex();
				int nWorldModelIndex = pWeapon->GetWorldModelIndex();
				if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
				{
					pWeapon->SetModelIndex( nWorldModelIndex );
				}

				pWeapon->GetAttachment( iAttachment, vecStart, angAttachment );

				if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
				{
					pWeapon->SetModelIndex( nModelIndex );
				}
			}
		}

		switch( pWeaponInfo->m_iTracerType )
		{
		case 1:		// Machine gun, heavy tracer
			UTIL_Tracer( vecStart, tr.endpos, iEntIndex, TRACER_DONT_USE_ATTACHMENT, 5000.0, true, "BrightTracer" );
			break;

		case 2:		// rifle, smg, light tracer
			vecStart += vecDirShooting * 150;
			UTIL_Tracer( vecStart, tr.endpos, iEntIndex, TRACER_DONT_USE_ATTACHMENT, 5000.0, true, "FaintTracer" );
			break;

		case 0:	// pistols etc, just do the sound
			{
				FX_TracerSound( vecStart, tr.endpos, TRACER_TYPE_DEFAULT );
			}
		default:
			break;
		}
	}
#endif

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif

	EndGroupingSounds();
}

