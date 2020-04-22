//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_replaymovie.h"
#include "replay/replayutils.h"
#include "replay/shared_defs.h"
#include "KeyValues.h"
#include "replay/replay.h"
#include "cl_replaycontext.h"
#include "cl_replaymanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CReplayMovie::CReplayMovie()
:	m_hReplay( REPLAY_HANDLE_INVALID ),
	m_bRendered( false ),
	m_bUploaded( false ),
	m_flRenderTime( 0.0f ),
	m_flLength( 0.0f ),
	m_pUserData( NULL )
{
	V_wcsncpy( m_wszTitle, L"Untitled", sizeof( m_wszTitle ) );
}

bool CReplayMovie::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_hReplay = (ReplayHandle_t)pIn->GetInt( "replay_handle", REPLAY_HANDLE_INVALID );
	m_bRendered = pIn->GetInt( "rendered" ) != 0;
	V_wcsncpy( m_wszTitle, pIn->GetWString( "title" ), sizeof( m_wszTitle ) );
	m_strFilename = pIn->GetString( "filename" );
	m_strUploadURL = pIn->GetString( "upload_url" );
	m_bUploaded = pIn->GetInt( "uploaded" ) != 0;
	m_flRenderTime = pIn->GetFloat( "rendertime" );
	m_flLength = pIn->GetFloat( "length" );
	m_RecordTime.Read( pIn );

	return ReadRenderSettings( pIn );
}

void CReplayMovie::Write( KeyValues* pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "replay_handle", (int)m_hReplay );
	pOut->SetInt( "rendered", (int)m_bRendered );
	pOut->SetWString( "title", m_wszTitle );
	pOut->SetString( "filename", m_strFilename.Get() );
	pOut->SetString( "upload_url", m_strUploadURL.Get() );
	pOut->SetInt( "uploaded", (int)m_bUploaded );
	pOut->SetFloat( "rendertime", m_flRenderTime );
	pOut->SetFloat( "length", m_flLength );
	m_RecordTime.Write( pOut );

	WriteRenderSettings( pOut );
}

bool CReplayMovie::ReadRenderSettings( KeyValues *pIn )
{
	KeyValues *pRenderSettingsSubKey = pIn->FindKey( "rendersettings" );
	if ( !pRenderSettingsSubKey )
	{
		AssertMsg( 0, "No render settings sub key found for movie!" );
		return true;	// Continue to load anyway
	}

	m_RenderSettings.m_nWidth = pRenderSettingsSubKey->GetInt( "width" );
	m_RenderSettings.m_nHeight = pRenderSettingsSubKey->GetInt( "height" );
	m_RenderSettings.m_nMotionBlurQuality = pRenderSettingsSubKey->GetInt( "motionblurquality" );
	m_RenderSettings.m_FPS.SetRaw( pRenderSettingsSubKey->GetInt( "fps.ups" ), pRenderSettingsSubKey->GetInt( "fps.upf" ) );
	m_RenderSettings.m_Codec = ( VideoEncodeCodec_t )pRenderSettingsSubKey->GetInt( "codec" );
	m_RenderSettings.m_nEncodingQuality = pRenderSettingsSubKey->GetInt( "encoding_quality" );
	m_RenderSettings.m_bMotionBlurEnabled = pRenderSettingsSubKey->GetBool( "mb_enabled" );
	m_RenderSettings.m_bAAEnabled = pRenderSettingsSubKey->GetBool( "aa_enabled" );
	m_RenderSettings.m_bRaw = pRenderSettingsSubKey->GetBool( "raw" );

	return true;
}

void CReplayMovie::WriteRenderSettings( KeyValues *pOut )
{
	KeyValues *pRenderSettingsSubKey = new KeyValues( "rendersettings" );
	if ( !pRenderSettingsSubKey )
	{
		AssertMsg( 0, "Failed to allocate render settings sub key for movie!" );
		return;
	}

	pOut->AddSubKey( pRenderSettingsSubKey );

	pRenderSettingsSubKey->SetInt( "width", m_RenderSettings.m_nWidth );
	pRenderSettingsSubKey->SetInt( "height", m_RenderSettings.m_nHeight );
	pRenderSettingsSubKey->SetInt( "motionblurquality", m_RenderSettings.m_nMotionBlurQuality );
	pRenderSettingsSubKey->SetInt( "fps.ups", m_RenderSettings.m_FPS.GetUnitsPerSecond() );
	pRenderSettingsSubKey->SetInt( "fps.upf", m_RenderSettings.m_FPS.GetUnitsPerFrame() );
	pRenderSettingsSubKey->SetInt( "codec", (int)m_RenderSettings.m_Codec );
	pRenderSettingsSubKey->SetInt( "encoding_quality", m_RenderSettings.m_nEncodingQuality );
	pRenderSettingsSubKey->SetInt( "mb_enabled", (int)m_RenderSettings.m_bMotionBlurEnabled );
	pRenderSettingsSubKey->SetInt( "aa_enabled", (int)m_RenderSettings.m_bAAEnabled );
	pRenderSettingsSubKey->SetInt( "raw", (int)m_RenderSettings.m_bRaw );
}

const char *CReplayMovie::GetSubKeyTitle() const
{
	return Replay_va( "movie_%i", GetHandle() );
}

const char *CReplayMovie::GetPath() const
{
	return Replay_va( "%s%s%c", g_pClientReplayContextInternal->GetBaseDir(), SUBDIR_MOVIES, CORRECT_PATH_SEPARATOR );
}

void CReplayMovie::OnDelete()
{
	// Remove the actual movie from disk
	g_pFullFileSystem->RemoveFile( Replay_va( "%s%s", CL_GetMovieManager()->GetRenderDir(), m_strFilename.Get() ) );
}

ReplayHandle_t CReplayMovie::GetMovieHandle() const
{
	return GetHandle();
}

ReplayHandle_t CReplayMovie::GetReplayHandle() const
{
	return m_hReplay;
}

const ReplayRenderSettings_t &CReplayMovie::GetRenderSettings()
{
	return m_RenderSettings;
}

void CReplayMovie::GetFrameDimensions( int &nWidth, int &nHeight )
{
	nWidth = m_RenderSettings.m_nWidth;
	nHeight = m_RenderSettings.m_nHeight;
}

void CReplayMovie::SetIsRendered( bool bIsRendered )
{
	m_bRendered = bIsRendered;
}

void CReplayMovie::SetMovieFilename( const char *pFilename )
{
	m_strFilename = pFilename;
}

const char *CReplayMovie::GetMovieFilename() const
{
	return m_strFilename.Get();
}

void CReplayMovie::SetMovieTitle( const wchar_t *pTitle )
{
	V_wcsncpy( m_wszTitle, pTitle, sizeof( m_wszTitle ) );
}

void CReplayMovie::SetRenderTime( float flRenderTime )
{
	m_flRenderTime = flRenderTime;
}

float CReplayMovie::GetRenderTime() const
{
	return m_flRenderTime;
}

void CReplayMovie::CaptureRecordTime()
{
	m_RecordTime.InitDateAndTimeToNow();
}

void CReplayMovie::SetLength( float flLength )
{
	m_flLength = flLength;
}

CReplay *CReplayMovie::GetReplay() const
{
	return static_cast< CReplay * >( ::GetReplay( m_hReplay ) );
}

bool CReplayMovie::IsUploaded() const
{
	return m_bUploaded;
}

void CReplayMovie::SetUploaded( bool bUploaded )
{
	m_bUploaded = bUploaded;
}

void CReplayMovie::SetUploadURL( const char *pURL )
{
	m_strUploadURL = pURL;
}

const char *CReplayMovie::GetUploadURL() const
{
	return m_strUploadURL.Get();
}

const CReplayTime &CReplayMovie::GetItemDate() const
{
	return m_RecordTime;
}

bool CReplayMovie::IsItemRendered() const
{
	return GetReplay()->IsItemRendered();
}

CReplay *CReplayMovie::GetItemReplay()
{
	return GetReplay();
}

ReplayHandle_t CReplayMovie::GetItemReplayHandle() const
{
	return m_hReplay;
}

QueryableReplayItemHandle_t	CReplayMovie::GetItemHandle() const
{
	return (QueryableReplayItemHandle_t)GetHandle();
}	

const wchar_t *CReplayMovie::GetItemTitle() const
{
	return m_wszTitle;
}

void CReplayMovie::SetItemTitle( const wchar_t *pTitle )
{
	V_wcsncpy( m_wszTitle, pTitle, sizeof( m_wszTitle ) );
}

float CReplayMovie::GetItemLength() const
{
	return m_flLength;
}

void *CReplayMovie::GetUserData()
{
	return m_pUserData;
}

void CReplayMovie::SetUserData( void *pUserData )
{
	m_pUserData = pUserData;
}

bool CReplayMovie::IsItemAMovie() const											
{
	return true;
}

//----------------------------------------------------------------------------------------
