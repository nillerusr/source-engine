//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map entity that adds a custom technology to the techtree
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_team.h"
#include "techtree.h"
#include "info_customtech.h"
#include "tier1/strtools.h"

BEGIN_DATADESC( CInfoCustomTechnology )

	// outputs
	DEFINE_OUTPUT( m_flTechPercentage, "TechPercentage" ),

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iszTech , FIELD_STRING, "TechToWatch" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_iszTechTreeFile , FIELD_STRING, "NewTechFile" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CInfoCustomTechnology, DT_InfoCustomTechnology )
	SendPropString( SENDINFO( m_szTechTreeFile ) ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( info_customtech, CInfoCustomTechnology );

//-----------------------------------------------------------------------------
// Purpose: Always transmit
//-----------------------------------------------------------------------------
int CInfoCustomTechnology::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Don't need to transmit ones that don't add new techs
	if ( !m_iszTechTreeFile )
		return FL_EDICT_DONTSEND;

	// Only transmit to members of my team
	CBaseEntity* pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
	if ( InSameTeam( pRecipientEntity ) )
	{
		//Msg( "SENDING\n" );
		return FL_EDICT_ALWAYS;
	}

	return FL_EDICT_DONTSEND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoCustomTechnology::Spawn( void )
{
	m_flTechPercentage.Set( 0, this, this );
	memset( m_szTechTreeFile.GetForModify(), 0, sizeof(m_szTechTreeFile) );
}

//-----------------------------------------------------------------------------
// Purpose: Add myself to the technology tree
//-----------------------------------------------------------------------------
void CInfoCustomTechnology::Activate( void )
{
	BaseClass::Activate();
	if ( !GetTeamNumber() )
	{
		Msg( "ERROR: info_customtech without a specified team.\n" );
		UTIL_Remove( this );
		return;
	}

	// Get the Team's Technology Tree
	CTFTeam *pTeam = (CTFTeam *)GetTeam(); 
	if ( pTeam )
	{
		CTechnologyTree *pTechTree = pTeam->GetTechnologyTree();
		if ( pTechTree )
		{
			// Am I supposed to add some new technologies to the tech tree?
			if ( m_iszTechTreeFile != NULL_STRING )
			{
				pTechTree->AddTechnologyFile( filesystem, GetTeamNumber(), (char*)STRING(m_iszTechTreeFile ) );

				// Tell our clients about the technology
				Q_strncpy( m_szTechTreeFile.GetForModify(), STRING(m_iszTechTreeFile), sizeof(m_szTechTreeFile) );
			}

			// Find the technology in the techtree 
			CBaseTechnology *pTechnology = pTechTree->GetTechnology( STRING(m_iszTech) );
			// Now hook the technology up to me
			if ( pTechnology )
			{
				pTechnology->RegisterWatcher( this );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the amount of our technology that's owned
//-----------------------------------------------------------------------------
void CInfoCustomTechnology::UpdateTechPercentage( float flPercentage )
{
	m_flTechPercentage.Set( flPercentage, this, this );
}