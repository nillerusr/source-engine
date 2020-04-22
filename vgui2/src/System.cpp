//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#if !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>


#ifdef ShellExecute
#undef ShellExecute
#endif

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#ifdef GetTickCount
#undef GetTickCount
#endif

#include <vgui/VGUI.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include "tier0/vcrmode.h"
#include "filesystem.h"

#include "vgui_internal.h"
#include "filesystem_helpers.h"
#include "vgui_key_translation.h"
#include "filesystem.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#define PROTECTED_THINGS_DISABLE
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>




#ifndef _X360


//////////////////////////////////////////////////////////////////////////
//
// Service Windows routines
//
//////////////////////////////////////////////////////////////////////////

static BOOL CALLBACK GetMainApplicationWindowHWND_EnumProc( HWND hWnd, LPARAM lParam )
{
	// Return TRUE to continue enumeration or FALSE to stop

	// Given window thread/process id
	DWORD dwProcessId, dwThreadId;
	dwThreadId = GetWindowThreadProcessId( hWnd, &dwProcessId );

	// Our thread/process id
	DWORD dwOurProcessId;
	dwOurProcessId = GetCurrentProcessId();

	// Foreign process
	if ( dwOurProcessId != dwProcessId )
		return TRUE;
	// Service window
	if ( !IsWindowVisible( hWnd ) || !IsWindowEnabled( hWnd ) )
		return TRUE;

	// Assume that we found it
	*( HWND * )lParam = hWnd;
	return FALSE; // stop enumeration
}

static HWND GetMainApplicationWindowHWND()
{
	HWND hWnd = NULL;

	//
	// Pass 1: calling on a GUI thread
	//
	DWORD dwThreadId = GetCurrentThreadId();

	GUITHREADINFO gti;
	memset( &gti, 0, sizeof( gti ) );
	gti.cbSize = sizeof( gti );
	GetGUIThreadInfo( dwThreadId, &gti );

	hWnd = gti.hwndActive;
	for ( HWND hParent = hWnd ? GetParent( hWnd ) : hWnd;
		hParent; hWnd = hParent, hParent = GetParent( hWnd ) )
		continue;

	if ( hWnd )
		return hWnd;

	//
	// Pass 2: non-GUI thread requiring the main winow
	//
	EnumWindows( GetMainApplicationWindowHWND_EnumProc, ( LPARAM ) &hWnd );
	if ( hWnd )
		return hWnd;

	// Failed to find the window by all means...
	return NULL;
}


#endif // #ifndef _X360



//////////////////////////////////////////////////////////////////////////
//
// ISystem implementation
//
//////////////////////////////////////////////////////////////////////////


using namespace vgui;

SHORT System_GetKeyState( int virtualKeyCode )
{
#ifndef _X360
	return VCRHook_GetKeyState(virtualKeyCode);
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
	virtual const char *GetDesktopFolderPath();
	virtual void ShellExecuteEx( const char *command, const char *file, const char *pParams );
private:
	// auto-away data
	bool m_bStaticWatchForComputerUse;
	HHOOK m_hStaticKeyboardHook;
	HHOOK m_hStaticMouseHook;
	double m_StaticLastComputerUseTime;
	int m_iStaticMouseOldX, m_iStaticMouseOldY;

	// timer data
	double m_flFrameTime;
	KeyValues *m_pUserConfigData;
	char m_szFileName[MAX_PATH];
	char m_szPathID[MAX_PATH];

};


CSystem g_System;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSystem, ISystem, VGUI_SYSTEM_INTERFACE_VERSION, g_System);

namespace vgui
{
vgui::ISystem *g_pSystem = &g_System;
}
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSystem::CSystem()
{
	m_bStaticWatchForComputerUse = false;
	m_hStaticKeyboardHook = NULL;
	m_hStaticMouseHook = NULL;
	m_StaticLastComputerUseTime = 0.0;
	m_iStaticMouseOldX = m_iStaticMouseOldY = -1;
	m_flFrameTime = 0.0;
	m_pUserConfigData = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSystem::~CSystem()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSystem::Shutdown()
{
	if (m_pUserConfigData)
	{
		m_pUserConfigData->deleteThis();
		m_pUserConfigData = NULL;
	}
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
			m_StaticLastComputerUseTime = GetTimeMillis() / 1000.0;
			m_iStaticMouseOldX = x;
			m_iStaticMouseOldY = y;
		}
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
	return (long)(GetCurrentTime() * 1000);
}

void CSystem::ShellExecute( const char *command, const char *file )
{
#ifndef _X360
	::ShellExecuteA(NULL, command, file, NULL, NULL, SW_SHOWNORMAL);
#endif
}

void CSystem::ShellExecuteEx( const char *command, const char *file, const char *pParams )
{
#ifndef _X360
	::ShellExecuteA(NULL, command, file, pParams, NULL, SW_SHOWNORMAL);
#endif
}


void CSystem::SetClipboardImage( void *pWnd, int x1, int y1, int x2, int y2 )
{
#ifndef _X360
	if ( x2 <= x1 || y2 <= y1 )
		return;

	// Main app window
	HWND hWnd = ( HWND ) ( pWnd );
	if ( !hWnd )
		hWnd = GetMainApplicationWindowHWND();
	if ( !hWnd )
		return;

	// Prepare the blit
	HBITMAP hBmMem = NULL;
	{
		// Device contexts
		HDC hDc = GetDC( hWnd );
		HDC hDcMem = CreateCompatibleDC( hDc );
		hBmMem = CreateCompatibleBitmap( hDc, x2 - x1, y2 - y1 );
		HBITMAP hBmOld = ( HBITMAP ) SelectObject( hDcMem, hBmMem );
		BitBlt( hDcMem, 0, 0, x2 - x1, y2 - y1, hDc, x1, y1, SRCCOPY );
		SelectObject( hDcMem, hBmOld );
		DeleteDC( hDcMem );
		ReleaseDC( hWnd, hDc );
		// hBmMem now holds the image.
	}

	if ( !OpenClipboard( GetDesktopWindow() ) )
		return;

	EmptyClipboard();

	if (hBmMem)
	{
		SetClipboardData(CF_BITMAP, hBmMem);
	}

	CloseClipboard();
#endif
}

void CSystem::SetClipboardText(const char *text, int textLen)
{
#ifndef _X360
	if (!text)
		return;

	if (textLen <= 0)
		return;

	if (!OpenClipboard(GetDesktopWindow() ))
		return;

	EmptyClipboard();

	HANDLE hmem = GlobalAlloc(GMEM_MOVEABLE, textLen + 1);
	if (hmem)
	{
		void *ptr = GlobalLock(hmem);
		if (ptr != null)
		{
			memset(ptr, 0, textLen + 1);
			memcpy(ptr, text, textLen);
			GlobalUnlock(hmem);

			SetClipboardData(CF_TEXT, hmem);
		}
	}
	
	CloseClipboard();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Puts unicode text into the clipboard
//-----------------------------------------------------------------------------
void CSystem::SetClipboardText(const wchar_t *text, int textLen)
{
#ifndef _X360
	if (!text)
		return;

	if (textLen <= 0)
		return;

	BOOL cb = OpenClipboard(GetDesktopWindow() );
	if (!cb)
		return;

	EmptyClipboard();

	HANDLE hmem = GlobalAlloc(GMEM_MOVEABLE, (textLen + 1) * sizeof(wchar_t));
	if (hmem)
	{
		void *ptr = GlobalLock(hmem);
		if (ptr != null)
		{
			memset(ptr, 0, (textLen + 1) * sizeof(wchar_t));
			memcpy(ptr, text, textLen * sizeof(wchar_t));
			GlobalUnlock(hmem);

			SetClipboardData( CF_UNICODETEXT, hmem );
		}
	}
	
	CloseClipboard();
#endif
}

int CSystem::GetClipboardTextCount()
{
#ifndef _X360
	int count = 0;
	
	if ( VCRGetMode() != VCR_Playback )
	{
		if (OpenClipboard(GetDesktopWindow() ))
		{
			HANDLE hmem = GetClipboardData(CF_TEXT);
			if (hmem)
			{
				count = GlobalSize(hmem);
			}

			CloseClipboard();
		}
	}
	VCRGenericValue( "clipboard", &count, sizeof( count ) );

	return count;
#else
	return 0;
#endif
}

int CSystem::GetClipboardText(int offset, char *buf, int bufLen)
{
#ifndef _X360
	int count = 0;
	if ( buf && bufLen > 0 && VCRGetMode() != VCR_Playback )
	{
		if (OpenClipboard(GetDesktopWindow()))
		{
			HANDLE hmem = GetClipboardData(CF_UNICODETEXT);
			if (hmem)
			{
				int len = GlobalSize(hmem);
				count = len - offset;
				if (count <= 0)
				{
					count = 0;
				}
				else
				{
					if (bufLen < count)
					{
						count = bufLen;
					}
					void *ptr = GlobalLock(hmem);
					if (ptr)
					{
						memcpy(buf, ((char *)ptr) + offset, count);
						GlobalUnlock(hmem);
					}
				}
			}

			CloseClipboard();
		}
	}
	VCRGenericValue( "cb", &count, sizeof( count ) );
	VCRGenericValue( "cb", buf, count );

	return count;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves unicode text from the clipboard
//-----------------------------------------------------------------------------
int CSystem::GetClipboardText(int offset, wchar_t *buf, int bufLen)
{
#ifndef _X360
	int retVal = 0;
	if ( buf && bufLen > 0 && VCRGetMode() != VCR_Playback )
	{
		if (OpenClipboard( GetDesktopWindow() ) )
		{
			HANDLE hmem = GetClipboardData(CF_UNICODETEXT);
			if (hmem)
			{
				int len = GlobalSize(hmem);
				int count = len - offset;
				if (count > 0)
				{
					if (bufLen < count)
					{
						count = bufLen;
					}
					void *ptr = GlobalLock(hmem);
					if (ptr)
					{
						memcpy(buf, ((wchar_t *)ptr) + offset, count);
						retVal = count / sizeof(wchar_t);
						GlobalUnlock(hmem);
					}
				}
			}
		}

		CloseClipboard();
	}

	VCRGenericValue( "cb", &retVal, sizeof( retVal ) );
	VCRGenericValue( "cb", buf, retVal*sizeof(wchar_t) );

	return retVal;
#else
	return 0;
#endif
}

static bool staticSplitRegistryKey(const char *key, char *key0, int key0Len, char *key1, int key1Len)
{
	if(key==null)
	{
		return false;
	}
	
	int len=strlen(key);
	if(len<=0)
	{
		return false;
	}

	int Start=-1;
	for(int i=len-1;i>=0;i--)
	{
		if(key[i]=='\\')
		{
			break;
		}
		else
		{
			Start=i;
		}
	}

	if(Start==-1)
	{
		return false;
	}
	
	vgui_strcpy(key0,Start+1,key);
	vgui_strcpy(key1,(len-Start)+1,key+Start);

	return true;
}

bool CSystem::SetRegistryString(const char *key, const char *value)
{
#ifndef _X360
	HKEY hKey;

	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256], key1[256];
	if (!staticSplitRegistryKey(key, key0, sizeof(key0), key1, sizeof(key1)))
	{
		return false;
	}

	if(VCRHook_RegCreateKeyEx(hSlot,key0,null,null,REG_OPTION_NON_VOLATILE, value ? KEY_WRITE : KEY_ALL_ACCESS,null,&hKey,null)!=ERROR_SUCCESS)
	{
		return false;
	}

	if (VCRHook_RegSetValueEx(hKey, key1, NULL, REG_SZ, (uchar*)value, strlen(value) + 1) == ERROR_SUCCESS)
	{
		VCRHook_RegCloseKey(hKey);
		return true;
	}

	VCRHook_RegCloseKey(hKey);
#endif

	return false;
}

bool CSystem::GetRegistryString(const char *key, char *value, int valueLen)
{
#ifndef _X360
	if (!value)
		return false;
	value[0] = 0;

	HKEY hKey;

	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(VCRHook_RegOpenKeyEx(hSlot,key0,null,KEY_READ,&hKey)!=ERROR_SUCCESS)
	{
		return false;
	}

	ulong len=valueLen;
	if(VCRHook_RegQueryValueEx(hKey,key1,null,null,(uchar*)value,&len)==ERROR_SUCCESS)
	{		
		VCRHook_RegCloseKey(hKey);
		return true;
	}

	VCRHook_RegCloseKey(hKey);
#endif

	return false;
}

bool CSystem::SetRegistryInteger(const char *key, int value)
{
#ifndef _X360
	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(VCRHook_RegCreateKeyEx(hSlot,key0,null,null,REG_OPTION_NON_VOLATILE,KEY_WRITE,null,&hKey,null)!=ERROR_SUCCESS)
	{
		return false;
	}
		
	if(VCRHook_RegSetValueEx(hKey,key1,null,REG_DWORD,(uchar*)&value,4)==ERROR_SUCCESS)
	{
		VCRHook_RegCloseKey(hKey);
		return true;
	}

	VCRHook_RegCloseKey(hKey);
#endif
	return false;
}

bool CSystem::GetRegistryInteger(const char *key, int &value)
{
#ifndef _X360
	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(VCRHook_RegOpenKeyEx(hSlot,key0,null,KEY_READ,&hKey)!=ERROR_SUCCESS)
	{
		return false;
	}

	ulong len=4;
	if(VCRHook_RegQueryValueEx(hKey,key1,null,null,(uchar*)&value,&len)==ERROR_SUCCESS)
	{		
		VCRHook_RegCloseKey(hKey);
		return true;
	}

	VCRHook_RegCloseKey(hKey);
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: recursively deletes a registry key and all it's subkeys
//-----------------------------------------------------------------------------
bool CSystem::DeleteRegistryKey(const char *key)
{
#ifndef _X360
	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	if (SHDeleteKey(hSlot, key) == ERROR_SUCCESS)
	{
		return true;
	}
#endif
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
		return (GetTimeMillis() / 1000.0) - m_StaticLastComputerUseTime;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Get the drives a user has available on their system
//-----------------------------------------------------------------------------
int CSystem::GetAvailableDrives(char *buf, int bufLen)
{
#if defined( _X360 ) || defined ( OSX )
	return 0;
#else // Windows
	return GetLogicalDriveStrings( bufLen, buf );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the amount of available disk space, in bytes, on the specified path
//-----------------------------------------------------------------------------
double CSystem::GetFreeDiskSpace(const char *path)
{
	char buf[_MAX_PATH];
	strcpy(buf, path);
	// strip of to first slash (to make it look like 'x:\')
	char *slash = strstr(buf, "\\");
	if (slash)
	{
		slash[1] = 0;
	}

	ULARGE_INTEGER userFreeBytes, totalBytes, totalFreeBytes;
	if (::GetDiskFreeSpaceEx(buf, &userFreeBytes, &totalBytes, &totalFreeBytes))
	{
		return (double)userFreeBytes.QuadPart;
	}
	return 0.0;
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
	if (!m_pUserConfigData)
	{
		m_pUserConfigData = new KeyValues("UserConfigData");
	}

	strncpy(m_szFileName, fileName, sizeof(m_szFileName) - 1);
	strncpy(m_szPathID, pathName, sizeof(m_szPathID) - 1);

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
	const char *cmdLine = GetFullCommandLine();
	const char *loc = strstr(cmdLine, paramName);
	return (loc != NULL);
}

//-----------------------------------------------------------------------------
// Purpose: gets the string following a command line param
//-----------------------------------------------------------------------------
bool CSystem::GetCommandLineParamValue(const char *paramName, char *value, int valueBufferSize)
{
	const char *cmdLine = GetFullCommandLine();
	const char *loc = strstr(cmdLine, paramName);
	if (!loc)
		return false;

	loc += strlen(paramName);

	char token[512];
	ParseFile(loc, token, NULL);

	strncpy(value, token, valueBufferSize - 1);
	value[valueBufferSize - 1] = 0;
	
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

//-----------------------------------------------------------------------------
// Purpose: returns the current local time and date
//-----------------------------------------------------------------------------
bool CSystem::GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second)
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	if (year)
	{
		*year = time.wYear;
	}
	if (month)
	{
		*month = time.wMonth;
	}
	if (dayOfWeek)
	{
		*dayOfWeek = time.wDayOfWeek;
	}
	if (day)
	{
		*day = time.wDay;
	}
	if (hour)
	{
		*hour = time.wHour;
	}
	if (minute)
	{
		*minute = time.wMinute;
	}
	if (second)
	{
		*second = time.wSecond;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a shortcut file
//-----------------------------------------------------------------------------
bool CSystem::CreateShortcut(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory, const char *iconFile)
{
#ifndef _X360
	bool bSuccess = false;
	char temp[MAX_PATH];
	strcpy(temp, linkFileName);

	// make sure it doesn't already exist
	struct _stat statBuf;
	if (_stat(linkFileName, &statBuf) != -1)
		return false;

	// Create the ShellLink object
	IShellLink *psl;
	HRESULT hres = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*) &psl);
	if (SUCCEEDED(hres))
	{
		// Set the target information from the link object
		psl->SetPath(targetPath);
		psl->SetArguments(arguments);
		if (workingDirectory && *workingDirectory)
		{
			psl->SetWorkingDirectory(workingDirectory);
		}
		if (iconFile && *iconFile)
		{
			psl->SetIconLocation(iconFile, 0);
		}

		// Bind the ShellLink object to the Persistent File
		IPersistFile *ppf;
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID *) &ppf);
		if (SUCCEEDED(hres))
		{
			wchar_t wsz[MAX_PATH];
			// Get a UNICODE wide string wsz from the link path
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH);
			hres = ppf->Save(wsz, TRUE);
			if (SUCCEEDED(hres))
			{
				bSuccess = true;
			}
			ppf->Release();
		}
		psl->Release();
	}
	return bSuccess;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: retrieves shortcut (.lnk) information
//-----------------------------------------------------------------------------
bool CSystem::GetShortcutTarget(const char *linkFileName, char *targetPath, char *arguments, int destBufferSizes)
{
#ifndef _X360
	char temp[MAX_PATH];
	strcpy(temp, linkFileName);
	strlwr(temp);

	targetPath[0] = 0;
	arguments[0] = 0;

	// Create the ShellLink object
	IShellLink *psl;
	HRESULT hres = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*) &psl);
	if (SUCCEEDED(hres))
	{
		IPersistFile *ppf;
		// Bind the ShellLink object to the Persistent File
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID *) &ppf);
		if (SUCCEEDED(hres))
		{
			wchar_t wsz[MAX_PATH];
			// Get a UNICODE wide string wsz from the link path
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH);
			
			// Read the link into the persistent file
			hres = ppf->Load(wsz, 0);
			
			if (SUCCEEDED(hres))
			{
				//Read the target information from the link object
				//UNC paths are supported (SLGP_UNCPRIORITY)
				psl->GetPath(targetPath, destBufferSizes, NULL, SLGP_UNCPRIORITY);

				//Read the arguments from the link object
				psl->GetArguments(arguments, destBufferSizes);
			}
			ppf->Release();
		}
		psl->Release();
	}
	return (targetPath[0] != 0);
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: sets shortcut (.lnk) information
//-----------------------------------------------------------------------------
bool CSystem::ModifyShortcutTarget(const char *linkFileName, const char *targetPath, const char *arguments, const char *workingDirectory)
{
#ifndef _X360
	bool bSuccess = false;
	char temp[MAX_PATH];
	strcpy(temp, linkFileName);
	strlwr(temp);

	// Create the ShellLink object
	IShellLink *psl;
	HRESULT hres = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*) &psl);
	if (SUCCEEDED(hres))
	{
		IPersistFile *ppf;
		// Bind the ShellLink object to the Persistent File
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID *) &ppf);
		if (SUCCEEDED(hres))
		{
			wchar_t wsz[MAX_PATH];
			// Get a UNICODE wide string wsz from the link path
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH);
			
			// Read the link into the persistent file
			hres = ppf->Load(wsz, 0);
			
			if (SUCCEEDED(hres))
			{ 
				// Set the target information from the link object
				psl->SetPath(targetPath);
				psl->SetArguments(arguments);
				psl->SetWorkingDirectory(workingDirectory);
				bSuccess = true;
				ppf->Save(wsz, TRUE);
			}
			ppf->Release();
		}
		psl->Release();
	}
	return bSuccess;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the full path of the current user's desktop folder
//-----------------------------------------------------------------------------
const char *CSystem::GetDesktopFolderPath()
{
#ifndef _X360
	static char folderPath[MAX_PATH];
	folderPath[0] = 0;

	// try the custom regkey
	if ( GetRegistryString( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Desktop", folderPath, sizeof(folderPath) ) 
		&& strlen(folderPath) > 6 )
	{
		return folderPath;
	}


	// try the default 
	if ( ::SHGetSpecialFolderPath( NULL, folderPath, CSIDL_DESKTOP, false ) 
		&& strlen(folderPath) > 6 )
	{
		return folderPath;
	}
#endif
	return NULL;
}
