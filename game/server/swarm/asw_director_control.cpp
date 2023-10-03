//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// Purpose: Alien Swarm Director Controller.
//
//=============================================================================
#include "cbase.h"
#include "asw_director_control.h"
#include "asw_director.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_director_control, CASW_Director_Control );

BEGIN_DATADESC( CASW_Director_Control )
	DEFINE_KEYFIELD( m_bWanderersStartEnabled, FIELD_BOOLEAN,	"wanderers" ),
	DEFINE_KEYFIELD( m_bHordesStartEnabled, FIELD_BOOLEAN,	"hordes" ),
	DEFINE_KEYFIELD( m_bDirectorControlsSpawners, FIELD_BOOLEAN,	"controlspawners" ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"EnableHordes",	InputEnableHordes ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableHordes",	InputDisableHordes ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"EnableWanderers",	InputEnableWanderers ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableWanderers",	InputDisableWanderers ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StartFinale",	InputStartFinale ),
	DEFINE_OUTPUT( m_OnEscapeRoomStart, "OnEscapeRoomStart"),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::Precache()
{
	BaseClass::Precache();
}

void CASW_Director_Control::OnEscapeRoomStart( CASW_Marine *pMarine )
{
	m_OnEscapeRoomStart.FireOutput( pMarine, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::InputEnableHordes( inputdata_t &inputdata )
{
	if ( !ASWDirector() )	
		return;

	ASWDirector()->SetHordesEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::InputDisableHordes( inputdata_t &inputdata )
{
	if ( !ASWDirector() )	
		return;

	ASWDirector()->SetHordesEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::InputEnableWanderers( inputdata_t &inputdata )
{
	if ( !ASWDirector() )	
		return;

	ASWDirector()->SetWanderersEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::InputDisableWanderers( inputdata_t &inputdata )
{
	if ( !ASWDirector() )	
		return;

	ASWDirector()->SetWanderersEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Director_Control::InputStartFinale( inputdata_t &inputdata )
{
	if ( !ASWDirector() )	
		return;

	ASWDirector()->StartFinale();
}