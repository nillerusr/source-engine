//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"
#include "filters.h"
#include "team_control_point.h"
#include "tf_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
//
// Team Fortress Team Filter
//
class CFilterTFTeam : public CBaseFilter
{
	DECLARE_CLASS( CFilterTFTeam, CBaseFilter );

public:

	void InputRoundSpawn( inputdata_t &inputdata );
	void InputRoundActivate( inputdata_t &inputdata );

	inline bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );

private:

	string_t m_iszControlPointName;

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CFilterTFTeam )

DEFINE_KEYFIELD( m_iszControlPointName, FIELD_STRING, "controlpoint" ),

// Inputs.
DEFINE_INPUTFUNC( FIELD_VOID, "RoundSpawn", InputRoundSpawn ),
DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( filter_activator_tfteam, CFilterTFTeam );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFilterTFTeam::PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity )
{
	// is the entity we're asking about on the winning 
	// team during the bonus time? (winners pass all filters)
	if (  TFGameRules() &&
		( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN ) && 
		( TFGameRules()->GetWinningTeam() == pEntity->GetTeamNumber() ) )
	{
		// this should open all doors for the winners
		if ( m_bNegated )
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	return ( pEntity->GetTeamNumber() == GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterTFTeam::InputRoundSpawn( inputdata_t &input )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterTFTeam::InputRoundActivate( inputdata_t &input )
{
	if ( m_iszControlPointName != NULL_STRING )
	{
		CTeamControlPoint *pControlPoint = dynamic_cast<CTeamControlPoint*>( gEntList.FindEntityByName( NULL, m_iszControlPointName ) );
		if ( pControlPoint )
		{
			ChangeTeam( pControlPoint->GetTeamNumber() );
		}
		else
		{
			Warning( "%s failed to find control point named '%s'\n", GetClassname(), STRING(m_iszControlPointName) );
		}
	}
}
