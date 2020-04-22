//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	VXCONSOLE.H
//
//	Master Header.
//=====================================================================================//
#pragma once

#include "tier0/platform.h"

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <share.h>
#include <richedit.h>
#include <assert.h>
#include <xbdm.h>
#include <time.h>
#include <sys/stat.h>

#include "resource.h"
#include "assert_resource.h"
#include "sys_utils.h"
#include "sys_scriptlib.h"
#include "bugreporter/bugreporter.h"
#include "jpeglib/jpeglib.h"
#include "UtlBuffer.h"
#include "strtools.h"
#include "xbox/xbox_vxconsole.h"
#include "UtlRBTree.h"
#include "UtlSymbol.h"
#include "UtlVector.h"
#include "UtlString.h"

#define VXCONSOLE_VERSION		"1.2"
#define VXCONSOLE_CLASSNAME		"VXConsole"
#define VXCONSOLE_TITLE			"VXConsole"
#define VXCONSOLE_FONT			"Courier"
#define VXCONSOLE_FONTSIZE		10
#define VXCONSOLE_MAGIC			"3\\"
#define VXCONSOLE_REGISTRY		"HKEY_CURRENT_USER\\Software\\VXConsole\\" VXCONSOLE_MAGIC
#ifdef _DEBUG
#define VXCONSOLE_BUILDTYPE		"Debug"
#else
#define VXCONSOLE_BUILDTYPE		"Release"
#endif

#define VXCONSOLE_WINDOWBYTES	( DLGWINDOWEXTRA + 4 )
#define VXCONSOLE_CONFIGID		( VXCONSOLE_WINDOWBYTES - 4 )	

#define MAX_QUEUEDSTRINGS		4096
#define MAX_QUEUEDSTRINGLEN		512
#define MAX_RCMDNAMELEN			32
#define MAX_RCMDS				4096
#define MAX_TOKENCHARS			256
#define MAX_XBOXNAMELEN			64
#define MAX_ARGVELEMS			20
#define MAX_COMMANDHISTORY		25

#define TIMERID_AUTOCONNECT		0x1000
#define TIMERID_MEMPROFILE		0x1001

#define IDC_COMMANDHINT			666

#define XBX_CLR_RED				( RGB( 255,0,0 ) )
#define XBX_CLR_GREEN			( RGB( 0,255,0 ) )
#define XBX_CLR_WHITE			( RGB( 255,255,255 ) )
#define XBX_CLR_BLACK			( RGB( 0,0,0 ) )
#define XBX_CLR_BLUE			( RGB( 0,0,255 ) )
#define XBX_CLR_YELLOW			( RGB( 255,255,0 ) )
#define XBX_CLR_LTGREY			( RGB( 180,180,180 ) )
#define XBX_CLR_DEFAULT			XBX_CLR_BLACK		

// The command prefix that is prepended to all communication between the Xbox
// app and the debug console app
#define VXCONSOLE_PRINT_PREFIX      "XPRT"
#define VXCONSOLE_COMMAND_PREFIX	"XCMD"
#define VXCONSOLE_COMMAND_ACK		"XACK"
#define VXCONSOLE_COLOR_PREFIX		"XCLR"

#define ICON_APPLICATION			0
#define ICON_DISCONNECTED			1
#define ICON_CONNECTED_XBOX			2
#define ICON_CONNECTED_APP0			3
#define ICON_CONNECTED_APP1			4
#define MAX_ICONS					5

typedef BOOL ( *cmdHandler_t )( int argc, char* argv[] );

#define IDM_BINDINGS				50000
#define IDM_BINDINGS_EDIT			( IDM_BINDINGS+1 )
#define IDM_BINDINGS_BIND1			( IDM_BINDINGS+2 )
#define IDM_BINDINGS_BIND2			( IDM_BINDINGS+3 )
#define IDM_BINDINGS_BIND3			( IDM_BINDINGS+4 )
#define IDM_BINDINGS_BIND4			( IDM_BINDINGS+5 )
#define IDM_BINDINGS_BIND5			( IDM_BINDINGS+6 )
#define IDM_BINDINGS_BIND6			( IDM_BINDINGS+7 )
#define IDM_BINDINGS_BIND7			( IDM_BINDINGS+8 )
#define IDM_BINDINGS_BIND8			( IDM_BINDINGS+9 )
#define IDM_BINDINGS_BIND9			( IDM_BINDINGS+10 )
#define IDM_BINDINGS_BIND10			( IDM_BINDINGS+11 )
#define IDM_BINDINGS_BIND11			( IDM_BINDINGS+12 )
#define IDM_BINDINGS_BIND12			( IDM_BINDINGS+13 )
#define MAX_BINDINGS				( VK_F12-VK_F1+1 )

// file serving
#define FSERVE_LOCALONLY	0
#define FSERVE_REMOTEONLY	1
#define FSERVE_LOCALFIRST	2

// file sync
#define FSYNC_OFF					0x00000000
#define FSYNC_ALWAYS				0x00000001
#define FSYNC_IFNEWER				0x00000002
#define FSYNC_TYPEMASK				0x0000000F
#define FSYNC_ANDEXISTSONTARGET		0x80000000

// track function invocations
typedef enum
{
	FL_INVALID,
	FL_STAT,
	FL_FOPEN,
	FL_FSEEK,
	FL_FTELL,
	FL_FREAD,
	FL_FWRITE,
	FL_FCLOSE,
	FL_FEOF,
	FL_FERROR,
	FL_FFLUSH,
	FL_FGETS,
	FL_MAXFUNCTIONCOUNTS,
} fileLogFunctions_e;

typedef enum
{
	VPROF_OFF = 0,
	VPROF_CPU,
	VPROF_TEXTURE,
	VPROF_TEXTUREFRAME,
} vprofState_e;

// funtion command types
#define FN_CONSOLE	0x00	// command runs at console
#define FN_XBOX		0x01	// command requires xbox
#define FN_APP		0x02	// command requires application

// shorthand
#define FA_NORMAL		FILE_ATTRIBUTE_NORMAL
#define FA_DIRECTORY	FILE_ATTRIBUTE_DIRECTORY
#define FA_READONLY		FILE_ATTRIBUTE_READONLY

typedef struct
{
    const CHAR*		strCommand;
	int				flags;
    cmdHandler_t	pfnHandler;
    const CHAR*		strHelp;
} localCommand_t;

typedef struct
{
	char*	strCommand;
	char*	strHelp;
} remoteCommand_t;

typedef struct
{
    CRITICAL_SECTION	CriticalSection;
    int					numMessages;
	bool				bInit;
    COLORREF			aColors[MAX_QUEUEDSTRINGS];
    CHAR				*pMessages[MAX_QUEUEDSTRINGS];
} printQueue_t;

class CProgress
{
public:
	CProgress();
	~CProgress();
	void				Open( const char* title, bool canCancel, bool bHasMeter );
	void				SetStatus( const char *line1, const char *line2, const char *line3 );
	void				SetMeter( int currentPos, int range );
	bool				IsCancel();
	HWND				m_hWnd;
	HWND				m_hWndStatus1;
	HWND				m_hWndStatus2;
	HWND				m_hWndStatus3;
	HWND				m_hWndPercent;
	HWND				m_hWndMeter;
	HWND				m_hWndCancel;
	bool				m_bCancelPressed;
	int					m_range;

private:
	void				Update();
};

typedef struct fileNode_s
{
	char				*filename;
    FILETIME			creationTime;
    FILETIME			changeTime;
    DWORD				sizeHigh;
    DWORD				sizeLow;
	DWORD				attributes;
	int					level;
	struct fileNode_s	*nextPtr;
	bool				needsUpdate;
}
fileNode_t;

//-----------------------------------------------------------------------------
// FILEIO.CPP
//-----------------------------------------------------------------------------
extern void RemoteToLocalFilename( const char* inFilename, char* outFilename, int outSize );
extern void RemoteToTargetFilename( const char* inFilename, char* outFilename, int outSize );
extern void FreeTargetFileList( fileNode_t* pFileList );
extern bool GetTargetFileList_r( char* targetPath, bool recurse, int attributes, int level, fileNode_t** pFileList );
extern char *SystemTimeToString( SYSTEMTIME *systemTime, char *buffer, int bufferSize );
extern bool CreateTargetPath( const char *pTargetFilename );

//-----------------------------------------------------------------------------
// BINDINGS.CPP
//-----------------------------------------------------------------------------
extern bool Bindings_TranslateKey( int vkKeyCode );
extern bool Bindings_MenuSelection( int wID );
extern void Bindings_LoadConfig();
extern void Bindings_SaveConfig();
extern void Bindings_Open();
extern bool Bindings_Init();

//-----------------------------------------------------------------------------
// COMMON.CPP
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// VXCONSOLE.CPP
//-----------------------------------------------------------------------------
extern void				SetConnectionIcon( int icon );
extern void				PrintToQueue( COLORREF color, const CHAR* strFormat, ... );
extern void				ProcessPrintQueue();
extern void				DmAPI_DisplayError( const char* strApiName, HRESULT hr );
extern int				ConsoleWindowPrintf( COLORREF rgb, LPCTSTR lpFmt, ... );
extern int				CmdToArgv( char* str, char* argv[], int maxargs );
extern char*			GetToken( char** tokenStreamPtr );
extern void				DebugCommand( const char* strFormat, ... );
extern bool				ProcessCommand( const char* strCmd );
extern HRESULT			DmAPI_SendCommand( const char* strCommand, bool wait );
extern void				NotImplementedYet();
extern void				SetMainWindowTitle();
extern HWND				g_hDlgMain;
extern HWND				g_hwndOutputWindow;
extern UINT_PTR			g_autoConnectTimer;
extern BOOL				g_autoConnect;
extern CHAR				g_xboxName[];
extern DWORD			g_xboxAddress;
extern CHAR				g_remotePath[];
extern CHAR				g_localPath[];
extern CHAR				g_targetPath[];
extern CHAR				g_installPath[];
extern CHAR				g_xboxTargetName[];
extern BOOL				g_connectedToApp;
extern BOOL				g_connectedToXBox;
extern PDMN_SESSION		g_pdmnSession;
extern PDM_CONNECTION	g_pdmConnection;
extern BOOL				g_debugCommands;
extern BOOL				g_captureDebugSpew;
extern HINSTANCE		g_hInstance;
extern HICON			g_hIcons[];
extern BOOL				g_reboot;
extern int				g_rebootArgc;
extern char*			g_rebootArgv[];
extern int				g_connectCount;
extern int				g_currentIcon;
extern BOOL				g_clsOnConnect;
extern DWORD			g_connectedTime;
extern HBRUSH			g_hBackgroundBrush;
extern COLORREF			g_backgroundColor;
extern HFONT			g_hFixedFont;
extern HFONT			g_hProportionalFont;
extern HANDLE			g_hCommandReadyEvent;
extern BOOL				g_loadSymbolsOnConnect;
extern HACCEL			g_hAccel;
extern BOOL				g_alwaysAutoConnect;
extern BOOL				g_startMinimized;
extern int				g_connectFailure;
extern BOOL				g_captureDebugSpew_StartupState;
extern bool             g_bSuppressBlink;
extern BOOL				g_bPlayTestMode;

//-----------------------------------------------------------------------------
// LOCAL_CMDS.CPP
//-----------------------------------------------------------------------------
extern BOOL				lc_bug( int argc, char* argv[] );
extern BOOL				lc_dir( int argc, char* argv[] );
extern BOOL				lc_del( int argc, char* argv[] );
extern BOOL				lc_memory( int argc, char* argv[] );
extern BOOL				lc_screenshot( int argc, char* argv[] );
extern BOOL				lc_help( int argc, char* argv[] );
extern BOOL				lc_cls( int argc, char* argv[] );
extern BOOL				lc_connect( int argc, char* argv[] );
extern BOOL				lc_autoConnect( int argc, char* argv[] );
extern BOOL				lc_disconnect( int argc, char* argv[] );
extern BOOL				lc_quit( int argc, char* argv[] );
extern BOOL				lc_crashdump( int argc, char* argv[] );
extern BOOL				lc_listen( int argc, char* argv[] );
extern BOOL				lc_run( int argc, char* argv[] );
extern BOOL				lc_reset( int argc, char* argv[] );
extern BOOL				lc_modules( int argc, char* argv[] );
extern BOOL				lc_sections( int argc, char* argv[] );
extern BOOL				lc_threads( int argc, char* argv[] );
extern BOOL				lc_ClearConfigs( int argc, char* argv[] );
extern void				AutoConnectTimerProc( HWND hwnd, UINT_PTR idEvent );
extern int				MatchLocalCommands( char* cmdStr, const char* cmdList[], int maxCmds );
extern void				DoDisconnect( BOOL bKeepConnection, int waitTime = 15 );
extern localCommand_t	g_localCommands[];
extern const int		g_numLocalCommands;

//-----------------------------------------------------------------------------
// REMOTE_CMDS.CPP
//-----------------------------------------------------------------------------
extern int				rc_AddCommands( char* commandPtr );
extern void				Remote_DeleteCommands();
extern DWORD __stdcall	Remote_NotifyDebugString( ULONG dwNotification, DWORD dwParam );
extern DWORD __stdcall	Remote_NotifyPrintFunc( const CHAR* strNotification );
extern DWORD __stdcall	Remote_NotifyCommandFunc( const CHAR* strNotification );
extern int				MatchRemoteCommands( char* cmdStr, const char* cmdList[], int maxCmds );
extern remoteCommand_t*	g_remoteCommands[];
extern int				g_numRemoteCommands;

//-----------------------------------------------------------------------------
// BUG.CPP
//-----------------------------------------------------------------------------
extern bool			BugDlg_Init( void );
extern void			BugDlg_Open( void );
extern void			BugReporter_FreeInterfaces();
extern int			rc_MapInfo( char* commandPtr );

//-----------------------------------------------------------------------------
// CONFIG.CPP
//-----------------------------------------------------------------------------
extern void ConfigDlg_Open( void );
extern void ConfigDlg_LoadConfig();

//-----------------------------------------------------------------------------
// FILELOG.CPP
//-----------------------------------------------------------------------------
extern void			FileLog_Open();
extern bool			FileLog_Init();
extern void			FileLog_Clear();
extern unsigned int FileLog_AddItem( const char* filename, unsigned int fp );
extern void			FileLog_UpdateItem( unsigned int log, unsigned int fp, fileLogFunctions_e functionId, int value );
extern void			FileLog_SaveConfig();
extern void			FileLog_LoadConfig();
extern bool			g_fileLogEnable;

//-----------------------------------------------------------------------------
// CPU_PROFILE.CPP
//-----------------------------------------------------------------------------
extern void		CpuProfileSamples_Open();
extern void		CpuProfileHistory_Open();
extern void		CpuProfile_SetTitle();
extern bool		CpuProfile_Init();
extern void		CpuProfile_Clear();
extern int		rc_SetCpuProfile( char* commandPtr );
extern int		rc_SetCpuProfileData( char* commandPtr );

//-----------------------------------------------------------------------------
// TEX_PROFILE.CPP
//-----------------------------------------------------------------------------
extern void		TexProfile_SetTitle();
extern void		TexProfileSamples_Open();
extern void		TexProfileHistory_Open();
extern bool		TexProfile_Init();
extern int		rc_SetTexProfile( char* commandPtr );
extern int		rc_SetTexProfileData( char* commandPtr );

//-----------------------------------------------------------------------------
// MEM_PROFILE.CPP
//-----------------------------------------------------------------------------
extern void		MemProfile_Open();
extern void		MemProfile_SetTitle();
extern bool		MemProfile_Init();
extern void		MemProfile_Clear();
extern int		rc_FreeMemory( char* commandPtr );

//-----------------------------------------------------------------------------
// MEMLOG.CPP
//-----------------------------------------------------------------------------
extern void		MemoryLog_Open();
extern bool		MemoryLog_Init();
extern void		MemoryLog_Clear();
extern void		MemoryLog_SaveConfig();
extern void		MemoryLog_LoadConfig();
extern void		MemoryLog_TreeView( bool enable );
extern void		MemoryLog_RefreshItems();
extern int		rc_MemoryLog( char* commandPtr );
extern bool		g_memoryLog_enable;

//-----------------------------------------------------------------------------
// SYNC_FILES.CPP
//-----------------------------------------------------------------------------
extern int			FileSyncEx( const char* localFilename, const char* remoteFilename, int fileSyncMode, bool bVerbose, bool bNoWrite );
extern bool			SyncFilesDlg_Init( void );
extern void			SyncFilesDlg_Open( void );
extern void			InstallDlg_Open( void );

//-----------------------------------------------------------------------------
// FILEIO.CPP
//-----------------------------------------------------------------------------
extern int			CompareFileTimes_NTFStoFATX( FILETIME* ntfsFileTime, char *ntfsTimeString, int ntfsStringSize, FILETIME* fatxFileTime, char *fatxTimeString, int fatxStringSize );
extern bool			LoadTargetFile( const char *pTargetPath, int *pFileSize, void **pData );

//-----------------------------------------------------------------------------
// PROGRESS.CPP
//-----------------------------------------------------------------------------
extern bool			Progress_Init();

//-----------------------------------------------------------------------------
// SHOW_TEXTURES.CPP
//-----------------------------------------------------------------------------
extern void			ShowTextures_Open();
extern bool			ShowTextures_Init();
extern int			rc_TextureList( char* commandPtr );

//-----------------------------------------------------------------------------
// TIMESTAMP_LOG.CPP
//-----------------------------------------------------------------------------
extern void			TimeStampLog_Open();
extern bool			TimeStampLog_Init();
extern void			TimeStampLog_Clear();
extern int			rc_TimeStampLog( char* commandPtr );

//-----------------------------------------------------------------------------
// COMMON.CPP
//-----------------------------------------------------------------------------
extern vprofState_e	VProf_GetState();
extern void			VProf_Enable( vprofState_e state );

//-----------------------------------------------------------------------------
// SHOW_MATERIALS.CPP
//-----------------------------------------------------------------------------
extern void			ShowMaterials_Open();
extern bool			ShowMaterials_Init();
extern int			rc_MaterialList( char* commandPtr );

//-----------------------------------------------------------------------------
// SHOW_SOUNDS.CPP
//-----------------------------------------------------------------------------
extern void			ShowSounds_Open();
extern bool			ShowSounds_Init();
extern int			rc_SoundList( char* commandPtr );

//-----------------------------------------------------------------------------
// SHOW_MEMDUMP.CPP
//-----------------------------------------------------------------------------
extern void			ShowMemDump_Open();
extern bool			ShowMemDump_Init();
extern int			rc_MemDump( char* commandPtr );

//-----------------------------------------------------------------------------
// EXCLUDE_PATHS.CPP
//-----------------------------------------------------------------------------
extern bool			ExcludePathsDlg_Init( void );
extern void			ExcludePathsDlg_Open( void );

//-----------------------------------------------------------------------------
// ASSERT_DIALOG.CPP
//-----------------------------------------------------------------------------
extern bool g_AssertDialogActive;
int rc_Assert( char *commandPtr );
