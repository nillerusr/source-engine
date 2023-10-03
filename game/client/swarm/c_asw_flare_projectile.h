#ifndef _INCLUDED_C_ASW_FLARE_PROJECTILE_H
#define _INCLUDED_C_ASW_FLARE_PROJECTILE_H
#pragma once

struct dlight_t;

#include "c_pixel_visibility.h"

class C_ASW_Flare_Projectile : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Flare_Projectile, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	C_ASW_Flare_Projectile();
	virtual ~C_ASW_Flare_Projectile();

	virtual Class_T	Classify() { return CLASS_FLARE; }

	void	OnDataChanged( DataUpdateType_t updateType );
	//void	Update( float timeDelta );
	virtual void	ClientThink( void );
	void	NotifyShouldTransmit( ShouldTransmitState_t state );
	const Vector& GetEffectOrigin();

	float	m_flTimeBurnOut;
	float	m_flScale;
	bool	m_bLight;
	dlight_t *m_pDLight;
	float	m_fStartLightTime;
	float m_fLightRadius;
	bool	m_bSmoke;
	pixelvis_handle_t m_queryHandle;

	// sound
	void SoundShutdown();
	void SoundInit();
	virtual void UpdateOnRemove();
	virtual void OnRestore();
	CSoundPatch		*m_pBurnSound;


private:
	C_ASW_Flare_Projectile( const C_ASW_Flare_Projectile & );

	CUtlReference<CNewParticleEffect> m_pFlareEffect;

public:
	C_ASW_Flare_Projectile* m_pNextFlare;		// next flare in the linked list of live flares
};

extern C_ASW_Flare_Projectile* g_pHeadFlare;	// access to a linked list of live flares

#endif // _INCLUDED_C_ASW_FLARE_PROJECTILE_H