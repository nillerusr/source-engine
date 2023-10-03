#ifndef _INCLUDED_ASW_WEAPON_RICOCHET_H
#define _INCLUDED_ASW_WEAPON_RICOCHET_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Ricochet C_ASW_Weapon_Ricochet
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#define CASW_Marine C_ASW_Marine
	#define CASW_Bouncing_Pellet C_ASW_Bouncing_Pellet
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class Beam_t;
class CASW_Bouncing_Pellet;
class CASW_Marine;

#define ASW_NUM_LASER_BEAMS 3

class CASW_Weapon_Ricochet : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Ricochet, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Ricochet();
	virtual ~CASW_Weapon_Ricochet();
	void Precache();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual void ItemPostFrame();

	virtual const float GetAutoAimAmount() { return 0.26; }
	virtual bool ShouldFlareAutoaim() { return true; }

	// test
	CASW_Bouncing_Pellet*  CreatePellet(Vector vecSrc, Vector newVel, CASW_Marine *pMarine);

	#ifndef CLIENT_DLL
		virtual const char* GetPickupClass() { return "asw_pickup_ricochet"; }
		
	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual int DrawModel( int flags, const RenderableInstance_t &instance );
		virtual void NotifyShouldTransmit( ShouldTransmitState_t state );

		void UpdateBounceLaser();
		void ReleaseLaserBeam();
		virtual bool Simulate();

		Beam_t	*m_pLaserBeam[ASW_NUM_LASER_BEAMS];
	#endif

	// aiming grenades at the ground
	virtual bool SupportsGroundShooting() { return false; }

	// is the bounce laser on or not?
	CNetworkVar(bool, m_bBounceLaser);

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_RICOCHET; }
};

#endif /* _INCLUDED_ASW_WEAPON_RICOCHET_H */
