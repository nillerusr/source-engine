//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "basehlcombatweapon.h"

class CWeaponAR2 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHLMachineGun );

	CWeaponAR2();

	DECLARE_SERVERCLASS();

	void	ItemPostFrame( void );
	void	Precache( void );

#ifdef EZ1
	void	PrimaryAttack(void); // Breadman
#endif	
	void	SecondaryAttack( void );
#ifdef EZ2
	virtual
#endif
	void	DelayedAttack( void );

	const char *GetTracerType( void ) { return "AR2Tracer"; }

	void	AddViewKick( void );

#ifdef EZ2
	virtual void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	virtual void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
#else
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
#endif
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	int		GetMinBurst( void ) { return 2; }
	int		GetMaxBurst( void ) { return 5; }
#ifdef EZ1
	float	GetFireRate( void ) { return 0.12f; } // Breadman - lowered for prototype
#else
	float	GetFireRate( void ) { return 0.1f; }
#endif

	bool	CanHolster( void );
	bool	Reload( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity( void );
	
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
#ifdef EZ2
		// Bad Cop has nearly perfect accuracy with the AR2 to give players
		// the advantage of a Combine elite
		if (this->GetOwner() &&  this->GetOwner()->IsPlayer())
		{
			cone = VECTOR_CONE_1DEGREES;
			return cone;
		}
#endif

#ifdef EZ1
		cone = VECTOR_CONE_10DEGREES;
#else
		cone = VECTOR_CONE_3DEGREES;
#endif

		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

protected:

	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;
	
#ifdef MAPBASE // Make act table accessible outside class
public:
#endif
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
};

#ifdef EZ2
class CWeaponAR2Proto : public CWeaponAR2
{
public:
	DECLARE_CLASS( CWeaponAR2Proto, CWeaponAR2 );

	CWeaponAR2Proto();

	DECLARE_SERVERCLASS();

	void	PrimaryAttack(void); // Breadman
	void	SecondaryAttack( void );
	void	DelayedAttack( void );

	void	AddViewKick( void );

	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );

	int		GetMinBurst( void ) { return 5; }
	int		GetMaxBurst( void ) { return 10; }
	float	GetFireRate( void ) { return 0.12f; } // Breadman - lowered for prototype

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;

		cone = VECTOR_CONE_10DEGREES;

		return cone;
	}

protected:

	int m_nBurstMax;

	DECLARE_DATADESC();
};
#endif

#endif	//WEAPONAR2_H
