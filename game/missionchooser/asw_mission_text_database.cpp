/** Defines an "objective specification" object and the classes used to read and load it.

*/

#include "convar.h"
#include "asw_mission_text_database.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "vstdlib/random.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



CUtlSymbolTable CASW_MissionTextDB::s_SymTab( 128, 128, true );

CUtlSymbolMsnTxt CASW_MissionTextSpec::SymbolForMissionFilename( const char *pMissionFilename, bool bCreateIfNotFound )
{
	if ( pMissionFilename == NULL || pMissionFilename[0] == 0 )
	{
		return UTL_INVAL_SYMBOL;
	}
	else
	{
		char buf[256];
		V_FileBase( pMissionFilename, buf, 255 );
		return bCreateIfNotFound ? SymTab().AddString( pMissionFilename ) : SymTab().Find( pMissionFilename ) ;
	}
}

inline const char *FindStrSubkey( KeyValues * RESTRICT pKV, const char *pKeyName )
{
	KeyValues * RESTRICT pSubKey = pKV->FindKey( pKeyName );
	return pSubKey ? pSubKey->GetString() : NULL;
}

/*
"objectivetext"
{
	"entityname" "foo"
	"missionfile" "mission_frotz.txt"
	"shortdesc" "Recover the alien sandwich."
	"longdesc" "Delicious, tender and moist: a perfect balance between starch and protein. 
	            Somewhere within the base lies this paragon among lunchmeats, and it is your sacred task to retrieve it."
}
*/
void CASW_MissionTextDB::LoadKeyValuesFile( const char *pFilename )
{
	KeyValues * RESTRICT pKeyValuesSource = new KeyValues( pFilename );
	KeyValues::AutoDelete raii( pKeyValuesSource ); // deletes the keyvalues at end of scope

	if ( !pKeyValuesSource->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
	{
		//AssertMsg1( false, "Could not open key values file %s", pFilename );
		Warning( "Could not open key values file %s\n", pFilename );
		return;
	}

	int numAdded = 0 ;
	for ( KeyValues *RESTRICT pEntry = pKeyValuesSource ; pEntry != NULL ; pEntry = pEntry->GetNextKey() )
	{
		if ( Q_stricmp( pEntry->GetName(), "objectivetext" ) == 0 )
		{
			const char *pEntName = FindStrSubkey( pEntry, "entityname" );
			if ( !pEntName )
			{
				Warning( "A mission text block did not specify an entityname!\n%s\n",
					pEntry->GetString() );
				continue;
			}

			const char *pShortDesc = FindStrSubkey( pEntry, "shortdesc" );
			if ( !pShortDesc )
			{
				Warning( "A mission text block did not specify a short description!\n%s\n",
					pEntry->GetString() );
				continue;
			}

			const char *pLongDesc = FindStrSubkey( pEntry, "longdesc" );
			if ( !pLongDesc )
			{
				Warning( "A mission text block did not specify a long description!\n%s\n",
					pEntry->GetString() );
				// substitute the short desc
				pLongDesc = pShortDesc;
			}

			const char *pMissionFilename = FindStrSubkey( pEntry, "missionfile" );

			// insert	
			AddSpec( pShortDesc, pLongDesc, pEntName, pMissionFilename );
			++numAdded;
		}
		else
		{
			Warning( "Keyvalues file contained unknown key type %s\n", pEntry->GetName() );
		}
	}

	Msg( "%d mission descriptions loaded from %s\n", numAdded, pFilename );
}

void CASW_MissionTextDB::Reset()
{
	m_missionSpecs.RemoveAll();
}

void CASW_MissionTextDB::AddSpec( const char *pszShortDescription,
								 const char *pszSLongDescription, const char *pszObjectiveEntityName, const char *pszMissionFilename )
{
	// assert mandatory fields
	Assert( pszShortDescription && pszSLongDescription );
	Assert( pszObjectiveEntityName );
	int nextInsertId = m_missionSpecs.AddToTail();
	m_missionSpecs[ nextInsertId ].Init( pszShortDescription, pszSLongDescription, pszObjectiveEntityName, pszMissionFilename, nextInsertId );
};

const CASW_MissionTextSpec * CASW_MissionTextDB::Find( const char *pObjectiveEntityName, const char *pMissionFilename /*= NULL */ )
{
	// simple algorithm. walk through building a vector of possible matches. then sort by match quality. return the best.
	// in the future this will actually use a data structure.
	if ( pObjectiveEntityName == NULL )
		return NULL;

	// the type for our local vector below
	struct MatchTuple 
	{
		const CASW_MissionTextSpec *pSpec;
		int score;
		MatchTuple( const CASW_MissionTextSpec * pSpec_, int score_ ) : pSpec(pSpec_), score(score_) {};
	};

	CUtlVector<MatchTuple> matches( Count() >> 2, Count() >> 2 );

	CUtlSymbolMsnTxt nEntName = SymTab().Find( pObjectiveEntityName );
	CUtlSymbolMsnTxt nMissionName = CASW_MissionTextSpec::SymbolForMissionFilename( pMissionFilename, false );
	if ( !nEntName.IsValid() )
	{
		Warning( "No mission text specifier mentions an entity named %s\n", pObjectiveEntityName );
		return NULL;
	}

	// march forward looking for matches. compute a score for each match.
	const int N = m_missionSpecs.Count();
	for ( int i = 0 ; i < N ; ++i )
	{
		const CASW_MissionTextSpec *pSpec = &m_missionSpecs[i];
		int matchscore = 0;
		// test mandatory field
		if ( pSpec->m_nObjectiveEntityName == nEntName )
		{
			// score optional fields.
			// compare each field in the spec against the query.
			// if the field in the spec is defined, then a match adds one to
			//  score, but a mismatch disqualifies the entry.
			// if the field in the spec is undefined, then it matches everything,
			//  but does not add to score.
			if ( pSpec->m_nMissionFilename.IsValid() )
			{
				matchscore = pSpec->m_nMissionFilename == nMissionName ? 2 : 0;
			}
			else
			{
				matchscore = 1;
			}
		}

		if ( matchscore > 0 )
		{
			// best possible score, return immediately
			if ( matchscore == 2 )
			{
				// (refactor this block if we want to choose randomly from many possibilities)
				return pSpec;
			}
			else
			{
				matches.AddToTail( MatchTuple(pSpec, matchscore) );
			}
		}
	}

	if ( matches.Count() == 0 )
		return NULL;  // nothing found

	// in theory we should sort this list to push the best matches to the beginning,
	// but right now the only possible score is 1, so return an arbitrary element
	return matches[0].pSpec;
}

CASW_MissionTextSpec * CASW_MissionTextDB::GetSpecById( CASW_MissionTextSpec::ID_t nId )
{
	AssertMsg2( nId < m_missionSpecs.Count(), "Tried to look up invalid mission text ID %d / %d\n",
		nId, m_missionSpecs.Count() );

	return nId < m_missionSpecs.Count() ? &m_missionSpecs[ nId ] : NULL;
}

const char * CASW_MissionTextDB::GetShortDescriptionByID( unsigned int id )
{
	CASW_MissionTextSpec * RESTRICT pSpec = GetSpecById( id );
	return pSpec ? pSpec->GetShortDescription() : NULL;
}

const char * CASW_MissionTextDB::GetLongDescriptionByID( unsigned int id )
{
	CASW_MissionTextSpec * RESTRICT pSpec = GetSpecById( id );
	return pSpec ? pSpec->GetLongDescription() : NULL;
}

IASW_Mission_Text_Database::ID_t CASW_MissionTextDB::FindMissionTextID( const char *pEntityName, const char *pMissionName )
{
	const CASW_MissionTextSpec *pSpec = Find( pEntityName, pMissionName );
	return pSpec ? pSpec->m_nId : IASW_Mission_Text_Database::INVALID_INDEX ; 
}


#ifdef ENABLE_MISSION_TEXT_DB_DEBUG_FUNCTIONS
CASW_MissionTextDB g_MissionTextDB;

void CC_ASW_TestMissionText_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_test_mission_text_q <entity name> [mission name]");
		return;
	}

	const char *pEntName = args[1];
	const char *pMissName = args.ArgC() >= 3 ? args[2] : NULL;
	const CASW_MissionTextSpec *pSpec = g_MissionTextDB.Find( pEntName, pMissName );
	if ( pSpec )
	{
		Msg( "\"%s\"\n", pSpec->GetShortDescription() );
	}
	else
	{
		Warning( "Not found.\n" );
	}
}
static ConCommand asw_test_mission_text_q("asw_test_mission_text_q", CC_ASW_TestMissionText_f, 0 );


void CC_ASW_TestMissionTextLoad_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_test_mission_text_load [filename]");
		return;
	}
	g_MissionTextDB.LoadKeyValuesFile( args[1] );
}
static ConCommand asw_test_mission_text_load("asw_test_mission_text_load", CC_ASW_TestMissionTextLoad_f, 0 );

void CC_ASW_TestMissionTextReset_f( const CCommand &args )
{
	g_MissionTextDB.Reset();
}
static ConCommand asw_test_mission_text_reset("asw_test_mission_text_reset", CC_ASW_TestMissionTextReset_f, 0 );

#endif