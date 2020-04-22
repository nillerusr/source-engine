//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <vgui/VGUI.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include "tier0/vcrmode.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"

#include "vgui_internal.h"
#include "filesystem_helpers.h"
#include "vgui_key_translation.h"
#include "filesystem.h"

#ifdef OSX
#include <Carbon/Carbon.h>
#elif defined(LINUX)
#include <sys/vfs.h>
#endif

#ifdef USE_SDL
#include "SDL_clipboard.h"
#include "SDL_error.h"
#endif

#define PROTECTED_THINGS_DISABLE
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

uint16 System_GetKeyState( int virtualKeyCode )
{
#ifndef _XBOX
	return g_pVCR->Hook_GetKeyState(virtualKeyCode);
#else
	return 0;
#endif
}

class CSystem : public ISystem
{
public:
	CSystem();
	~CSystem();

	virtual void Shutdown();
	virtual void RunFrame();

	virtual long GetTimeMillis();

	// returns the time at the start of the frame
	virtual double GetFrameTime();

	// returns the current time
	virtual double GetCurrentTime();

	virtual void ShellExecute(const char *command, const char *file);

	virtual int GetClipboardTextCount();
	virtual void SetClipboardText(const char *text, int textLen);
	virtual void SetClipboardText(const wchar_t *text, int textLen);
	virtual int GetClipboardText(int offset, char *buf, int bufLen);
	virtual int GetClipboardText(int offset, wchar_t *buf, int bufLen);

	virtual void SetClipboardImage( void *pWnd, int x1, int y1, int x2, int y2 );

	virtual bool SetRegistryString(const char *key, const char *value);
	virtual bool GetRegistryString(const char *key, char *value, int valueLen);
	virtual bool SetRegistryInteger(const char *key, int value);
	virtual bool GetRegistryInteger(const char *key, int &value);
	virtual bool DeleteRegistryKey(const char *keyName);

	virtual bool SetWatchForComputerUse(bool state);
	virtual double GetTimeSinceLastUse();
	virtual int GetAvailableDrives(char *buf, int bufLen);
	virtual double GetFreeDiskSpace(const char *path);

	virtual KeyValues *GetUserConfigFileData(const char *dialogName, int dialogID);
	virtual void SetUserConfigFile(const char *fileName, const char *pathName);
	virtual void SaveUserConfigFile();

	virtual bool CommandLineParamExists(const char *commandName);
	virtual bool GetCommandLineParamValue(const char *paramName, char *value, int valueBufferSize);
	virtual const char *GetFullCommandLine();
	virtual bool GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second);

	// shortcut (.lnk) modification functions
	virtual bool CreateShortcut(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory, const char *iconFile);
	virtual bool GetShortcutTarget(const char *linkFileName, char *targetPath, char *arguments, int destBufferSizes);
	virtual bool ModifyShortcutTarget(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory);

	virtual KeyCode KeyCode_VirtualKeyToVGUI( int keyCode );
	virtual int KeyCode_VGUIToVirtualKey( KeyCode keyCode );
//	virtual MouseCode MouseCode_VirtualKeyToVGUI( int keyCode );
//	virtual int MouseCode_VGUIToVirtualKey( MouseCode keyCode );
	virtual const char *GetDesktopFolderPath();
	virtual const char *GetStartMenuFolderPath();
	virtual const char *GetAllUserDesktopFolderPath();
	virtual const char *GetAllUserStartMenuFolderPath();

	virtual void ShellExecuteEx( const char *command, const char *file, const char *pParams );
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, char *pchName );
#endif

private:
	void SaveRegistryToFile( bool bForce = false );
	bool m_bStaticWatchForComputerUse;
	double m_StaticLastComputerUseTime;
	int m_iStaticMouseOldX, m_iStaticMouseOldY;
	// timer data
	double m_flFrameTime;
	KeyValues *m_pUserConfigData;
	char m_szFileName[MAX_PATH];
	char m_szPathID[MAX_PATH];
	
	KeyValues *m_pRegistry;
	double m_flRegistrySaveTime;
	bool m_bRegistryDirty;
	
	char m_szRegistryPath[ MAX_PATH ];
#ifdef OSX
	PasteboardRef m_PasteBoardRef;
#endif
	
};


CSystem g_System;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSystem, ISystem, VGUI_SYSTEM_INTERFACE_VERSION, g_System);

namespace vgui
{
vgui::ISystem *g_pSystem = &g_System;
}

#define REGISTRY_NAME "cfg/registry.vdf"
#define REGISTRY_SAVE_INTERVAL 30

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSystem::CSystem()
{
	m_bStaticWatchForComputerUse = false;
	m_flFrameTime = 0.0;
	m_flRegistrySaveTime = 0.0;
	m_bRegistryDirty = false;
	m_pUserConfigData = NULL;
#ifdef OSX
	PasteboardCreate( kPasteboardClipboard, &m_PasteBoardRef );
#endif
	
	Q_snprintf( m_szRegistryPath, sizeof(m_szRegistryPath), "%s", REGISTRY_NAME );
	
	m_pRegistry = new KeyValues( "registry" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSystem::~CSystem()
{
	SaveRegistryToFile( true );
#ifdef OSX
	CFRelease( m_PasteBoardRef );
#endif
}
							
void CSystem::SaveRegistryToFile( bool bForce )
{
	/*if ( m_pRegistry && ( m_bRegistryDirty || bForce ) && g_pFullFileSystem )
	{
		m_pRegistry->SaveToFile( g_pFullFileSystem, m_szRegistryPath, "MOD" );
	}*/
	m_bRegistryDirty = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSystem::Shutdown()
{
	if (m_pUserConfigData)
	{
		m_pUserConfigData->deleteThis();
	}
	SaveRegistryToFile( true );
	if ( m_pRegistry )
	{
		m_pRegistry->deleteThis();
	}
	m_pRegistry = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Handles all the per frame actions
//-----------------------------------------------------------------------------
void CSystem::RunFrame()
{
	// record the current frame time
	m_flFrameTime = GetCurrentTime();

	if (m_bStaticWatchForComputerUse)
	{
		// check for mouse movement
		int x, y;
		g_pInput->GetCursorPos(x, y);
		// allow a little slack for jittery mice, don't reset until it's moved more than fifty pixels
		if (abs((x + y) - (m_iStaticMouseOldX + m_iStaticMouseOldY)) > 50)
		{
			m_StaticLastComputerUseTime = Plat_MSTime();
			m_iStaticMouseOldX = x;
			m_iStaticMouseOldY = y;
		}
	}
	
	if ( m_flFrameTime - m_flRegistrySaveTime > REGISTRY_SAVE_INTERVAL )
	{
		m_flRegistrySaveTime = m_flFrameTime;
		SaveRegistryToFile();
//		Registry_RunFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the time at the start of the frame
//-----------------------------------------------------------------------------
double CSystem::GetFrameTime()
{
	return m_flFrameTime;
}

//-----------------------------------------------------------------------------
// Purpose: returns the current time
//-----------------------------------------------------------------------------
double CSystem::GetCurrentTime()
{
	return Plat_FloatTime();
}


//-----------------------------------------------------------------------------
// Purpose: returns the current time in milliseconds
//-----------------------------------------------------------------------------
long CSystem::GetTimeMillis()
{
	return (long)(Plat_MSTime() );
}

//-----------------------------------------------------------------------------
// Purpose: Legacy stub to allow ShellExecute( "open", "file" ) -- doesn't otherwise work
//-----------------------------------------------------------------------------
void CSystem::ShellExecute(const char *command, const char *file)
{
	if ( V_strcmp( command, "open" ) != 0 )
	{
		// Nope
		Assert( !"This legacy command is only supported in the form of open <foo>" );
		return;
	}

#ifdef OSX
	const char *szCommand = "open";
#else
	const char *szCommand = "xdg-open";
#endif

	pid_t pid = fork();
	if ( pid == 0 )
	{
		// Child
#ifdef LINUX
		// Escape steam runtime if necessary
		const char *szSteamRuntime = getenv( "STEAM_RUNTIME" );
		if ( szSteamRuntime )
		{
			unsetenv( "STEAM_RUNTIME" );

			const char *szSystemLibraryPath = getenv( "SYSTEM_LD_LIBRARY_PATH" );
			const char *szSystemPath = getenv( "SYSTEM_PATH" );
			if ( szSystemLibraryPath )
			{
				setenv( "LD_LIBRARY_PATH", szSystemLibraryPath, 1 );
			}
			if ( szSystemPath )
			{
				setenv( "PATH", szSystemPath, 1 );
			}
		}
#endif
		execlp( szCommand, szCommand, file, (char *)0 );
		Assert( !"execlp failed" );
	}
}

void CSystem::ShellExecuteEx( const char *command, const char *file, const char *pParams )
{
	NOTE_UNUSED( pParams );
	ShellExecute( command, file );
}

void CSystem::SetClipboardText(const char *text, int textLen)
{
#ifdef OSX
	PasteboardSynchronize( m_PasteBoardRef );
	PasteboardClear( m_PasteBoardRef );
	CFDataRef theData = CFDataCreate( kCFAllocatorDefault, (const UInt8*)text, textLen );
	PasteboardPutItemFlavor( m_PasteBoardRef, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), theData, 0 );
	CFRelease( theData );
#elif defined( USE_SDL )
	if ( Q_strlen( text ) <= textLen )
	{
		if ( SDL_SetClipboardText( text ) )
		{
			Msg( "SDL_SetClipboardText failed: %s\n", SDL_GetError() );
		}
	}
	else
	{
		char *ClipText = ( char *)malloc( textLen + 1 );
		if ( ClipText )
		{
			Q_strncpy( ClipText, text, textLen + 1 );
			if ( SDL_SetClipboardText( ClipText ) )
			{
				Msg( "SDL_SetClipboardText failed: %s\n", SDL_GetError() );
			}
			free( ClipText );
		}
	}
#endif
}

void CSystem::SetClipboardImage( void *pWnd, int x1, int y1, int x2, int y2 )
{
	Assert( false );
}



//-----------------------------------------------------------------------------
// Purpose: Puts unicode text into the clipboard
//-----------------------------------------------------------------------------
void CSystem::SetClipboardText(const wchar_t *text, int textLen)
{
	char *charStr = (char *)malloc( textLen * 4 );

	Q_UnicodeToUTF8( text, charStr, textLen*4 );

#ifdef OSX
	PasteboardSynchronize( m_PasteBoardRef );
	PasteboardClear( m_PasteBoardRef );

	CFDataRef theData = CFDataCreate( kCFAllocatorDefault, (const UInt8*)charStr, Q_strlen(charStr) );
	PasteboardPutItemFlavor( m_PasteBoardRef, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), theData, 0 );
	CFRelease( theData );
#elif defined( USE_SDL )
	SetClipboardText( charStr, Q_strlen( charStr ) );
#endif

	free( charStr );
}

int CSystem::GetClipboardTextCount()
{
#ifdef OSX
	ItemCount count;
	PasteboardSynchronize( m_PasteBoardRef );
	
	OSStatus err = PasteboardGetItemCount( m_PasteBoardRef, &count );
	if ( err != noErr )
		return 0;
	
	if ( count <= 0 )
		return 0;
	
	PasteboardItemID ItemID;
	// always use the last item on the clipboard for any cut and paste data
	err = PasteboardGetItemIdentifier( m_PasteBoardRef, count, &ItemID );
	if ( err != noErr )
		return 0;
	CFDataRef outData;
	err = PasteboardCopyItemFlavorData ( m_PasteBoardRef, ItemID, CFSTR ("public.utf8-plain-text"), &outData);
	if ( err != noErr )
		return 0;
	
	int copyLen = CFDataGetLength( outData );
	CFRelease( outData );
	return (int)copyLen + 1;
#elif defined( USE_SDL )
	int Count = 0;

	if ( SDL_HasClipboardText() )
	{
		char *text = SDL_GetClipboardText();

		if ( text )
		{
			Count = Q_strlen( text ) + 1;
			SDL_free( text );
		}
	}

	return Count;
#else
	return 0;
#endif
}

int CSystem::GetClipboardText(int offset, char *buf, int bufLen)
{
	Assert( !offset );

#ifdef OSX
	ItemCount count;
	PasteboardSynchronize( m_PasteBoardRef );
	
	OSStatus err = PasteboardGetItemCount( m_PasteBoardRef, &count );
	if ( err != noErr )
		return 0;
	
	char *pchOutData;
	PasteboardItemID ItemID;
	// pull the last item from the clipboard
	err = PasteboardGetItemIdentifier( m_PasteBoardRef, count, &ItemID );
	if ( err != noErr )
		return 0;
	CFDataRef outData;
	err = PasteboardCopyItemFlavorData ( m_PasteBoardRef, ItemID, CFSTR ("public.utf8-plain-text"), &outData);
	if ( err != noErr )
		return 0;
	pchOutData = (char *)CFDataGetBytePtr(outData );
	int copyLen = MIN( CFDataGetLength( outData ), bufLen ) ;
	if ( pchOutData )
		memcpy( buf, pchOutData, copyLen );
	CFRelease( outData );
	return copyLen;
#elif defined( USE_SDL )
	if( SDL_HasClipboardText() )
	{
		char *text = SDL_GetClipboardText();

		if ( text )
		{
			Q_strncpy( buf, text, bufLen );
			SDL_free( text );
			return Q_strlen( buf );
		}
	}

	return 0;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves unicode text from the clipboard
//-----------------------------------------------------------------------------
int CSystem::GetClipboardText(int offset, wchar_t *buf, int bufLen)
{
	Assert( !offset );

	char *outputUTF8 = (char *)malloc( bufLen*4 );
	int ret = GetClipboardText( offset, outputUTF8, bufLen );

	if ( ret )
	{
		Q_UTF8ToUnicode( outputUTF8, buf, bufLen );
	}
	else if( bufLen > 0 )
	{
		buf[ 0 ] = 0;
	}

	free( outputUTF8 );
	return ret;
}


bool CSystem::SetRegistryString(const char *key, const char *value)
{
	m_bRegistryDirty = true;
	m_pRegistry->SetString( key, value );
	return true;
}

bool CSystem::GetRegistryString(const char *key, char *value, int valueLen)
{
	const char *pchVal = m_pRegistry->GetString( key );
	if ( pchVal )
		Q_strncpy( value, pchVal, valueLen );
	return pchVal != NULL;
}

bool CSystem::SetRegistryInteger(const char *key, int value)
{
	m_bRegistryDirty = true;
	m_pRegistry->SetInt( key, value );
	return false;
}

bool CSystem::GetRegistryInteger(const char *key, int &value)
{
	value = m_pRegistry->GetInt( key );
	return value != 0;
}

//-----------------------------------------------------------------------------
// Purpose: recursively deletes a registry key and all it's subkeys
//-----------------------------------------------------------------------------
bool CSystem::DeleteRegistryKey(const char *key)
{
	Assert( false );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the app watches for global computer use
//-----------------------------------------------------------------------------
bool CSystem::SetWatchForComputerUse(bool state)
{
	if (state == m_bStaticWatchForComputerUse)
		return true;

	m_bStaticWatchForComputerUse = state;

	if (m_bStaticWatchForComputerUse)
	{
		// enable watching
	}
	else
	{
		// disable watching
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the time, in seconds, since the last computer use.
//-----------------------------------------------------------------------------
double CSystem::GetTimeSinceLastUse()
{
	if (m_bStaticWatchForComputerUse)
	{
		return ( Plat_MSTime() - m_StaticLastComputerUseTime ) / 1000.0f;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Get the drives a user has available on thier system
//-----------------------------------------------------------------------------
int CSystem::GetAvailableDrives(char *buf, int bufLen)
{
	Assert( false );
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the amount of available disk space, in bytes, on the specified path
//-----------------------------------------------------------------------------
double CSystem::GetFreeDiskSpace(const char *path)
{
	struct statfs64 buf;
	int ret = statfs64( path, &buf );
	if ( ret < 0 )
		return 0.0;
	return (double) ( buf.f_bsize * buf.f_bfree );
}

//-----------------------------------------------------------------------------
// Purpose: user config
//-----------------------------------------------------------------------------
KeyValues *CSystem::GetUserConfigFileData(const char *dialogName, int dialogID)
{
	if (!m_pUserConfigData)
		return NULL;

	Assert(dialogName && *dialogName);

	if (dialogID)
	{
		char buf[256];
		Q_snprintf(buf, sizeof(buf), "%s_%d", dialogName, dialogID);
		dialogName = buf;
	}

	return m_pUserConfigData->FindKey(dialogName, true);
}

//-----------------------------------------------------------------------------
// Purpose: sets the name of the config file to save/restore from.  Settings are loaded immediately.
//-----------------------------------------------------------------------------
void CSystem::SetUserConfigFile(const char *fileName, const char *pathName)
{
	//m_pRegistry->LoadFromFile( g_pFullFileSystem, m_szRegistryPath, NULL );
	
	if (!m_pUserConfigData)
	{
		m_pUserConfigData = new KeyValues("UserConfigData");
	}
	else
	{
		// delete all the existing keys so when we reload from the new file we don't
		// get duplicate entries in our key value
		m_pUserConfigData->Clear();
	}

	Q_strncpy(m_szFileName, fileName, sizeof(m_szFileName));
	Q_strncpy(m_szPathID, pathName, sizeof(m_szPathID));

	// open
	m_pUserConfigData->UsesEscapeSequences( true ); // VGUI may use this
	m_pUserConfigData->LoadFromFile(g_pFullFileSystem, m_szFileName, m_szPathID);
}

//-----------------------------------------------------------------------------
// Purpose: saves all the current settings to the user config file
//-----------------------------------------------------------------------------
void CSystem::SaveUserConfigFile()
{
	if (m_pUserConfigData)
	{
		m_pUserConfigData->SaveToFile(g_pFullFileSystem, m_szFileName, m_szPathID);
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns whether or not the parameter was on the command line
//-----------------------------------------------------------------------------
bool CSystem::CommandLineParamExists(const char *paramName)
{
	if ( Q_strstr( Plat_GetCommandLine(), paramName ) )
		return true;
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: gets the string following a command line param
//-----------------------------------------------------------------------------
bool CSystem::GetCommandLineParamValue(const char *paramName, char *value, int valueBufferSize)
{
	Assert( false );
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the currently running exe
//-----------------------------------------------------------------------------
const char *CSystem::GetFullCommandLine()
{
	return VCRHook_GetCommandLine();
}


KeyCode CSystem::KeyCode_VirtualKeyToVGUI( int keyCode )
{
	return ::KeyCode_VirtualKeyToVGUI( keyCode );
}

int CSystem::KeyCode_VGUIToVirtualKey( KeyCode keyCode )
{
	return ::KeyCode_VGUIToVirtualKey( keyCode );
}

/*MouseCode CSystem::MouseCode_VirtualKeyToVGUI( int keyCode )
{
	return ::MouseCode_VirtualKeyToVGUI( keyCode );
}

int CSystem::MouseCode_VGUIToVirtualKey( MouseCode mouseCode )
{
	return ::MouseCode_VGUIToVirtualKey( mouseCode );
}*/


//-----------------------------------------------------------------------------
// Purpose: returns the current local time and date
//-----------------------------------------------------------------------------
bool CSystem::GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second)
{
	time_t t = time( NULL );
	struct tm *now = localtime( &t );
	if ( now )
	{
		if ( year ) *year = now->tm_year + 1900;
		if ( month ) *month = now->tm_mon + 1;
		if ( dayOfWeek ) *dayOfWeek = now->tm_wday;
		if ( day ) *day = now->tm_mday;
		if ( hour ) *hour = now->tm_hour;
		if ( minute ) *minute = now->tm_min;
		if ( second )  *second = now->tm_sec;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a shortcut file
//-----------------------------------------------------------------------------
bool CSystem::CreateShortcut(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory, const char *iconFile)
{
	Assert( false );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: retrieves shortcut (.lnk) information
//-----------------------------------------------------------------------------
bool CSystem::GetShortcutTarget(const char *linkFileName, char *targetPath, char *arguments, int destBufferSizes)
{
	Assert( false );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: sets shortcut (.lnk) information
//-----------------------------------------------------------------------------
bool CSystem::ModifyShortcutTarget(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory)
{
	Assert( false );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the full path of the current user's desktop folder
//-----------------------------------------------------------------------------
const char *CSystem::GetDesktopFolderPath()
{
	Assert( false );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: returns the full path of the all user's desktop folder
//-----------------------------------------------------------------------------
const char *CSystem::GetAllUserDesktopFolderPath()
{
	Assert( false );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: returns the full path of the current user's start->program files
//-----------------------------------------------------------------------------
const char *CSystem::GetStartMenuFolderPath()
{
	Assert( false );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: returns the full path of the all user's start->program files
//-----------------------------------------------------------------------------
const char *CSystem::GetAllUserStartMenuFolderPath()
{
	Assert( false );
	return NULL;
}



//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSystem::Validate( CValidator &validator, char *pchName )
{
	VALIDATE_SCOPE();
	ValidatePtr( m_pUserConfigData );
}


void Validate_System( CValidator &validator )
{
	ValidateObj( g_System );
}
#endif
