//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "ai_basenpc.h"
#include "animation.h"
#include "vstdlib/random.h"
#include "h_cycler.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCycler_TF2Commando : public CCycler
{
	DECLARE_CLASS( CCycler_TF2Commando, CCycler );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Spawn( void );
	virtual void Think( void );

	// Inputs
	void	InputRaiseShield( inputdata_t &inputdata );
	void	InputLowerShield( inputdata_t &inputdata );

private:
	CNetworkVar( bool, m_bShieldActive );
	CNetworkVar( float, m_flShieldRaiseTime );
	CNetworkVar( float, m_flShieldLowerTime );
};

IMPLEMENT_SERVERCLASS_ST(CCycler_TF2Commando, DT_Cycler_TF2Commando)
	SendPropInt	(SENDINFO(m_bShieldActive),	1, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(m_flShieldRaiseTime), 0,	SPROP_NOSCALE ),
	SendPropFloat(SENDINFO(m_flShieldLowerTime), 0,	SPROP_NOSCALE ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( cycler_tf2commando, CCycler_TF2Commando );
LINK_ENTITY_TO_CLASS( cycler_aliencommando, CCycler_TF2Commando );

BEGIN_DATADESC( CCycler_TF2Commando )

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "RaiseShield", InputRaiseShield ),
	DEFINE_INPUTFUNC( FIELD_VOID, "LowerShield", InputLowerShield ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCycler_TF2Commando::Spawn( void )
{
	if (GetTeamNumber() == 1)
	{
		GenericCyclerSpawn( "models/player/human_commando.mdl", Vector(-16, -16, 0), Vector(16, 16, 72) ); 
	}
	else
	{
		GenericCyclerSpawn( "models/player/alien_commando.mdl", Vector(-16, -16, 0), Vector(16, 16, 72) ); 
	}

	m_bShieldActive = false;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCycler_TF2Commando::Think( void )
{
	// Change sequence
	if ( IsSequenceFinished() )
	{
		// Raising our shield?
		if ( m_bShieldActive )
		{
			ResetSequence( LookupSequence( "ShieldUpIdle" ) );
		}
		else if ( GetSequence() == LookupSequence( "ShieldDown" ) )
		{
			ResetSequence( LookupSequence( "Idle" ) );
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1f );

	if (m_animate)
	{
		StudioFrameAdvance ( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input that raises the cycler's shield
//-----------------------------------------------------------------------------
void CCycler_TF2Commando::InputRaiseShield( inputdata_t &inputdata )
{
	if (m_animate)
	{
		m_bShieldActive = true;
		ResetSequence( LookupSequence( "ShieldUp" ) );
		m_flShieldRaiseTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input that lowers the cycler's shield
//-----------------------------------------------------------------------------
void CCycler_TF2Commando::InputLowerShield( inputdata_t &inputdata )
{
	if (m_animate)
	{
		m_bShieldActive = false;
		ResetSequence( LookupSequence( "ShieldDown" ) );
		m_flShieldLowerTime = gpGlobals->curtime;
	}
}

