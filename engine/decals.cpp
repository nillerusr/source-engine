//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// Functionality common to wad and decal code in gl_draw.c and draw.c
//
//===========================================================================//
#include "render_pch.h"
#include "decal.h"
#include "decal_private.h"
#include "zone.h"
#include "sys.h"
#include "gl_matsysiface.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "materialsystem/imaterial.h"
#include <malloc.h>
#include "utldict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct DecalEntry
{
#ifdef _DEBUG
	char		*m_pDebugName;	// only used in debug builds
#endif
	IMaterial	*material;
	int			index;
};

// This stores the list of all decals
CUtlMap< FileNameHandle_t, DecalEntry >	g_DecalDictionary( 0, 0, DefLessFunc( FileNameHandle_t ) );

// This is a list of indices into the dictionary.
// This list is indexed by network id, so it maps network ids to decal dictionary entries
CUtlVector< int > g_DecalLookup;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int Draw_DecalMax( void )
{
	return g_nMaxDecals;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Sets the name of the bitmap from decals.wad to be used in a specific slot #
// called from cl_parse.cpp twice
// This sets the name of a decal prototype texture
// Input  : decal - 
//			*name - 
//-----------------------------------------------------------------------------
// called from gl_rsurf.cpp
IMaterial *Draw_DecalMaterial( int index )
{
	if ( index < 0 || index >= g_DecalLookup.Count() )
		return NULL;

	int slot = g_DecalLookup[index];
	if ( slot < 0 || slot >= (int)g_DecalDictionary.MaxElement() )
		return NULL;

	DecalEntry * entry = &g_DecalDictionary[slot];
	
	if ( entry )
	{
		return entry->material;
	}
	else
	{
		return NULL;
	}
}

#ifndef SWDS
void Draw_DecalSetName( int decal, char *name )
{
	while ( decal >= g_DecalLookup.Count() )
	{
		MEM_ALLOC_CREDIT();
		int idx = g_DecalLookup.AddToTail();
		g_DecalLookup[idx] = g_DecalDictionary.InvalidIndex();
	}

	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( name );
	int lookup = g_DecalDictionary.Find( fnHandle );
	if ( lookup == g_DecalDictionary.InvalidIndex() )
	{
		DecalEntry entry;
#ifdef _DEBUG
		int len = strlen(name) + 1;
		entry.m_pDebugName = new char[len];
		memcpy( entry.m_pDebugName, name, len );
#endif
		entry.material = GL_LoadMaterial( name, TEXTURE_GROUP_DECAL );
		entry.index = decal;

		lookup = g_DecalDictionary.Insert( fnHandle, entry );
	}
	else
	{
		g_DecalDictionary[lookup].index = decal;
	}

	g_DecalLookup[decal] = lookup;
}



// called from cl_parse.cpp
// find the server side decal id given it's name.
// used for save/restore
int Draw_DecalIndexFromName( char *name, bool *found )
{
	Assert( found );

	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( name );
	int lookup = g_DecalDictionary.Find( fnHandle );
	if ( lookup == g_DecalDictionary.InvalidIndex() )
	{
		if ( found )
		{
			*found = false;
		}
		return 0;
	}

	if ( found )
	{
		*found = true;
	}
	return g_DecalDictionary[lookup].index;
}
#endif


const char *Draw_DecalNameFromIndex( int index )
{
#if !defined(SWDS)
	return g_DecalDictionary[index].material ? g_DecalDictionary[index].material->GetName() : "";
#else
	return "";
#endif
}

// This is called to reset all loaded decals
// called from cl_parse.cpp and host.cpp
void Decal_Init( void )
{
	Decal_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Decal_Shutdown( void )
{
	for ( int index = g_DecalDictionary.FirstInorder(); index != g_DecalDictionary.InvalidIndex(); index = g_DecalDictionary.NextInorder(index) )
	{
		IMaterial *mat = g_DecalDictionary[index].material;
		if ( mat )
		{
			GL_UnloadMaterial( mat );
		}
#ifdef _DEBUG
		delete[] g_DecalDictionary[index].m_pDebugName;
#endif
	}
	g_DecalLookup.Purge();
	g_DecalDictionary.RemoveAll();
}

