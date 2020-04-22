//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity used to highlight laser designation points to clients
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "tf_obj_manned_plasmagun.h"
#include "env_laserdesignation.h"

#if !defined( CLIENT_DLL )

#include "tf_vehicle_tank.h"
#include "tf_obj_manned_missilelauncher.h"

#endif

extern ConVar weapon_grenade_rocket_track_range_mod;
extern ConVar vehicle_tank_range;
extern ConVar weapon_rocket_launcher_range;
extern ConVar obj_manned_missilelauncher_range_off;

//-----------------------------------------------------------------------------
// Stores a list of all laser designations
//-----------------------------------------------------------------------------
CUtlVector< EHANDLE >	CEnvLaserDesignation::m_LaserDesignatorsTeam1;
CUtlVector< EHANDLE >	CEnvLaserDesignation::m_LaserDesignatorsTeam2;

IMPLEMENT_NETWORKCLASS_ALIASED( EnvLaserDesignation, DT_EnvLaserDesignation )

BEGIN_NETWORK_TABLE( CEnvLaserDesignation, DT_EnvLaserDesignation )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_bActive ), 1, SPROP_UNSIGNED ),
#else
	RecvPropInt( RECVINFO( m_bActive ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CEnvLaserDesignation  )
	DEFINE_PRED_FIELD( m_bActive, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( env_laserdesignation, CEnvLaserDesignation );
PRECACHE_REGISTER( env_laserdesignation );

//-----------------------------------------------------------------------------
// Purpose: Create a laser designation
//-----------------------------------------------------------------------------
CEnvLaserDesignation *CEnvLaserDesignation::Create( CBasePlayer *pOwner )
{
	CEnvLaserDesignation *pDesignation = (CEnvLaserDesignation*)CreateEntityByName("env_laserdesignation"); 
	pDesignation->Spawn();
	pDesignation->SetOwnerEntity( pOwner );
	pDesignation->ChangeTeam( pOwner->GetTeamNumber() );
	pDesignation->SetActive( false );

	return pDesignation;
}

//-----------------------------------------------------------------------------
// Purpose: Create a laser designation
//-----------------------------------------------------------------------------
CEnvLaserDesignation *CEnvLaserDesignation::CreatePredicted( CBasePlayer *pOwner )
{
#if !defined( NO_ENTITY_PREDICTION )
	CEnvLaserDesignation *pDesignation = (CEnvLaserDesignation*)CREATE_PREDICTED_ENTITY("env_laserdesignation"); 
	if ( pDesignation )
	{
		pDesignation->Spawn();
		pDesignation->SetOwnerEntity( pOwner );
		pDesignation->SetPlayerSimulated( pOwner );
		pDesignation->ChangeTeam( pOwner->GetTeamNumber() );
		pDesignation->SetActive( false );
	}

	return pDesignation;
#else
	return NULL;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvLaserDesignation::CEnvLaserDesignation( void )
{
	m_bActive = -1;	// So the first setactive will take effect
	m_bPrevActive = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvLaserDesignation::~CEnvLaserDesignation( void )
{
	EHANDLE hLaser;
	hLaser = this;

	if ( GetTeamNumber() == 1 )
	{
		m_LaserDesignatorsTeam1.FindAndRemove( hLaser );
	}
	else if ( GetTeamNumber() == 2 )
	{
		m_LaserDesignatorsTeam2.FindAndRemove( hLaser );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaserDesignation::Spawn( void )
{
	SetModel( "models/projectiles/grenade_limpet.mdl" );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	SetSize( vec3_origin, vec3_origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaserDesignation::ChangeTeam( int iTeamNum )
{
	Assert( iTeamNum > 0 && iTeamNum < MAX_TF_TEAMS );

	EHANDLE hLaser;
	hLaser = this;
	if ( iTeamNum == 1 )
	{
		m_LaserDesignatorsTeam1.AddToTail( hLaser );
	}
	else if ( iTeamNum == 2 )
	{
		m_LaserDesignatorsTeam2.AddToTail( hLaser );
	}

	BaseClass::ChangeTeam( iTeamNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaserDesignation::SetActive( bool bActive )
{
	if ( bActive == m_bActive )
		return;

	if ( !bActive )
	{
		AddEffects( EF_NODRAW );
	}
	else
	{
		IncrementInterpolationFrame();
		RemoveEffects( EF_NODRAW );
	}

#if defined( CLIENT_DLL )
	ENTITY_PANEL_ACTIVATE( "laserdesignation", bActive );
#endif

	m_bActive = bActive;
}

#if !defined( CLIENT_DLL )


int CEnvLaserDesignation::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvLaserDesignation::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Only transmit to players who care about laser designation: 
	//	- Player designating
	//	- Players in tanks
	//	- Commandos
	CBaseEntity* pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
	if ( pRecipientEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)pRecipientEntity;

		// Designating player?
		if ( pPlayer == GetOwnerEntity() )
			return SetTransmitState( FL_EDICT_ALWAYS );
					
		if ( !InSameTeam( pPlayer ) )
			return FL_EDICT_DONTSEND;
			
		// In a tank?
		if ( pPlayer->IsInAVehicle() )
		{
			CBaseEntity	*pVehicle  = pPlayer->GetVehicle()->GetVehicleEnt();
			if ( dynamic_cast<CVehicleTank*>(pVehicle) )
			{
				// Make sure it's within range of the tank's fire
				static float flTankRange = 0;
				if ( !flTankRange )
				{
					flTankRange = vehicle_tank_range.GetFloat() * weapon_grenade_rocket_track_range_mod.GetFloat();
					flTankRange *= flTankRange;
				}

				float flDistanceSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
				if ( flDistanceSqr < flTankRange )
					return FL_EDICT_ALWAYS;
			}
			else if ( dynamic_cast<CObjectMannedMissileLauncher*>(pVehicle) )
			{
				// Make sure it's within range of the manned missile launcher's fire
				static float flGunRange = 0;
				if ( !flGunRange )
				{
					flGunRange = obj_manned_missilelauncher_range_off.GetFloat() * weapon_grenade_rocket_track_range_mod.GetFloat();
					flGunRange *= flGunRange;
				}

				float flDistanceSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
				if ( flDistanceSqr < flGunRange )
					return FL_EDICT_ALWAYS;
			}
		}

		// Is the player a commando?
		if ( pPlayer->PlayerClass() == TFCLASS_COMMANDO )
		{
			// Make sure it's within range of the commando's rockets
			static float flCommandoRange = 0;
			if ( !flCommandoRange )
			{
				flCommandoRange = weapon_rocket_launcher_range.GetFloat() * weapon_grenade_rocket_track_range_mod.GetFloat();
				flCommandoRange *= flCommandoRange;
			}

			float flDistanceSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
			if ( flDistanceSqr < flCommandoRange )
				return FL_EDICT_ALWAYS;
		}
	}

	return FL_EDICT_DONTSEND;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CEnvLaserDesignation::GetNumLaserDesignators( int iTeamNumber )
{
	Assert( iTeamNumber > 0 && iTeamNumber < MAX_TF_TEAMS );

	if ( iTeamNumber == 1 )
		return m_LaserDesignatorsTeam1.Count();

	return m_LaserDesignatorsTeam2.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEnvLaserDesignation::GetLaserDesignation( int iTeamNumber, int iDesignator, Vector *vecOrigin )
{
	Assert( iTeamNumber > 0 && iTeamNumber < MAX_TF_TEAMS );

	CHandle<CEnvLaserDesignation> hLaser;
	if ( iTeamNumber == 1 )
	{
		Assert( iDesignator < m_LaserDesignatorsTeam1.Count() );
		hLaser = m_LaserDesignatorsTeam1[iDesignator];
	}
	else
	{
		Assert( iDesignator < m_LaserDesignatorsTeam2.Count() );
		hLaser = m_LaserDesignatorsTeam2[iDesignator];
	}

	// Active?
	if ( !hLaser.Get() || !hLaser->IsActive() )
		return false;

	*vecOrigin = hLaser->GetAbsOrigin();
	return true;
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvLaserDesignation::DrawModel( int flags )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CEnvLaserDesignation::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( m_bActive != m_bPrevActive )
	{
		ENTITY_PANEL_ACTIVATE( "laserdesignation", m_bActive );
	}
	m_bPrevActive = m_bActive.Get();
}

//-----------------------------------------------------------------------------
// Add, remove object from the panel 
//-----------------------------------------------------------------------------
void CEnvLaserDesignation::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );

	ENTITY_PANEL_ACTIVATE( "laserdesignation", (!bDormant && m_bActive) );
}

#endif