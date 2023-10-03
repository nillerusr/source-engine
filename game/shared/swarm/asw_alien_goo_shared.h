#ifndef _INCLUDED_ASW_ALIEN_GOO_H
#define _INCLUDED_ASW_ALIEN_GOO_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#include "iasw_client_aim_target.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Stim C_ASW_Weapon_Stim
#define CASW_Marine C_ASW_Marine
class CNewParticleEffect;
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"

// spawnflags
#define ASW_SF_BURST_WHEN_TOUCHED		0x0001
#define	ASW_SF_BURST_WHEN_NEAR			0x00000002
#define	ASW_SF_BURST_WHEN_DAMAGED		0x00000004
#define	ASW_SF_BURST_ON_INPUT			0x00000008

// This class represents some pulsating alien goo in the world.  There are two varieties:

//   ==== asw_alien_goo ====
//   Pulsating goo that can only be harmed by fire.  Will hurt marines who touch it.  Destroying this
//   type of goo is necessary for the 'Destroy Biomass' mission objective.

//   ==== asw_grub_sac ====
//   Pulsating sac that hangs on walls.  Will burst open if shot or touched, spewing out grubs.

#ifndef CLIENT_DLL
extern CUtlVector<CASW_Alien_Goo*>	g_AlienGoo;
#endif


class CASW_Alien_Goo : public CBaseFlex
{
public:
	DECLARE_CLASS( CASW_Alien_Goo, CBaseFlex );
	DECLARE_NETWORKCLASS();

	CASW_Alien_Goo();
	virtual ~CASW_Alien_Goo();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	void Precache();
	void Spawn();
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_ALIEN_GOO; }
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	virtual void Extinguish();

	void BurningLinkThink();
	string_t GetBurningLinkName() { return m_BurningLinkName; }
	string_t m_BurningLinkName;	// all alien goo with this name will be set on fire when this one is

	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void StartGooSound();
	virtual void StopGooSound();
	virtual void StopLoopingSounds();
	virtual void StartScreaming();
	virtual bool Dissolve( const char *pMaterialName, float flStartTime, bool bNPCOnly, int nDissolveType );
	void GrubSacThink();
	void InitThink();
	void GooTouch(CBaseEntity* pOther);
	void GooAcidTouch(CBaseEntity* pOther);
	void SpawnGrubs();
	void InputBurst( inputdata_t &inputdata );
	bool m_bSpawnedGrubs;
	bool m_bHasGrubs;						// true if this is an asw_grub_sac
	bool m_bHasAmbientSound, m_bPlayingAmbientSound, m_bPlayingGooScream;
	bool m_bRequiredByObjective;
	bool RoomToSpawnGrub(const Vector& pos);

	float m_fGrubSpawnAngle;				// size of random yaw used when spawning grubs
	float m_fNextAcidBurnTime;				// timer for periodic acid damage dealt to touching marines

	static float s_fNextSpottedChatterTime;

	COutputEvent m_OnGooDestroyed;
#else
	virtual void UpdateOnRemove();
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	float m_fPulseCycle;
	void UpdateFireEmitters();
	bool m_bClientOnFire;
	CNewParticleEffect	*m_pBurningEffect;
#endif
	CNetworkVar(float, m_fPulseStrength);
	CNetworkVar(float, m_fPulseSpeed);	
	CNetworkVar(bool, m_bOnFire);
};

#ifdef CLIENT_DLL
class CASW_Grub_Sac : public CASW_Alien_Goo, public IASW_Client_Aim_Target
#else
class CASW_Grub_Sac : public CASW_Alien_Goo
#endif
{
public:
	DECLARE_CLASS( CASW_Grub_Sac, CASW_Alien_Goo );	
	DECLARE_NETWORKCLASS();

	CASW_Grub_Sac();
	virtual ~CASW_Grub_Sac();

#ifdef CLIENT_DLL
	// aim target interface - so clients have autoaim vs this entity
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 30; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
#endif
};

#endif /* _INCLUDED_ASW_ALIEN_GOO_H */
