 //===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utilities for setting vproject settings
//
//===========================================================================//

#ifndef _RESOURCEPRECACHER_H
#define _RESOURCEPRECACHER_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Resource list
//-----------------------------------------------------------------------------
FORWARD_DECLARE_HANDLE( ResourceList_t );
#define RESOURCE_LIST_INVALID ( (ResourceList_t)-1 )

//-----------------------------------------------------------------------------
// Resource 'systems', which use other resources
// NOTE: If you add types here, be sure to fix s_pResourceSystemName
//-----------------------------------------------------------------------------
enum PrecacheSystem_t
{
	CLIENTGLOBAL = 0,			// Always precache these
	SERVERGLOBAL,
	VGUI_PANEL,			// What to precache when using a vgui panel
	DISPATCH_EFFECT,	// What to precache when using a dispatch effect
	SHARED_SYSTEM,		// Precache lists which are reused and can be referenced as a resource type

	PRECACHE_SYSTEM_COUNT,

#if defined( GAME_DLL )	
	GLOBAL = SERVERGLOBAL,
#elif defined( CLIENT_DLL ) || defined( GAMEUI_EXPORTS )
	GLOBAL = CLIENTGLOBAL,
#endif
};

//-----------------------------------------------------------------------------
// Resource types
// NOTE: If you add a type here, modify s_pResourceTypeName in resourceaccesscontrol.cpp
//-----------------------------------------------------------------------------
enum ResourceTypeOld_t	// called 'Old' to disambiguate with ResourceSystem
{
	RESOURCE_VGUI_PANEL = 0,	// .res file
	RESOURCE_MATERIAL,			// .vmt file
	RESOURCE_MODEL,				// .mdl file
	RESOURCE_PARTICLE_SYSTEM,	// particle system
	RESOURCE_GAMESOUND,			// game sound

	RESOURCE_TYPE_OLD_COUNT,
};


//-----------------------------------------------------------------------------
// Resource types
// NOTE: If you add types here, be sure to fix s_pPrecacheResourceTypeName
// A compile-time assert will trigger if you don't.
//-----------------------------------------------------------------------------
enum PrecacheResourceType_t
{
	VGUI_RESOURCE = 0,	// .res file
	MATERIAL,			// .vmt file
	MODEL,				// .mdl file
	GAMESOUND,			// sound
	PARTICLE_SYSTEM,	// particle system
	ENTITY,				// Other entity
	DECAL,				// A decal
	PARTICLE_MATERIAL,	// A particle system material (old-style, obsolete)
	KV_DEP_FILE,		// keyvalues file containing a resource dependency list
	GAME_MATERIAL_DECALS,	// All decals related to game materials ( resource name is ignored )
	PHYSICS_GAMESOUNDS,	// Resource names are either "BulletSounds", "StepSounds", or "PhysicsImpactSounds"
	SHARED,				// a shared precache group (see PrecacheSystem_t SHARED)

	PRECACHE_RESOURCE_TYPE_COUNT,
};

//-----------------------------------------------------------------------------
// Callback interface for handler who knows how to precache particular kinds of resources
//-----------------------------------------------------------------------------
abstract_class IPrecacheHandler
{
public:
	virtual void CacheResource( PrecacheResourceType_t nType, const char *pName, 
		bool bPrecache, ResourceList_t hResourceList, int *pIndex = NULL ) = 0;
};

//-----------------------------------------------------------------------------
// Interface to automated system for precaching resources
//-----------------------------------------------------------------------------
abstract_class IResourcePrecacher
{
public:
	virtual void Cache( IPrecacheHandler *pPrecacheHandler, bool bPrecache, ResourceList_t hResourceList, bool bIgnoreConditionals ) = 0;	
	virtual PrecacheSystem_t GetSystem() = 0;
	virtual const char *GetName() = 0;
	virtual IResourcePrecacher *GetNext() = 0;
	virtual void SetNext( IResourcePrecacher * pNext ) = 0;
};

//-----------------------------------------------------------------------------
// Actually does the precaching
//-----------------------------------------------------------------------------
class CBaseResourcePrecacher : public IResourcePrecacher
{
	// Other public methods
public:
	CBaseResourcePrecacher( PrecacheSystem_t nSystem, const char *pName )
	{
		m_nSystem = nSystem;
		m_pName = pName;
		m_pNext = sm_pFirst[nSystem];
		sm_pFirst[nSystem] = this;
	}

	static void RegisterAll();

	PrecacheSystem_t GetSystem() { return m_nSystem; }
	const char *GetName() { return m_pName; }
	IResourcePrecacher *GetNext() { return m_pNext; }
	void SetNext( IResourcePrecacher * pNext ) { m_pNext = pNext; }

	static CBaseResourcePrecacher *sm_pFirst[PRECACHE_SYSTEM_COUNT];

	PrecacheSystem_t m_nSystem;
	const char *m_pName;
	IResourcePrecacher *m_pNext;	

	friend class CPrecacheRegister;
};


//-----------------------------------------------------------------------------
// Automatic precache macros
//-----------------------------------------------------------------------------

// Beginning
#define	PRECACHE_REGISTER_BEGIN_CONDITIONAL( _system, _className, _condition )	\
	namespace _className ## Precache						\
{														\
class CResourcePrecacher : public CBaseResourcePrecacher\
{														\
public:													\
	CResourcePrecacher() : CBaseResourcePrecacher( _system, #_className ) {}	\
public:													\
	virtual void Cache( IPrecacheHandler *pPrecacheHandler, bool bPrecache, ResourceList_t hResourceList, bool bIgnoreConditionals );	\
};														\
	void CResourcePrecacher::Cache( IPrecacheHandler *pPrecacheHandler, bool bPrecache, ResourceList_t hResourceList, bool bIgnoreConditionals )	\
{														\
	if ( !bIgnoreConditionals && !( _condition ) )		\
	return;

#define	PRECACHE_REGISTER_BEGIN( _system, _className )		\
	PRECACHE_REGISTER_BEGIN_CONDITIONAL( _system, _className, true )

// Resource precache definitions
#define	PRECACHE( _type, _name )				pPrecacheHandler->CacheResource( _type, _name, bPrecache, hResourceList, NULL ); 

// NOTE: PRECACHE_INDEX_CONDITIONAL doesn't initialize the index to 0
// on the assumption that some other conditional will

//MCCLEANUP //NOTE: PRECACHE_INDEX and PRECACHE_INDEX_CONDITIONAL won't work in 64 bit because the old-school particle mgr is sending ptr data types into here. Hopefully the old-school particle mgr will die before this is an issue.

#define	PRECACHE_INDEX( _type, _name, _index )	pPrecacheHandler->CacheResource( _type, _name, bPrecache, hResourceList, (int*)( &(_index) ) ); 
#define	PRECACHE_CONDITIONAL( _type, _name, _condition )			\
	if ( !bIgnoreConditionals && ( _condition ) )					\
	pPrecacheHandler->CacheResource( _type, _name, bPrecache, hResourceList, NULL ); 
#define	PRECACHE_INDEX_CONDITIONAL( _type, _name, _index, _func )	\
	if ( bIgnoreConditionals || ( _condition ) )				\
{															\
	pPrecacheHandler->CacheResource( _type, _name, bPrecache, hResourceList, (int*)( &(_index) ) );	\
}

//End
#define	PRECACHE_REGISTER_END( )							\
}														\
	CResourcePrecacher	s_ResourcePrecacher;				\
}

// FIXME: Remove! Backward compat
#define PRECACHE_WEAPON_REGISTER( _className )		\
	PRECACHE_REGISTER_BEGIN( GLOBAL, _className )	\
	PRECACHE( ENTITY, #_className )				\
	PRECACHE_REGISTER_END()

#define PRECACHE_REGISTER( _className )				\
	PRECACHE_REGISTER_BEGIN( GLOBAL, _className )	\
	PRECACHE( ENTITY, #_className )				\
	PRECACHE_REGISTER_END()

#endif // _RESOURCEPRECACHER_H


