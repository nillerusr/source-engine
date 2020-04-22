//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#if defined( REPLAY_ENABLED )

#include "replay/ienginereplay.h"
#include "replay/replayutils.h"
#include "client.h"
#include "server.h"
#include "cl_demo.h"
#include "ivideomode.h"
#include "replayserver.h"
#include "cl_steamauth.h"
#include "host_state.h"
#include "globalvars_base.h"
#include "vgui_baseui_interface.h"
#include "replay_internal.h"
#include "sv_steamauth.h"
#include "lzss.h"
#include "checksum_engine.h"

#if !defined( DEDICATED )
#include "con_nprint.h"
#include "net_chan.h"
#include "download.h"
#include "audio/public/snd_device.h"
#include "audio/private/snd_wave_temp.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

class CEngineReplay : public IEngineReplay
{
public:
	virtual bool IsSupportedModAndPlatform()
	{
		return Replay_IsSupportedModAndPlatform();
	}

	virtual const char *GetGameDir()
	{
		return com_gamedir;
	}

	virtual float GetHostTime()
	{
		return host_time;
	}

	virtual int GetHostTickCount()
	{
		return host_tickcount;
	}

	virtual int TimeToTicks( float flTime )
	{
		return TIME_TO_TICKS( flTime );
	}

	virtual float TicksToTime( int nTick )
	{
		return TICKS_TO_TIME( nTick );
	}

	virtual void Cbuf_AddText( const char *pCmd )
	{
		::Cbuf_AddText( pCmd );
	}

	virtual void Cbuf_Execute()
	{
		::Cbuf_Execute();
	}

	virtual void Host_Disconnect( bool bShowMainMenu )
	{
		::Host_Disconnect( bShowMainMenu );
	}

	virtual void HostState_Shutdown()
	{
		::HostState_Shutdown();
	}

	virtual const char *GetModDir()
	{
		return COM_GetModDirectory();
	}

	virtual bool CopyFile( const char *pSource, const char *pDest )
	{
		return COM_CopyFile( pSource, pDest );
	}

	virtual bool LZSS_Compress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return COM_BufferToBufferCompress( pDest, pDestLen, pSource, nSourceLen );
	}

	virtual bool LZSS_Decompress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return COM_BufferToBufferDecompress( pDest, pDestLen, pSource, nSourceLen );
	}

	virtual bool MD5_HashBuffer( unsigned char pDigest[16], const unsigned char *pBuffer, int nSize, unsigned int pSeed[4] )
	{
		return ::MD5_Hash_Buffer( pDigest, pBuffer, nSize, pSeed != NULL, pSeed );
	}

	virtual bool ReadDemoHeader( const char *pFilename, demoheader_t &header )
	{
		V_memset( &header, 0, sizeof( header ) );

		CDemoFile demofile;
		if ( !demofile.Open( pFilename, true ) )
			return false;

		demofile.ReadDemoHeader();

		V_memcpy( &header, &demofile.m_DemoHeader, sizeof( header ) );
		
		return true;
	}

	virtual IReplayServer *GetReplayServer()
	{
		return replay;
	}

	virtual IServer *GetReplayServerAsIServer()
	{
		return replay;
	}

	virtual IServer *GetGameServer()
	{
		if ( sv.IsDedicated() )
		{
			return &sv;
		}

		return NULL;
	}

	virtual bool GetSessionRecordBuffer( uint8 **ppSessionBuffer, int *pSize )
	{
		if ( !replay )
		{
			AssertMsg( 0, "Why is this being called when replay is inactive?" );
			*ppSessionBuffer = NULL;
			*pSize = 0;
			return false;
		}

		*ppSessionBuffer = (uint8 *)replay->m_DemoRecorder.m_DemoFile.m_pBuffer->Base();
		*pSize = replay->m_DemoRecorder.m_DemoFile.m_pBuffer->TellPut();

		return true;
	}

	virtual void ResetReplayRecordBuffer()
	{
		replay->m_DemoRecorder.m_DemoFile.m_pBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	}

	virtual bool IsDedicated()
	{
		return sv.IsDedicated();
	}

	virtual demoheader_t *GetReplayDemoHeader()
	{
		return &replay->m_DemoRecorder.m_DemoFile.m_DemoHeader;
	}

	virtual void RecalculateTags()
	{
		sv.RecalculateTags();
	}

	virtual bool NET_GetHostnameAsIP( const char *pHostname, char *pOut, int nOutSize )
	{
		netadr_t adr;
		if ( !NET_StringToAdr( pHostname, &adr ) )
			return false;

		V_strncpy( pOut, adr.ToString( true ), nOutSize );

		return true;
	}
};

//-----------------------------------------------------------------------------

#if !defined( DEDICATED )

class CEngineClientReplay : public IEngineClientReplay
{
public:
	virtual float GetLastServerTickTime()
	{
		return TIME_TO_TICKS( cl.m_flLastServerTickTime );
	}

	virtual const char *GetLevelName()
	{
		return cl.m_szLevelFileName;
	}

	virtual const char *GetLevelNameShort()
	{
		return cl.m_szLevelBaseName;
	}

	virtual int GetPlayerSlot()
	{
		return cl.m_nPlayerSlot;
	}

	virtual bool IsPlayingReplayDemo()
	{
		extern IDemoPlayer *g_pReplayDemoPlayer;
		return demoplayer == g_pReplayDemoPlayer &&
			   demoplayer->IsPlayingBack();
	}

	virtual INetChannel *GetNetChannel()
	{
		return cl.m_NetChannel;
	}

	virtual bool IsConnected()
	{
		return cl.IsConnected();
	}

	virtual bool IsListenServer()
	{
		return sv.IsActive();
	}

	virtual IClientEntityList *GetClientEntityList()
	{
		extern IClientEntityList *entitylist;
		return entitylist;
	}

	virtual IClientReplay *GetClientReplayInt()
	{
		extern IClientReplay *g_pClientReplay;
		return g_pClientReplay;
	}
	
	virtual uint32 GetClientSteamID()
	{
		CSteamID steamID = Steam3Client().SteamUser()->GetSteamID();
		return steamID.GetAccountID();
	}

	void Con_NPrintf( int nPos, const char *pFormat, ... )
	{
		va_list argptr;
		char szText[4096];

		va_start ( argptr, pFormat );
		Q_vsnprintf( szText, sizeof( szText ), pFormat, argptr );
		va_end ( argptr );

		::Con_NPrintf( nPos, "%s", szText );
	}

	virtual CGlobalVarsBase	*GetClientGlobalVars()
	{
		return &g_ClientGlobalVariables;
	}

	virtual void VGui_PlaySound( const char *pSound )
	{
		::VGui_PlaySound( pSound );
	}

	virtual void EngineVGui_ConfirmQuit()
	{
		EngineVGui()->ConfirmQuit();
	}

	virtual bool IsDemoPlayingBack()
	{
		return demoplayer->IsPlayingBack();
	}

	virtual int GetScreenWidth()
	{
		return videomode->GetModeStereoWidth();
	}

	virtual int GetScreenHeight()
	{
		return videomode->GetModeStereoHeight();
	}

	virtual bool IsGamePathValidAndSafeForDownload( const char *pGamePath )
	{
		return CL_IsGamePathValidAndSafeForDownload( pGamePath );
	}

	virtual bool IsInGame()
	{
		return cl.IsActive();
	}

	virtual void InitSoundRecord()
	{
		extern void SND_RecordInit();
		SND_RecordInit();
	}

	virtual void Wave_CreateTmpFile( const char *pFilename )
	{
		::WaveCreateTmpFile( pFilename, SOUND_DMA_SPEED, 16, 2 );
	}

	virtual void Wave_AppendTmpFile( const char *pFilename, void *pBuffer, int nNumSamples )
	{
		::WaveAppendTmpFile( pFilename, pBuffer, 16, nNumSamples );
	}

	virtual void Wave_FixupTmpFile( const char *pFilename )
	{
		::WaveFixupTmpFile( pFilename );
	}
};

#endif	// !defined( DEDICATED )

//-----------------------------------------------------------------------------

static CEngineReplay s_EngineReplay;
IEngineReplay *g_pEngineReplay = &s_EngineReplay;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineReplay, IEngineReplay, ENGINE_REPLAY_INTERFACE_VERSION, s_EngineReplay );

#if !defined( DEDICATED )
static CEngineClientReplay s_EngineClientReplay;
IEngineClientReplay *g_pEngineClientReplay = &s_EngineClientReplay;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineClientReplay, IEngineClientReplay, ENGINE_REPLAY_CLIENT_INTERFACE_VERSION, s_EngineClientReplay );
#endif

//-----------------------------------------------------------------------------

#endif
