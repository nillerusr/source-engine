//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "server_pch.h"
#include "sv_precache.h"
#include "host.h"
#include "tier0/icommandline.h"
#include "MapReslistGenerator.h"
#include "DownloadListGenerator.h"
#include "soundchars.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar sv_forcepreload( "sv_forcepreload", "0", FCVAR_ARCHIVE, "Force server side preloading.");

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int SV_ModelIndex
//-----------------------------------------------------------------------------
int SV_ModelIndex (const char *name)
{
	return sv.LookupModelIndex( name );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : int
//-----------------------------------------------------------------------------
int SV_FindOrAddModel(const char *name, bool preload )
{
	// Look for a match or an empty slot...
	int flags = RES_FATALIFMISSING;
	if ( preload )
	{
		flags |= RES_PRELOAD;
	}

	return sv.PrecacheModel( name, flags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int SV_SoundIndex(const char *name)
{
	return sv.LookupSoundIndex( name );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : int
//-----------------------------------------------------------------------------
int SV_FindOrAddSound(const char *name, bool preload )
{
	// Look for a match or an empty slot...
	int flags = RES_FATALIFMISSING;
	if ( preload )
	{
		flags |= RES_PRELOAD;
	}

	return sv.PrecacheSound( name, flags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int SV_GenericIndex(const char *name)
{
	return sv.LookupGenericIndex( name );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : int
//-----------------------------------------------------------------------------
int SV_FindOrAddGeneric(const char *name, bool preload )
{
	// Look for a match or an empty slot...
	int flags = RES_FATALIFMISSING;
	if ( preload )
	{
		flags |= RES_PRELOAD;
	}

	return sv.PrecacheGeneric( name, flags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int SV_DecalIndex(const char *name)
{
	return sv.LookupDecalIndex( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : int
//-----------------------------------------------------------------------------
int SV_FindOrAddDecal(const char *name, bool preload )
{
	// Look for a match or an empty slot...
	int flags = RES_FATALIFMISSING;
	if ( preload )
	{
		flags |= RES_PRELOAD;
	}

	return sv.PrecacheDecal( name, flags );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void SV_ForceSimpleMaterial( const char *name )
{
	DownloadListGenerator().ForceSimpleMaterial( name );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			&mins - 
//			&maxs - 
//-----------------------------------------------------------------------------
void SV_ForceModelBounds( const char *name, const Vector &mins, const Vector &maxs )
{
	DownloadListGenerator().ForceModelBounds( name, mins, maxs );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CGameServer::GetModelPrecacheTable( void ) const
{
	return m_pModelPrecacheTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			flags - 
//			*model - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::PrecacheModel( char const *name, int flags, model_t *model /*=NULL*/ )
{
	if ( !m_pModelPrecacheTable )
		return -1;

	int idx = m_pModelPrecacheTable->AddString( true, name );
	if ( idx == INVALID_STRING_INDEX )
	{
		return -1;
	}

	CPrecacheUserData p;

	// first time, set file size & flags
	CPrecacheUserData const *pExisting = (CPrecacheUserData const *)m_pModelPrecacheTable->GetStringUserData( idx, NULL );
	if ( !pExisting )
	{
		p.flags = flags;
	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	m_pModelPrecacheTable->SetStringUserData( idx, sizeof( p ), &p );

	CPrecacheItem *slot = &model_precache[ idx ];

	if ( model )
	{
		slot->SetModel( model );
	}

	bool bLoadNow;
	bLoadNow = ( !slot->GetModel() && ( ( flags & RES_PRELOAD ) || IsX360() ) );
	if ( CommandLine()->FindParm( "-nopreload" ) ||	CommandLine()->FindParm( "-nopreloadmodels" ))
	{
		bLoadNow = false;
	}
	else if ( sv_forcepreload.GetInt() || CommandLine()->FindParm( "-preload" ) )
	{
		bLoadNow = true;
	}

	if ( idx != 0 )
	{
		if ( bLoadNow )
		{
			slot->SetModel( modelloader->GetModelForName( name, IModelLoader::FMODELLOADER_SERVER ) );
#ifndef SWDS
			EngineVGui()->UpdateProgressBar(PROGRESS_PRECACHE); 
#endif
			MapReslistGenerator().OnModelPrecached(name);
		}
		else
		{
			modelloader->ReferenceModel( name, IModelLoader::FMODELLOADER_SERVER );
			slot->SetModel( NULL );
		}
	}

	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CGameServer::GetModel( int index )
{
	if ( index <= 0 || !m_pModelPrecacheTable )
		return NULL;

	if ( index >= m_pModelPrecacheTable->GetNumStrings() )
	{
		return NULL;
	}

	CPrecacheItem *slot = &model_precache[ index ];
	model_t *m = slot->GetModel();
	if ( m )
	{
		return m;
	}

	char const *modelname = m_pModelPrecacheTable->GetString( index );
	Assert( modelname );

	if ( host_showcachemiss.GetBool() )
	{
		ConDMsg( "server model cache miss on %s\n", modelname );
	}

	m = modelloader->GetModelForName( modelname, IModelLoader::FMODELLOADER_SERVER );
	slot->SetModel( m );

	return m;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::LookupModelIndex( char const *name )
{
	if ( !m_pModelPrecacheTable )
		return -1;

	int idx = m_pModelPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? -1 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CGameServer::GetSoundPrecacheTable( void ) const
{
	return m_pSoundPrecacheTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			flags - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::PrecacheSound( char const *name, int flags )
{
	if ( !m_pSoundPrecacheTable )
		return -1;

	int idx = m_pSoundPrecacheTable->AddString( true, name );
	if ( idx == INVALID_STRING_INDEX )
	{
		return -1;
	}

	// mark the sound as being precached, but check first that reslist generation is enabled to save on the va() call
	if (MapReslistGenerator().IsEnabled() && name[0])
	{
		MapReslistGenerator().OnResourcePrecached( va( "sound/%s", PSkipSoundChars( name ) ) );
	}

	// first time, set file size & flags
	CPrecacheUserData p;
	CPrecacheUserData const *pExisting = (CPrecacheUserData const *)m_pSoundPrecacheTable->GetStringUserData( idx, NULL );
	if ( !pExisting )
	{
		p.flags = flags;
	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	m_pSoundPrecacheTable->SetStringUserData( idx, sizeof( p ), &p );

	CPrecacheItem *slot = &sound_precache[ idx ];
	slot->SetName( name );

	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CGameServer::GetSound( int index )
{
	if ( index <= 0 || !m_pSoundPrecacheTable )
	{
		return NULL;
	}

	if ( index >= m_pSoundPrecacheTable->GetNumStrings() )
	{
		return NULL;
	}

	CPrecacheItem *slot = &sound_precache[ index ];
	return slot->GetName();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::LookupSoundIndex( char const *name )
{
	if ( !m_pSoundPrecacheTable )
		return 0;

	int idx = m_pSoundPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? 0 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CGameServer::GetGenericPrecacheTable( void ) const
{
	return m_pGenericPrecacheTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			flags - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::PrecacheGeneric( char const *name, int flags )
{
	if ( !m_pGenericPrecacheTable )
		return -1;

	int idx = m_pGenericPrecacheTable->AddString( true, name );

	if ( idx == INVALID_STRING_INDEX )
	{
		return -1;
	}

	MapReslistGenerator().OnResourcePrecached( name );

	CPrecacheUserData p;

	// first time, set file size & flags
	CPrecacheUserData const *pExisting = (CPrecacheUserData const *)m_pGenericPrecacheTable->GetStringUserData( idx, NULL );
	if ( !pExisting )
	{
		p.flags = flags;

	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	m_pGenericPrecacheTable->SetStringUserData( idx, sizeof( p ), &p );

	CPrecacheItem *slot = &generic_precache[ idx ];
	slot->SetGeneric( name );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CGameServer::GetGeneric( int index )
{
	// Bogus index
	if ( index < 0 || !m_pGenericPrecacheTable )
		return "";

	if ( index >= m_pGenericPrecacheTable->GetNumStrings() )
	{
		return "";
	}

	CPrecacheItem *slot = &generic_precache[ index ];
	return slot->GetGeneric();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::LookupGenericIndex( char const *name )
{
	if ( !m_pGenericPrecacheTable )
		return 0;

	int idx = m_pGenericPrecacheTable->FindStringIndex( name );

	return ( idx == INVALID_STRING_INDEX ) ? 0 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : TABLEID
//-----------------------------------------------------------------------------
INetworkStringTable *CGameServer::GetDecalPrecacheTable( void ) const
{
	return m_pDecalPrecacheTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			flags - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::PrecacheDecal( char const *name, int flags )
{
	if ( !m_pDecalPrecacheTable )
		return -1;

	int idx = m_pDecalPrecacheTable->AddString( true, name );
	if ( idx == INVALID_STRING_INDEX )
	{
		return -1;
	}

	MapReslistGenerator().OnResourcePrecached(name);

	CPrecacheUserData p;

	// first time, set file size & flags
	CPrecacheUserData const *pExisting = (CPrecacheUserData const *)m_pDecalPrecacheTable->GetStringUserData( idx, NULL );
	if ( !pExisting )
	{
		p.flags = flags;
	}
	else
	{
		// Just or in any new flags
		p = *pExisting;
		p.flags |= flags;
	}

	m_pDecalPrecacheTable->SetStringUserData( idx, sizeof( p ), &p );

	CPrecacheItem *slot = &decal_precache[ idx ];
	slot->SetDecal( name );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CGameServer::LookupDecalIndex( char const *name )
{
	if ( !m_pDecalPrecacheTable )
		return -1;

	int idx = m_pDecalPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? -1 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameServer::DumpPrecacheStats( INetworkStringTable *table )
{
	if ( table == NULL )
	{
		ConMsg( "Can only dump stats when active in a level\n" );
		return;
	}

	CPrecacheItem *items = NULL;
	if ( table == m_pModelPrecacheTable )
	{
		items = model_precache;
	}
	else if ( table == m_pGenericPrecacheTable )
	{
		items = generic_precache;
	}
	else if ( table == m_pSoundPrecacheTable )
	{
		items = sound_precache;
	}
	else if ( table == m_pDecalPrecacheTable )
	{
		items = decal_precache;
	}

	if ( !items )
		return;

	int count = table->GetNumStrings();
	int maxcount = table->GetMaxStrings();

	ConMsg( "\n" );
	ConMsg( "Precache table %s:  %i of %i slots used\n", table->GetTableName(),
		count, maxcount );

	for ( int i = 0; i < count; i++ )
	{
		char const *name = table->GetString( i );
		CPrecacheItem *slot = &items[ i ];
		
		int testLength;
		const CPrecacheUserData *p = ( const CPrecacheUserData * )table->GetStringUserData( i, &testLength );
		ErrorIfNot( testLength == sizeof( *p ),
			("CGameServer::DumpPrecacheStats: invalid CPrecacheUserData length (%d)", testLength)
		);

		if ( !name || !slot || !p )
			continue;

		ConMsg( "%03i:  %s (%s):   ",
			i,
			name, 
			GetFlagString( p->flags ) );

		if ( slot->GetReferenceCount() == 0 )
		{
			ConMsg( " never used\n" );
		}
		else
		{
			ConMsg( " %i refs, first %.2f mru %.2f\n",
				slot->GetReferenceCount(), 
				slot->GetFirstReference(), 
				slot->GetMostRecentReference() );
		}
	}

	ConMsg( "\n" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( sv_precacheinfo, "Show precache info." )
{
	if ( args.ArgC() == 2 )
	{
		char const *table = args[ 1 ];

		bool dumped = true;
		if ( !Q_strcasecmp( table, "generic" ) )
		{
			sv.DumpPrecacheStats( sv.GetGenericPrecacheTable() );
		}
		else if ( !Q_strcasecmp( table, "sound" ) )
		{
			sv.DumpPrecacheStats( sv.GetSoundPrecacheTable() );
		}
		else if ( !Q_strcasecmp( table, "decal" ) )
		{
			sv.DumpPrecacheStats( sv.GetDecalPrecacheTable() );
		}
		else if ( !Q_strcasecmp( table, "model" ) )
		{
			sv.DumpPrecacheStats( sv.GetModelPrecacheTable() );
		}
		else
		{
			dumped = false;
		}

		if ( dumped )
		{
			return;
		}
	}
	
	// Show all data
	sv.DumpPrecacheStats( sv.GetGenericPrecacheTable() );
	sv.DumpPrecacheStats( sv.GetDecalPrecacheTable() );
	sv.DumpPrecacheStats( sv.GetSoundPrecacheTable() );
	sv.DumpPrecacheStats( sv.GetModelPrecacheTable() );
}
