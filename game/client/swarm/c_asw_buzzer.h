#ifndef _INLCUDE_C_ASW_BUZZER_H
#define _INLCUDE_C_ASW_BUZZER_H

#include "c_asw_alien.h"
#include "asw_shareddefs.h"
#include "glow_outline_effect.h"

class CNewParticleEffect;

// Buzzer is our flying poisoning alien (based on the hl2 manhack code)

class C_ASW_Buzzer : public C_AI_BaseNPC, public IASW_Client_Aim_Target
{
public:
	C_ASW_Buzzer();
	virtual ~C_ASW_Buzzer();

	DECLARE_CLASS( C_ASW_Buzzer, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	// Purpose: Start the buzzer's engine sound.
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void UpdateOnRemove( void );
	virtual void OnRestore();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BUZZER; }

	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 18; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return m_vecLastRenderedPos; }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return m_vecLastRenderedPos; }

	CNetworkVar(bool, m_bOnFire);
	bool m_bClientOnFire;
	CNewParticleEffect	*m_pBurningEffect;
	virtual void UpdateFireEmitters();

	CNetworkVar(bool, m_bElectroStunned);
	float m_fNextElectroStunEffect;
	virtual void ClientThink();

	// storing our location for autoaim
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	Vector m_vecLastRenderedPos;

private:
	C_ASW_Buzzer( const C_ASW_Buzzer & );

	// Purpose: Start + stop the buzzer's engine sound.
	void SoundInit( void );
	void SoundShutdown( void );

	CGlowObject m_GlowObject;

	CSoundPatch		*m_pEngineSound1;

	int				m_nEnginePitch1;
	float			m_flEnginePitch1Time;	

	CUtlReference<CNewParticleEffect> m_pTrailEffect;	
};

#endif /* _INLCUDE_C_ASW_BUZZER_H */