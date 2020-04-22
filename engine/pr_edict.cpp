//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "quakedef.h"
#include <stddef.h>
#include "vengineserver_impl.h"
#include "server.h"
#include "pr_edict.h"
#include "world.h"
#include "ispatialpartition.h"
#include "utllinkedlist.h"
#include "framesnapshot.h"
#include "sv_log.h"
#include "tier1/utlmap.h"
#include "tier1/utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Edicts won't get reallocated for this many seconds after being freed.
#define EDICT_FREETIME	1.0



#ifdef _DEBUG
#include <malloc.h>
#endif // _DEBUG

static ConVar		sv_useexplicitdelete( "sv_useexplicitdelete", "1", FCVAR_DEVELOPMENTONLY, "Explicitly delete dormant client entities caused by AllowImmediateReuse()." );
static ConVar		sv_lowedict_threshold( "sv_lowedict_threshold", "8", FCVAR_NONE, "When only this many edicts are free, take the action specified by sv_lowedict_action.", true, 0, true, MAX_EDICTS );
static ConVar		sv_lowedict_action( "sv_lowedict_action", "0", FCVAR_NONE, "0 - no action, 1 - warn to log file, 2 - attempt to restart the game, if applicable, 3 - restart the map, 4 - go to the next map in the map cycle, 5 - spew all edicts.", true, 0, true, 5 );

// Bitmask of free edicts.
static CBitVec< MAX_EDICTS > g_FreeEdicts;

/*
=================
ED_ClearEdict

Sets everything to NULL, done when new entity is allocated for game.dll
=================
*/
static void ED_ClearEdict( edict_t *e )
{
	e->ClearFree();
	e->ClearStateChanged();
	e->SetChangeInfoSerialNumber( 0 );
	
	serverGameEnts->FreeContainingEntity(e);
	InitializeEntityDLLFields(e);

	e->m_NetworkSerialNumber = -1;  // must be filled by game.dll
	Assert( (int) e->m_EdictIndex == (e - sv.edicts) );
}

void ED_ClearFreeFlag( edict_t *e )
{
	e->ClearFree();
	g_FreeEdicts.Clear( e->m_EdictIndex );
}

void ED_ClearFreeEdictList()
{
	// Clear the free edicts bitfield.
	g_FreeEdicts.ClearAll();
}

static void SpewEdicts()
{
	CUtlMap< const char*, int > mapEnts;
	mapEnts.SetLessFunc( StringLessThan );

	// Tally up each class
	int nEdictNum = 1;
	for( int i=0; i<sv.num_edicts; ++i )
	{
		edict_t *e = &sv.edicts[i];
		++nEdictNum;
		unsigned short nIndex = mapEnts.Find( e->GetClassName() );
		if ( nIndex == mapEnts.InvalidIndex() )
		{
			nIndex = mapEnts.Insert( e->GetClassName() );
			mapEnts[ nIndex ] = 0;
		}
		mapEnts[ nIndex ] = mapEnts[ nIndex ] + 1;
	}

	struct EdictCount_t
	{
		EdictCount_t( const char *pszClassName, int nCount )
			:
			m_pszClassName( pszClassName ),
			m_nCount( nCount )
		{}

		const char *m_pszClassName;
		int m_nCount;
	};

	CUtlVector<EdictCount_t*> vecEnts;

	FOR_EACH_MAP_FAST( mapEnts, i )
	{
		vecEnts.AddToTail( new EdictCount_t( mapEnts.Key( i ), mapEnts[ i ] ) );
	}

	struct EdictSorter_t
	{
		static int SortEdicts( EdictCount_t* const *p1, EdictCount_t* const *p2 )
		{
			return (*p1)->m_nCount - (*p2)->m_nCount;
		}
	};

	vecEnts.Sort( &EdictSorter_t::SortEdicts );

	g_Log.Printf( "Spewing edict counts:\n" );
	FOR_EACH_VEC( vecEnts, i )
	{
		g_Log.Printf( "(%3.2f%%) %d\t%s\n", vecEnts[i]->m_nCount/(float)nEdictNum * 100.f, vecEnts[i]->m_nCount, vecEnts[i]->m_pszClassName );
	}
	g_Log.Printf( "Total edicts: %d\n", nEdictNum );

	vecEnts.PurgeAndDeleteElements();
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/


edict_t *ED_Alloc( int iForceEdictIndex )
{
	if ( iForceEdictIndex >= 0 )
	{
		if ( iForceEdictIndex >= sv.num_edicts )
		{
			Warning( "ED_Alloc( %d ) - invalid edict index specified.", iForceEdictIndex );
			return NULL;
		}
		
		edict_t *e = &sv.edicts[ iForceEdictIndex ];
		if ( e->IsFree() )
		{
			Assert( iForceEdictIndex == e->m_EdictIndex );
			--sv.free_edicts;
			Assert( g_FreeEdicts.IsBitSet( iForceEdictIndex ) );
			g_FreeEdicts.Clear( iForceEdictIndex );
			ED_ClearEdict( e );
			return e;
		}
		else
		{
			return NULL;
		}
	}

	// Check the free list first.
	int iBit = -1;
	for ( ;; )
	{
		iBit = g_FreeEdicts.FindNextSetBit( iBit + 1 );
		if ( iBit < 0 )
			break;

		edict_t *pEdict = &sv.edicts[ iBit ];

		// If this assert goes off, someone most likely called pedict->ClearFree() and not ED_ClearFreeFlag()?
		Assert( pEdict->IsFree() );
		Assert( iBit == pEdict->m_EdictIndex );
		if ( ( pEdict->freetime < 2 ) || ( sv.GetTime() - pEdict->freetime >= EDICT_FREETIME ) )
		{
			// If we have no freetime, we've had AllowImmediateReuse() called. We need
			// to explicitly delete this old entity.
			if ( pEdict->freetime == 0 && sv_useexplicitdelete.GetBool() )
			{
				//Warning("ADDING SLOT to snapshot: %d\n", i );
				framesnapshotmanager->AddExplicitDelete( iBit );
			}

			--sv.free_edicts;
			g_FreeEdicts.Clear( pEdict->m_EdictIndex );
			ED_ClearEdict( pEdict );
			return pEdict;
		}
	}

	// Allocate a new edict.
	if ( sv.num_edicts >= sv.max_edicts )
	{
		AssertMsg( 0, "Can't allocate edict" );

		SpewEdicts(); // Log the entities we have before we die

		if ( sv.max_edicts == 0 )
			Sys_Error( "ED_Alloc: No edicts yet" );
		Sys_Error ("ED_Alloc: no free edicts");
	}

	// Do this before clearing since clear now needs to call back into the edict to deduce the index so can get the changeinfo data in the parallel structure
	edict_t *pEdict = sv.edicts + sv.num_edicts++; 

	// We should not be in the free list...
	Assert( !g_FreeEdicts.IsBitSet( pEdict->m_EdictIndex ) );
	ED_ClearEdict( pEdict );

	if ( sv_lowedict_action.GetInt() > 0 && sv.num_edicts >= sv.max_edicts - sv_lowedict_threshold.GetInt() )
	{
		int nEdictsRemaining = sv.max_edicts - sv.num_edicts;
		g_Log.Printf( "Warning: free edicts below threshold. %i free edict%s remaining.\n", nEdictsRemaining, nEdictsRemaining == 1 ? "" : "s" );

		switch ( sv_lowedict_action.GetInt() )
		{
		case 2:
			// restart the game
			{
				ConVarRef mp_restartgame_immediate( "mp_restartgame_immediate" );
				if ( mp_restartgame_immediate.IsValid() )
				{
					mp_restartgame_immediate.SetValue( 1 );
				}
				else
				{
					ConVarRef mp_restartgame( "mp_restartgame" );
					if ( mp_restartgame.IsValid() )
					{
						mp_restartgame.SetValue( 1 );
					}
				}
			}
			break;
		case 3:
			// restart the map
			g_pVEngineServer->ChangeLevel( sv.GetMapName(), NULL );
			break;
		case 4:
			// go to the next map
			g_pVEngineServer->ServerCommand( "changelevel_next\n" );
			break;
		case 5:
			// spew all edicts
			SpewEdicts();
			break;
		}
	}
	
	return pEdict;
}


void ED_AllowImmediateReuse()
{
	edict_t *pEdict = sv.edicts + sv.GetMaxClients() + 1;
	for ( int i=sv.GetMaxClients()+1; i < sv.num_edicts; i++ )
	{
		if ( pEdict->IsFree() )
		{
			pEdict->freetime = 0;
		}

		pEdict++;
	}
}


/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	if (ed->IsFree())
	{
#ifdef _DEBUG
//		ConDMsg("duplicate free on '%s'\n", pr_strings + ed->classname );
#endif
		return;
	}

	// don't free player edicts
	if ( (ed - sv.edicts) >= 1 && (ed - sv.edicts) <= sv.GetMaxClients() )
		return;

	// release the DLL entity that's attached to this edict, if any
	serverGameEnts->FreeContainingEntity( ed );

	ed->SetFree();
	ed->freetime = sv.GetTime();

	++sv.free_edicts;
	Assert( !g_FreeEdicts.IsBitSet( ed->m_EdictIndex ) );
	g_FreeEdicts.Set( ed->m_EdictIndex );

	// Increment the serial number so it knows to send explicit deletes the clients.
	ed->m_NetworkSerialNumber++; 
}

//
// 	serverGameEnts->FreeContainingEntity( pEdict )  frees up memory associated with a DLL entity.
// InitializeEntityDLLFields clears out fields to NULL or UNKNOWN.
// Release is for terminating a DLL entity.  Initialize is for initializing one.
//
void InitializeEntityDLLFields( edict_t *pEdict )
{
	// clear all the game variables
	size_t sz = offsetof( edict_t, m_pUnk ) + sizeof( void* );
	memset( ((byte*)pEdict) + sz, 0, sizeof(edict_t) - sz );
}

int NUM_FOR_EDICT(const edict_t *e)
{
	if ( &sv.edicts[e->m_EdictIndex] == e ) // NOTE: old server.dll may stomp m_EdictIndex
		return e->m_EdictIndex;
	int index = e - sv.edicts;
	if ( (unsigned int) index >= (unsigned int) sv.num_edicts )
		Sys_Error ("NUM_FOR_EDICT: bad pointer");
	return index;
}

edict_t *EDICT_NUM(int n)
{
	if ((unsigned int) n >= (unsigned int) sv.max_edicts)
		Sys_Error ("EDICT_NUM: bad number %i", n);
	return &sv.edicts[n];
}


static inline int NUM_FOR_EDICTINFO(const edict_t *e)
{
	if ( &sv.edicts[e->m_EdictIndex] == e ) // NOTE: old server.dll may stomp m_EdictIndex
		return e->m_EdictIndex;
	int index = e - sv.edicts;
	if ( (unsigned int) index >= (unsigned int) sv.max_edicts )
		Sys_Error ("NUM_FOR_EDICTINFO: bad pointer");
	return index;
}


IChangeInfoAccessor *CBaseEdict::GetChangeAccessor()
{
	return &sv.edictchangeinfo[ NUM_FOR_EDICTINFO( (const edict_t*)this ) ];
}

const IChangeInfoAccessor *CBaseEdict::GetChangeAccessor() const
{
	return &sv.edictchangeinfo[ NUM_FOR_EDICTINFO( (const edict_t*)this ) ];
}
