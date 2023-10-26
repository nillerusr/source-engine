//===== Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: provide client-side access to the new particle system, with similar
// usage to CSimpleEmitter
//
// $NoKeywords: $
//===========================================================================//

#ifndef PARTICLES_NEW_H
#define PARTICLES_NEW_H
#ifdef _WIN32
#pragma once
#endif

#include "particlemgr.h"
#include "particles/particles.h"
#include "ParticleSphereRenderer.h"
#include "smartptr.h"
#include "particles_simple.h"
#include "tier1/utlobjectreference.h"


//-----------------------------------------------------------------------------
// Particle effect
//-----------------------------------------------------------------------------
class CNewParticleEffect : public IParticleEffect, public CParticleCollection, public CDefaultClientRenderable
{
public:
	DECLARE_CLASS_NOBASE( CNewParticleEffect );
	DECLARE_REFERENCED_CLASS( CNewParticleEffect );

public:
	friend class CRefCountAccessor;
	friend class CParticleSystemQuery;

	// list management
	CNewParticleEffect *m_pNext;
	CNewParticleEffect *m_pPrev;

	// Call this before adding a bunch of particles to give it a rough estimate of where
	// your particles are for sorting amongst other translucent entities.
	void SetSortOrigin( const Vector &vSortOrigin );
	bool ShouldDraw( void );
	virtual int GetRenderFlags( void );
	const QAngle& GetRenderAngles( void );
	const matrix3x4_t& RenderableToWorldTransform();
	void GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool	ShouldDrawForSplitScreenUser( int nSlot );

	// check if the new bounds of the particle system needs its client-leaf info needs to be updated
	void DetectChanges( void );
	const Vector &GetRenderOrigin( void );
	PMaterialHandle GetPMaterial( const char *name );
	bool RecalculateBoundingBox();
	Particle* AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin );
	const char *GetEffectName();
	void SetDontRemove( bool bSet );
	void SetDrawn( bool bDrawn );
	void SetFirstFrameFlag( bool bFirst );
	void SetNeedsBBoxUpdate( bool bNeedsUpdate );
	void SetAutoUpdateBBox( bool bNeedsUpdate );
	void SetRemoveFlag( void );
	bool GetRemoveFlag( void );
	bool GetFirstFrameFlag( void );
	bool GetNeedsBBoxUpdate( void );
	bool GetAutoUpdateBBox( void );
	bool ShouldPerformCullCheck() const;
	void MarkShouldPerformCullCheck( bool bEnable );
	CBaseEntity *GetOwner( void ) { return m_hOwner; }
	void SetOwner( CBaseEntity *pOwner ) { m_hOwner = pOwner; }
	CNewParticleEffect* ReplaceWith( const char *pParticleSystemName );

	static CNewParticleEffect *Create( CBaseEntity *pOwner, const char *pParticleSystemName,
												 const char *pDebugName = NULL );
	static CNewParticleEffect *Create( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
												 const char *pDebugName = NULL );
	static CNewParticleEffect *CreatePrecached( CBaseEntity *pOwner, int nPrecacheIndex,
												 const char *pDebugName = NULL );	
	static CNewParticleEffect *CreateOrAggregate( CBaseEntity *pOwner, const char *pParticleSystemName,
												  Vector const &vecAggregatePosition, const char *pDebugName = NULL, 
												  int nSplitScreenUser = -1 );
	static CNewParticleEffect *CreateOrAggregate( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
												  Vector const &vecAggregatePosition, const char *pDebugName = NULL,
												  int nSplitScreenUser = -1 );
	static CNewParticleEffect *CreateOrAggregatePrecached( CBaseEntity *pOwner, int nPrecacheIndex,
														   Vector const &vecAggregatePosition,
														   const char *pDebugName = NULL,
														   int nSplitScreenUser = -1 );

	static void RemoveParticleEffect( int nPrecacheIndex );

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	void SetDrawOnlyForSplitScreenUser( int nSlot );

	// CParticleCollection overrides
public:
	void StopEmission( bool bInfiniteOnly = false, bool bRemoveAllParticles = false, bool bWakeOnStop = false, bool bPlayEndCap = false );
	void SetDormant( bool bDormant );
	void SetControlPoint( int nWhichPoint, const Vector &v );
	void SetControlPointEntity( int nWhichPoint, CBaseEntity *pEntity );
	void SetControlPointOrientation( int nWhichPoint, const Quaternion &q );
	void SetControlPointOrientation( int nWhichPoint, const Vector &forward, const Vector &right, const Vector &up );
	void SetControlPointForwardVector( int nWhichPoint, const Vector &v );
	void SetControlPointUpVector( int nWhichPoint, const Vector &v );
	void SetControlPointRightVector( int nWhichPoint, const Vector &v );

	FORCEINLINE EHANDLE const &GetControlPointEntity( int nWhichPoint )
	{
		return m_hControlPointOwners[ nWhichPoint ];
	}


// IParticleEffect overrides
public:

	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator )
	{
	}
	virtual void	RenderParticles( CParticleRenderIterator *pIterator )
	{
	}		

	virtual void				SetParticleCullRadius( float radius );
	virtual void				NotifyRemove( void );
	virtual const Vector &		GetSortOrigin( void );

//	virtual void				NotifyDestroyParticle( Particle* pParticle );
	virtual void				Update( float flTimeDelta );

	// All Create() functions should call this so the effect deletes itself
	// when it is removed from the particle manager.
	void SetDynamicallyAllocated( bool bDynamic = true );

	virtual bool				ShouldSimulate() const { return m_bSimulate; }
	virtual void				SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	void SetToolRecording( bool bRecord );

	int AllocateToolParticleEffectId();
	int GetToolParticleEffectId() const;

	virtual ~CNewParticleEffect();

protected:
	CNewParticleEffect( CBaseEntity *pOwner, const char *pEffectName );
	CNewParticleEffect( CBaseEntity *pOwner, CParticleSystemDefinition *pEffect );
	CNewParticleEffect( CBaseEntity *pOwner, int nPrecacheIndex );

	// Returns nonzero if Release() has been called.
	int		IsReleased();
	
	// Used to track down bugs.
	const char	*m_pDebugName;

	bool		m_bDontRemove : 1;
	bool		m_bRemove : 1;
	bool		m_bDrawn : 1;
	bool		m_bNeedsBBoxUpdate : 1;
	bool		m_bIsFirstFrame : 1;
	bool		m_bAutoUpdateBBox : 1;
	bool		m_bAllocated : 1;
	bool		m_bSimulate : 1;
	bool		m_bRecord : 1;
	bool		m_bShouldPerformCullCheck : 1;

	// if a particle system is created through the non-aggregation entry point, we can't aggregate into it
	bool        m_bDisableAggregation : 1;

	
	
	int			m_nToolParticleEffectId;
	Vector		m_vSortOrigin;
	EHANDLE		m_hOwner;
	EHANDLE     m_hControlPointOwners[MAX_PARTICLE_CONTROL_POINTS];

	// holds the min/max bounds used to manage this thing in the client leaf system
	Vector		m_LastMin;
	Vector		m_LastMax;

	int			m_nSplitScreenUser; // -1 means don't care
	Vector m_vecAggregationCenter;							// origin for aggregation if aggregation enabled


private:
	// Update the reference count.
	void		AddRef();
	void		Release();
	void		RecordControlPointOrientation( int nWhichPoint );
	void		Construct();
	void		RecordCreation();
	
	int			m_RefCount;		// When this goes to zero and the effect has no more active
								// particles, (and it's dynamically allocated), it will delete itself.

	matrix3x4a_t m_DrawModelMatrix;
	
	CNewParticleEffect( const CNewParticleEffect & ); // not defined, not accessible
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

inline int CNewParticleEffect::GetToolParticleEffectId() const
{
	return m_nToolParticleEffectId;
}

inline int CNewParticleEffect::AllocateToolParticleEffectId()
{
	m_nToolParticleEffectId = ParticleMgr()->AllocateToolParticleEffectId();
	return m_nToolParticleEffectId;
}

// Call this before adding a bunch of particles to give it a rough estimate of where
// your particles are for sorting amongst other translucent entities.
inline void CNewParticleEffect::SetSortOrigin( const Vector &vSortOrigin )
{
	m_vSortOrigin = vSortOrigin;
}

inline const Vector &CNewParticleEffect::GetSortOrigin( void )
{
	return m_vSortOrigin;
}

inline bool CNewParticleEffect::ShouldDraw( void )
{
	return m_bDrawn;
}

inline const QAngle& CNewParticleEffect::GetRenderAngles( void )
{
	return vec3_angle;
}

inline const matrix3x4_t &	CNewParticleEffect::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix( mat );
	PositionMatrix( GetRenderOrigin(), mat );
	return mat;
}

inline Vector const &CNewParticleEffect::GetRenderOrigin( void )
{
	return m_vSortOrigin;
}

inline PMaterialHandle CNewParticleEffect::GetPMaterial(const char *name)
{
	//!!
	Assert( 0 );
	return NULL;
}

inline Particle* CNewParticleEffect::AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin )
{
	//!!
	Assert( 0 );
	return NULL;
}


inline const char *CNewParticleEffect::GetEffectName()
{
	return GetName();
}

inline void CNewParticleEffect::SetDontRemove( bool bSet )
{
	m_bDontRemove = bSet;
}

inline void CNewParticleEffect::SetDrawn( bool bDrawn )
{
	m_bDrawn = bDrawn;
}

inline void CNewParticleEffect::SetFirstFrameFlag( bool bFirst )
{
	m_bIsFirstFrame = bFirst;
}

inline void CNewParticleEffect::SetDynamicallyAllocated( bool bDynamic )
{
	m_bAllocated = bDynamic;
}

inline void CNewParticleEffect::SetNeedsBBoxUpdate( bool bNeedsUpdate )
{
	m_bNeedsBBoxUpdate = bNeedsUpdate;
}

inline void CNewParticleEffect::SetAutoUpdateBBox( bool bNeedsUpdate )
{
	m_bAutoUpdateBBox = bNeedsUpdate;
}

inline void CNewParticleEffect::SetRemoveFlag( void )
{
	m_bRemove = true;
}

inline bool CNewParticleEffect::GetRemoveFlag( void )
{
	return m_bRemove;
}

inline bool CNewParticleEffect::GetFirstFrameFlag( void )
{
	return m_bIsFirstFrame;
}

inline bool CNewParticleEffect::GetNeedsBBoxUpdate( void )
{
	return m_bNeedsBBoxUpdate;
}

inline bool CNewParticleEffect::GetAutoUpdateBBox( void )
{
	return m_bAutoUpdateBBox;
}

inline bool CNewParticleEffect::ShouldPerformCullCheck() const
{
	return m_bShouldPerformCullCheck;
}

inline void CNewParticleEffect::MarkShouldPerformCullCheck( bool bEnable )
{
	m_bShouldPerformCullCheck = bEnable;
}

inline CNewParticleEffect *CNewParticleEffect::Create( CBaseEntity *pOwner, const char *pParticleSystemName, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, pParticleSystemName );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

inline CNewParticleEffect *CNewParticleEffect::Create( CBaseEntity *pOwner, CParticleSystemDefinition *pDef, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, pDef );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

inline CNewParticleEffect *CNewParticleEffect::CreatePrecached( CBaseEntity *pOwner, int nPrecacheIndex, const char *pDebugName )
{
	CNewParticleEffect *pRet = new CNewParticleEffect( pOwner, nPrecacheIndex );
	pRet->m_pDebugName = pDebugName;
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}


//--------------------------------------------------------------------------------
// If you use an HPARTICLEFFECT instead of a cnewparticleeffect *, you get a pointer
// which will go to null when the effect is deleted
//--------------------------------------------------------------------------------
typedef CUtlReference<CNewParticleEffect> HPARTICLEFFECT;


#endif // PARTICLES_NEW_H
