//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "shake.h" 
#include "weapon_dodfullauto.h"

#if defined( CLIENT_DLL )
	#define CDODBipodWeapon C_DODBipodWeapon
#endif

class CDODBipodWeapon : public CDODFullAutoWeapon
{
public:
	DECLARE_CLASS( CDODBipodWeapon, CDODFullAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CDODBipodWeapon();

	virtual void Spawn();
	virtual void Precache();
	virtual void Drop( const Vector &vecVelocity );
	virtual void SecondaryAttack( void );
	virtual bool Reload( void );

	virtual float GetWeaponAccuracy( float flPlayerSpeed );

	virtual Activity GetUndeployActivity( void );
	virtual Activity GetDeployActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetReloadActivity( void );
	virtual Activity GetIdleActivity( void );

	virtual bool CanDrop( void );
	virtual bool CanHolster( void );

	inline void SetDeployed( bool bDeployed );
	inline bool IsDeployed( void ) { return m_bDeployed; }

	virtual void ItemBusyFrame( void );
	virtual void ItemPostFrame( void );
	void BipodThink( void );

	bool AttemptToDeploy( void );
	bool CheckDeployEnt( void );

	bool TestDeploy( float *flDeployedHeight, CBaseEntity **pDeployedOn, float *flYawLimitLeft = NULL, float *flYawLimitRight = NULL );
	bool TestDeployAngle( CDODPlayer *pPlayer, float *flDeployedHeight, CBaseEntity **pDeployedOn, QAngle angles );

	bool FindYawLimits( float *flLeftLimit, float *flRightLimit );

	virtual void DoFireEffects();

	void DeployBipod( float flHeight, CBaseEntity *pDeployedOn, float flYawLimitLeft, float flYawLimitRight );
	void UndeployBipod( void );

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }

#ifdef CLIENT_DLL
	virtual int GetWorldModelIndex( void );
	virtual void CheckForAltWeapon( int iCurrentState );

	virtual void OverrideMouseInput( float *x, float *y );
#endif

private:
	CDODBipodWeapon( const CDODBipodWeapon & );

	CNetworkVar( bool, m_bDeployed );

	CNetworkVar( int, m_iDeployedModelIndex );
	CNetworkVar( int, m_iDeployedReloadModelIndex );
	CNetworkVar( int, m_iProneDeployedReloadModelIndex );

	int m_iCurrentWorldModel;

	EHANDLE m_hDeployedOnEnt;
	float m_flDeployedHeight;
	float m_flNextDeployCheckTime;

	Vector m_DeployedEntOrigin;

	bool m_bDuckedWhenDeployed;

#ifdef CLIENT_DLL
	bool m_bUseDeployedReload;
#endif
};
