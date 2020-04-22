//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides structures and classes necessary to simulate a portal.
//
// $NoKeywords: $
//=====================================================================================//

#ifndef PORTALSIMULATION_H
#define PORTALSIMULATION_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/polyhedron.h"
#include "const.h"
#include "tier1/utlmap.h"
#include "tier1/utlvector.h"

#define PORTAL_SIMULATORS_EMBED_GUID //define this to embed a unique integer with each portal simulator for debugging purposes

struct StaticPropPolyhedronGroups_t //each static prop is made up of a group of polyhedrons, these help us pull those groups from an array
{
	int iStartIndex;
	int iNumPolyhedrons;
};

enum PortalSimulationEntityFlags_t
{
	PSEF_OWNS_ENTITY = (1 << 0), //this environment is responsible for the entity's physics objects
	PSEF_OWNS_PHYSICS = (1 << 1),
	PSEF_IS_IN_PORTAL_HOLE = (1 << 2), //updated per-phyframe
	PSEF_CLONES_ENTITY_FROM_MAIN = (1 << 3), //entity is close enough to the portal to affect objects intersecting the portal
	//PSEF_HAS_LINKED_CLONE = (1 << 1), //this environment has a clone of the entity which is transformed from its linked portal
};

enum PS_PhysicsObjectSourceType_t
{
	PSPOST_LOCAL_BRUSHES,
	PSPOST_REMOTE_BRUSHES,
	PSPOST_LOCAL_STATICPROPS,
	PSPOST_REMOTE_STATICPROPS,
	PSPOST_HOLYWALL_TUBE
};

struct PortalTransformAsAngledPosition_t //a matrix transformation from this portal to the linked portal, stored as vector and angle transforms
{
	Vector ptOriginTransform;
	QAngle qAngleTransform;
};

inline bool LessFunc_Integer( const int &a, const int &b ) { return a < b; };


class CPortalSimulatorEventCallbacks //sends out notifications of events to game specific code
{
public:
	virtual void PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity ) { };
	virtual void PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity ) { };

	virtual void PortalSimulator_TookPhysicsOwnershipOfEntity( CBaseEntity *pEntity ) { };
	virtual void PortalSimulator_ReleasedPhysicsOwnershipOfEntity( CBaseEntity *pEntity ) { };
};

//====================================================================================
// To any coder trying to understand the following nested structures....
//
// You may be wondering... why? wtf?
//
// The answer. The previous incarnation of server side portal simulation suffered
// terribly from evolving variables with increasingly cryptic names with no clear
// definition of what part of the system the variable was involved with.
//
// It's my hope that a nested structure with clear boundaries will eliminate that 
// horrible, awful, nasty, frustrating confusion. (It was really really bad). This
// system has the added benefit of pseudo-forcing a naming structure.
//
// Lastly, if it all roots in one struct, we can const reference it out to allow 
// easy reads without writes
//
// It's broken out like this to solve a few problems....
// 1. It cleans up intellisense when you don't actually define a structure
//		within a structure.
// 2. Shorter typenames when you want to have a pointer/reference deep within
//		the nested structure.
// 3. Needed at least one level removed from CPortalSimulator so
//		pointers/references could be made while the primary instance of the
//		data was private/protected.
//
// It may be slightly difficult to understand in it's broken out structure, but
// intellisense brings all the data together in a very cohesive manner for
// working with.
//====================================================================================

struct PS_PlacementData_t //stuff useful for geometric operations
{
	Vector ptCenter;
	QAngle qAngles;
	Vector vForward;
	Vector vUp;
	Vector vRight;
	VPlane PortalPlane;
	VMatrix matThisToLinked;
	VMatrix matLinkedToThis;
	PortalTransformAsAngledPosition_t ptaap_ThisToLinked;
	PortalTransformAsAngledPosition_t ptaap_LinkedToThis;
	CPhysCollide *pHoleShapeCollideable; //used to test if a collideable is in the hole, should NOT be collided against in general
	PS_PlacementData_t( void )
	{
		memset( this, 0, sizeof( PS_PlacementData_t ) );
	}
};

struct PS_SD_Static_World_Brushes_t
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CPhysCollide *pCollideable;
#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_World_Brushes_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_World_Brushes_t() : pCollideable(NULL) {};
#endif
	
};


struct PS_SD_Static_World_StaticProps_ClippedProp_t
{
	StaticPropPolyhedronGroups_t	PolyhedronGroup;
	CPhysCollide *					pCollide;
#ifndef CLIENT_DLL
	IPhysicsObject *				pPhysicsObject;
#endif
	IHandleEntity *					pSourceProp;

	int								iTraceContents;
	short							iTraceSurfaceProps;
	static CBaseEntity *			pTraceEntity;
	static const char *				szTraceSurfaceName; //same for all static props, here just for easy reference
	static const int				iTraceSurfaceFlags; //same for all static props, here just for easy reference
};

struct PS_SD_Static_World_StaticProps_t
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CUtlVector<PS_SD_Static_World_StaticProps_ClippedProp_t> ClippedRepresentations;
	bool bCollisionExists; //the shortcut to know if collideables exist for each prop
	bool bPhysicsExists; //the shortcut to know if physics obects exist for each prop
	PS_SD_Static_World_StaticProps_t( void ) : bCollisionExists( false ), bPhysicsExists( false ) { };
};

struct PS_SD_Static_World_t //stuff in front of the portal
{
	PS_SD_Static_World_Brushes_t Brushes;
	PS_SD_Static_World_StaticProps_t StaticProps;
};

struct PS_SD_Static_Wall_Local_Tube_t //a minimal tube, an object must fit inside this to be eligible for portaling
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CPhysCollide *pCollideable;

#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_Wall_Local_Tube_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_Wall_Local_Tube_t() : pCollideable(NULL) {};
#endif
};

struct PS_SD_Static_Wall_Local_Brushes_t 
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CPhysCollide *pCollideable;

#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_Wall_Local_Brushes_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_Wall_Local_Brushes_t() : pCollideable(NULL) {};
#endif
};

struct PS_SD_Static_Wall_Local_t //things in the wall that are completely independant of having a linked portal
{
	PS_SD_Static_Wall_Local_Tube_t Tube;
	PS_SD_Static_Wall_Local_Brushes_t Brushes;
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t
{
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t() : pPhysicsObject(NULL) {};
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_StaticProps_t
{
	CUtlVector<IPhysicsObject *> PhysicsObjects;
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_t //things taken from the linked portal's "World" collision and transformed into local space
{
	PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t Brushes;
	PS_SD_Static_Wall_RemoteTransformedToLocal_StaticProps_t StaticProps;
};

struct PS_SD_Static_Wall_t //stuff behind the portal
{
	PS_SD_Static_Wall_Local_t Local;
#ifndef CLIENT_DLL
	PS_SD_Static_Wall_RemoteTransformedToLocal_t RemoteTransformedToLocal;
#endif
};

struct PS_SD_Static_SurfaceProperties_t //surface properties to pretend every collideable here is using
{
	int contents;
	csurface_t surface;
	CBaseEntity *pEntity;
};

struct PS_SD_Static_t //stuff that doesn't move around
{
	PS_SD_Static_World_t World;
	PS_SD_Static_Wall_t Wall;
	PS_SD_Static_SurfaceProperties_t SurfaceProperties;
};

class CPhysicsShadowClone;

struct PS_SD_Dynamic_PhysicsShadowClones_t
{
	CUtlVector<CBaseEntity *> ShouldCloneFromMain; //a list of entities that should be cloned from main if physics simulation is enabled
													//in single-environment mode, this helps us track who should collide with who
	
	CUtlVector<CPhysicsShadowClone *> FromLinkedPortal;
};

struct PS_SD_Dynamic_t //stuff that moves around
{
	unsigned int EntFlags[MAX_EDICTS]; //flags maintained for every entity in the world based on its index

	PS_SD_Dynamic_PhysicsShadowClones_t ShadowClones;

	CUtlVector<CBaseEntity *> OwnedEntities;

	PS_SD_Dynamic_t()
	{
		memset( EntFlags, 0, sizeof( EntFlags ) );
	}
};

class CPSCollisionEntity;

struct PS_SimulationData_t //compartmentalized data for coherent management
{
	PS_SD_Static_t Static;

#ifndef CLIENT_DLL
	PS_SD_Dynamic_t Dynamic;

	IPhysicsEnvironment *pPhysicsEnvironment;
	CPSCollisionEntity *pCollisionEntity; //the entity we'll be tying physics objects to for collision

	PS_SimulationData_t() : pPhysicsEnvironment(NULL), pCollisionEntity(NULL) {};
#endif
};

struct PS_InternalData_t
{
	PS_PlacementData_t Placement;
	PS_SimulationData_t Simulation;
};


class CPortalSimulator
{
public:
	CPortalSimulator( void );
	~CPortalSimulator( void );

	void				MoveTo( const Vector &ptCenter, const QAngle &angles );
	void				ClearEverything( void );

	void				AttachTo( CPortalSimulator *pLinkedPortalSimulator );
	void				DetachFromLinked( void ); //detach portals to sever the connection, saves work when planning on moving both portals
	CPortalSimulator	*GetLinkedPortalSimulator( void ) const;

	void				SetPortalSimulatorCallbacks( CPortalSimulatorEventCallbacks *pCallbacks );
	
	bool				IsReadyToSimulate( void ) const; //is active and linked to another portal
	
	void				SetCollisionGenerationEnabled( bool bEnabled ); //enable/disable collision generation for the hole in the wall, needed for proper vphysics simulation
	bool				IsCollisionGenerationEnabled( void ) const;

	void				SetVPhysicsSimulationEnabled( bool bEnabled ); //enable/disable vphysics simulation. Will automatically update the linked portal to be the same
	bool				IsSimulatingVPhysics( void ) const; //this portal is setup to handle any physically simulated object, false means the portal is handling player movement only
	
	bool				EntityIsInPortalHole( CBaseEntity *pEntity ) const; //true if the entity is within the portal cutout bounds and crossing the plane. Not just *near* the portal
	bool				EntityHitBoxExtentIsInPortalHole( CBaseAnimating *pBaseAnimating ) const; //true if the entity is within the portal cutout bounds and crossing the plane. Not just *near* the portal
	void				RemoveEntityFromPortalHole( CBaseEntity *pEntity ); //if the entity is in the portal hole, this forcibly moves it out by any means possible

	bool				RayIsInPortalHole( const Ray_t &ray ) const; //traces a ray against the same detector for EntityIsInPortalHole(), bias is towards false positives

#ifndef CLIENT_DLL
	int				GetMoveableOwnedEntities( CBaseEntity **pEntsOut, int iEntOutLimit ); //gets owned entities that aren't either world or static props. Excludes fake portal ents such as physics clones

	static CPortalSimulator *GetSimulatorThatOwnsEntity( const CBaseEntity *pEntity ); //fairly cheap to call
	static CPortalSimulator *GetSimulatorThatCreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType = NULL );
	static void			Pre_UTIL_Remove( CBaseEntity *pEntity );
	static void			Post_UTIL_Remove( CBaseEntity *pEntity );

	//these three really should be made internal and the public interface changed to a "watch this entity" setup
	void				TakeOwnershipOfEntity( CBaseEntity *pEntity ); //general ownership, not necessarily physics ownership
	void				ReleaseOwnershipOfEntity( CBaseEntity *pEntity, bool bMovingToLinkedSimulator = false ); //if bMovingToLinkedSimulator is true, the code skips some steps that are going to be repeated when the entity is added to the other simulator
	void				ReleaseAllEntityOwnership( void ); //go back to not owning any entities

	//void				TeleportEntityToLinkedPortal( CBaseEntity *pEntity );
	void				StartCloningEntity( CBaseEntity *pEntity );
	void				StopCloningEntity( CBaseEntity *pEntity );

	bool				OwnsEntity( const CBaseEntity *pEntity ) const;
	bool				OwnsPhysicsForEntity( const CBaseEntity *pEntity ) const;

	bool				CreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType = NULL ) const; //true if the physics object was generated by this portal simulator

	static void			PrePhysFrame( void );
	static void			PostPhysFrame( void );

#endif //#ifndef CLIENT_DLL

#ifdef PORTAL_SIMULATORS_EMBED_GUID
	int					GetPortalSimulatorGUID( void ) const { return m_iPortalSimulatorGUID; };
#endif

protected:
	bool				m_bLocalDataIsReady; //this side of the portal is properly setup, no guarantees as to linkage to another portal
	bool				m_bSimulateVPhysics;
	bool				m_bGenerateCollision;
	bool				m_bSharedCollisionConfiguration; //when portals are in certain configurations, they need to cross-clip and share some collision data and things get nasty. For the love of all that is holy, pray that this is false.
	CPortalSimulator	*m_pLinkedPortal;
	bool				m_bInCrossLinkedFunction; //A flag to mark that we're already in a linked function and that the linked portal shouldn't call our side
	CPortalSimulatorEventCallbacks *m_pCallbacks; 
#ifdef PORTAL_SIMULATORS_EMBED_GUID
	int					m_iPortalSimulatorGUID;
#endif

	struct
	{
		bool			bPolyhedronsGenerated;
		bool			bLocalCollisionGenerated;
		bool			bLinkedCollisionGenerated;
		bool			bLocalPhysicsGenerated;
		bool			bLinkedPhysicsGenerated;
	} m_CreationChecklist;

	friend class CPSCollisionEntity;

#ifndef CLIENT_DLL //physics handled purely by server side
	void				TakePhysicsOwnership( CBaseEntity *pEntity );
	void				ReleasePhysicsOwnership( CBaseEntity *pEntity, bool bContinuePhysicsCloning = true, bool bMovingToLinkedSimulator = false );

	void				CreateAllPhysics( void );
	void				CreateMinimumPhysics( void ); //stuff needed by any part of physics simulations
	void				CreateLocalPhysics( void );
	void				CreateLinkedPhysics( void );

	void				ClearAllPhysics( void );
	void				ClearMinimumPhysics( void );
	void				ClearLocalPhysics( void );
	void				ClearLinkedPhysics( void );

	void				ClearLinkedEntities( void ); //gets rid of transformed shadow clones
#endif

	void				CreateAllCollision( void );
	void				CreateLocalCollision( void );
	void				CreateLinkedCollision( void );

	void				ClearAllCollision( void );
	void				ClearLinkedCollision( void );
	void				ClearLocalCollision( void );

	void				CreatePolyhedrons( void ); //carves up the world around the portal's position into sets of polyhedrons
	void				ClearPolyhedrons( void );

	void				UpdateLinkMatrix( void );

	void				MarkAsOwned( CBaseEntity *pEntity );
	void				MarkAsReleased( CBaseEntity *pEntity );

	PS_InternalData_t m_InternalData;

public:
	const PS_InternalData_t &m_DataAccess;

	friend class CPS_AutoGameSys_EntityListener;
};

extern CUtlVector<CPortalSimulator *> const &g_PortalSimulators;


#ifndef CLIENT_DLL
class CPSCollisionEntity : public CBaseEntity
{
	DECLARE_CLASS( CPSCollisionEntity, CBaseEntity );
private:
	CPortalSimulator *m_pOwningSimulator;

public:
	CPSCollisionEntity( void );
	virtual ~CPSCollisionEntity( void );

	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual int		ObjectCaps( void );
	virtual IPhysicsObject *VPhysicsGetObject( void );
	virtual int		VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	virtual void	UpdateOnRemove( void );
	virtual	bool	ShouldCollide( int collisionGroup, int contentsMask ) const;
	virtual void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent ) {}
	virtual void	VPhysicsFriction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ) {}

	static bool		IsPortalSimulatorCollisionEntity( const CBaseEntity *pEntity );
	friend class CPortalSimulator;
};
#endif
















#ifndef CLIENT_DLL
inline bool CPortalSimulator::OwnsEntity( const CBaseEntity *pEntity ) const
{
	return ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_OWNS_ENTITY) != 0);
}

inline bool CPortalSimulator::OwnsPhysicsForEntity( const CBaseEntity *pEntity ) const
{
	return ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_OWNS_PHYSICS) != 0);
}
#endif

inline bool CPortalSimulator::IsReadyToSimulate( void ) const
{
	return m_bLocalDataIsReady && m_pLinkedPortal && m_pLinkedPortal->m_bLocalDataIsReady;
}

inline bool CPortalSimulator::IsSimulatingVPhysics( void ) const
{
	return m_bSimulateVPhysics;
}

inline bool CPortalSimulator::IsCollisionGenerationEnabled( void ) const
{
	return m_bGenerateCollision;
}

inline CPortalSimulator	*CPortalSimulator::GetLinkedPortalSimulator( void ) const
{
	return m_pLinkedPortal;
}




#endif //#ifndef PORTALSIMULATION_H

