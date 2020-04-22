//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "cs_player.h"


//======================================
// Bomb target area
//
//

class CBuyZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBuyZone, CBaseTrigger );
	DECLARE_DATADESC();

	CBuyZone();
	void Spawn();
	void EXPORT BuyZoneTouch( CBaseEntity* pOther );

public:
	int m_LegacyTeamNum;
};


LINK_ENTITY_TO_CLASS( func_buyzone, CBuyZone );

BEGIN_DATADESC( CBuyZone )
	DEFINE_FUNCTION( BuyZoneTouch ),
	
	// This is here to support maps that haven't updated to using "teamnum" yet.
	DEFINE_INPUT( m_LegacyTeamNum, FIELD_INTEGER, "team" )
END_DATADESC()


CBuyZone::CBuyZone()
{
	m_LegacyTeamNum = -1;
}


void CBuyZone::Spawn()
{
	InitTrigger();
	SetTouch( &CBuyZone::BuyZoneTouch );

	// Support for legacy-style teamnums.
	if ( m_LegacyTeamNum == 1 )
	{
		ChangeTeam( TEAM_TERRORIST );
	}
	else if ( m_LegacyTeamNum == 2 )
	{
		ChangeTeam( TEAM_CT );
	}
}

	
void CBuyZone::BuyZoneTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		// compare player team with buy zone team number
		if ( p->GetTeamNumber() == GetTeamNumber() )
		{
			p->m_bInBuyZone = true;
		}
	}
}

