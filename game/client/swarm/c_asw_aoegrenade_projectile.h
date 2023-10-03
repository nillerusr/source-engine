#ifndef _C_ASW_AOEGREN_PROJECTILE_H
#define _C_ASW_AOEGREN_PROJECTILE_H
#pragma once

struct dlight_t;

#include "c_pixel_visibility.h"

class C_ASW_AOEGrenade_Projectile : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_AOEGrenade_Projectile, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	C_ASW_AOEGrenade_Projectile();
	virtual ~C_ASW_AOEGrenade_Projectile();

	void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );
	void	NotifyDestroyParticle( Particle* pParticle );
	void	NotifyShouldTransmit( ShouldTransmitState_t state );
	void	RestoreResources( void );
	void	UpdateTargetAOEEffects( void );
	void	UpdateParticleAttachments( CNewParticleEffect *pEffect, C_BaseEntity *pTarget );
	virtual void UpdatePingEffects( void );
	const Vector& GetEffectOrigin();
	virtual float GetEffectRadius( void ) { return m_flRadius.Get(); }

	virtual Color GetGrenadeColor( void ) { return Color( 16, 16, 80, 255 ); }
	virtual const char* GetIdleLoopSoundName( void ) { return "ASW_BuffGrenade.ActiveLoop"; }
	virtual const char* GetLoopSoundName( void ) { return "ASW_BuffGrenade.BuffLoop"; }
	virtual const char* GetStartSoundName( void ) { return "ASW_BuffGrenade.StartBuff"; }
	virtual const char* GetActivateSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetPingEffectName( void ) { return "buffgrenade_pulse"; }
	virtual const char* GetArcEffectName( void ) { return "buffgrenade_attach_arc"; }
	virtual const char* GetArcAttachmentName( void ) { return "weapon_aim_attachment"; }
	virtual bool ShouldAttachEffectToWeapon( void ) { return false; }
	virtual bool ShouldSpawnSphere( void ) { return false; }
	virtual float GetSphereScale( void ) { return 1.0f; }
	virtual int GetSphereSkin( void ) { return 0; }

	float	m_flTimeBurnOut;
	float	m_flScale;
	bool	m_bSettled;
	dlight_t *m_pDLight;
	float	m_fStartLightTime;
	float	m_fLightRadius;
	float	m_fUpdateAttachFXTime;

	pixelvis_handle_t m_queryHandle;

	// sound
	void SoundShutdown();
	void SoundInit();
	virtual void UpdateOnRemove();
	virtual void OnRestore();
	CSoundPatch		*m_pLoopedSound;

	bool m_bUpdateAOETargets;
	CUtlVector< CHandle<C_BaseEntity> > m_hAOETargets;

	EHANDLE m_hSphereModel;
	float m_flTimeCreated;

private:
	bool	m_bPlayingSound;
	CNetworkVar( float, m_flRadius );

	C_ASW_AOEGrenade_Projectile( const C_ASW_AOEGrenade_Projectile & );

	struct AOETargetEffects_t
	{
		CHandle<C_BaseEntity>		hTarget;
		CUtlReference<CNewParticleEffect> pEffect;
		CSoundPatch		*pBuffLoopSound;
#ifdef DEBUG
		AOETargetEffects_t * me;
#endif
		AOETargetEffects_t()
		{
			hTarget = NULL;
			pEffect = NULL;
			pBuffLoopSound  = NULL;
#ifdef DEBUG
			me = this;
#endif
		}

	};

	typedef CUtlFixedLinkedList<AOETargetEffects_t> AOEGrenTargetFXList_t;
	AOEGrenTargetFXList_t m_hAOETargetEffects;

	CUtlReference<CNewParticleEffect> m_pPulseEffect;
};


#endif // _C_ASW_AOEGREN_PROJECTILE_H