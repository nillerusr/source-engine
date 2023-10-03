#ifndef _INCLUDED_ASW_WEAPON_WELDER_H
#define _INCLUDED_ASW_WEAPON_WELDER_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Welder C_ASW_Weapon_Welder
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Door;
class CASW_Door_Area;
class CSoundPatch;

class CASW_Weapon_Welder : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Welder, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Welder();
	virtual ~CASW_Weapon_Welder();
	void Precache();

	virtual float GetFireRate( void );
	virtual const float GetAutoAimAmount() { return 0.3f; }
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual void WeldDoor(bool bSeal);

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual const char* GetPickupClass() { return "asw_pickup_welder"; }
	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		void ProcessMuzzleFlashEvent();		
	#endif	
	virtual bool IsOffensiveWeapon() { return false; }
	virtual bool OffhandActivate();

	virtual void ItemPostFrame();
	virtual void BaseItemPostFrame();

	CASW_Door* FindDoor();

	// aiming grenades at the ground
	virtual bool SupportsGroundShooting() { return false; }

#ifdef CLIENT_DLL
	void OnDataChanged( DataUpdateType_t type );
	void ClientThink();
	void CreateWeldingEffects( C_ASW_Door* pDoor );
	void RemoveWeldingEffects( void );
	void UpdateDoorWeldingEffects( void );
	bool m_bWeldSealLast;	// were we sealing last frame, but cutting now?

	CUtlReference<CNewParticleEffect> m_hWeldEffects;
#endif

	//CNetworkVar( bool, m_bIsWeldingDoor );		// the bool is networked to client to store when a door is being welded
	CNetworkVar( bool, m_bWeldSeal );		// is the door being sealed?  False if being cut

	float m_fWeldTime;
	//bool m_bWeldSeal;
	CASW_Door* m_pWeldDoor;

	void StartWelderSound();
	void StopWelderSound();
	CSoundPatch* GetWelderSound();
	CSoundPatch* m_pWeldingSound;
	bool m_bPlayingWelderSound;

	int m_iAutomaticWeldDirection;	// -1 = cut  +1 = seal

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_WELDER; }
};


#endif /* _INCLUDED_ASW_WEAPON_WELDER_H */
