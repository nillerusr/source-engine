//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "dod_team.h"
#include "entitylist.h"
#include "dod_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Datatable
IMPLEMENT_SERVERCLASS_ST(CDODTeam, DT_DODTeam)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( dod_team_manager, CDODTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified TF team manager
//-----------------------------------------------------------------------------
CDODTeam *GetGlobalDODTeam( int iIndex )
{
	return (CDODTeam*)GetGlobalTeam( iIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
void CDODTeam::Init( const char *pName, int iNumber )
{
	BaseClass::Init( pName, iNumber );

	// Only detect changes every half-second.
	NetworkProp()->SetUpdateInterval( 0.75f );

	m_hPlayerClassInfoHandles.Purge();
}

void CDODTeam::AddPlayerClass( const char *szClassName )
{
	PLAYERCLASS_FILE_INFO_HANDLE hPlayerClassInfo;

	if ( ReadPlayerClassDataFromFileForSlot( filesystem, szClassName, &hPlayerClassInfo, GetEncryptionKey() ) )
	{
		m_hPlayerClassInfoHandles.AddToTail( hPlayerClassInfo );
	}
	else
	{
		Assert( !"missing playerclass script file" );
		Msg( "Missing playerclass script file for class: %s\n", szClassName );
	}	
}

const CDODPlayerClassInfo &CDODTeam::GetPlayerClassInfo( int iPlayerClass ) const
{
	Assert( iPlayerClass >= 0 && iPlayerClass < m_hPlayerClassInfoHandles.Count() );

	const FilePlayerClassInfo_t *pPlayerClassInfo = GetFilePlayerClassInfoFromHandle( m_hPlayerClassInfoHandles[iPlayerClass] );
	const CDODPlayerClassInfo *pDODInfo;

	#ifdef _DEBUG
		pDODInfo = dynamic_cast< const CDODPlayerClassInfo* >( pPlayerClassInfo );
		Assert( pDODInfo );
	#else
		pDODInfo = static_cast< const CDODPlayerClassInfo* >( pPlayerClassInfo );
	#endif

	return *pDODInfo;
}

bool CDODTeam::IsClassOnTeam( const char *pszClassName, int &iClassNum ) const
{
	iClassNum = PLAYERCLASS_UNDEFINED;

	// Random is always on every team
	if( FStrEq( pszClassName, "cls_random" ) )
	{
		iClassNum = PLAYERCLASS_RANDOM;
		return true;
	}
	
	for( int i=0;i<m_hPlayerClassInfoHandles.Count(); i++ )
	{
		FilePlayerClassInfo_t *pPlayerClassInfo = GetFilePlayerClassInfoFromHandle( m_hPlayerClassInfoHandles[i] );

		if( stricmp( pszClassName, pPlayerClassInfo->m_szSelectCmd ) == 0 )
		{
			iClassNum = i;
			return true;
		}
	}

	return false;
}

void CDODTeam::ResetScores( void )
{
	SetRoundsWon(0);
	SetScore(0);
}

//==================================
// TEAMS!
//==================================

// US REGULARS
//==================

class CDODTeam_Allies : public CDODTeam
{
DECLARE_CLASS( CDODTeam_Allies, CDODTeam );

DECLARE_SERVERCLASS();

	virtual void Init( const char *pName, int iNumber )
	{
		BaseClass::Init( pName, iNumber );

		int i = 0;
		while( pszTeamAlliesClasses[i] != NULL )
		{
			AddPlayerClass( pszTeamAlliesClasses[i] );
			i++;
		}	
	}

	virtual const char *GetTeamName( void ) { return "#Teamname_Allies"; }
};

IMPLEMENT_SERVERCLASS_ST(CDODTeam_Allies, DT_DODTeam_Allies)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( dod_team_allies, CDODTeam_Allies );


// AXIS REGULAR
//==================

class CDODTeam_Axis : public CDODTeam
{
DECLARE_CLASS( CDODTeam_Axis, CDODTeam );

DECLARE_SERVERCLASS();

	virtual void Init( const char *pName, int iNumber )
	{
		BaseClass::Init( pName, iNumber );

		int i = 0;
		while( pszTeamAxisClasses[i] != NULL )
		{
			AddPlayerClass( pszTeamAxisClasses[i] );
			i++;
		}	
	}

	virtual const char *GetTeamName( void ) { return "#Teamname_Allies"; }
};

IMPLEMENT_SERVERCLASS_ST(CDODTeam_Axis, DT_DODTeam_Axis)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( dod_team_axis, CDODTeam_Axis );