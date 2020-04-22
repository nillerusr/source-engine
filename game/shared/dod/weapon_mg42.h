//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_MG42_H
#define WEAPON_MG42_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_dodbase.h"
#include "weapon_dodbipodgun.h"

#if defined( CLIENT_DLL )
	#define CWeaponMG42 C_WeaponMG42
#endif


#define COOL_CONTEXT "CoolContext"

class CWeaponMG42 : public CDODBipodWeapon
{
public:
	DECLARE_CLASS( CWeaponMG42, CDODBipodWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif
	
	CWeaponMG42()  {}
#ifdef CLIENT_DLL
	virtual ~CWeaponMG42();
#endif

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_MG42; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( !IsDeployed() )
			return WEAPON_MG42_UNDEPLOYED;
		else 
			return WEAPON_MG42;
	}

	virtual void Spawn();
	virtual void Precache();
	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void FinishReload( void );
	virtual void ItemPostFrame( void );
	virtual void ItemBusyFrame( void );
	void ItemFrameCool( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Drop( const Vector &vecVelocity );

	virtual Activity GetReloadActivity( void );
	virtual Activity GetDrawActivity( void );
	virtual Activity GetDeployActivity( void );
	virtual Activity GetUndeployActivity( void );
	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );

	int GetWeaponHeat( void ) { return m_iWeaponHeat; }
	
	virtual bool ShouldDrawCrosshair( void ) { return IsDeployed(); }

	void Cool( void );

#ifdef CLIENT_DLL
	virtual void ClientThink( void );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void CleanupToolRecordingState( KeyValues *msg );

	void EmitSmokeParticle( void );
#else
	void CoolThink( void );
#endif 

	virtual float GetRecoil( void );

private:
	CWeaponMG42( const CWeaponMG42 & );

	CNetworkVar( int, m_iWeaponHeat );
	CNetworkVar( float,	m_flNextCoolTime );

	CNetworkVar( bool,	m_bOverheated );

#ifdef CLIENT_DLL

	CSmartPtr<CSimpleEmitter> m_pEmitter;

	float			m_flParticleAccumulator;
	PMaterialHandle m_hParticleMaterial;

#endif

};

#endif //WEAPON_MG42_H