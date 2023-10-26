#include "cbase.h"
#include "asw_ai_senses.h"
#include "iasw_spawnable_npc.h"
#include "ai_basenpc.h"
#include "saverestore_utlvector.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef DEBUG_SENSES
#define AI_PROFILE_SENSES(tag) AI_PROFILE_SCOPE(tag)
#else
#define AI_PROFILE_SENSES(tag) ((void)0)
#endif

// in asw the drones don't really need to sense for NPCs other than the marines at all, so boost these search times up
const float ASW_AI_STANDARD_NPC_SEARCH_TIME = 1.0;		// in HL2 is .25
const float ASW_AI_EFFICIENT_NPC_SEARCH_TIME = 1.0;		// in HL2 is .35
const float ASW_AI_HIGH_PRIORITY_SEARCH_TIME = 0.15;
const float ASW_AI_MISC_SEARCH_TIME  = 0.45;

// sense a little less frequently than in HL2 since we have so many aliens
const float ASW_AI_STANDARD_MARINE_SEARCH_TIME = .35;
const float ASW_AI_EFFICIENT_MARINE_SEARCH_TIME = .45;
const float ASW_AI_SUPER_EFFICIENT_MARINE_SEARCH_TIME = .55;

const float ASW_AI_MARINE_LOOK_HEIGHT = 128.0f;

#pragma pack(push)
#pragma pack(1)

struct AISightIterVal_t
{
	char  array;
	short iNext;
	char  SwarmSensedArray;
};

#pragma pack(pop)

bool CASW_BaseAI_Senses::WaitingUntilSeen( CBaseEntity *pSightEnt )
{
	if ( GetOuter()->GetSpawnFlags() & SF_NPC_WAIT_TILL_SEEN )
	{
		// asw, wake up if marines see us (not players)
		if ( pSightEnt && pSightEnt->Classify() == CLASS_ASW_MARINE )
		{
			CBaseCombatCharacter *pBCC = dynamic_cast<CBaseCombatCharacter*>( pSightEnt );
			Vector zero =  Vector(0,0,0);
			// don't link this client in the list if the npc is wait till seen and the player isn't facing the npc
			if (// && pPlayer->FVisible( GetOuter() ) 
				pBCC->FInViewCone( GetOuter() )
				&& FBoxVisible( pSightEnt, static_cast<CBaseEntity*>(GetOuter()), zero ) )
			{
				// marine sees us, become normal now.
				GetOuter()->RemoveSpawnFlags( SF_NPC_WAIT_TILL_SEEN );
				return false;
			}
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( CASW_Marine_AI_Senses )
	DEFINE_FIELD( m_flLookHeight,			FIELD_FLOAT	),
END_DATADESC()

CASW_Marine_AI_Senses::CASW_Marine_AI_Senses()
: m_flLookHeight( ASW_AI_MARINE_LOOK_HEIGHT )
{

}

bool CASW_Marine_AI_Senses::IsWithinSenseDistance( const Vector &source, const Vector &dest, float dist )
{
	return CASW_BaseAI_Senses::IsWithinSenseDistance( source, dest, dist ) &&
		( dest.z - source.z < m_flLookHeight ); // can see infinitely down, but not up
}

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( CASW_AI_Senses )
	DEFINE_FIELD( m_SwarmSenseDist,			FIELD_FLOAT	),
	DEFINE_FIELD( m_LastSwarmSenseDist, 	FIELD_FLOAT	),
	DEFINE_FIELD( m_TimeLastSwarmSense, 	FIELD_TIME	),
	DEFINE_UTLVECTOR(m_SwarmSensedMarines, 		FIELD_EHANDLE ),
	//								m_SwarmSenseArrays		(not saved, rebuilt)	
	DEFINE_EMBEDDED( 	m_SwarmSenseMarinesTimer ),
END_DATADESC()

CASW_AI_Senses::CASW_AI_Senses() :
	m_SwarmSenseDist(576),
	m_TimeLastSwarmSense(0),
	m_LastSwarmSenseDist(0)
{
	m_SwarmSenseArrays[0] = &m_SwarmSensedMarines;
}

CASW_AI_Senses::~CASW_AI_Senses()
{
}

void CASW_AI_Senses::PerformSensing()
{
	BaseClass::PerformSensing();

	// Use our Swarm senses to detect marines through walls
	SwarmSense(m_SwarmSenseDist);
}

void CASW_AI_Senses::SwarmSense(int iDistance)
{
	IASW_Spawnable_NPC* pAlienOuter = dynamic_cast<IASW_Spawnable_NPC*>(GetOuter());
	if (!pAlienOuter)
		return;

	if ( m_TimeLastSwarmSense != gpGlobals->curtime || m_LastSwarmSenseDist != iDistance )
	{		
		SwarmSenseMarines(iDistance);
				
		m_LastSwarmSenseDist = iDistance;
		m_TimeLastSwarmSense = gpGlobals->curtime;
	}
	
	pAlienOuter->OnSwarmSensed( iDistance );
}

int CASW_AI_Senses::SwarmSenseMarines(int iDistance)
{
	bool bRemoveStaleFromCache = false;
	float distSq = ( iDistance * iDistance );
	const Vector &origin = GetAbsOrigin();
	if ( m_SwarmSenseMarinesTimer.Expired() )
	{
		AI_PROFILE_SENSES(CASW_AI_Senses_SwarmSenseMarines);
		AI_Efficiency_t efficiency = GetOuter()->GetEfficiency();

		float fSenseTime = ASW_AI_STANDARD_MARINE_SEARCH_TIME;
		if (efficiency == AIE_VERY_EFFICIENT)
			fSenseTime = ASW_AI_EFFICIENT_MARINE_SEARCH_TIME;
		else if (efficiency == AIE_SUPER_EFFICIENT)
			fSenseTime = ASW_AI_SUPER_EFFICIENT_MARINE_SEARCH_TIME;
		//m_SwarmSenseMarinesTimer.Reset( ( efficiency < AIE_VERY_EFFICIENT ) ? ASW_AI_STANDARD_NPC_SEARCH_TIME : ASW_AI_EFFICIENT_NPC_SEARCH_TIME );
		m_SwarmSenseMarinesTimer.Reset( fSenseTime );

		//if ( GetOuter()->GetEfficiency() < AIE_SUPER_EFFICIENT )
		{
			int nSeen = 0;

			BeginGather();

			CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
			
			for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
			{
#if OTHER_IMPORTANT_ENTITIES_NOT_BAKED
				if ( ppAIs[i] != GetOuter()->GetTarget() && ppAIs[i] != GetOuter()->GetEnemy() )
#endif
				{
					if ( ppAIs[i] != GetOuter() && ( ppAIs[i]->ShouldNotDistanceCull() || origin.DistToSqr(ppAIs[i]->GetAbsOrigin()) < distSq ) && CanSwarmSense( ppAIs[i] ) )
					{
						nSeen++;
					}
				}
			}

			EndGather( nSeen, &m_SwarmSensedMarines );

			return nSeen;
		}

		bRemoveStaleFromCache = true;
		// Fall through
	}

    for ( int i = m_SwarmSensedMarines.Count() - 1; i >= 0; --i )
    {
    	if ( m_SwarmSensedMarines[i].Get() == NULL )
		{
    		m_SwarmSensedMarines.FastRemove( i );
		}
		else if ( bRemoveStaleFromCache )
		{
			if ( ( !((CAI_BaseNPC *)m_SwarmSensedMarines[i].Get())->ShouldNotDistanceCull() && 
				   origin.DistToSqr(m_SwarmSensedMarines[i]->GetAbsOrigin()) > distSq ) ||
				 !CanSwarmSense( m_SwarmSensedMarines[i] ) )
			{
	    		m_SwarmSensedMarines.FastRemove( i );
			}
		}
    }

    return m_SwarmSensedMarines.Count();
}

bool CASW_AI_Senses::CanSwarmSense( CBaseEntity *pSightEnt )
{
	if ( WaitingUntilSeen( pSightEnt ) )
		return false;
	
	if ( ShouldSeeEntity( pSightEnt ) )	// && CanSeeEntity( pSightEnt )  // can always swarm sense things in our search radius
	{
		return SwarmSenseEntity( pSightEnt );
	}
	return false;
}

bool CASW_AI_Senses::SwarmSenseEntity( CBaseEntity *pSightEnt )
{
	IASW_Spawnable_NPC* pAlienOuter = dynamic_cast<IASW_Spawnable_NPC*>(GetOuter());
	if (!pAlienOuter)
		return false;

	pAlienOuter->OnSwarmSenseEntity( pSightEnt );

	// insert at the head of my sight list
	NoteSeenEntity( pSightEnt );

	return true;
}

CBaseEntity *CASW_AI_Senses::GetFirstSwarmSenseEntity( AISightIter_t *pIter, seentype_t iSeenType ) const
{ 
	COMPILE_TIME_ASSERT( sizeof( AISightIter_t ) == sizeof( AISightIterVal_t ) );
	
	AISightIterVal_t *pIterVal = (AISightIterVal_t *)pIter;
	
	// If we're searching for a specific type, start in that array
	pIterVal->SwarmSensedArray = (char)iSeenType;
	int iFirstArray = ( iSeenType == SEEN_ALL ) ? 0 : iSeenType;

	for ( int i = iFirstArray; i < ARRAYSIZE( m_SwarmSenseArrays ); i++ )
	{
		if ( m_SwarmSenseArrays[i]->Count() != 0 )
		{
			pIterVal->array = i;
			pIterVal->iNext = 1;
			return (*m_SwarmSenseArrays[i])[0];
		}
	}
	
	(*pIter) = (AISightIter_t)(-1); 
	return NULL;
}

//-----------------------------------------------------------------------------

CBaseEntity *CASW_AI_Senses::GetNextSwarmSenseEntity( AISightIter_t *pIter ) const	
{ 
	if ( ((int)*pIter) != -1 )
	{
		AISightIterVal_t *pIterVal = (AISightIterVal_t *)pIter;
		
		for ( int i = pIterVal->array;  i < ARRAYSIZE( m_SwarmSenseArrays ); i++ )
		{
			for ( int j = pIterVal->iNext; j < m_SwarmSenseArrays[i]->Count(); j++ )
			{
				if ( (*m_SwarmSenseArrays[i])[j].Get() != NULL )
				{
					pIterVal->array = i;
					pIterVal->iNext = j+1;
					return (*m_SwarmSenseArrays[i])[j];
				}
			}
			pIterVal->iNext = 0;

			// If we're searching for a specific type, don't move to the next array
			if ( pIterVal->SwarmSensedArray != SEEN_ALL )
				break;
		}
		(*pIter) = (AISightIter_t)(-1); 
	}
	return NULL;
}