//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clones a physics object by use of shadows
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICSSHADOWCLONE_H
#define PHYSICSSHADOWCLONE_H

#ifdef _WIN32
#pragma once
#endif

#include "vphysics_interface.h"
#include "baseentity.h"
#include "baseanimating.h"

class CPhysicsShadowClone;

struct PhysicsObjectCloneLink_t
{
	IPhysicsObject *pSource;
	IPhysicsShadowController *pShadowController;
	IPhysicsObject *pClone;
};

struct CPhysicsShadowCloneLL
{
	CPhysicsShadowClone *pClone;
	CPhysicsShadowCloneLL *pNext;
};

#define FVPHYSICS_IS_SHADOWCLONE 0x4000

class CPhysicsShadowClone : public CBaseAnimating
{
	DECLARE_CLASS( CPhysicsShadowClone, CBaseAnimating );

private:
	EHANDLE			m_hClonedEntity; //the entity we're supposed to be cloning the physics of
	VMatrix			m_matrixShadowTransform; //all cloned coordinates and angles will be run through this matrix before being applied
	VMatrix			m_matrixShadowTransform_Inverse;
	
	CUtlVector<PhysicsObjectCloneLink_t> m_CloneLinks; //keeps track of which of our physics objects are linked to the source's objects
	bool			m_bShadowTransformIsIdentity; //the shadow transform doesn't update often, so we can cache this
	bool			m_bImmovable; //cloning a track train or door, something that doesn't really work on a force-based level
	bool			m_bInAssumedSyncState;

	void			FullSyncClonedPhysicsObjects( bool bTeleport );
	void			SyncEntity( bool bPullChanges );

	IPhysicsEnvironment *m_pOwnerPhysEnvironment; //clones exist because of multi-environment situations


public:
	CPhysicsShadowClone( void );
	virtual ~CPhysicsShadowClone( void );
	
	bool			m_bShouldUpSync;
	DBG_CODE_NOSCOPE( const char *m_szDebugMarker; );

	//do the thing with the stuff, you know, the one that goes WooooWooooWooooWooooWoooo
	virtual void	Spawn( void );

	//crush, kill, DESTROY!!!!!
	void			Free( void );

	//syncs to the source entity in every way possible, assumed sync does some rudimentary tests to see if the object is in sync, and if so, skips the update
	void			FullSync( bool bAllowAssumedSync = false );

	//syncs just the physics objects, bPullChanges should be true when this clone should match it's source, false when it should force differences onto the source entity
	void			PartialSync( bool bPullChanges );

	//virtual bool CreateVPhysics( void );
	virtual void	VPhysicsDestroyObject( void );
	virtual int		VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	virtual int		ObjectCaps( void );
	virtual void	UpdateOnRemove( void );



	//routing to the source entity for cloning goodness
	virtual	bool	ShouldCollide( int collisionGroup, int contentsMask ) const;

	//avoid blocking traces that are supposed to hit our source entity
	virtual bool	TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );




	//is this clone occupying the exact same space as the object it's cloning?
	inline bool		IsUntransformedClone( void ) const { return m_bShadowTransformIsIdentity; };
	void			SetCloneTransformationMatrix( const matrix3x4_t &matTransform );

	inline bool		IsInAssumedSyncState( void ) const { return m_bInAssumedSyncState; }
	inline IPhysicsEnvironment *GetOwnerEnvironment( void ) const { return m_pOwnerPhysEnvironment; }

	//what entity are we cloning?
	void			SetClonedEntity( EHANDLE hEntToClone );
	EHANDLE			GetClonedEntity( void );


	virtual void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

	//damage relays to source entity if anything ever hits the clone
	virtual bool	PassesDamageFilter( const CTakeDamageInfo &info );
	virtual bool	CanBeHitByMeleeAttack( CBaseEntity *pAttacker );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual int		TakeHealth( float flHealth, int bitsDamageType );
	virtual void	Event_Killed( const CTakeDamageInfo &info );


	static CPhysicsShadowClone *CreateShadowClone( IPhysicsEnvironment *pInPhysicsEnvironment, EHANDLE hEntToClone, const char *szDebugMarker, const matrix3x4_t *pTransformationMatrix = NULL );

	//given a physics object that is part of this clone, tells you which physics object in the source
	IPhysicsObject *TranslatePhysicsToClonedEnt( const IPhysicsObject *pPhysics );

	static bool IsShadowClone( const CBaseEntity *pEntity );
	static CPhysicsShadowCloneLL *GetClonesOfEntity( const CBaseEntity *pEntity );
	static void FullSyncAllClones( void );

	static CUtlVector<CPhysicsShadowClone *> const &g_ShadowCloneList;
};



class CTraceFilterTranslateClones : public CTraceFilter //give it another filter, and it'll translate shadow clones into their source entity for tests
{
	ITraceFilter *m_pActualFilter; //the filter that tests should be forwarded to after translating clones

public:
	CTraceFilterTranslateClones( ITraceFilter *pOtherFilter ) : m_pActualFilter(pOtherFilter) {};
	virtual bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask );
	virtual TraceType_t	GetTraceType() const;
};

#endif //#ifndef PHYSICSSHADOWCLONE_H
