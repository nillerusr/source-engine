#ifndef ASW_SENTRY_BASE_H
#define ASW_SENTRY_BASE_H
#pragma once

#include "asw_shareddefs.h"
#include "iasw_server_usable_entity.h"

class CASW_Player;
class CASW_Marine;
class CASW_Sentry_Top;

class CASW_Sentry_Base : public CBaseAnimating, public IASW_Server_Usable_Entity
{
public:
	DECLARE_CLASS( CASW_Sentry_Base, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Sentry_Base();
	virtual ~CASW_Sentry_Base();
	void	Spawn( void );
	void	AnimThink( void );
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();	
	
	void PlayDeploySound();
	CASW_Sentry_Top* GetSentryTop();
	EHANDLE m_hSentryTop;
	CHandle<CASW_Marine> m_hDeployer;
	CNetworkVar(bool, m_bAssembled);
	CNetworkVar(bool, m_bIsInUse);
	CNetworkVar(float, m_fAssembleProgress);
	CNetworkVar(float, m_fAssembleCompleteTime);
	CNetworkVar(int, m_iAmmo);
	CNetworkVar(int, m_iMaxAmmo);
	CNetworkVar(bool, m_bSkillMarineHelping);
	float m_fSkillMarineHelping;
	float m_fDamageScale;
	bool m_bAlreadyTaken;

	void OnFiredShots( int nNumShots = 1 );
	inline int GetAmmo() { return m_iAmmo; }
	void SetAmmo( int nAmmo ) { m_iAmmo = nAmmo; }


	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet( CBaseEntity *pUser ) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime);
	virtual void MarineStartedUsing(CASW_Marine* pMarine);
	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	virtual bool NeedsLOSCheck() { return true; }

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_BASE; }

	/// Here are the different types of sentry gun we support.
	/// This array must match the entries in sm_gunTypeToEntityName
	enum GunType_t
	{
		kAUTOGUN = 0,
		kCANNON,
		kFLAME,
		kICE,
		// not a valid gun type:
		kGUNTYPE_MAX
	};

	static const char *GetEntityNameForGunType( GunType_t guntype );
	static const char *GetWeaponNameForGunType( GunType_t guntype );
	static int GetBaseAmmoForGunType( GunType_t guntype );
	GunType_t GetGunType( void ) const;
	inline void SetGunType( GunType_t iType );
	inline void SetGunType( int iType );

protected:
	
	CNetworkVar( int, m_nGunType );

	// specifications for info about the sentry gun types
	struct SentryGunTypeInfo_t 
	{
		SentryGunTypeInfo_t( const char *entname, const char *weaponname, const int baseammo ) :
			m_entityName(entname), m_weaponName(weaponname), m_nBaseAmmo(baseammo) {};

		const char *m_entityName; ///< name of sentry top entity
		const char *m_weaponName; ///< name of the weapon class that deployed this sentry
		int m_nBaseAmmo; ///< ammo to start with
	};

	static const SentryGunTypeInfo_t sm_gunTypeToInfo[kGUNTYPE_MAX];
};


void CASW_Sentry_Base::SetGunType( int iType )
{
	Assert( iType >= 0 && iType < kGUNTYPE_MAX );
	SetGunType( (GunType_t) iType );
}

inline void CASW_Sentry_Base::SetGunType( CASW_Sentry_Base::GunType_t iType )
{
	m_nGunType = iType;
}

#endif /* ASW_SENTRY_BASE_H */