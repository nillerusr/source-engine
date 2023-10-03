//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A simple test entity for creating special effects
//
//=============================================================================//

#include "cbase.h"
#include "asw_generic_emitter_entity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Link our class to the "env_sparkler" entity classname
LINK_ENTITY_TO_CLASS( asw_emitter, CASW_Emitter );

// Declare our data description for this entity
BEGIN_DATADESC( CASW_Emitter )
	DEFINE_KEYFIELD( m_bEmit, FIELD_BOOLEAN, "starton" ),	
	DEFINE_KEYFIELD( m_szTemplate, FIELD_STRING, "template" ),
	DEFINE_KEYFIELD( m_fScale, FIELD_FLOAT, "scale" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_FIELD( m_fDesiredScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_fScaleRate, FIELD_FLOAT ),
	//DEFINE_FIELD( m_szTemplateName, FIELD_STRING ),		// can't save these, need to set it on restore from m_szTemplate
	//DEFINE_FIELD( m_bSetTemplateName, FIELD_BOOLEAN ),		// can't save this if we can't send m_szTemplateName
END_DATADESC()


// Declare the data-table for server/client communication
IMPLEMENT_SERVERCLASS_ST( CASW_Emitter, DT_ASW_Emitter )
	SendPropInt( SENDINFO( m_bEmit ), 1, SPROP_UNSIGNED ),	// Declare our boolean state variable
	SendPropString( SENDINFO( m_szTemplateName ) ), 
	SendPropFloat( SENDINFO( m_fDesiredScale ), 8, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_fScaleRate ), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

CASW_Emitter::CASW_Emitter()
{
	m_fScale = 1.0f;
	m_fDesiredScale = 1.0f;
	m_fScaleRate = 10.0f;
	m_bSetTemplateName = false;
	m_bEmit = true;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn function for this entity
//-----------------------------------------------------------------------------
void CASW_Emitter::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );	// Will not move on its own
	SetSolid( SOLID_NONE );			// Will not collide with anything
	
	if (m_fScale == 0)
		m_fScale = 1.0f;

	if (m_fDesiredScale == 0)
		m_fDesiredScale = 1.0f;

	if (m_fScaleRate == 0)
		m_fScaleRate = 10.0f;

	// Set a size for culling
	//UTIL_SetSize( this, -Vector(2,2,2), Vector(2,2,2) );
	UTIL_SetSize( this, -Vector(800,800,800), Vector(800,800,800) );

	if (!m_bSetTemplateName)
		Q_strncpy( m_szTemplateName.GetForModify(), STRING( m_szTemplate ), 255 );

	m_bSetTemplateName = true;

	// We must add this flag to receive network transmitions
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}


void CASW_Emitter::OnRestore( void )
{
	BaseClass::OnRestore();

	if (!m_bSetTemplateName)
	{
		Q_strncpy( m_szTemplateName.GetForModify(), STRING( m_szTemplate ), 255 );
		m_bSetTemplateName = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Toggles the emission state of the effect
//-----------------------------------------------------------------------------
void CASW_Emitter::InputToggle( inputdata_t &input )
{
	// Toggle our state
	m_bEmit = !m_bEmit;
}

void CASW_Emitter::InputTurnOn( inputdata_t &input )
{
	// Toggle our state
	m_bEmit = true;
}

void CASW_Emitter::InputTurnOff( inputdata_t &input )
{
	// Toggle our state
	m_bEmit = false;
}

void CASW_Emitter::UseTemplate(const char *szTemplateName)
{
	m_bSetTemplateName = true;
	Q_strncpy( m_szTemplateName.GetForModify(), szTemplateName, 255 );
}