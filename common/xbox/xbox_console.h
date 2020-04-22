//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox console
//
//=====================================================================================//

#pragma once

#include "tier0/platform.h"

#ifndef STATIC_TIER0

#ifdef TIER0_DLL_EXPORT
	#define XBXCONSOLE_INTERFACE	DLL_EXPORT
	#define XBXCONSOLE_OVERLOAD	DLL_GLOBAL_EXPORT
#else
	#define XBXCONSOLE_INTERFACE	DLL_IMPORT
	#define XBXCONSOLE_OVERLOAD	DLL_GLOBAL_IMPORT
#endif

#else	// BUILD_AS_DLL

#define XBXCONSOLE_INTERFACE	extern
#define XBXCONSOLE_OVERLOAD	

#endif	// BUILD_AS_DLL

// all redirecting funneled here, stop redirecting in this module only
#undef OutputDebugString

#define XBX_MAX_PROFILE_COUNTERS 64

#if !defined( _X360 )
#define xMaterialList_t void
#define xTextureList_t	void
#define xSoundList_t	void
#define xMapInfo_t		void
#endif

class IXboxConsole
{
public:
	virtual void SendRemoteCommand( const char* dbgCommand, bool bAsync ) = 0;
	virtual void DebugString( unsigned int color, const char* format, ... ) = 0;
	virtual bool IsConsoleConnected() = 0;
	virtual void InitConsoleMonitor( bool bWaitForConnect = false ) = 0;
	virtual void DisconnectConsoleMonitor() = 0;
	virtual void FlushDebugOutput() = 0;
	virtual bool GetXboxName(char *, unsigned *) = 0;
	virtual void CrashDump( bool ) = 0;
	virtual void DumpDllInfo( const char *pBasePath ) = 0;
	virtual void OutputDebugString( const char * ) = 0;
	virtual bool IsDebuggerPresent() = 0;

	virtual int	 SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], unsigned int colors[] ) = 0;
	virtual void SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters ) = 0;
	virtual int  MemDump( const char *pDumpFileName ) = 0;
	virtual int	 TimeStampLog( float time, const char *pString ) = 0;
	virtual int  MaterialList( int nMaterials, const xMaterialList_t* pXMaterialList ) = 0;
	virtual int  TextureList( int nTextures, const xTextureList_t* pXTextureList ) = 0;
	virtual int  SoundList( int nSounds, const xSoundList_t* pXSoundList ) = 0;
	virtual int  MapInfo( const xMapInfo_t *pXMapInfo ) = 0;
	virtual int  AddCommands( int numCommands, const char* commands[], const char* help[] ) = 0;
};

class CXboxConsole : public IXboxConsole
{
public:
	void SendRemoteCommand( const char* dbgCommand, bool bAsync );
	void DebugString( unsigned int color, const char* format, ... );
	bool IsConsoleConnected();
	void InitConsoleMonitor( bool bWaitForConnect = false );
	void DisconnectConsoleMonitor();
	void FlushDebugOutput();
	bool GetXboxName(char *, unsigned *);
	void CrashDump( bool );
	int DumpModuleSize( const char *pName );
	void DumpDllInfo( const char *pBasePath );
	void OutputDebugString( const char * );
	bool IsDebuggerPresent();

	int	 SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], unsigned int colors[] );
	void SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters );
	int  MemDump( const char *pDumpFileName );
	int	 TimeStampLog( float time, const char *pString );
	int  MaterialList( int nMaterials, const xMaterialList_t* pXMaterialList );
	int  TextureList( int nTextures, const xTextureList_t* pXTextureList );
	int  SoundList( int nSounds, const xSoundList_t* pXSoundList );
	int  MapInfo( const xMapInfo_t *pXMapInfo );
	int  AddCommands( int numCommands, const char* commands[], const char* help[] );
};

XBXCONSOLE_INTERFACE IXboxConsole *g_pXboxConsole;
XBXCONSOLE_INTERFACE void XboxConsoleInit();

#define XBX_SendRemoteCommand			if ( !g_pXboxConsole ) ; else g_pXboxConsole->SendRemoteCommand
#define XBX_DebugString					if ( !g_pXboxConsole ) ; else g_pXboxConsole->DebugString
#define XBX_IsConsoleConnected			( !g_pXboxConsole ) ? false : g_pXboxConsole->IsConsoleConnected
#define XBX_InitConsoleMonitor			if ( !g_pXboxConsole ) ; else g_pXboxConsole->InitConsoleMonitor
#define XBX_DisconnectConsoleMonitor	if ( !g_pXboxConsole ) ; else g_pXboxConsole->DisconnectConsoleMonitor
#define XBX_FlushDebugOutput			if ( !g_pXboxConsole ) ; else g_pXboxConsole->FlushDebugOutput
#define XBX_GetXboxName					( !g_pXboxConsole ) ? false : g_pXboxConsole->GetXboxName
#define XBX_CrashDump					if ( !g_pXboxConsole ) ; else g_pXboxConsole->CrashDump
#define XBX_DumpDllInfo					if ( !g_pXboxConsole ) ; else g_pXboxConsole->DumpDllInfo
#define XBX_OutputDebugString			if ( !g_pXboxConsole ) ; else g_pXboxConsole->OutputDebugString
#define XBX_IsDebuggerPresent			( !g_pXboxConsole ) ? false : g_pXboxConsole->IsDebuggerPresent

#define XBX_rSetProfileAttributes		( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SetProfileAttributes
#define XBX_rSetProfileData				if ( !g_pXboxConsole ) ; else g_pXboxConsole->SetProfileData
#define XBX_rMemDump					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MemDump
#define XBX_rTimeStampLog				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->TimeStampLog
#define XBX_rMaterialList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MaterialList
#define XBX_rTextureList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->TextureList
#define XBX_rSoundList					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SoundList
#define XBX_rMapInfo					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MapInfo
#define XBX_rAddCommands				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->AddCommands


