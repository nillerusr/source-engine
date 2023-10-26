#ifndef _INCLUDED_C_ASW_ALIEN_H
#define _INCLUDED_C_ASW_ALIEN_H

#include "asw_alien_shared.h"
#include "c_ai_basenpc.h"
#include "iasw_client_aim_target.h"
#include "asw_shareddefs.h"
#include "glow_outline_effect.h"
#include "object_motion_blur_effect.h"

class CNewParticleEffect;

class C_ASW_Alien : public C_AI_BaseNPC, public IASW_Client_Aim_Target
{
public:
	DECLARE_CLASS( C_ASW_Alien, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();
	#include "asw_alien_shared_classmembers.h"

					C_ASW_Alien();
	virtual			~C_ASW_Alien();
		
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	// death;
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	virtual void DoBloodDecal( float flDamage, const Vector &vecPos, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	virtual const char *GetDeathParticleEffectName( void ) { return "drone_death"; }
	virtual const char *GetBigDeathParticleEffectName( void ) { return "drone_death_big"; }
	virtual const char *GetSmallDeathParticleEffectName( void ) { return "drone_death_sml"; }
	virtual const char *GetRagdollGibParticleEffectName( void ) { return "drone_ragdoll_gib"; }
	virtual C_ClientRagdoll* CreateClientRagdoll( bool bRestoring = false );
	virtual C_BaseAnimating* BecomeRagdollOnClient( void );
	DeathStyle_t m_nDeathStyle;
	inline  bool IsHurler(); ///< is this drone set to go flinging at the camera
	inline bool IsMeleeThrown();
	virtual bool HasCustomDeathForce(){ return false; };
	virtual Vector GetCustomDeathForce(){ return vec3_origin; };

	// footsteps
	void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	void MarineStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity );
	surfacedata_t* GetGroundSurface();
	void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void DoAlienFootstep( Vector &vecOrigin, float fvol );	
	bool m_bStepSideLeft;

	// stun
	CNetworkVar(bool, m_bElectroStunned);
	float m_fNextElectroStunEffect;

	// electro shocked
	//CNetworkVar(bool, m_bElectroShockSmall);
	//CNetworkVar(bool, m_bElectroShockBig);
	// fire
	CNetworkVar(bool, m_bOnFire);
	bool m_bClientOnFire;
	CUtlReference<CNewParticleEffect> m_pBurningEffect;
	virtual void UpdateFireEmitters();
	virtual void UpdateOnRemove();

	// aim target interface
	IMPLEMENT_AUTO_LIST_GET();
	
	virtual float GetRadius() { return 23; }
	virtual bool IsAimTarget() { return GetHealth() > 0; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return m_vecLastRenderedPos; }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return m_vecAutoTargetRadiusPos; }
	virtual Vector GetLocalAutoTargetRadiusPos();

	// custom shadow
	virtual bool GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;
	ShadowType_t ShadowCastType();
	void GetShadowFromFlashlight(Vector &vecDir, float &fContribution) const;
	float m_fLastCustomContribution;
	Vector m_vecLastCustomDir;
	int m_iLastCustomFrame;

	int m_nLastSetModel;
	virtual void ASWUpdateClientSideAnimation();
	virtual void ClientThink();

	// storing our location for autoaim
	Vector m_vecLastRenderedPos;
	Vector m_vecAutoTargetRadiusPos;

	// health
	virtual int	GetHealth() const { return m_iHealth; }
	int GetMaxHealth( void ) const { return m_iMaxHealth; }
	int  m_iMaxHealth;

	virtual float	GetInterpolationAmount( int flags );

	// Glows are enabled when the sniper scope is used
	CGlowObject m_GlowObject;
	CMotionBlurObject m_MotionBlurObject;
private:
	C_ASW_Alien( const C_ASW_Alien & ); // not defined, not accessible
	static float sm_flLastFootstepTime;
};

extern ConVar asw_drone_ridiculous;
inline bool C_ASW_Alien::IsHurler()
{
	return m_nDeathStyle == kDIE_HURL || asw_drone_ridiculous.GetBool();
}

inline bool C_ASW_Alien::IsMeleeThrown()
{
	return m_nDeathStyle == kDIE_MELEE_THROW;
}

#endif /* _INCLUDED_C_ASW_ALIEN_H */