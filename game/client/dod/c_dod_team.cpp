//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side C_DODTeam class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "engine/IEngineSound.h"
#include "hud.h"
#include "recvproxy.h"
#include "c_dod_player.h"
#include "c_dod_team.h"
#include "dod_shareddefs.h"
#include "c_dod_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_DODTeam, DT_DODTeam, CDODTeam)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODTeam::C_DODTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODTeam::~C_DODTeam()
{
}

void C_DODTeam::AddPlayerClass( const char *szClassName )
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

const CDODPlayerClassInfo &C_DODTeam::GetPlayerClassInfo( int iPlayerClass ) const
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

bool C_DODTeam::IsClassOnTeam( const char *pszClassName, int &iClassNum ) const
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

bool C_DODTeam::IsClassOnTeam( int iClassNum ) const
{
	return ( iClassNum >= 0 && iClassNum < m_hPlayerClassInfoHandles.Count() );
}

int C_DODTeam::CountPlayersOfThisClass( int iPlayerClass )
{
	int count = 0;

	C_DOD_PlayerResource *dod_PR = dynamic_cast<C_DOD_PlayerResource *>(g_PR);

	Assert( dod_PR );

	for ( int i=0;i<Get_Number_Players();i++ )
	{
		if ( iPlayerClass == dod_PR->GetPlayerClass(m_aPlayers[i]) )
			count++;
	}

	return count;
}

IMPLEMENT_CLIENTCLASS_DT(C_DODTeam_Allies, DT_DODTeam_Allies, CDODTeam_Allies)
END_RECV_TABLE()

C_DODTeam_Allies::C_DODTeam_Allies()
{
	//parse our classes
	int i = 0;
	while( pszTeamAlliesClasses[i] != NULL )
	{
		AddPlayerClass( pszTeamAlliesClasses[i] );
		i++;
	}	
}


IMPLEMENT_CLIENTCLASS_DT(C_DODTeam_Axis, DT_DODTeam_Axis, CDODTeam_Axis)
END_RECV_TABLE()

C_DODTeam_Axis::C_DODTeam_Axis()
{
	//parse our classes
	int i = 0;
	while( pszTeamAxisClasses[i] != NULL )
	{
		AddPlayerClass( pszTeamAxisClasses[i] );
		i++;
	}	
}