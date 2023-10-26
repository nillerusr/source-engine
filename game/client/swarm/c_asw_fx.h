#ifndef _INCLUDED_C_ASW_FX_H
#define _INCLUDED_C_ASW_FX_H

#include "c_gib.h"

class C_ASW_Marine;

// Optional tracer types
enum ASW_FX_TracerType_t
{
	ASW_FX_TRACER_DUAL_LEFT	= 0x00000001,	// fire from left dual attachment point
	ASW_FX_TRACER_DUAL_RIGHT	= 0x00000002,	// fire from right dual attachment point
};

class CDroneGibManager : public CAutoGameSystem
{
public:
	// Methods of IGameSystem
	virtual void Update( float frametime );
	virtual void LevelInitPreEntity( void );

	void	AddGib( C_BaseEntity *pEntity ); 
	void	RemoveGib( C_BaseEntity *pEntity );

private:
	typedef CHandle<C_BaseEntity> CGibHandle;
	CUtlLinkedList< CGibHandle > m_LRU; 
};

void ASW_FX_BloodBulletImpact( const Vector &origin, const Vector &normal, float scale, unsigned char r, unsigned char g, unsigned char b );
void FX_DroneBleed( const Vector &origin, const Vector &direction, float scale );
void FX_GibMeshEmitter( const char *szModel, const char *szTemplate, const Vector &origin, const Vector &direction, int skinm, float fScale=1.0f, bool bFrozen = false );
void FX_GrubGib( const Vector &origin, const Vector &direction, float scale, bool bOnFire );
void FX_DroneGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bOnFire );
void FX_HarvesterGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bOnFire );
void FX_ParasiteGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bUseGibImpactSounds, bool bOnFire );
void FX_EggGibs( const Vector &origin, int flags, int iEntIndex );
void FX_QueenSpitBurst( const Vector &origin, const Vector &direction, float scale, int skin );

void FX_ProbeStunElectroBeam( CBaseEntity *pEntity, mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset );
void FX_ElectroStun(C_BaseAnimating *pAnimating);
void FX_ElectroStunSplash( const Vector &pos, const Vector &normal, int nFlags );
void FX_QueenDie(C_BaseAnimating *pAnimating);

void FX_ASW_RGEffect(const Vector &vecStart, const Vector &vecEnd);
void FX_ASWTracer( const Vector& start, const Vector& end, int velocity, bool makeWhiz, bool bRedTracer, int iForceStyle=-1 );
// user message based tracers
void ASWUTracer( C_ASW_Marine *pMarine, const Vector& vecEnd, int iAttributeEffects = 0 );
void ASWUTracerless( C_ASW_Marine *pMarine, const Vector& vecEnd, int iAttributeEffects = 0 );	// just muzzle flash and impact, no tracer line
void ASWUTracerDual( C_ASW_Marine *pMarine, const Vector& vecEnd, int nDualType = (ASW_FX_TRACER_DUAL_LEFT | ASW_FX_TRACER_DUAL_RIGHT), int iAttributeEffects = 0 );
void ASWUTracerUnattached( C_ASW_Marine *pMarine, const Vector &vecStart, const Vector &vecEnd, int iAttributeEffects = 0 );
void ASWUTracerRG( C_ASW_Marine *pMarine, const Vector& vecEnd, int iAttributeEffects = 0 );
void FX_ASW_ShotgunSmoke( const Vector& vecOrigin, const QAngle& angFacing );
void FX_ASW_MuzzleEffectAttached( float scale, ClientEntityHandle_t hEntity, int attachmentIndex, unsigned char *pFlashColor = NULL, bool bOneFrame = false  );
void FX_ASW_RedMuzzleEffectAttached( float scale, ClientEntityHandle_t hEntity, int attachmentIndex, unsigned char *pFlashColor = NULL, bool bOneFrame = false  );
void FX_ASW_ParticleMuzzleFlashAttached( float scale, ClientEntityHandle_t hEntity, int attachmentIndex, bool bIsRed );

void FX_ASW_StunExplosion(const Vector &origin);
void FX_ASW_Potential_Burst_Pipe( const Vector &vecImpactPoint, const Vector &vecReflect, const Vector &vecShotBackward, const Vector &vecNormal );

void ASW_AttachFireToHitboxes(C_BaseAnimating *pAnimating, int iNumFires, float fMaxScale);

void FX_ASWWaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime=1.5, float flAlpha=1 );
void FX_ASWSplash( const Vector &origin, const Vector &normal, float scale );

void FX_ASWExplodeMap();

extern CDroneGibManager s_DroneGibManager;

#endif // _INCLUDED_C_ASW_FX_H