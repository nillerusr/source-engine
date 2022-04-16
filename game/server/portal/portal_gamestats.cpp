//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "portal_gamestats.h"
#include "tier1/utlbuffer.h"
#include "portal_player.h"

#define PORTALSTATS_TRIMEVENT( varName, varType )\
	if( varName->Count() > varType::TRIMSIZE )\
		varName->RemoveMultiple( 0, (varName->Count() - varType::TRIMSIZE) );

#define PORTALSTATS_PREPCHUNK( structType, bufName, SizePositionVarNameToUse )\
	SaveBuffer.PutUnsignedShort( structType::CHUNKID );\
	int SizePositionVarNameToUse = bufName.TellPut();\
	bufName.PutUnsignedInt( 0 );

#define PORTALSTATS_WRITECHUNKSIZE( bufName, SizePositionVariable ) \
{\
	int SizePositionVariable ## _askdjbhas = bufName.TellPut() - SizePositionVariable;\
	bufName.SeekPut( CUtlBuffer::SEEK_HEAD, SizePositionVariable );\
	bufName.PutUnsignedInt( SizePositionVariable ## _askdjbhas );\
	bufName.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );\
}

static Portal_Gamestats_LevelStats_t s_DummyStats;
CPortalGameStats g_PortalGameStats;

class CPortalGameStatsSingleton //used to remove the constructor destructor from the general class
{
public:
	CPortalGameStatsSingleton( void )
	{
		gamestats = (CBaseGameStats *)&g_PortalGameStats;
		CreateLevelStatPointers( &s_DummyStats );
	}

	~CPortalGameStatsSingleton( void )
	{
		AssertMsg( (CBaseGameStats::StatTrackingAllowed() == false) ||
			((s_DummyStats.m_pDeaths->Count() == 0) &&
			(s_DummyStats.m_pPlacements->Count() == 0) &&
			(s_DummyStats.m_pUseEvents->Count() == 0) &&
			(s_DummyStats.m_pStuckSpots->Count() == 0) &&
			(s_DummyStats.m_pJumps->Count() == 0) &&
			(s_DummyStats.m_pTimeSpentInVisLeafs->Count() == 0)), 
			"Some stats were deferred to the dummy entry." );
		DestroyLevelStatPointers( &s_DummyStats );
	}
};
CPortalGameStatsSingleton s_CPGSS_ThisJustSitsInMemory;

CPortalGameStats::CPortalGameStats( void )
: m_pCurrentMapStats( &s_DummyStats )
{

}

CPortalGameStats::~CPortalGameStats( void )
{
	Clear();
}



void Portal_Gamestats_LevelStats_t::AppendSubChunksToBuffer( CUtlBuffer &SaveBuffer )
{
	if( m_pDeaths->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pDeaths, PlayerDeaths_t );
		PORTALSTATS_PREPCHUNK( PlayerDeaths_t, SaveBuffer, iSubChunkSizePosition );

		int iDeathStatsCount = m_pDeaths->Count();
		SaveBuffer.PutUnsignedInt( iDeathStatsCount );
		for( int i = 0; i != iDeathStatsCount; ++i )
		{
			PlayerDeaths_t &DeathStat = m_pDeaths->Element( i );
			
			SaveBuffer.PutFloat( DeathStat.ptPositionOfDeath.x );
			SaveBuffer.PutFloat( DeathStat.ptPositionOfDeath.y );
			SaveBuffer.PutFloat( DeathStat.ptPositionOfDeath.z );
			SaveBuffer.PutInt( DeathStat.iDamageType );
			SaveBuffer.PutString( DeathStat.szAttackerClassName );
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}

	if( m_pPlacements->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pPlacements, PortalPlacement_t );
		PORTALSTATS_PREPCHUNK( PortalPlacement_t, SaveBuffer, iSubChunkSizePosition );

		int iPlacementStatsCount = m_pPlacements->Count();
		SaveBuffer.PutUnsignedInt( iPlacementStatsCount );
		for( int i = 0; i != iPlacementStatsCount; ++i )
		{
			PortalPlacement_t &PortalPlacementStat = m_pPlacements->Element( i );

			SaveBuffer.PutFloat( PortalPlacementStat.ptPlayerFiredFrom.x );
			SaveBuffer.PutFloat( PortalPlacementStat.ptPlayerFiredFrom.y );
			SaveBuffer.PutFloat( PortalPlacementStat.ptPlayerFiredFrom.z );

			SaveBuffer.PutFloat( PortalPlacementStat.ptPlacementPosition.x );
			SaveBuffer.PutFloat( PortalPlacementStat.ptPlacementPosition.y );
			SaveBuffer.PutFloat( PortalPlacementStat.ptPlacementPosition.z );

			SaveBuffer.PutChar( PortalPlacementStat.iSuccessCode );
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}

	if( m_pUseEvents->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pUseEvents, PlayerUse_t );
		PORTALSTATS_PREPCHUNK( PlayerUse_t, SaveBuffer, iSubChunkSizePosition );

		int iUseEventCount = m_pUseEvents->Count();
		SaveBuffer.PutUnsignedInt( iUseEventCount );
		for( int i = 0; i != iUseEventCount; ++i )
		{
			PlayerUse_t &UseEvent = m_pUseEvents->Element( i );

			SaveBuffer.PutFloat( UseEvent.ptTraceStart.x );
			SaveBuffer.PutFloat( UseEvent.ptTraceStart.y );
			SaveBuffer.PutFloat( UseEvent.ptTraceStart.z );

			SaveBuffer.PutFloat( UseEvent.vTraceDelta.x );
			SaveBuffer.PutFloat( UseEvent.vTraceDelta.y );
			SaveBuffer.PutFloat( UseEvent.vTraceDelta.z );

			SaveBuffer.PutString( UseEvent.szUseEntityClassName );
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}

	if( m_pStuckSpots->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pStuckSpots, StuckEvent_t );
		PORTALSTATS_PREPCHUNK( StuckEvent_t, SaveBuffer, iSubChunkSizePosition );

		int iStuckStatsCount = m_pStuckSpots->Count();
		SaveBuffer.PutUnsignedInt( iStuckStatsCount );
		for( int i = 0; i != iStuckStatsCount; ++i )
		{
			StuckEvent_t &StuckStat = m_pStuckSpots->Element( i );

			SaveBuffer.PutFloat( StuckStat.ptPlayerPosition.x );
			SaveBuffer.PutFloat( StuckStat.ptPlayerPosition.y );
			SaveBuffer.PutFloat( StuckStat.ptPlayerPosition.z );

			SaveBuffer.PutFloat( StuckStat.qPlayerAngles.x );
			SaveBuffer.PutFloat( StuckStat.qPlayerAngles.y );
			SaveBuffer.PutFloat( StuckStat.qPlayerAngles.z );

			unsigned char bitFlags = 0;
			if( StuckStat.bNearPortal )
				bitFlags |= (1 << 0);

			if( StuckStat.bDucking )
				bitFlags |= (1 << 1);

			SaveBuffer.PutUnsignedChar( bitFlags );			
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}

	if( m_pJumps->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pJumps, JumpEvent_t );
		PORTALSTATS_PREPCHUNK( JumpEvent_t, SaveBuffer, iSubChunkSizePosition );

		int iJumpStatsCount = m_pJumps->Count();
		SaveBuffer.PutUnsignedInt( iJumpStatsCount );
		for( int i = 0; i != iJumpStatsCount; ++i )
		{
			JumpEvent_t &JumpStat = m_pJumps->Element( i );

			SaveBuffer.PutFloat( JumpStat.ptPlayerPositionAtJumpStart.x );
			SaveBuffer.PutFloat( JumpStat.ptPlayerPositionAtJumpStart.y );
			SaveBuffer.PutFloat( JumpStat.ptPlayerPositionAtJumpStart.z );

			SaveBuffer.PutFloat( JumpStat.vPlayerVelocityAtJumpStart.x );
			SaveBuffer.PutFloat( JumpStat.vPlayerVelocityAtJumpStart.y );
			SaveBuffer.PutFloat( JumpStat.vPlayerVelocityAtJumpStart.z );
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}
	
	if( m_pTimeSpentInVisLeafs->Count() != 0 )
	{
		PORTALSTATS_TRIMEVENT( m_pTimeSpentInVisLeafs, LeafTimes_t );
		PORTALSTATS_PREPCHUNK( LeafTimes_t, SaveBuffer, iSubChunkSizePosition );

		int iLeafTimeStatsCount = m_pTimeSpentInVisLeafs->Count();
		SaveBuffer.PutUnsignedInt( iLeafTimeStatsCount );
		for( int i = 0; i != iLeafTimeStatsCount; ++i )
		{
			LeafTimes_t &LeafTimeStat = m_pTimeSpentInVisLeafs->Element( i );

			SaveBuffer.PutFloat( LeafTimeStat.fTimeSpentInVisLeaf ); //assumes visleafs will be the same when this data is loaded again, or that there will be a way to invalidate the data
		}

		PORTALSTATS_WRITECHUNKSIZE( SaveBuffer, iSubChunkSizePosition );
	}
}

void Portal_Gamestats_LevelStats_t::LoadSubChunksFromBuffer( CUtlBuffer &LoadBuffer, unsigned int iChunkEndPosition )
{
	Clear();

	while( ((unsigned int)LoadBuffer.TellGet()) != iChunkEndPosition )
	{
		Assert( (iChunkEndPosition - LoadBuffer.TellGet()) > (sizeof( unsigned short ) + sizeof( unsigned int )) ); //at least an empty chunk left

		unsigned short iChunkID = LoadBuffer.GetUnsignedShort();
		unsigned int iChunkSize = LoadBuffer.GetUnsignedInt() - sizeof( unsigned int ); //chunk size includes the chunk size data itself
#ifdef _DEBUG
		unsigned int iChunkEndPosition = LoadBuffer.TellGet() + iChunkSize; //used in an assert later
#endif

		
		switch( iChunkID )
		{
#ifdef PORTAL_GAMESTATS_VERBOSE //don't bother loading verbose chunks if we're not going to save them back out
		case PlayerDeaths_t::CHUNKID:
			{
				unsigned int iDeathStatCount = LoadBuffer.GetUnsignedInt();
				for( unsigned int i = 0; i != iDeathStatCount; ++i )
				{
					int index = m_pDeaths->AddToTail();
					PlayerDeaths_t &DeathStat = m_pDeaths->Element( index );

					DeathStat.ptPositionOfDeath.x = LoadBuffer.GetFloat();
					DeathStat.ptPositionOfDeath.y = LoadBuffer.GetFloat();
					DeathStat.ptPositionOfDeath.z = LoadBuffer.GetFloat();
					DeathStat.iDamageType = LoadBuffer.GetInt();
					LoadBuffer.GetString( DeathStat.szAttackerClassName );
				}

				break;
			}

		case PortalPlacement_t::CHUNKID:
			{
				unsigned int iPlacementStatCount = LoadBuffer.GetUnsignedInt();
				for( unsigned int i = 0; i != iPlacementStatCount; ++i )
				{
					int index = m_pPlacements->AddToTail();
					PortalPlacement_t &PlacementStat = m_pPlacements->Element( index );

					PlacementStat.ptPlayerFiredFrom.x = LoadBuffer.GetFloat();
					PlacementStat.ptPlayerFiredFrom.y = LoadBuffer.GetFloat();
					PlacementStat.ptPlayerFiredFrom.z = LoadBuffer.GetFloat();

					PlacementStat.ptPlacementPosition.x = LoadBuffer.GetFloat();
					PlacementStat.ptPlacementPosition.y = LoadBuffer.GetFloat();
					PlacementStat.ptPlacementPosition.z = LoadBuffer.GetFloat();

					PlacementStat.iSuccessCode = LoadBuffer.GetChar();
				}

				break;
			}

		case PlayerUse_t::CHUNKID:
			{
				int iUseEventCount = LoadBuffer.GetUnsignedInt();
				for( int i = 0; i != iUseEventCount; ++i )
				{
					int index = m_pUseEvents->AddToTail();
					PlayerUse_t &UseEvent = m_pUseEvents->Element( index );
					
					UseEvent.ptTraceStart.x = LoadBuffer.GetFloat();
					UseEvent.ptTraceStart.y = LoadBuffer.GetFloat();
					UseEvent.ptTraceStart.z = LoadBuffer.GetFloat();

					UseEvent.vTraceDelta.x = LoadBuffer.GetFloat();
					UseEvent.vTraceDelta.y = LoadBuffer.GetFloat();
					UseEvent.vTraceDelta.z = LoadBuffer.GetFloat();

					LoadBuffer.GetString( UseEvent.szUseEntityClassName );
				}

				break;
			}

		case StuckEvent_t::CHUNKID:
			{
				unsigned int iStuckEventCount = LoadBuffer.GetUnsignedInt();
				for( unsigned int i = 0; i != iStuckEventCount; ++i )
				{
					int index = m_pStuckSpots->AddToTail();
					StuckEvent_t &StuckEvent = m_pStuckSpots->Element( index );

					StuckEvent.ptPlayerPosition.x = LoadBuffer.GetFloat();
					StuckEvent.ptPlayerPosition.y = LoadBuffer.GetFloat();
					StuckEvent.ptPlayerPosition.z = LoadBuffer.GetFloat();

					StuckEvent.qPlayerAngles.x = LoadBuffer.GetFloat();
					StuckEvent.qPlayerAngles.y = LoadBuffer.GetFloat();
					StuckEvent.qPlayerAngles.z = LoadBuffer.GetFloat();

					unsigned char bitFlags = LoadBuffer.GetUnsignedChar();

					StuckEvent.bNearPortal = ( (bitFlags & (1 << 0)) != 0 );
					StuckEvent.bDucking = ( (bitFlags & (1 << 1)) != 0 );
				}	

				break;
			}

		case JumpEvent_t::CHUNKID:
			{
				unsigned int iJumpEventCount = LoadBuffer.GetUnsignedInt();
				for( unsigned int i = 0; i != iJumpEventCount; ++i )
				{
					int index = m_pJumps->AddToTail();
					JumpEvent_t &JumpEvent = m_pJumps->Element( index );

					JumpEvent.ptPlayerPositionAtJumpStart.x = LoadBuffer.GetFloat();
					JumpEvent.ptPlayerPositionAtJumpStart.y = LoadBuffer.GetFloat();
					JumpEvent.ptPlayerPositionAtJumpStart.z = LoadBuffer.GetFloat();

					JumpEvent.vPlayerVelocityAtJumpStart.x = LoadBuffer.GetFloat();
					JumpEvent.vPlayerVelocityAtJumpStart.y = LoadBuffer.GetFloat();
					JumpEvent.vPlayerVelocityAtJumpStart.z = LoadBuffer.GetFloat();
				}

				break;
			}

		case LeafTimes_t::CHUNKID:
			{
				//IMPORTANT TODO
				//TODO: Detect if the leaves have changed and invalidate these counts

				unsigned int iLeafCount = LoadBuffer.GetUnsignedInt();
				for( unsigned int i = 0; i != iLeafCount; ++i )
				{
					int index = m_pTimeSpentInVisLeafs->AddToTail();
					LeafTimes_t &LeafTime = m_pTimeSpentInVisLeafs->Element( index );

					LeafTime.fTimeSpentInVisLeaf = LoadBuffer.GetFloat();
				}

				break;
			}
#else
		case PlayerDeaths_t::CHUNKID: //warning workaround
#endif
		default:
			{
				//an unknown chunk, skip it
				LoadBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, iChunkSize );
			}
		};

		Assert( ((unsigned int)LoadBuffer.TellGet()) == iChunkEndPosition );

	};
}

void Portal_Gamestats_LevelStats_t::Clear( void )
{
	m_pDeaths->RemoveAll();	
	m_pPlacements->RemoveAll();
	m_pStuckSpots->RemoveAll();
	m_pJumps->RemoveAll();
	m_pTimeSpentInVisLeafs->RemoveAll();
}





void CPortalGameStats::AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer )
{
#ifdef PORTAL_GAMESTATS_VERBOSE //we only have verbose chunks for now, so only write custom data if we're verbosely tracking
	
	SaveBuffer.PutUnsignedShort( PORTAL_GAMESTATS_VERSION );

	//no headers allowed, chunks were chosen for their flexibility in loading even when parts of the data are unknown
	//you can simulate a header by enclosing the entirety of the custom data in a chunk that starts with a header, but you'll kill loading in old versions

	if( m_CustomMapStats.Count() != 0 ) //we have some map stats
	{
		//put out a map chunk for each map
		for ( int i =  m_CustomMapStats.First(); i != m_CustomMapStats.InvalidIndex(); i =  m_CustomMapStats.Next( i ) )
		{
			SaveBuffer.PutShort( Portal_Gamestats_LevelStats_t::CHUNKID );

			//we can trivially find the chunk size after the chunk is written, but chunk size needs to be at the beginning, reserve the space for chunk size now
			int iChunkSizePosition = SaveBuffer.TellPut();
			SaveBuffer.PutUnsignedInt( 0 );

			char const *szMapName = m_CustomMapStats.GetElementName( i );
			Portal_Gamestats_LevelStats_t &mapStats = m_CustomMapStats[ i ];

			SaveBuffer.PutString( szMapName );
			mapStats.AppendSubChunksToBuffer( SaveBuffer );


			//write out the map stats chunk size
			{
				int iChunkSize = SaveBuffer.TellPut() - iChunkSizePosition; 
				Assert( iChunkSize >= sizeof( int ) ); //minimum sizeof( int )

#ifdef _DEBUG
				int iOldTellPut = SaveBuffer.TellPut(); //needed for an assert below
#endif

				SaveBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, iChunkSizePosition );
				SaveBuffer.PutUnsignedInt( iChunkSize );
				SaveBuffer.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );

				Assert( iOldTellPut == SaveBuffer.TellPut() ); //writing the chunk size should have overwritten, not inserted
			}
		}		
	}

#endif //#ifdef PORTAL_GAMESTATS_VERBOSE
}

void CPortalGameStats::LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer )
{
#ifdef _DEBUG
	unsigned short iSaveStatsVersion = LoadBuffer.GetUnsignedShort();
	AssertOnce( iSaveStatsVersion <= PORTAL_GAMESTATS_VERSION ); //useful to know, but shouldn't be a failure case
#else
	LoadBuffer.GetUnsignedShort(); //don't really need the version
#endif

	int iEndPosition = LoadBuffer.TellPut();

	while( LoadBuffer.TellGet() != iEndPosition )
	{
		Assert( (iEndPosition - LoadBuffer.TellGet()) > (sizeof( unsigned short ) + sizeof( unsigned int )) ); //at least an empty chunk left

		unsigned short iChunkID = LoadBuffer.GetUnsignedShort();
		unsigned int iChunkSize = LoadBuffer.GetUnsignedInt() - sizeof( unsigned int ); //chunk size includes the chunk size data itself
#ifdef PORTAL_GAMESTATS_VERBOSE
		unsigned int iChunkEndPosition = LoadBuffer.TellGet() + iChunkSize; //used in an assert later
#endif

		switch( iChunkID )
		{
#ifdef PORTAL_GAMESTATS_VERBOSE //levelstats only have verbose data for the time being, so only bother to load verboseness if we're still tracking it
		case Portal_Gamestats_LevelStats_t::CHUNKID:
			{
				//map chunk				
				char szMapName[256];
				LoadBuffer.GetString( szMapName );

				Portal_Gamestats_LevelStats_t *mapStats = FindOrAddMapStats( szMapName );
				mapStats->LoadSubChunksFromBuffer( LoadBuffer, iChunkEndPosition );

				break;
			}
#else
		case Portal_Gamestats_LevelStats_t::CHUNKID: //warning workaround
#endif
		default:
			{
				//an unknown chunk, skip it
				LoadBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, iChunkSize );
			}
		};

#ifdef PORTAL_GAMESTATS_VERBOSE
		Assert( ((unsigned int)LoadBuffer.TellGet()) == iChunkEndPosition );
#endif
	}
}


void CPortalGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
#ifdef PORTAL_GAMESTATS_VERBOSE
	if( CBaseGameStats::StatTrackingAllowed() == false )
		return;

	int index = m_pCurrentMapStats->m_pDeaths->AddToTail();
	Portal_Gamestats_LevelStats_t::PlayerDeaths_t &DeathStat = m_pCurrentMapStats->m_pDeaths->Element( index );

	DeathStat.ptPositionOfDeath = pPlayer->GetAbsOrigin();
	DeathStat.iDamageType = info.GetDamageType();
	DeathStat.szAttackerClassName[0] = '\0';
	
	CBaseEntity *pInflictor = info.GetInflictor();
	if( pInflictor )
		Q_strncpy( DeathStat.szAttackerClassName, pInflictor->GetClassname(), sizeof( DeathStat.szAttackerClassName ) );
#endif
}

void CPortalGameStats::Event_PlayerJump( const Vector &ptStartPosition, const Vector &vStartVelocity )
{
#ifdef PORTAL_GAMESTATS_VERBOSE
	if( CBaseGameStats::StatTrackingAllowed() == false )
		return;

	int index = m_pCurrentMapStats->m_pJumps->AddToTail();
	Portal_Gamestats_LevelStats_t::JumpEvent_t &JumpStat = m_pCurrentMapStats->m_pJumps->Element( index );
	
	JumpStat.ptPlayerPositionAtJumpStart = ptStartPosition;
	JumpStat.vPlayerVelocityAtJumpStart = vStartVelocity;
#endif
}

void CPortalGameStats::Event_PortalPlacement( const Vector &ptPlayerFiredFrom, const Vector &ptAttemptedPosition, char iSuccessCode )
{
#ifdef PORTAL_GAMESTATS_VERBOSE
	if( CBaseGameStats::StatTrackingAllowed() == false )
		return;

	int index = m_pCurrentMapStats->m_pPlacements->AddToTail();
	Portal_Gamestats_LevelStats_t::PortalPlacement_t &PlacementStat = m_pCurrentMapStats->m_pPlacements->Element( index );

	PlacementStat.ptPlacementPosition = ptAttemptedPosition;
	PlacementStat.ptPlayerFiredFrom = ptPlayerFiredFrom;
	PlacementStat.iSuccessCode = iSuccessCode;
#endif
}

void CPortalGameStats::Event_PlayerUsed( const Vector &ptTraceStart, const Vector &vTraceDelta, CBaseEntity *pUsedEntity )
{
#ifdef PORTAL_GAMESTATS_VERBOSE
	if( CBaseGameStats::StatTrackingAllowed() == false )
		return;

	static float fLastUseTime = 0.0f;
	
	if( fLastUseTime > gpGlobals->curtime ) //I'm not positive, but I think curtime resets between levels, cheap to do this
		fLastUseTime = 0.0f;

	if( (gpGlobals->curtime - fLastUseTime) < 0.25f ) //use events cluster
		return;

	fLastUseTime = gpGlobals->curtime;

	int index = m_pCurrentMapStats->m_pUseEvents->AddToTail();
	Portal_Gamestats_LevelStats_t::PlayerUse_t &UseEvent = m_pCurrentMapStats->m_pUseEvents->Element( index );

	UseEvent.ptTraceStart = ptTraceStart;
	UseEvent.vTraceDelta = vTraceDelta;
	UseEvent.szUseEntityClassName[0] = '\0';

	if( pUsedEntity )
		Q_strncpy( UseEvent.szUseEntityClassName, pUsedEntity->GetClassname(), sizeof( UseEvent.szUseEntityClassName ) );
#endif
}

void CPortalGameStats::Event_PlayerStuck( CPortal_Player *pPlayer )
{
#ifdef PORTAL_GAMESTATS_VERBOSE
	if( CBaseGameStats::StatTrackingAllowed() == false )
		return;

	static float fLastStuckTime = 0.0f;

	if( fLastStuckTime > gpGlobals->curtime ) //I'm not positive, but I think curtime resets between levels, cheap to do this
		fLastStuckTime = 0.0f;

	if( (gpGlobals->curtime - fLastStuckTime) < 10.0f ) //only log one stuck spot per 10 second interval (in case it oscillates)
		return;

	fLastStuckTime = gpGlobals->curtime;

	int index = m_pCurrentMapStats->m_pStuckSpots->AddToTail();
	Portal_Gamestats_LevelStats_t::StuckEvent_t &StuckSpot = m_pCurrentMapStats->m_pStuckSpots->Element( index );

	StuckSpot.ptPlayerPosition = pPlayer->GetAbsOrigin();
	StuckSpot.qPlayerAngles = pPlayer->GetAbsAngles();
	StuckSpot.bNearPortal = (pPlayer->m_hPortalEnvironment.Get() != NULL);
	StuckSpot.bDucking = ((pPlayer->m_nButtons & IN_DUCK) != 0);
#endif
}

void CPortalGameStats::Event_LevelInit( void )
{
	BaseClass::Event_LevelInit();
	m_pCurrentMapStats = FindOrAddMapStats( STRING( gpGlobals->mapname ) );
}

void CPortalGameStats::Event_MapChange( const char *szOldMapName, const char *szNewMapName )
{
	BaseClass::Event_MapChange( szOldMapName, szNewMapName );
	m_pCurrentMapStats = FindOrAddMapStats( szNewMapName );
}



void CPortalGameStats::Clear( void )
{
	for( int i =  m_CustomMapStats.First(); i != m_CustomMapStats.InvalidIndex(); i =  m_CustomMapStats.Next( i ) )
	{
		DestroyLevelStatPointers( &m_CustomMapStats[i] );
	}

	m_CustomMapStats.RemoveAll();
}

Portal_Gamestats_LevelStats_t *CPortalGameStats::FindOrAddMapStats( const char *szMapName )
{
	int idx = m_CustomMapStats.Find( szMapName );
	if( idx == m_CustomMapStats.InvalidIndex() )
	{
		idx = m_CustomMapStats.Insert( szMapName );

		CreateLevelStatPointers( &m_CustomMapStats[idx] );
	}	

	return &m_CustomMapStats[ idx ];
}


void CreateLevelStatPointers( Portal_Gamestats_LevelStats_t *pFillIn )
{
	pFillIn->m_pDeaths = new CUtlVector<Portal_Gamestats_LevelStats_t::PlayerDeaths_t>;
	pFillIn->m_pPlacements = new CUtlVector<Portal_Gamestats_LevelStats_t::PortalPlacement_t>;
	pFillIn->m_pUseEvents = new CUtlVector<Portal_Gamestats_LevelStats_t::PlayerUse_t>;
	pFillIn->m_pStuckSpots = new CUtlVector<Portal_Gamestats_LevelStats_t::StuckEvent_t>;
	pFillIn->m_pJumps = new CUtlVector<Portal_Gamestats_LevelStats_t::JumpEvent_t>;
	pFillIn->m_pTimeSpentInVisLeafs = new CUtlVector<Portal_Gamestats_LevelStats_t::LeafTimes_t>;
}

void DestroyLevelStatPointers( Portal_Gamestats_LevelStats_t *pDestroyFrom )
{
	delete pDestroyFrom->m_pDeaths;
	delete pDestroyFrom->m_pPlacements;
	delete pDestroyFrom->m_pUseEvents;
	delete pDestroyFrom->m_pStuckSpots;
	delete pDestroyFrom->m_pJumps;
	delete pDestroyFrom->m_pTimeSpentInVisLeafs;
}



static char const *portalMaps[] =
{
	"testchmb_a_00",
	"testchmb_a_01",
	"testchmb_a_02",
	"testchmb_a_03",
	"testchmb_a_04",
	"testchmb_a_05",
	"testchmb_a_06",
	"testchmb_a_07",
	"testchmb_a_08",
	"testchmb_a_09",
	"testchmb_a_10",
	"testchmb_a_11",
	//"testchmb_a_12", //12 got deleted/skipped
	"testchmb_a_13",
	"testchmb_a_14",
	"testchmb_a_15",
	"escape_00",
	"escape_01",
	"escape_02"
};


bool CPortalGameStats::UserPlayedAllTheMaps( void )
{
	int c = ARRAYSIZE( portalMaps );
	for ( int i = 0; i < c; ++i )
	{
		int idx = m_BasicStats.m_MapTotals.Find( portalMaps[ i ] );
		if( idx == m_BasicStats.m_MapTotals.InvalidIndex() )
			return false;
	}

	return true;
}


