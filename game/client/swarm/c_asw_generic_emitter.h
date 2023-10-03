#ifndef _DEFINED_ASW_GENERIC_EMITTER_H
#define _DEFINED_ASW_GENERIC_EMITTER_H

#include "particles_simple.h"

enum ASWParticleType
{
	aswpt_normal = 1,
	aswpt_glow
};

enum ASWParticleDrawType
{
	aswpdt_sprite = 0,
	aswpdt_tracer,	
	aswpdt_mesh,
};

class C_ASW_Mesh_Emitter;

// Our custom particle class
class ASWParticle : public SimpleParticle
{
public:
	ASWParticle() : SimpleParticle() {}
	virtual ~ASWParticle()
	{
		if (m_pPartner) m_pPartner->m_pPartner = NULL;	// unlink us from our partner if we have one
	}

	// AddASWParticle automatically initializes these fields.
	Vector		m_vecAccn;		// particle's velocity is affected by this
	float m_fExtraSimulateTime;	// any extra time here will be added to the simulate deltaTime in the next simulation of this particle (used by DoPresimulate)
	// aswhack: some properties the flamethrower emitter needs to look good
	ASWParticleType m_ParticleType;
	float m_fDropTime;
	ASWParticle* m_pPartner;	// glow particles get attached to the main particle
	bool bPlacedDecal;
};

// different types of collision
enum ASWParticleCollision
{
	aswpc_none = 0,
	aswpc_brush,
	aswpc_all
};

enum
{
	ASW_EMITTER_BEAM_POS_BEHIND,
	ASW_EMITTER_BEAM_POS_CENTER,
	ASW_EMITTER_BEAM_POS_FRONT,
};

class CASWGenericEmitter : public CSimpleEmitter
{
	DECLARE_CLASS( CASWGenericEmitter, CSimpleEmitter );	
public:
							CASWGenericEmitter( const char *pDebugName );
	static CSmartPtr<CASWGenericEmitter>	Create( const char *pDebugName );
	
	// call every frame from host object to make this emitter process
	virtual void	Think(float deltaTime, const Vector& Position, const QAngle& Angle);	// called by our host object

	// set whether this emitter should spit out particles or not
	virtual void	SetActive(bool b);

	// get whether this emitter thinks it is spitting out particles now
	inline  bool	GetActive( void );
	
	// Causes all particles to be destroyed in the next simulate and the particle spawn timer is reset
	void ResetEmitter();

	// set which template this emitter should use
	void UseTemplate(const char* templatename, bool bReset = true, bool bLoadFromCache=true);

	// resize this emitter (applies on top of any settings in the template)
	void SetEmitterScale(float f) { m_fEmitterScale = f; SetParticleCullRadius(m_fLargestParticleSize * m_fEmitterScale); }
	float GetEmitterScale() { return m_fEmitterScale; }

	// creates a single particle
	virtual ASWParticle* SpawnParticle(const Vector& Position, const QAngle& Angle);

	// set the time at which the emitter should kill itself
	virtual void SetDieTime(float fTime);

	// Runs through the m_fPresimulateTime instantly, creating particles and simulating them over that period of time
	virtual void DoPresimulate(const Vector& Position, const QAngle& Angle);

	// ==============
	// internal stuff
	// ==============

protected:
	virtual void	StartRender( VMatrix &effectMatrix );
	virtual void	RenderParticles( CParticleRenderIterator *pIterator );
	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual bool	SimulateParticle(ASWParticle* pParticle, float timeDelta);	// simulate a particle for timeDelta seconds. returns true if particle should be removed
	virtual ASWParticle*	AddASWParticle( PMaterialHandle hMaterial, const Vector &vOrigin, float flDieTime=3, unsigned char uchSize=10 );
	virtual void	Update();	// should be called whenever the emitter's look is changed	

	virtual	float	UpdateAlpha( const SimpleParticle *pParticle );
	virtual float	ASWUpdateScale( const ASWParticle *pParticle );
	virtual	void	ASWUpdateVelocity( ASWParticle *pParticle, float timeDelta );
	virtual Vector	UpdateColor( const SimpleParticle *pParticle );	
	
	virtual ASWParticle* SpawnGlowParticle(const Vector& Position, const QAngle& Angle, ASWParticle* pParent);

	// save settings from the specified template	
	void SaveTemplateAs(const char* templatename);	
	const char* GetTemplateName() { return m_szTemplateName; }

	// returns how many particles this emitter has currently
	int GetNumParticles() { return GetBinding().GetNumActiveParticles(); }

	// to get/set which material this emitter uses
	void SetMaterial(const char* materialname );
	const char* GetMaterial() { return m_szMaterialName; }
	
public:		// asw temp

	// to get/set which template this emitter uses for collision effects
	void SetCollisionTemplate(const char* templatename );
	const char* GetCollisionTemplate() { return m_szCollisionTemplateName; }
	// to get/set which template this emitter uses for droplet effects
	void SetDropletTemplate(const char* templatename );
	const char* GetDropletTemplate() { return m_szDropletTemplateName; }

	void SetGlowMaterial(const char* materialname );
	const char* GetGlowMaterial() { return m_szGlowMaterialName; }
	float m_fGlowScale;
	float m_fGlowDeviation;
public:	
	// Structs for our nodes, which control transitions of various particle properties over their lifetimes
	struct ColorNode
	{
		bool bUse;
		float fTime;
		color32 Color;
		float fBandLength;
	};
	struct ScaleNode
	{
		bool bUse;
		float fTime;
		float fScale;
		float fBandLength;
	};
	struct AlphaNode
	{
		bool bUse;
		float fTime;
		float fAlpha;
		float fBandLength;
	};

	bool	m_bEmit;	// Determines whether or not we should emit particles
	float	m_CurrentParticlesPerSecond;	// used to track changes to the particlespersecond
	int		m_iResetEmitter;

	// Properties that describe how this emitter looks
	ColorNode m_Colors[5];
	ScaleNode m_Scales[5];
	AlphaNode m_Alphas[5];		
	float m_ParticlesPerSecond;
	float m_fParticleLifeMin, m_fParticleLifeMax;
	float m_fPresimulateTime;
	Vector velocityMin;
	Vector velocityMax;
	Vector positionMin;
	Vector positionMax;
	Vector accelerationMin;
	Vector accelerationMax;
	float fRollMin, fRollMax;
	float fRollDeltaMin, fRollDeltaMax;	
	float fGravity;
	ASWParticleDrawType m_DrawType;
	float m_fBeamLength;
	bool m_bScaleBeamByVelocity;
	bool m_bScaleBeamByLifeLeft;
	int m_iBeamPosition;
	float m_fDropletChance;		// chance of a particular particle spawning a droplet, if our droplet template is set.  Scale is 0 to 100.

	float m_fEmitterScale;		// our own scale var, applied on top of the template
	int m_iParticleSupply;	// how many particles are left to spawn
	int m_iInitialParticleSupply;	// the initial particle supply
	int m_iLightingType;		// 0 = no lighting   1 = scale color by lighting  2 = scale alpha by lighting  3 = scale alpha and color by lighting
	float m_fLightApply;		// how much to apply lighting (1.0f = fully apply lighting, 0.5f = only apply half of the darkening/coloring)
	Vector m_vecLighting;	// stored lighting vector
	
	float m_fParticleLocal;	// if set to 1.0, the particles will move with the emitter, at 0 they will be unaffected by the emitter's movement once spawned

	Vector m_vecPosition;
	Vector m_vecLastSimulatePosition;
	Vector m_vecEmitterPositionDelta;
	QAngle m_angFacing;
	float m_fDieTime;	// at this time, the emitter will be stop emitting particles and then when all are gone, it'll destroy itself

public:
	bool m_bWrapParticlesToSpawnBounds;	// on X/Y only (note: only works on emitters with no rotation)
	bool m_bLocalCoordSpace;	// note:bugged  - if true, particles are stored in local space and transformed to the position of the emitter when rendered
	ASWParticleCollision m_UseCollision;
	EHANDLE m_hCollisionIgnoreEntity;
	float m_fCollisionDampening;

	// Custom collision stuff - currently accessible only through code on a specific instance, not throught templates
	void SetCustomCollisionMask(int iMask);
	void SetCustomCollisionGroup(int iColGroup);
	bool m_bUseCustomCollisionMask;
	bool m_bUseCustomCollisionGroup;
	int m_CustomCollisionMask;
	int m_CustomCollisionGroup;
		
protected:
	// calculates the times between each node, for quickly picking which nodes to transition between in the update functions
	virtual void	CalcBandLengths();

	PMaterialHandle				m_hMaterial;		// Material handle used for this entity's particles	
	PMaterialHandle				m_hGlowMaterial;		// Material handle used for this entity's particles' glow
	char m_szMaterialName[MAX_PATH];	
	char m_szGlowMaterialName[MAX_PATH];
	char m_szTemplateName[MAX_PATH];
	char m_szCollisionTemplateName[MAX_PATH];
	char m_szDropletTemplateName[MAX_PATH];

	TimedEvent					m_tParticleTimer;	// Timer used to control particle emission rate

	CSmartPtr<CASWGenericEmitter> m_hCollisionEmitter;
	CSmartPtr<CASWGenericEmitter> m_hDropletEmitter;

	float 	m_fLargestParticleSize;	// cache this for quick setting of particle cull size during scaling of the emitter

public:
	float m_fLifeLostOnCollision;

	void SetMeshEmitter(C_ASW_Mesh_Emitter *pMeshEmitter);
	CHandle<C_ASW_Mesh_Emitter> m_hMeshEmitter;
	Vector m_vecTraceMins;
	Vector m_vecTraceMaxs;
	bool m_bHullTraces;
	float m_fReduceRollRateOnCollision;
	void SetCollisionSound(const char* szSoundName );
	char m_szCollisionSoundName[128];
	void SetCollisionDecal(const char* szDecalName );
	char m_szCollisionDecalName[128];

private:
	CASWGenericEmitter( const CASWGenericEmitter & ); // not defined, not accessible

	friend class C_ASW_Emitter;
	friend class C_ASW_Mesh_Emitter;
	friend class CASW_VGUI_Edit_Emitter;
};

// this caches templates
class CASWGenericEmitterCache
{
public:
	virtual ~CASWGenericEmitterCache();
	KeyValues* FindTemplate(const char* szTemplateName);
	void ListCachedEmitters();
	void PrecacheTemplates();

	CUtlVector<KeyValues*> m_Templates;
	CUtlVector<const char*> m_TemplateNames;
};

extern CASWGenericEmitterCache g_ASWGenericEmitterCache;

inline bool CASWGenericEmitter::GetActive()
{
	return m_bEmit;
}


#endif /* _DEFINED_ASW_GENERIC_EMITTER_H */