#include "cbase.h"
#include "c_asw_campaign_save.h"
#include "steam/isteamremotestorage.h"
#include "filesystem.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Campaign_Save, DT_ASW_Campaign_Save, CASW_Campaign_Save )
	RecvPropString(RECVINFO(m_CampaignName)),	
	RecvPropInt(RECVINFO(m_iCurrentPosition)),
	RecvPropInt(RECVINFO(m_iNumMissionsComplete)),
	RecvPropArray3( RECVINFO_ARRAY(m_MissionComplete), RecvPropInt( RECVINFO(m_MissionComplete[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_NumRetries), RecvPropInt( RECVINFO(m_NumRetries[0]))),
	
	RecvPropArray3( RECVINFO_ARRAY( m_bMarineWounded ), RecvPropBool( RECVINFO( m_bMarineWounded[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bMarineDead ), RecvPropBool( RECVINFO( m_bMarineDead[0] ) ) ),
	
	RecvPropArray( RecvPropString( RECVINFO( m_MissionsCompleteNames[0]) ), m_MissionsCompleteNames ),
	RecvPropArray( RecvPropString( RECVINFO( m_Medals[0]) ), m_Medals ),
	RecvPropBool(RECVINFO(m_bMultiplayerGame)),
	RecvPropString(RECVINFO(m_DateTime)),	
	
	RecvPropArray3( RECVINFO_ARRAY(m_NumVotes), RecvPropInt( RECVINFO(m_NumVotes[0]))),
	RecvPropFloat( RECVINFO(m_fVoteEndTime) ),
	RecvPropBool( RECVINFO(m_bFixedSkillPoints) ),
END_RECV_TABLE()

C_ASW_Campaign_Save::C_ASW_Campaign_Save()
{
	m_iVersion = 0;
	m_CampaignName[0] = '\0';
	m_szPreviousCampaignName[0] = '\0';
	m_iCurrentPosition = 0;
	m_bMultiplayerGame = false;
}

C_ASW_Campaign_Save::~C_ASW_Campaign_Save()
{
}

const char* C_ASW_Campaign_Save::GetCampaignName()
{
	return m_CampaignName;
}

int C_ASW_Campaign_Save::GetRetries()
{
	if (m_iCurrentPosition <0 || m_iCurrentPosition >= ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return -1;

	return m_NumRetries[m_iCurrentPosition];
}

void C_ASW_Campaign_Save::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( Q_strcmp( m_szPreviousCampaignName, m_CampaignName ) )
	{
		Q_strncpy( m_szPreviousCampaignName, m_CampaignName, sizeof( m_szPreviousCampaignName ) );
		
		IGameEvent * event = gameeventmanager->CreateEvent( "campaign_changed" );
		if ( event )
		{
			event->SetString( "campaign", m_CampaignName );
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

ConVar asw_steam_cloud_debug( "asw_steam_cloud_debug", "1", FCVAR_NONE, "Print Steam Cloud messages" );

bool GetFileFromRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName )
{
	bool bSuccess = false;

	// check if file exists in Steam Cloud first
	int32 nFileSize = pRemoteStorage->GetFileSize( pszRemoteFileName );

	if ( nFileSize > 0 )
	{
		CUtlMemory<char> buf( 0, nFileSize );
		if ( pRemoteStorage->FileRead( pszRemoteFileName, buf.Base(), nFileSize ) == nFileSize )
		{
			FileHandle_t hFile = g_pFullFileSystem->Open( pszLocalFileName, "wb", "MOD" );
			if( hFile )
			{
				bSuccess = g_pFullFileSystem->Write( buf.Base(), nFileSize, hFile ) == nFileSize;
				g_pFullFileSystem->Close( hFile );

				if ( asw_steam_cloud_debug.GetBool() )
				{
					if ( bSuccess )
					{
						DevMsg( "[Cloud]: SUCCEESS retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
					}
					else
					{
						DevMsg( "[Cloud]: FAILED retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
					}
				}
			}
		}
	}

	return bSuccess;
}

bool WriteFileToRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName )
{
	bool bSuccess = false;

	if ( g_pFullFileSystem->FileExists( pszLocalFileName, "MOD" ) )
	{
		FileHandle_t hFile = g_pFullFileSystem->Open( pszLocalFileName, "rb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != hFile )
		{
			unsigned int unSize = g_pFullFileSystem->Size( hFile );

			byte *pBuffer = (byte*) malloc( unSize );
			if ( g_pFullFileSystem->Read( pBuffer, unSize, hFile ) == (int) unSize )
			{
				bSuccess = pRemoteStorage->FileWrite( pszRemoteFileName, pBuffer, unSize );
			}
			free( pBuffer );
			g_pFullFileSystem->Close( hFile );
		}
	}

	if ( asw_steam_cloud_debug.GetBool() )
	{
		if ( bSuccess )
		{
			DevMsg( "[Cloud]: SUCCEESS wrote %s from local storage into remote file %s\n", pszLocalFileName, pszRemoteFileName );
		}
		else
		{
			DevMsg( "[Cloud]: FAILED writing %s from local storage into remote file %s\n", pszLocalFileName, pszRemoteFileName );
		}
	}

	return bSuccess;
}