//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "precache_register.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pPrecacheResourceTypeName[] =
{
	"vgui_resource",
	"material",		
	"model",			
	"scriptsound",		// NOTE: This is named this way for backward compat in reading from kv files	
	"particle_system",	
	"entity",
	"decal",
	"pmaterial",
	"dependency_file",
	"game_material_decals",
	"physics_gamesounds",
	"shared",
};

//-----------------------------------------------------------------------------
// Provides callback to do actual precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheHandler : public IPrecacheHandler
{
public:
	void CacheResource( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex = NULL );

private:
	void CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList );
	void PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList );
	void PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList );
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CPrecacheRegister	s_PrecacheRegister;
CPrecacheRegister *g_pPrecacheRegister = &s_PrecacheRegister;

static CPrecacheHandler		s_PrecacheHandler;
IPrecacheHandler *g_pPrecacheHandler = &s_PrecacheHandler;

bool CPrecacheRegister::Init()
{ 
	return true; 
}

//-----------------------------------------------------------------------------
// Level startup, shutdown
//-----------------------------------------------------------------------------
void CPrecacheRegister::LevelInitPreEntity()
{
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Start" );
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, GLOBAL, NULL, true, RESOURCE_LIST_INVALID, false );
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Finish" );

#ifdef CLIENT_DLL
	//FIXME: Double check this
	//Finally, force the cache of these materials
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Start" );
	materials->CacheUsedMaterials();
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Finish" );
#endif
}

void CPrecacheRegister::LevelShutdownPostEntity()
{
	// FIXME: How to uncache all resources cached during the course of the level?
	g_pPrecacheSystem->UncacheAll( g_pPrecacheHandler );

	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->DestroyAllResourceLists();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE(s_pPrecacheResourceTypeName) == PRECACHE_RESOURCE_TYPE_COUNT );

	KeyValues *pValues = new KeyValues( "ResourceFile" );

	if ( !pValues->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
	{
		Warning( "Can't open %s for client precache info.", pFilename );
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( Q_strlen( pszType ) == 0 || Q_strlen( pszFile ) == 0 )
			continue;

		bool bFoundMatch = false;
		for ( int i = 0; i < PRECACHE_RESOURCE_TYPE_COUNT; ++i )
		{
			if ( !Q_stricmp( pData->GetName(), s_pPrecacheResourceTypeName[i] ) )
			{
				CacheResource( (PrecacheResourceType_t)i, pszFile, bPrecache, hResourceList );
				bFoundMatch = true;
				break;
			}
		}

		if ( !bFoundMatch )
		{
			Warning( "Error in precache file \"%s\":\n", pFilename );
			Warning( "\tUnknown resource type specified \"%s\", value \"%s\"\n", pszType, pszFile );
		}
	}

	pValues->deleteThis();
}


//-----------------------------------------------------------------------------
// Precaches game material decals
//-----------------------------------------------------------------------------
void CPrecacheHandler::PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList )
{
}

void CPrecacheHandler::PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList )
{
	if ( !bPrecache )
		return;

	// precache the surface prop sounds
	bool bBulletSounds = !Q_stricmp( pName, "BulletSounds" );
	bool bStepSounds = !Q_stricmp( pName, "StepSounds" );
	bool bPhysicsImpactSounds = !Q_stricmp( pName, "PhysicsImpactSounds" );

	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		Assert( pprop );

		if ( bBulletSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.bulletImpact );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
		}

		if ( bStepSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.walkStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.walkStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.runStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.runStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
		}

		if ( bPhysicsImpactSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.impactSoft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.impactHard );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.scrapeSmooth );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.scrapeRough );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.rolling );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.breakSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
			pSoundName = physprops->GetString( pprop->sounds.strainSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Caches/uncaches resources
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResource( PrecacheResourceType_t nType, const char *pName, 
	bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	if ( bPrecache )
	{
		if ( pIndex )
		{
			*pIndex = 0;
		}

		switch( nType )
		{
		case VGUI_RESOURCE:
			break;

		case MATERIAL:
			PrecacheMaterial( pName ); 
			if ( pIndex )
			{
				*pIndex = GetMaterialIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MATERIAL, pName );
			}
			break;

		case PARTICLE_MATERIAL:
			{
#ifdef CLIENT_DLL
				// FIXME: NOTE! This is not 64-bit safe.
				// Hopefully we can remove PARTICLE_MATERIAL soon though.
				// It can go away once we retire the old-style particle system
				int nIndex = (int)( ParticleMgr()->GetPMaterial( pName ) );
#else
				int nIndex = 0;
#endif
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
			}
			break;

		case GAME_MATERIAL_DECALS:
			PrecacheGameMaterialDecals( bPrecache, hResourceList );
			break;

		case PHYSICS_GAMESOUNDS:
			PrecachePhysicsSounds( pName, bPrecache, hResourceList );
			break;

		case DECAL:
			{
				int nIndex = UTIL_PrecacheDecal( pName, true );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
			}
			break;

		case MODEL:
			{
				int nIndex = CBaseEntity::PrecacheModel( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MODEL, pName );
				}
			}
			break;

		case GAMESOUND:
			{
				int nIndex = CBaseEntity::PrecacheScriptSound( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_GAMESOUND, pName );
				}
			}
			break;

		case PARTICLE_SYSTEM:
			PrecacheParticleSystem( pName );
			if ( pIndex )
			{
				*pIndex = GetParticleSystemIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_PARTICLE_SYSTEM, pName );
			}
			break;

		case ENTITY:
			UTIL_PrecacheOther( pName );
			break;

		case SHARED:
			g_pPrecacheSystem->Cache( this, SHARED_SYSTEM, pName, bPrecache, hResourceList, false );
			break;

		case KV_DEP_FILE:
			CacheResourceFile( pName, bPrecache, hResourceList );
			break;
		}
		return;
	}

	// Blat out value
	if ( pIndex )
	{
		*pIndex = 0;
	}

	switch( nType )
	{
	case VGUI_RESOURCE:
		break;
	case MATERIAL:
		break;
	case MODEL:
		break;
	case GAMESOUND:
		break;
	case PARTICLE_SYSTEM:
		break;
	case ENTITY:
		break;
	case DECAL:
		break;
	case KV_DEP_FILE:
		break;
	}
}
