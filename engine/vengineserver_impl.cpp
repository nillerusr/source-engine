//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//
#include "server_pch.h"
#include "tier0/valve_minmax_off.h"
#include <algorithm>
#include "tier0/valve_minmax_on.h"
#include "vengineserver_impl.h"
#include "vox.h"
#include "sound.h"
#include "gl_model_private.h"
#include "host_saverestore.h"
#include "world.h"
#include "l_studio.h"
#include "decal.h"
#include "sys_dll.h"
#include "sv_log.h"
#include "sv_main.h"
#include "tier1/strtools.h"
#include "collisionutils.h"
#include "staticpropmgr.h"
#include "string_t.h"
#include "vstdlib/random.h"
#include "EngineSoundInternal.h"
#include "dt_send_eng.h"
#include "PlayerState.h"
#include "irecipientfilter.h"
#include "sv_user.h"
#include "server_class.h"
#include "cdll_engine_int.h"
#include "enginesingleuserfilter.h"
#include "ispatialpartitioninternal.h"
#include "con_nprint.h"
#include "tmessage.h"
#include "iscratchpad3d.h"
#include "pr_edict.h"
#include "networkstringtableserver.h"
#include "networkstringtable.h"
#include "LocalNetworkBackdoor.h"
#include "host_phonehome.h"
#include "matchmaking.h"
#include "sv_plugin.h"
#include "sv_steamauth.h"
#include "replay_internal.h"
#include "replayserver.h"
#include "replay/iserverengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_MESSAGE_SIZE	2500
#define MAX_TOTAL_ENT_LEAFS		128

void SV_DetermineMulticastRecipients( bool usepas, const Vector& origin, CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits );

int MapList_ListMaps( const char *pszSubString, bool listobsolete, bool verbose, int maxcount, int maxitemlength, char maplist[][ 64 ] );

extern CNetworkStringTableContainer *networkStringTableContainerServer;

CSharedEdictChangeInfo g_SharedEdictChangeInfo;
CSharedEdictChangeInfo *g_pSharedChangeInfo = &g_SharedEdictChangeInfo;
IAchievementMgr *g_pAchievementMgr = NULL;
CGamestatsData *g_pGamestatsData = NULL;

void InvalidateSharedEdictChangeInfos()
{
	if ( g_SharedEdictChangeInfo.m_iSerialNumber == 0xFFFF )
	{
		// Reset all edicts to 0.
		g_SharedEdictChangeInfo.m_iSerialNumber = 1;
		for ( int i=0; i < sv.num_edicts; i++ )
			sv.edicts[i].SetChangeInfoSerialNumber( 0 );
	}
	else
	{
		g_SharedEdictChangeInfo.m_iSerialNumber++;
	}
	g_SharedEdictChangeInfo.m_nChangeInfos = 0;
}


// ---------------------------------------------------------------------- //
// Globals.
// ---------------------------------------------------------------------- //

struct MsgData
{
	MsgData()
	{
		Reset();

		// link buffers to messages
		entityMsg.m_DataOut.StartWriting( entitydata, sizeof(entitydata) );
		entityMsg.m_DataOut.SetDebugName( "s_MsgData.entityMsg.m_DataOut" );

		userMsg.m_DataOut.StartWriting( userdata, sizeof(userdata) );
		userMsg.m_DataOut.SetDebugName( "s_MsgData.userMsg.m_DataOut" );
	}

	void Reset()
	{
		filter			= NULL;
		reliable		= false;
		subtype			= 0;
		started			= false;
		usermessagesize = -1;
		usermessagename = NULL;
		currentMsg		= NULL;
	}

	byte				userdata[ PAD_NUMBER( MAX_USER_MSG_DATA, 4 ) ];	// buffer for outgoing user messages
	byte				entitydata[ PAD_NUMBER( MAX_ENTITY_MSG_DATA, 4 ) ]; // buffer for outgoing entity messages

	IRecipientFilter	*filter;		// clients who get this message
	bool				reliable;
	
	INetMessage			*currentMsg;				// pointer to entityMsg or userMessage
	int					subtype;			// usermessage index
	bool				started;			// IS THERE A MESSAGE IN THE PROCESS OF BEING SENT?
	int					usermessagesize;
	char const			*usermessagename;
	

	SVC_EntityMessage	entityMsg;
	SVC_UserMessage		userMsg;

};

static MsgData s_MsgData;

void SeedRandomNumberGenerator( bool random_invariant )
{
	if (!random_invariant)
	{
		long iSeed;
		g_pVCR->Hook_Time( &iSeed );
		float flAppTime = Plat_FloatTime();
		ThreadId_t threadId = ThreadGetCurrentId();

		iSeed ^= (*((int *)&flAppTime));
		iSeed ^= threadId;

		RandomSeed( iSeed );
	}
	else
	{
		// Make those random numbers the same every time!
		RandomSeed( 0 );
	}
}

// ---------------------------------------------------------------------- //
// Static helpers.
// ---------------------------------------------------------------------- //
static void PR_CheckEmptyString (const char *s)
{
	if (s[0] <= ' ')
		Host_Error ("Bad string: %s", s);
}

// Average a list a vertices to find an approximate "center"
static void CenterVerts( Vector verts[], int vertCount, Vector& center )
{
	int i;
	float scale;

	if ( vertCount )
	{
		Vector edge0, edge1, normal;

		VectorCopy( vec3_origin, center );
		// sum up verts
		for ( i = 0; i < vertCount; i++ )
		{
			VectorAdd( center, verts[i], center );
		}
		scale = 1.0f / (float)vertCount;
		VectorScale( center, scale, center );	// divide by vertCount

		// Compute 2 poly edges
		VectorSubtract( verts[1], verts[0], edge0 );
		VectorSubtract( verts[vertCount-1], verts[0], edge1 );
		// cross for normal
		CrossProduct( edge0, edge1, normal );
		// Find the component of center that is outside of the plane
		scale = DotProduct( center, normal ) - DotProduct( verts[0], normal );
		// subtract it off
		VectorMA( center, scale, normal, center );
		// center is in the plane now
	}
}


// Copy the list of verts from an msurface_t int a linear array
static void SurfaceToVerts( model_t *model, SurfaceHandle_t surfID, Vector verts[], int *vertCount )
{
	if ( *vertCount > MSurf_VertCount( surfID ) )
		*vertCount = MSurf_VertCount( surfID );

	// Build the list of verts from 0 to n
	for ( int i = 0; i < *vertCount; i++ )
	{
		int vertIndex = model->brush.pShared->vertindices[ MSurf_FirstVertIndex( surfID ) + i ];
		Vector& vert = model->brush.pShared->vertexes[ vertIndex ].position;
		VectorCopy( vert, verts[i] );
	}
	// vert[0] is the first and last vert, there is no copy
}


// Calculate the surface area of an arbitrary msurface_t polygon (convex with collinear verts)
static float SurfaceArea( model_t *model, SurfaceHandle_t surfID )
{
	Vector	center, verts[32];
	int		vertCount = 32;
	float	area;
	int		i;

	// Compute a "center" point and fan
	SurfaceToVerts( model, surfID, verts, &vertCount );
	CenterVerts( verts, vertCount, center );

	area = 0;
	// For a triangle of the center and each edge
	for ( i = 0; i < vertCount; i++ )
	{
		Vector edge0, edge1, out;
		int next;

		next = (i+1)%vertCount;
		VectorSubtract( verts[i], center, edge0 );			// 0.5 * edge cross edge (0.5 is done once at the end)
		VectorSubtract( verts[next], center, edge1 );
		CrossProduct( edge0, edge1, out );
		area += VectorLength( out );
	}
	return area * 0.5;										// 0.5 here
}


// Average the list of vertices to find an approximate "center"
static void SurfaceCenter( model_t *model, SurfaceHandle_t surfID, Vector& center )
{
	Vector	verts[32];		// We limit faces to 32 verts elsewhere in the engine
	int		vertCount = 32;

	SurfaceToVerts( model, surfID, verts, &vertCount );
	CenterVerts( verts, vertCount, center );
}


static bool ValidCmd( const char *pCmd )
{
	int len;

	len = strlen(pCmd);

	// Valid commands all have a ';' or newline '\n' as their last character
	if ( len && (pCmd[len-1] == '\n' || pCmd[len-1] == ';') )
		return true;

	return false;
}

// ---------------------------------------------------------------------- //
// CVEngineServer
// ---------------------------------------------------------------------- //
class CVEngineServer : public IVEngineServer
{
public:

	virtual void ChangeLevel( const char* s1, const char* s2)
	{
		if ( !s1 )
		{
			Sys_Error( "CVEngineServer::Changelevel with NULL s1\n" );
		}

		char cmd[ 256 ];
		char s1Escaped[ sizeof( cmd ) ];
		char s2Escaped[ sizeof( cmd ) ];
		if ( !Cbuf_EscapeCommandArg( s1, s1Escaped, sizeof( s1Escaped ) ) ||
		     ( s2 && !Cbuf_EscapeCommandArg( s2, s2Escaped, sizeof( s2Escaped ) )))
		{
			Warning( "Illegal map name in ChangeLevel\n" );
			return;
		}

		int cmdLen = 0;
		if ( !s2 ) // no indication of where they are coming from;  so just do a standard old changelevel
		{
			cmdLen = Q_snprintf( cmd, sizeof( cmd ), "changelevel %s\n", s1Escaped );
		}
		else
		{
			cmdLen = Q_snprintf( cmd, sizeof( cmd ), "changelevel2 %s %s\n", s1Escaped, s2Escaped );
		}

		if ( !cmdLen || cmdLen >= sizeof( cmd ) )
		{
			Warning( "Paramter overflow in ChangeLevel\n" );
			return;
		}

		Cbuf_AddText( cmd );
	}

	virtual int	IsMapValid( const char *filename )
	{
		return modelloader->Map_IsValid( filename );
	}

	virtual bool IsDedicatedServer( void )
	{
		return sv.IsDedicated();
	}
	
	virtual int IsInEditMode( void )
	{
#ifdef SWDS
		return false;
#else
		return g_bInEditMode;
#endif
	}

	virtual int IsInCommentaryMode( void )
	{
#ifdef SWDS
		return false;
#else
		return g_bInCommentaryMode;
#endif
	}
	
	virtual void NotifyEdictFlagsChange( int iEdict )
	{
		if ( g_pLocalNetworkBackdoor )
			g_pLocalNetworkBackdoor->NotifyEdictFlagsChange( iEdict );
	}

	virtual const CCheckTransmitInfo* GetPrevCheckTransmitInfo( edict_t *pPlayerEdict )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		if ( entnum < 1 || entnum > sv.GetClientCount() )
		{
			Error( "Invalid client specified in GetPrevCheckTransmitInfo\n" );
			return NULL;
		}
		
		CGameClient *client = sv.Client( entnum-1 );
		return client->GetPrevPackInfo();		
	}
	
	virtual int PrecacheDecal( const char *name, bool preload /*=false*/ )
	{
		PR_CheckEmptyString( name );
		int i = SV_FindOrAddDecal( name, preload );
		if ( i >= 0 )
		{
			return i;
		}
		
		Host_Error( "CVEngineServer::PrecacheDecal: '%s' overflow, too many decals", name );
		return 0;
	}
	
	virtual int PrecacheModel( const char *s, bool preload /*= false*/ )
	{
		PR_CheckEmptyString (s);
		int i = SV_FindOrAddModel( s, preload );
		if ( i >= 0 )
		{
			return i;
		}
		
		Host_Error( "CVEngineServer::PrecacheModel: '%s' overflow, too many models", s );
		return 0;
	}
	
	
	virtual int PrecacheGeneric(const char *s, bool preload /*= false*/ )
	{
		int		i;
		
		PR_CheckEmptyString (s);
		i = SV_FindOrAddGeneric( s, preload );
		if (i >= 0)
		{
			return i;
		}
		
		Host_Error ("CVEngineServer::PrecacheGeneric: '%s' overflow", s);
		return 0;
	}
	
	virtual bool IsModelPrecached( char const *s ) const
	{
		int idx = SV_ModelIndex( s );
		return idx != -1 ? true : false;
	}

	virtual bool IsDecalPrecached( char const *s ) const
	{
		int idx = SV_DecalIndex( s );
		return idx != -1 ? true : false;
	}

	virtual bool IsGenericPrecached( char const *s ) const
	{
		int idx = SV_GenericIndex( s );
		return idx != -1 ? true : false;
	}

	virtual void ForceExactFile( const char *s )
	{
		Warning( "ForceExactFile no longer supported.  Use sv_pure instead.  (%s)\n", s );
	}

	virtual void ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs )
	{
		PR_CheckEmptyString( s );
		SV_ForceModelBounds( s, mins, maxs );
	}

	virtual void ForceSimpleMaterial( const char *s )
	{
		PR_CheckEmptyString( s );
		SV_ForceSimpleMaterial( s );
	}

	virtual bool IsInternalBuild( void )
	{
		return !phonehome->IsExternalBuild();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Precache a sentence file (parse on server, send to client)
	// Input  : *s - file name
	//-----------------------------------------------------------------------------
	virtual int PrecacheSentenceFile( const char *s, bool preload /*= false*/ )
	{
		// UNDONE:  Set up preload flag
		
		// UNDONE: Send this data to the client to support multiple sentence files
		VOX_ReadSentenceFile( s );
		
		return 0;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Retrieves the pvs for an origin into the specified array
	// Input  : *org - origin
	//			outputpvslength - size of outputpvs array in bytes
	//			*outputpvs - If null, then return value is the needed length
	// Output : int - length of pvs array used ( in bytes )
	//-----------------------------------------------------------------------------
	virtual int GetClusterForOrigin( const Vector& org )
	{
		return CM_LeafCluster( CM_PointLeafnum( org ) );
	}
	
	virtual int GetPVSForCluster( int clusterIndex, int outputpvslength, unsigned char *outputpvs )
	{
		int length = (CM_NumClusters()+7)>>3;
		
		if ( outputpvs )
		{
			if ( outputpvslength < length )
			{
				Sys_Error( "GetPVSForOrigin called with inusfficient sized pvs array, need %i bytes!", length );
				return length;
			}
			
			CM_Vis( outputpvs, outputpvslength, clusterIndex, DVIS_PVS );
		}
		
		return length;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Test origin against pvs array retreived from GetPVSForOrigin
	// Input  : *org - origin to chec
	//			checkpvslength - length of pvs array
	//			*checkpvs - 
	// Output : bool - true if entity is visible
	//-----------------------------------------------------------------------------
	virtual bool CheckOriginInPVS( const Vector& org, const unsigned char *checkpvs, int checkpvssize )
	{
		int clusterIndex = CM_LeafCluster( CM_PointLeafnum( org ) );
		
		if ( clusterIndex < 0 )
			return false;
		
		int offset = clusterIndex>>3;
		if ( offset > checkpvssize )
		{
			Sys_Error( "CheckOriginInPVS:  cluster would read past end of pvs data (%i:%i)\n",
				offset, checkpvssize );
			return false;
		}
		
		if ( !(checkpvs[offset] & (1<<(clusterIndex&7)) ) )
		{
			return false;
		}
		
		return true;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Test origin against pvs array retreived from GetPVSForOrigin
	// Input  : *org - origin to chec
	//			checkpvslength - length of pvs array
	//			*checkpvs - 
	// Output : bool - true if entity is visible
	//-----------------------------------------------------------------------------
	virtual bool CheckBoxInPVS( const Vector& mins, const Vector& maxs, const unsigned char *checkpvs, int checkpvssize )
	{
		if ( !CM_BoxVisible( mins, maxs, checkpvs, checkpvssize ) )
		{
			return false;
		}
		
		return true;
	}
	
	virtual int GetPlayerUserId( const edict_t *e )
	{
		if ( !sv.IsActive() || !e)
			return -1;
		
		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *pClient = sv.Client(i);
			
			if ( pClient->edict == e )
			{
				return pClient->m_UserID;
			}
		}
		
		// Couldn't find it
		return -1;
	}

	virtual const char *GetPlayerNetworkIDString( const edict_t *e )
	{
		if ( !sv.IsActive() || !e)
			return NULL;
		
		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *pGameClient = sv.Client(i);
			
			if ( pGameClient->edict == e )
			{
				return pGameClient->GetNetworkIDString();
			}
		}
		
		// Couldn't find it
		return NULL;

	}
	
	virtual bool IsPlayerNameLocked( const edict_t *pEdict )
	{
		if ( !sv.IsActive() || !pEdict )
			return false;

		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *pClient = sv.Client( i );

			if ( pClient->edict == pEdict )
			{
				return pClient->IsPlayerNameLocked();
			}
		}

		return false;
	}

	virtual bool CanPlayerChangeName( const edict_t *pEdict )
	{
		if ( !sv.IsActive() || !pEdict )
			return false;

		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *pClient = sv.Client( i );

			if ( pClient->edict == pEdict )
			{
				return ( !pClient->IsPlayerNameLocked() && !pClient->IsNameChangeOnCooldown() );
			}
		}

		return false;
	}

	// See header comment. This is the canonical map lookup spot, and a superset of the server gameDLL's
	// CanProvideLevel/PrepareLevelResources
	virtual eFindMapResult FindMap( /* in/out */ char *pMapName, int nMapNameMax )
	{
		char szOriginalName[256] = { 0 };
		V_strncpy( szOriginalName, pMapName, sizeof( szOriginalName ) );

		IServerGameDLL::eCanProvideLevelResult eCanGameDLLProvide = IServerGameDLL::eCanProvideLevel_CannotProvide;
		if ( g_iServerGameDLLVersion >= 10 )
		{
			eCanGameDLLProvide = serverGameDLL->CanProvideLevel( pMapName, nMapNameMax );
		}

		if ( eCanGameDLLProvide == IServerGameDLL::eCanProvideLevel_Possibly )
		{
			return eFindMap_PossiblyAvailable;
		}
		else if ( eCanGameDLLProvide == IServerGameDLL::eCanProvideLevel_CanProvide )
		{
			// See if the game dll fixed up the map name
			return ( V_strcmp( szOriginalName, pMapName ) == 0 ) ? eFindMap_Found : eFindMap_NonCanonical;
		}

		AssertMsg( eCanGameDLLProvide == IServerGameDLL::eCanProvideLevel_CannotProvide,
		           "Unhandled enum member" );

		char szDiskName[MAX_PATH] = { 0 };
		// Check if we can directly use this as a map
		Host_DefaultMapFileName( pMapName, szDiskName, sizeof( szDiskName ) );
		if ( *szDiskName && modelloader->Map_IsValid( szDiskName, true ) )
		{
			return eFindMap_Found;
		}

		// Fuzzy match in map list and check file
		char match[1][64] = { {0} };
		if ( MapList_ListMaps( pMapName, false, false, 1, sizeof( match[0] ), match ) && *(match[0]) )
		{
			Host_DefaultMapFileName( match[0], szDiskName, sizeof( szDiskName ) );
			if ( modelloader->Map_IsValid( szDiskName, true ) )
			{
				V_strncpy( pMapName, match[0], nMapNameMax );
				return eFindMap_FuzzyMatch;
			}
		}

		return eFindMap_NotFound;
	}

	virtual int IndexOfEdict(const edict_t *pEdict)
	{
		if ( !pEdict )
		{
			return 0;
		}
		
		int index = (int) ( pEdict - sv.edicts );
		if ( index < 0 || index > sv.max_edicts )
		{
			Sys_Error( "Bad entity in IndexOfEdict() index %i pEdict %p sv.edicts %p\n",
				index, pEdict, sv.edicts );
		}
		
		return index;
	}
	
	
	// Returns a pointer to an entity from an index,  but only if the entity
	// is a valid DLL entity (ie. has an attached class)
	virtual edict_t* PEntityOfEntIndex(int iEntIndex)
	{
		if ( iEntIndex >= 0 && iEntIndex < sv.max_edicts )
		{
			edict_t *pEdict = EDICT_NUM( iEntIndex );
			if ( !pEdict->IsFree() )
			{
				return pEdict;
			}
		}
		
		return NULL;
	}
	
	virtual int	GetEntityCount( void )
	{
		return sv.num_edicts - sv.free_edicts;
	}
	
	

	virtual INetChannelInfo* GetPlayerNetInfo( int playerIndex )
	{
		if ( playerIndex < 1 || playerIndex > sv.GetClientCount() )
			return NULL;

		CGameClient *client = sv.Client( playerIndex - 1 );
		
		return client->m_NetChannel;
	}

	virtual edict_t* CreateEdict( int iForceEdictIndex )
	{
		edict_t	*pedict = ED_Alloc( iForceEdictIndex );
		if ( g_pServerPluginHandler )
		{
			g_pServerPluginHandler->OnEdictAllocated( pedict );
		}
		return pedict;
	}
	
	
	virtual void RemoveEdict(edict_t* ed)
	{
		if ( g_pServerPluginHandler )
		{
			g_pServerPluginHandler->OnEdictFreed( ed );
		}
		ED_Free(ed);
	}
	
	//
	// Request engine to allocate "cb" bytes on the entity's private data pointer.
	//
	virtual void *PvAllocEntPrivateData( long cb )
	{
		return calloc( 1, cb );
	}
	
	
	//
	// Release the private data memory, if any.
	//
	virtual void FreeEntPrivateData( void *pEntity )
	{
#if defined( _DEBUG ) && defined( WIN32 )
		// set the memory to a known value
		int size = _msize( pEntity );
		memset( pEntity, 0xDD, size );
#endif		
		
		if ( pEntity )
		{
			free( pEntity );
		}
	}
	
	virtual void		*SaveAllocMemory( size_t num, size_t size )
	{
#ifndef SWDS
		return ::SaveAllocMemory(num, size);
#else
		return NULL;
#endif
	}
	
	virtual void		SaveFreeMemory( void *pSaveMem )
	{
#ifndef SWDS
		::SaveFreeMemory(pSaveMem);
#endif
	}
	
	/*
	=================
	EmitAmbientSound
	
	  =================
	*/
	virtual void EmitAmbientSound( int entindex, const Vector& pos, const char *samp, float vol, 
		soundlevel_t soundlevel, int fFlags, int pitch, float soundtime /*=0.0f*/ )
	{
		SoundInfo_t sound; 
		sound.SetDefault();
		
		sound.nEntityIndex = entindex;
		sound.fVolume = vol;
		sound.Soundlevel = soundlevel;
		sound.nFlags = fFlags;
		sound.nPitch = pitch;
		sound.nChannel = CHAN_STATIC;
		sound.vOrigin = pos;
		sound.bIsAmbient = true;

		ASSERT_COORD( sound.vOrigin );
		
		// set sound delay
		
		if ( soundtime != 0.0f )
		{
			sound.fDelay = soundtime - sv.GetTime();
			sound.nFlags |= SND_DELAY;
		}
		
		// if this is a sentence, get sentence number
		if ( TestSoundChar(samp, CHAR_SENTENCE) )
		{
			sound.bIsSentence = true;
			sound.nSoundNum = Q_atoi( PSkipSoundChars(samp) );
			if ( sound.nSoundNum >= VOX_SentenceCount() )
			{
				ConMsg("EmitAmbientSound: invalid sentence number: %s", PSkipSoundChars(samp));
				return;
			}
		}
		else
		{
			// check to see if samp was properly precached
			sound.bIsSentence = false;
			sound.nSoundNum = SV_SoundIndex( samp );
			if (sound.nSoundNum <= 0)
			{
				ConMsg ("EmitAmbientSound:  sound not precached: %s\n", samp);
				return;
			}
		}

		if ( (fFlags & SND_SPAWNING) && sv.allowsignonwrites )
		{
			SVC_Sounds	sndmsg;
			char		buffer[32];

			sndmsg.m_DataOut.StartWriting(buffer, sizeof(buffer) );
			sndmsg.m_nNumSounds = 1;
			sndmsg.m_bReliableSound = true;

			SoundInfo_t	defaultSound; defaultSound.SetDefault();

			sound.WriteDelta( &defaultSound, sndmsg.m_DataOut );
			
			 // write into signon buffer
			if ( !sndmsg.WriteToBuffer( sv.m_Signon ) )
			{
				Sys_Error( "EmitAmbientSound: Init message would overflow signon buffer!\n" );
				return;
			}
		}
		else
		{
			if ( fFlags & SND_SPAWNING )
			{
				DevMsg("EmitAmbientSound: warning, broadcasting sound labled as SND_SPAWNING.\n" );
			}

			// send sound to all active players
			CEngineRecipientFilter filter;
			filter.AddAllPlayers();
			filter.MakeReliable();
			sv.BroadcastSound( sound, filter );
		}
	}
	
	
	virtual void FadeClientVolume(const edict_t *clientent,
		float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds)
	{
		int entnum = NUM_FOR_EDICT(clientent);
		
		if (entnum < 1 || entnum > sv.GetClientCount() )
		{
			ConMsg ("tried to DLL_FadeClientVolume a non-client\n");
			return;
		}
		
		IClient	*client = sv.Client(entnum-1);

		NET_StringCmd sndMsg( va("soundfade	%.1f %.1f %.1f %.1f", fadePercent, holdTime, fadeOutSeconds, fadeInSeconds ) );
				
		client->SendNetMsg( sndMsg );
	}
	
	
	//-----------------------------------------------------------------------------
	//
	// Sentence API
	//
	//-----------------------------------------------------------------------------
	
	virtual int SentenceGroupPick( int groupIndex, char *name, int nameLen )
	{
		if ( !name )
		{
			Sys_Error( "SentenceGroupPick with NULL name\n" );
		}
		
		Assert( nameLen > 0 );
		
		return VOX_GroupPick( groupIndex, name, nameLen );
	}
	
	
	virtual int SentenceGroupPickSequential( int groupIndex, char *name, int nameLen, int sentenceIndex, int reset )
	{
		if ( !name )
		{
			Sys_Error( "SentenceGroupPickSequential with NULL name\n" );
		}
		
		Assert( nameLen > 0 );
		
		return VOX_GroupPickSequential( groupIndex, name, nameLen, sentenceIndex, reset );
	}
	
	virtual int SentenceIndexFromName( const char *pSentenceName )
	{
		if ( !pSentenceName )
		{
			Sys_Error( "SentenceIndexFromName with NULL pSentenceName\n" );
		}
		
		int sentenceIndex = -1;
		
		VOX_LookupString( pSentenceName, &sentenceIndex );
		
		return sentenceIndex;
	}
	
	virtual const char *SentenceNameFromIndex( int sentenceIndex )
	{
		return VOX_SentenceNameFromIndex( sentenceIndex );
	}
	
	
	virtual int SentenceGroupIndexFromName( const char *pGroupName )
	{
		if ( !pGroupName )
		{
			Sys_Error( "SentenceGroupIndexFromName with NULL pGroupName\n" );
		}
		
		return VOX_GroupIndexFromName( pGroupName );
	}
	
	virtual const char *SentenceGroupNameFromIndex( int groupIndex )
	{
		return VOX_GroupNameFromIndex( groupIndex );
	}
	
	
	virtual float SentenceLength( int sentenceIndex )
	{
		return VOX_SentenceLength( sentenceIndex );
	}
	//-----------------------------------------------------------------------------
	
	virtual int			CheckHeadnodeVisible( int nodenum, const byte *visbits, int vissize )
	{
		return CM_HeadnodeVisible(nodenum, visbits, vissize );
	}
	
	/*
	=================
	ServerCommand
	
	  Sends text to servers execution buffer
	  
		localcmd (string)
		=================
	*/
	virtual void ServerCommand( const char *str )
	{
		if ( !str )
		{
			Sys_Error( "ServerCommand with NULL string\n" );
		}
		if ( ValidCmd( str ) )
		{
			Cbuf_AddText( str );
		}
		else
		{
			ConMsg( "Error, bad server command %s\n", str );
		}
	}
	
	
	/*
	=================
	ServerExecute
	
	  Executes all commands in server buffer
	  
		localcmd (string)
		=================
	*/
	virtual void ServerExecute( void )
	{
		Cbuf_Execute();
	}
	
	
	/*
	=================
	ClientCommand
	
	  Sends text over to the client's execution buffer
	  
		stuffcmd (clientent, value)
		=================
	*/
	virtual void ClientCommand(edict_t* pEdict, const char* szFmt, ...)
	{
		va_list		argptr; 
		static char	szOut[1024];
		
		va_start(argptr, szFmt);
		Q_vsnprintf(szOut, sizeof( szOut ), szFmt, argptr);
		va_end(argptr);

		if ( szOut[0] == 0 )
		{
			Warning( "ClientCommand, 0 length string supplied.\n" );
			return;
		}

		int entnum = NUM_FOR_EDICT( pEdict );
		
		if ( ( entnum < 1 ) || ( entnum >  sv.GetClientCount() ) )
		{
			ConMsg("\n!!!\n\nStuffCmd:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
				szOut, entnum, sv.GetMaxClients() );
			return;
		}
		
		NET_StringCmd string( szOut );
		sv.GetClient(entnum-1)->SendNetMsg( string );
		
	}

	// Send a client command keyvalues
	// keyvalues are deleted inside the function
	virtual void ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand )
	{
		if ( !pCommand )
			return;

		int entnum = NUM_FOR_EDICT( pEdict );

		if ( ( entnum < 1 ) || ( entnum >  sv.GetClientCount() ) )
		{
			ConMsg("\n!!!\n\nClientCommandKeyValues:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
				pCommand->GetName(), entnum, sv.GetMaxClients() );
			return;
		}

		SVC_CmdKeyValues cmd( pCommand );
		sv.GetClient(entnum-1)->SendNetMsg( cmd );
	}
	
	/*
	===============
	LightStyle
	
	  void(float style, string value) lightstyle
	  ===============
	*/
	virtual void LightStyle(int style, const char* val)
	{
		if ( !val )
		{
			Sys_Error( "LightStyle with NULL value!\n" );
		}

		// change the string in string table

		INetworkStringTable *stringTable = sv.GetLightStyleTable();

		stringTable->SetStringUserData( style, Q_strlen(val)+1, val );
	}
		
		
	virtual void StaticDecal( const Vector& origin, int decalIndex, int entityIndex, int modelIndex, bool lowpriority )
	{
		SVC_BSPDecal decal;
		
		decal.m_Pos = origin;
		decal.m_nDecalTextureIndex = decalIndex;
		decal.m_nEntityIndex = entityIndex;
		decal.m_nModelIndex = modelIndex;
		decal.m_bLowPriority = lowpriority;
		
		if ( sv.allowsignonwrites )
		{
			decal.WriteToBuffer( sv.m_Signon );
		}
		else
		{
			sv.BroadcastMessage( decal, false, true );
		}
	}
	
	void Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits )
	{
		SV_DetermineMulticastRecipients( usepas, origin, playerbits );
	}
	
	/*
	===============================================================================
	
	  MESSAGE WRITING
	  
		===============================================================================
	*/
	
	virtual bf_write *EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable )
	{
		if ( s_MsgData.started )
		{
			Sys_Error( "EntityMessageBegin:  New message started before matching call to EndMessage.\n " );
			return NULL;
		}
		
		s_MsgData.Reset();
		
		Assert( ent_class );
				
		s_MsgData.filter = NULL;
		s_MsgData.reliable = reliable;
		
		s_MsgData.started = true;
		
		s_MsgData.currentMsg = &s_MsgData.entityMsg;
		
		s_MsgData.entityMsg.m_nEntityIndex = ent_index;
		s_MsgData.entityMsg.m_nClassID = ent_class->m_ClassID;
		s_MsgData.entityMsg.m_DataOut.Reset();	
				
		return &s_MsgData.entityMsg.m_DataOut;
	}
	
	virtual bf_write *UserMessageBegin( IRecipientFilter *filter, int msg_index )
	{
		if ( s_MsgData.started )
		{
			Sys_Error( "UserMessageBegin:  New message started before matching call to EndMessage.\n " );
			return NULL;
		}
		
		s_MsgData.Reset();
		
		Assert( filter );

		s_MsgData.filter = filter;
		s_MsgData.reliable = filter->IsReliable();
		s_MsgData.started = true;
		
		s_MsgData.currentMsg = &s_MsgData.userMsg;
		
		s_MsgData.userMsg.m_nMsgType = msg_index;
		
		
		s_MsgData.userMsg.m_DataOut.Reset();	
		
		return &s_MsgData.userMsg.m_DataOut;
	}
	
	// Validates user message type and checks to see if it's variable length
	// returns true if variable length
	int Message_CheckMessageLength()
	{
		if ( s_MsgData.currentMsg == &s_MsgData.userMsg )
		{	
			char msgname[ 256 ];
			int  msgsize = -1;
			int  msgtype = s_MsgData.userMsg.m_nMsgType;

			if ( !serverGameDLL->GetUserMessageInfo( msgtype, msgname, sizeof(msgname), msgsize ) )
			{
				Warning( "Unable to find user message for index %i\n", msgtype );
				return -1;
			}
					
			int bytesWritten = s_MsgData.userMsg.m_DataOut.GetNumBytesWritten();
			
			if ( msgsize == -1 )
			{
				if ( bytesWritten > MAX_USER_MSG_DATA )
				{
					Warning( "DLL_MessageEnd:  Refusing to send user message %s of %i bytes to client, user message size limit is %i bytes\n",
						msgname, bytesWritten, MAX_USER_MSG_DATA );
					return -1;
				}
			}
			else if ( msgsize != bytesWritten )
			{
				Warning( "User Msg '%s': %d bytes written, expected %d\n",
					msgname, bytesWritten, msgsize );
				return -1;
			}
			
			return bytesWritten; // all checks passed, estimated final length
		}
		
		if ( s_MsgData.currentMsg == &s_MsgData.entityMsg )
		{
			int bytesWritten = s_MsgData.entityMsg.m_DataOut.GetNumBytesWritten();
			
			if ( bytesWritten > MAX_ENTITY_MSG_DATA )	// TODO use a define or so
			{
				Warning( "Entity Message to %i, %i bytes written (max is %d)\n",
					s_MsgData.entityMsg.m_nEntityIndex, bytesWritten, MAX_ENTITY_MSG_DATA );
				return -1;
			}
			
			return bytesWritten; // all checks passed, estimated final length
		}
		
		Warning( "MessageEnd unknown message type.\n" );
		return -1;
		
	}
	
	virtual void MessageEnd( void )
	{
		if ( !s_MsgData.started )
		{
			Sys_Error( "MESSAGE_END called with no active message\n" );
			return;
		}
		
		int length = Message_CheckMessageLength();
		
		// check to see if it's a valid message
		if ( length < 0 )
		{
			s_MsgData.Reset(); // clear message data
			return;
		}

		if ( s_MsgData.filter )
		{
			// send entity/user messages only to full connected clients in filter
			sv.BroadcastMessage( *s_MsgData.currentMsg, *s_MsgData.filter );
		}
		else
		{
			// send entity messages to all full connected clients 
			sv.BroadcastMessage( *s_MsgData.currentMsg, true, s_MsgData.reliable );
		}
		
		s_MsgData.Reset(); // clear message data
	}
	
	/* single print to a specific client */
	virtual void ClientPrintf( edict_t *pEdict, const char *szMsg )
	{
		int entnum = NUM_FOR_EDICT( pEdict );
		
		if (entnum < 1 || entnum > sv.GetClientCount() )
		{
			ConMsg ("tried to sprint to a non-client\n");
			return;
		}
		
		sv.Client(entnum-1)->ClientPrintf( "%s", szMsg );
	}
	
#ifdef SWDS
	void Con_NPrintf( int pos, const char *fmt, ... )
	{
	}

	void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
	{
	}
#else

	void Con_NPrintf( int pos, const char *fmt, ... )
	{
		if ( IsDedicatedServer() )
			return;

		va_list		argptr;
		char		text[4096];
		va_start (argptr, fmt);
		Q_vsnprintf(text, sizeof( text ), fmt, argptr);
		va_end (argptr);

		::Con_NPrintf( pos, "%s", text );
	}

	void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
	{
		if ( IsDedicatedServer() )
			return;

		va_list		argptr;
		char		text[4096];
		va_start (argptr, fmt);
		Q_vsnprintf(text, sizeof( text ), fmt, argptr);
		va_end (argptr);

		::Con_NXPrintf( info, "%s", text );
	}
#endif

	virtual void SetView(const edict_t *clientent, const edict_t *viewent)
	{
		int clientnum = NUM_FOR_EDICT( clientent );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_SetView: not a client");
		
		CGameClient *client = sv.Client(clientnum-1);

		client->m_pViewEntity = viewent;
		
		SVC_SetView view( NUM_FOR_EDICT(viewent) );
		client->SendNetMsg( view );
	}
	
	virtual float Time(void)
	{
		return Sys_FloatTime();
	}
	
	virtual void CrosshairAngle(const edict_t *clientent, float pitch, float yaw)
	{
		int clientnum = NUM_FOR_EDICT( clientent );

		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_Crosshairangle: not a client");
		
		IClient *client = sv.Client(clientnum-1);

		if (pitch > 180)
			pitch -= 360;
		if (pitch < -180)
			pitch += 360;
		if (yaw > 180)
			yaw -= 360;
		if (yaw < -180)
			yaw += 360;
		
		SVC_CrosshairAngle crossHairMsg;
		
		crossHairMsg.m_Angle.x = pitch;
		crossHairMsg.m_Angle.y = yaw;
		crossHairMsg.m_Angle.y = 0;
		
		client->SendNetMsg( crossHairMsg );
	}
	
	
	virtual void GetGameDir( char *szGetGameDir, int maxlength )
	{
		COM_GetGameDir(szGetGameDir, maxlength );
	}		
	
	virtual int CompareFileTime( const char *filename1, const char *filename2, int *iCompare)
	{
		return COM_CompareFileTime(filename1, filename2, iCompare);
	}
	
	virtual bool LockNetworkStringTables( bool lock )
	{
		return networkStringTableContainerServer->Lock( lock );
	}

	// For use with FAKE CLIENTS
	virtual edict_t* CreateFakeClient( const char *netname )
	{
		CGameClient *fcl = static_cast<CGameClient*>( sv.CreateFakeClient( netname ) );
		if ( !fcl )
		{
			// server is full
			return NULL;
		}

		return fcl->edict;
	}

	// For use with FAKE CLIENTS
	virtual edict_t* CreateFakeClientEx( const char *netname, bool bReportFakeClient /*= true*/ )
	{
		sv.SetReportNewFakeClients( bReportFakeClient );
		edict_t *ret = CreateFakeClient( netname );
		sv.SetReportNewFakeClients( true ); // Leave this set as true so other callers of sv.CreateFakeClient behave correctly.

		return ret;
	}
	
	// Get a keyvalue for s specified client
	virtual const char *GetClientConVarValue( int clientIndex, const char *name )
	{
		if ( clientIndex < 1 || clientIndex > sv.GetClientCount() )
		{
			DevMsg( 1, "GetClientConVarValue: player invalid index %i\n", clientIndex );
			return "";
		}

		return sv.GetClient( clientIndex - 1 )->GetUserSetting( name );
	}
	
	virtual const char *ParseFile(const char *data, char *token, int maxlen)
	{
		return ::COM_ParseFile(data, token, maxlen );
	}

	virtual bool CopyFile( const char *source, const char *destination )
	{
		return ::COM_CopyFile( source, destination );
	}
	
	virtual void AddOriginToPVS( const Vector& origin )
	{
		::SV_AddOriginToPVS(origin);
	}
	
	virtual void ResetPVS( byte* pvs, int pvssize )
	{
		::SV_ResetPVS( pvs, pvssize );
	}
	
	virtual void		SetAreaPortalState( int portalNumber, int isOpen )
	{
		CM_SetAreaPortalState(portalNumber, isOpen);
	}

	virtual void		SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals )
	{
		CM_SetAreaPortalStates( portalNumbers, isOpen, nPortals );
	}

	virtual void		DrawMapToScratchPad( IScratchPad3D *pPad, unsigned long iFlags )
	{
		worldbrushdata_t *pData = host_state.worldmodel->brush.pShared;
		if ( !pData )
			return;

		SurfaceHandle_t surfID = SurfaceHandleFromIndex( host_state.worldmodel->brush.firstmodelsurface, pData );
		for (int i=0; i< host_state.worldmodel->brush.nummodelsurfaces; ++i, ++surfID)
		{
			// Don't bother with nodraw surfaces
			if( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
				continue;

			CSPVertList vertList;
			for ( int iVert=0; iVert < MSurf_VertCount( surfID ); iVert++ )
			{
				int iWorldVert = pData->vertindices[surfID->firstvertindex + iVert];
				const Vector &vPos = pData->vertexes[iWorldVert].position;

				vertList.m_Verts.AddToTail( CSPVert( vPos ) );
			}

			pPad->DrawPolygon( vertList );
		}
	}

	const CBitVec<MAX_EDICTS>* GetEntityTransmitBitsForClient( int iClientIndex )
	{
		if ( iClientIndex < 0 || iClientIndex >= sv.GetClientCount() )
		{
			Assert( false );
			return NULL;
		}

		CGameClient *pClient = sv.Client( iClientIndex );
		CClientFrame *deltaFrame = pClient->GetClientFrame( pClient->m_nDeltaTick );
		if ( !deltaFrame )
			return NULL;

		return &deltaFrame->transmit_entity;
	}

	virtual bool IsPaused()
	{
		return sv.IsPaused();
	}

	virtual void SetFakeClientConVarValue( edict_t *pEntity, const char *pCvarName, const char *value )
	{
		int clientnum = NUM_FOR_EDICT( pEntity );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_SetView: not a client");

		CGameClient *client = sv.Client(clientnum-1);
		if ( client->IsFakeClient() )
		{
			client->SetUserCVar( pCvarName, value );
			client->m_bConVarsChanged = true;
		}
	}

	virtual CSharedEdictChangeInfo* GetSharedEdictChangeInfo()
	{
		return &g_SharedEdictChangeInfo;
	}

	virtual IChangeInfoAccessor *GetChangeAccessor( const edict_t *pEdict )
	{
		return &sv.edictchangeinfo[ NUM_FOR_EDICT( pEdict ) ];
	}

	virtual QueryCvarCookie_t StartQueryCvarValue( edict_t *pPlayerEntity, const char *pCvarName )
	{
		int clientnum = NUM_FOR_EDICT( pPlayerEntity );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error( "StartQueryCvarValue: not a client" );

		CGameClient *client = sv.Client( clientnum-1 );
		return SendCvarValueQueryToClient( client, pCvarName, false );
	}

	// Name of most recently load .sav file
	virtual char const *GetMostRecentlyLoadedFileName()
	{
#if !defined( SWDS )
		return saverestore->GetMostRecentlyLoadedFileName();
#else
		return "";
#endif
	}

	virtual char const *GetSaveFileName()
	{
#if !defined( SWDS )
		return saverestore->GetSaveFileName();
#else
		return "";
#endif
	}

	// Tells the engine we can immdiately re-use all edict indices
	// even though we may not have waited enough time
	virtual void AllowImmediateEdictReuse( )
	{
		ED_AllowImmediateReuse();
	}
			
	virtual void MultiplayerEndGame()
	{
#if !defined( SWDS )
		g_pMatchmaking->EndGame();
#endif
	}

	virtual void ChangeTeam( const char *pTeamName )
	{
#if !defined( SWDS )
		g_pMatchmaking->ChangeTeam( pTeamName );
#endif
	}

	virtual void SetAchievementMgr( IAchievementMgr *pAchievementMgr )
	{
		g_pAchievementMgr = pAchievementMgr;
	}
	
	virtual IAchievementMgr *GetAchievementMgr() 
	{
		return g_pAchievementMgr;
	}

	virtual int GetAppID()
	{
		return GetSteamAppID();
	}
	
	virtual bool IsLowViolence();

	/*
	=================
	InsertServerCommand
	
	  Sends text to servers execution buffer
	  
		localcmd (string)
		=================
	*/
	virtual void InsertServerCommand( const char *str )
	{
		if ( !str )
		{
			Sys_Error( "InsertServerCommand with NULL string\n" );
		}
		if ( ValidCmd( str ) )
		{
			Cbuf_InsertText( str );
		}
		else
		{
			ConMsg( "Error, bad server command %s (InsertServerCommand)\n", str );
		}
	}

	bool GetPlayerInfo( int ent_num, player_info_t *pinfo )
	{
		// Entity numbers are offset by 1 from the player numbers
		return sv.GetPlayerInfo( (ent_num-1), pinfo );
	}

	bool IsClientFullyAuthenticated( edict_t *pEdict )
	{
		int entnum = NUM_FOR_EDICT( pEdict );
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return false;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( client )
			return client->IsFullyAuthenticated();

		return false;
	}

	void SetDedicatedServerBenchmarkMode( bool bBenchmarkMode )
	{
		g_bDedicatedServerBenchmarkMode = bBenchmarkMode;
		if ( bBenchmarkMode )
		{
			extern ConVar sv_stressbots;
			sv_stressbots.SetValue( (int)1 );
		}
	}

	// Returns the SteamID of the game server
	const CSteamID	*GetGameServerSteamID()
	{
		if ( !Steam3Server().GetGSSteamID().IsValid() )
			return NULL;

		return &Steam3Server().GetGSSteamID();
	}

	// Returns the SteamID of the specified player. It'll be NULL if the player hasn't authenticated yet.
	const CSteamID	*GetClientSteamID( edict_t *pPlayerEdict )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		return GetClientSteamIDByPlayerIndex( entnum );
	}
	
	const CSteamID	*GetClientSteamIDByPlayerIndex( int entnum )
	{
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return NULL;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( !client )
			return NULL;

		// Make sure they are connected and Steam ID is valid
		if ( !client->IsConnected() || !client->m_SteamID.IsValid() )
			return NULL;

		return &client->m_SteamID;
	}
	
	void SetGamestatsData( CGamestatsData *pGamestatsData )
	{
		g_pGamestatsData = pGamestatsData;
	}

	CGamestatsData *GetGamestatsData()
	{
		return g_pGamestatsData;
	}

	virtual IReplaySystem *GetReplay()
	{
		return g_pReplay;
	}

	virtual int GetClusterCount()
	{
		CCollisionBSPData *pBSPData = GetCollisionBSPData();
		if ( pBSPData && pBSPData->map_vis )
			return pBSPData->map_vis->numclusters;
		return 0;
	}

	virtual int GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox )
	{
		CCollisionBSPData *pBSPData = GetCollisionBSPData();
		if ( pBSPData && pBSPData->map_vis && host_state.worldbrush )
		{
			// clamp to max clusters in the map
			if ( maxBBox > pBSPData->map_vis->numclusters )
			{
				maxBBox = pBSPData->map_vis->numclusters;
			}
			// reset all of the bboxes
			for ( int i =  0; i < maxBBox; i++ )
			{
				ClearBounds( pBBoxList[i].mins, pBBoxList[i].maxs );
			}
			// add each leaf's bounds to the bounds for that cluster
			for ( int i = 0; i < host_state.worldbrush->numleafs; i++ )
			{
				mleaf_t *pLeaf = &host_state.worldbrush->leafs[i];
				// skip solid leaves and leaves with cluster < 0
				if ( !(pLeaf->contents & CONTENTS_SOLID) && pLeaf->cluster >= 0 && pLeaf->cluster < maxBBox )
				{
					Vector mins, maxs;
					mins = pLeaf->m_vecCenter - pLeaf->m_vecHalfDiagonal;
					maxs = pLeaf->m_vecCenter + pLeaf->m_vecHalfDiagonal;
					AddPointToBounds( mins, pBBoxList[pLeaf->cluster].mins, pBBoxList[pLeaf->cluster].maxs );
					AddPointToBounds( maxs, pBBoxList[pLeaf->cluster].mins, pBBoxList[pLeaf->cluster].maxs );
				}
			}

			return pBSPData->map_vis->numclusters;
		}
		return 0;
	}

	virtual int GetServerVersion() const OVERRIDE
	{
		return GetSteamInfIDVersionInfo().ServerVersion;
	}

	virtual float GetServerTime() const OVERRIDE
	{
		return sv.GetTime();
	}

	virtual IServer *GetIServer() OVERRIDE
	{
		return (IServer *)&sv;
	}

	virtual void SetPausedForced( bool bPaused, float flDuration /*= -1.f*/ ) OVERRIDE
	{
		sv.SetPausedForced( bPaused, flDuration );
	}

private:
	
	// Purpose: Sends a temp entity to the client ( follows the format of the original MESSAGE_BEGIN stuff from HL1
	virtual void PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  );
	virtual int	CheckAreasConnected( int area1, int area2 );
	virtual int GetArea( const Vector& origin );
	virtual void GetAreaBits( int area, unsigned char *bits, int buflen );
	virtual bool GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane );
	virtual client_textmessage_t *TextMessageGet( const char *pName );
	virtual void LogPrint(const char * msg);
	virtual bool LoadGameState( char const *pMapName, bool createPlayers );
	virtual void LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
	virtual void ClearSaveDir();
	virtual void ClearSaveDirAfterClientLoad();

	virtual const char* GetMapEntitiesString();
	virtual void BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo );
	virtual void CleanUpEntityClusterList( PVSInfo_t *pPVSInfo );
	virtual void SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks );
	virtual void TriggerMoved( edict_t *pTriggerEnt, bool accurateBboxTriggerChecks );

	virtual ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax ) { return ::CreateSpatialPartition( worldmin, worldmax );	}
	virtual void 		DestroySpatialPartition( ISpatialPartition *pPartition )						{ ::DestroySpatialPartition( pPartition );					}
};

// Backwards-compat shim that inherits newest then provides overrides for the legacy behavior
class CVEngineServer22 : public CVEngineServer
{
	virtual int	IsMapValid( const char *filename ) OVERRIDE
	{
		// For users of the older interface, preserve here the old modelloader behavior of wrapping maps/%.bsp around
		// the filename. This went away in newer interfaces since maps can now live in other places.
		char szWrappedName[MAX_PATH] = { 0 };
		V_snprintf( szWrappedName, sizeof( szWrappedName ), "maps/%s.bsp", filename );

		return modelloader->Map_IsValid( szWrappedName );
	}
};

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the game DLL.
//-----------------------------------------------------------------------------
static CVEngineServer   g_VEngineServer;
static CVEngineServer22 g_VEngineServer22;
// INTERFACEVERSION_VENGINESERVER_VERSION_21 is compatible with 22 latest since we only added virtuals to the end, so expose that as well.
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVEngineServer, IVEngineServer021, INTERFACEVERSION_VENGINESERVER_VERSION_21, g_VEngineServer22 );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVEngineServer, IVEngineServer022, INTERFACEVERSION_VENGINESERVER_VERSION_22, g_VEngineServer22 );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVEngineServer, IVEngineServer, INTERFACEVERSION_VENGINESERVER, g_VEngineServer );

// When bumping the version to this interface, check that our assumption is still valid and expose the older version in the same way
COMPILE_TIME_ASSERT( INTERFACEVERSION_VENGINESERVER_INT == 23 );

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the engine.
//-----------------------------------------------------------------------------
IVEngineServer *g_pVEngineServer = &g_VEngineServer;


//-----------------------------------------------------------------------------
// Used to allocate pvs infos
//-----------------------------------------------------------------------------
static CUtlMemoryPool s_PVSInfoAllocator( 128, 128 * 64, CUtlMemoryPool::GROW_SLOW, "pvsinfopool", 128 );


//-----------------------------------------------------------------------------
// Purpose: Sends a temp entity to the client ( follows the format of the original MESSAGE_BEGIN stuff from HL1
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pSender - 
//			*pST - 
//			classID - 
//-----------------------------------------------------------------------------
void CVEngineServer::PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  )
{
	VPROF( "PlaybackTempEntity" );

	// don't add more events to a snapshot than a client can receive
	if ( sv.m_TempEntities.Count() >= ((1<<CEventInfo::EVENT_INDEX_BITS)-1) )
	{
		// remove oldest effect
		delete sv.m_TempEntities[0]; 
		sv.m_TempEntities.Remove( 0 );
	}
	
	// Make this start at 1
	classID = classID + 1;

	// Encode now!
	ALIGN4 unsigned char data[ CEventInfo::MAX_EVENT_DATA ] ALIGN4_POST;
	bf_write buffer( "PlaybackTempEntity", data, sizeof(data) );

	// write all properties, if init or reliable message delta against zero values
	if( !SendTable_Encode( pST, pSender, &buffer, classID, NULL, false ) )
	{
		Host_Error( "PlaybackTempEntity: SendTable_Encode returned false (ent %d), overflow? %i\n", classID, buffer.IsOverflowed() ? 1 : 0 );
		return;
	}
	
	// create CEventInfo:
	CEventInfo *newEvent = new CEventInfo;

	//copy client filter
	newEvent->filter.AddPlayersFromFilter( &filter );

	newEvent->classID	= classID;
	newEvent->pSendTable= pST;
	newEvent->fire_delay= delay;

	newEvent->bits = buffer.GetNumBitsWritten();
	int size = Bits2Bytes( buffer.GetNumBitsWritten() );
	newEvent->pData = new byte[size];
	Q_memcpy( newEvent->pData, data, size );

	// add to list
	sv.m_TempEntities[sv.m_TempEntities.AddToTail()] = newEvent;
}

int	CVEngineServer::CheckAreasConnected( int area1, int area2 )
{
	return CM_AreasConnected(area1, area2);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *origin - 
//			*bits - 
// Output : void
//-----------------------------------------------------------------------------
int CVEngineServer::GetArea( const Vector& origin )
{
	return CM_LeafArea( CM_PointLeafnum( origin ) );
}

void CVEngineServer::GetAreaBits( int area, unsigned char *bits, int buflen )
{
	CM_WriteAreaBits( bits, buflen, area );
}

bool CVEngineServer::GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane )
{
	return CM_GetAreaPortalPlane( vViewOrigin, portalKey, pPlane );
}

client_textmessage_t *CVEngineServer::TextMessageGet( const char *pName )
{
	return ::TextMessageGet( pName );
}

void CVEngineServer::LogPrint(const char * msg)
{
	g_Log.Print( msg );
}

// HACKHACK: Save/restore wrapper - Move this to a different interface
bool CVEngineServer::LoadGameState( char const *pMapName, bool createPlayers )
{
#ifndef SWDS
	return saverestore->LoadGameState( pMapName, createPlayers ) != 0;
#else
	return 0;
#endif
}

void CVEngineServer::LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )
{
#ifndef SWDS
	saverestore->LoadAdjacentEnts( pOldLevel, pLandmarkName );
#endif
}

void CVEngineServer::ClearSaveDir()
{
#ifndef SWDS
	saverestore->ClearSaveDir();
#endif
}

void CVEngineServer::ClearSaveDirAfterClientLoad()
{
#ifndef SWDS
	saverestore->RequestClearSaveDir();
#endif
}


const char* CVEngineServer::GetMapEntitiesString()
{
	return CM_EntityString();
}

//-----------------------------------------------------------------------------
// Builds PVS information for an entity
//-----------------------------------------------------------------------------
inline bool SortClusterLessFunc( const int &left, const int &right )
{
	return left < right;
}

void CVEngineServer::BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo )
{
	int		i, j;
	int		topnode;
	int		leafCount;
	int		leafs[MAX_TOTAL_ENT_LEAFS], clusters[MAX_TOTAL_ENT_LEAFS];
	int		area;

	CleanUpEntityClusterList( pPVSInfo );
	pPVSInfo->m_pClusters = 0;
	pPVSInfo->m_nClusterCount = 0;
	pPVSInfo->m_nAreaNum = 0;
	pPVSInfo->m_nAreaNum2 = 0;
	if ( !pEdict )
		return;

	ICollideable *pCollideable = pEdict->GetCollideable();
	Assert( pCollideable );
	if ( !pCollideable )
		return;

	topnode = -1;

	//get all leafs, including solids
	Vector vecWorldMins, vecWorldMaxs;
	pCollideable->WorldSpaceSurroundingBounds( &vecWorldMins, &vecWorldMaxs );
	leafCount = CM_BoxLeafnums( vecWorldMins, vecWorldMaxs, leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// set areas
	for ( i = 0; i < leafCount; i++ )
	{
		clusters[i] = CM_LeafCluster( leafs[i] );
		area = CM_LeafArea( leafs[i] );
		if ( area == 0 )
			continue;

		// doors may legally straggle two areas,
		// but nothing should ever need more than that
		if ( pPVSInfo->m_nAreaNum && pPVSInfo->m_nAreaNum != area )
		{
			if ( pPVSInfo->m_nAreaNum2 && pPVSInfo->m_nAreaNum2 != area && sv.IsLoading() )
			{
				ConDMsg ("Object touching 3 areas at %f %f %f\n",
					vecWorldMins[0], vecWorldMins[1], vecWorldMins[2]);
			}
			pPVSInfo->m_nAreaNum2 = area;
		}
		else
		{
			pPVSInfo->m_nAreaNum = area;
		}
	}

	Vector center = (vecWorldMins+vecWorldMaxs) * 0.5f; // calc center

	pPVSInfo->m_nHeadNode = topnode;	// save headnode

	// save origin
	pPVSInfo->m_vCenter[0] = center[0];
	pPVSInfo->m_vCenter[1] = center[1];
	pPVSInfo->m_vCenter[2] = center[2];

	if ( leafCount >= MAX_TOTAL_ENT_LEAFS )
	{
		// assume we missed some leafs, and mark by headnode
		pPVSInfo->m_nClusterCount = -1;
		return;
	}

	pPVSInfo->m_pClusters = pPVSInfo->m_pClustersInline;
	if ( leafCount >= 16 )
	{
		std::make_heap( clusters, clusters + leafCount, SortClusterLessFunc ); 
		std::sort_heap( clusters, clusters + leafCount, SortClusterLessFunc ); 
		for ( i = 0; i < leafCount; i++ )
		{
			if ( clusters[i] == -1 )
				continue;		// not a visible leaf

			if ( ( i > 0 ) && ( clusters[i] == clusters[i-1] ) )
				continue;

			if ( pPVSInfo->m_nClusterCount == MAX_FAST_ENT_CLUSTERS )
			{
				unsigned short *pClusters = (unsigned short *)s_PVSInfoAllocator.Alloc();
				memcpy( pClusters, pPVSInfo->m_pClusters, MAX_FAST_ENT_CLUSTERS * sizeof(unsigned short) );
				pPVSInfo->m_pClusters = pClusters;
			}
			else if ( pPVSInfo->m_nClusterCount == MAX_ENT_CLUSTERS )
			{
				// assume we missed some leafs, and mark by headnode
				s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
				pPVSInfo->m_pClusters = 0;
				pPVSInfo->m_nClusterCount = -1;
				break;
			}

			pPVSInfo->m_pClusters[pPVSInfo->m_nClusterCount++] = (short)clusters[i];
		}
		return;
	}

	for ( i = 0; i < leafCount; i++ )
	{
		if ( clusters[i] == -1 )
			continue;		// not a visible leaf

		for ( j = 0; j < i; j++ )
		{
			if ( clusters[j] == clusters[i] )
				break;
		}

		if ( j != i )
			continue;

		if ( pPVSInfo->m_nClusterCount == MAX_FAST_ENT_CLUSTERS )
		{
			unsigned short *pClusters = (unsigned short*)s_PVSInfoAllocator.Alloc();
			memcpy( pClusters, pPVSInfo->m_pClusters, MAX_FAST_ENT_CLUSTERS * sizeof(unsigned short) );
			pPVSInfo->m_pClusters = pClusters;
		}
		else if ( pPVSInfo->m_nClusterCount == MAX_ENT_CLUSTERS )
		{
			// assume we missed some leafs, and mark by headnode
			s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
			pPVSInfo->m_pClusters = 0;
			pPVSInfo->m_nClusterCount = -1;
			break;
		}

		pPVSInfo->m_pClusters[pPVSInfo->m_nClusterCount++] = (short)clusters[i];
	}
}


//-----------------------------------------------------------------------------
// Cleans up the cluster list
//-----------------------------------------------------------------------------
void CVEngineServer::CleanUpEntityClusterList( PVSInfo_t *pPVSInfo )
{
	if ( pPVSInfo->m_nClusterCount > MAX_FAST_ENT_CLUSTERS )
	{
		s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
		pPVSInfo->m_pClusters = 0;
		pPVSInfo->m_nClusterCount = 0;
	}
}


//-----------------------------------------------------------------------------
// Adds a handle to the list of entities to update when a partition query occurs
//-----------------------------------------------------------------------------
void CVEngineServer::SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{
	SV_SolidMoved( pSolidEnt, pSolidCollide, pPrevAbsOrigin, accurateBboxTriggerChecks );
}

void CVEngineServer::TriggerMoved( edict_t *pTriggerEnt, bool accurateBboxTriggerChecks )
{
	SV_TriggerMoved( pTriggerEnt, accurateBboxTriggerChecks );
}


//-----------------------------------------------------------------------------
// Called by the server to determine violence settings.
//-----------------------------------------------------------------------------
bool CVEngineServer::IsLowViolence()
{
	return g_bLowViolence;
}
