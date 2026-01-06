#include "cbase.h"
#include "info_remarkable.h"

ConVar ai_debug_remarkables( "ai_debug_remarkables", "0", FCVAR_NONE );

BEGIN_DATADESC( CInfoRemarkable )
    DEFINE_KEYFIELD( m_iszContextSubject, FIELD_STRING, "contextsubject" ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_FIELD( m_iTimesRemarked, FIELD_INTEGER ),
	//DEFINE_FIELD( m_flLastTimeRemarked, FIELD_TIME ), // Currently only for debug purposes

	// Inputs	
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

// info remarkables, similar to info targets, are like point entities except you can force them to spawn on the client
void CInfoRemarkable::Spawn( void )
{
	BaseClass::Spawn();

	AddFlag( FL_OBJECT );
}

//------------------------------------------------------------------------------
// Purpose: Create criteria set for this remarkable
//------------------------------------------------------------------------------
AI_CriteriaSet& CInfoRemarkable::GetModifiers( CBaseEntity * pEntity )
{
	Assert( pEntity != NULL );

	AI_CriteriaSet * modifiers = new AI_CriteriaSet();
	float flDist = ( this->GetAbsOrigin() - pEntity->GetAbsOrigin() ).Length();

	modifiers->AppendCriteria( "subject", STRING( this->GetContextSubject() ) );
	modifiers->AppendCriteria( "distance", UTIL_VarArgs( "%f", flDist ) );
	modifiers->AppendCriteria( "timesremarked", UTIL_VarArgs( "%i", m_iTimesRemarked ) );
	return *modifiers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoRemarkable::DrawDebugGeometryOverlays( void )
{
	BaseClass::DrawDebugGeometryOverlays();

	if (gpGlobals->curtime - m_flLastTimeRemarked < 5.0f)
		NDebugOverlay::Box( GetAbsOrigin(), Vector( -8, -8, -8 ), Vector( 8, 8, 8 ), 0, 255, 0, 192, 0.5f );
	else
		NDebugOverlay::Box( GetAbsOrigin(), Vector( -4, -4, -4 ), Vector( 4, 4, 4 ), 192, 128, 128, 128, 0.5f );

	// Only draw these if we *don't* have the text debug bit
	if (!(m_debugOverlays & OVERLAY_TEXT_BIT))
	{
		int iTextOffset = 0;
		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), iTextOffset, GetDebugName(), 0.5f );
		iTextOffset++;
		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), iTextOffset, UTIL_VarArgs( "Subject: %s", STRING( GetContextSubject() ) ), 0.5f );
		iTextOffset++;
		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), iTextOffset, UTIL_VarArgs( "Times remarked: %i", GetTimesRemarked() ), 0.5f );
		iTextOffset++;
		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), iTextOffset, UTIL_VarArgs( "Last remarked %.2f seconds ago", m_flLastTimeRemarked ), 0.5f );
		iTextOffset++;
	}
}

//------------------------------------------------------------------------------
// Purpose: Input handler to turn on this trigger.
//------------------------------------------------------------------------------
void CInfoRemarkable::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}


//------------------------------------------------------------------------------
// Purpose: Input handler to turn off this trigger.
//------------------------------------------------------------------------------
void CInfoRemarkable::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

LINK_ENTITY_TO_CLASS( info_remarkable, CInfoRemarkable );