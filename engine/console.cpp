//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "client_pch.h"
#include <time.h>
#include "console.h"
#include "ivideomode.h"
#include "zone.h"
#include "sv_main.h"
#include "server.h"
#include "MapReslistGenerator.h"
#include "tier0/vcrmode.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if !defined( _X360 )
#define	MAXPRINTMSG	4096
#else
#define	MAXPRINTMSG	1024
#endif

bool con_debuglog = false;
bool con_initialized = false;
bool con_debuglogmapprefixed = false;

CThreadFastMutex g_AsyncNotifyTextMutex;

static ConVar con_timestamp( "con_timestamp", "0", 0, "Prefix console.log entries with timestamps" );

// In order to avoid excessive opening and closing of the console log file
// we wrap it in an object and keep the handle open. This is necessary
// because of the sometimes considerable cost of opening and closing files
// on Windows. Opening and closing files on Windows is always moderately
// expensive, but profiling may dramatically underestimate the true cost
// because some anti-virus software can make closing a file handle take
// 20-90 ms!
class ConsoleLogManager
{
public:
	ConsoleLogManager();
	~ConsoleLogManager();

	void RemoveConsoleLogFile();
	bool ReadConsoleLogFile( CUtlBuffer& buf );
	FileHandle_t GetConsoleLogFileHandleForAppend();
	void CloseFileIfOpen();

private:
	FileHandle_t m_fh;

	const char *GetConsoleLogFilename() const;
};

// Wrap the ConsoleLogManager in a function to ensure that the object is always
// constructed before it is used.
ConsoleLogManager& GetConsoleLogManager()
{
	static ConsoleLogManager object;
	return object;
}

void ConsoleLogFileCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef con_logfile( var->GetName() );
	const char *logFile = con_logfile.GetString();
	// close any existing file, because we have changed the name
	GetConsoleLogManager().CloseFileIfOpen();

	// validate the path and the .log/.txt extensions
	if ( !COM_IsValidPath( logFile ) || !COM_IsValidLogFilename( logFile ) )
	{
		ConMsg( "invalid log filename\n" );
		con_logfile.SetValue( "console.log" );
		return;
	}
	else
	{
		const char *extension = Q_GetFileExtension( logFile );
		if ( !extension || ( Q_strcasecmp( extension, "log" ) && Q_strcasecmp( extension, "txt" ) ) )
		{
			char szTemp[MAX_PATH];
			V_sprintf_safe( szTemp, "%s.log", logFile );
			con_logfile.SetValue( szTemp );
			return;
		}
	}
	
	if ( !COM_IsValidPath( logFile ) )
	{
		con_debuglog = CommandLine()->FindParm( "-condebug" ) != 0;
	}
	else
	{
		con_debuglog = true;
	}
}

ConVar con_logfile( "con_logfile", "", 0, "Console output gets written to this file", false, 0.0f, false, 0.0f, ConsoleLogFileCallback );

static const char *GetTimestampString( void )
{
	static char string[128];
	tm today;
	VCRHook_LocalTime( &today );
	Q_snprintf( string, sizeof( string ), "%02i/%02i/%04i - %02i:%02i:%02i",
		today.tm_mon+1, today.tm_mday, 1900 + today.tm_year,
		today.tm_hour, today.tm_min, today.tm_sec );
	return string;
}

#ifndef SWDS

static ConVar con_trace( "con_trace", "0", FCVAR_MATERIAL_SYSTEM_THREAD, "Print console text to low level printout." );
static ConVar con_notifytime( "con_notifytime","8", FCVAR_MATERIAL_SYSTEM_THREAD, "How long to display recent console text to the upper part of the game window" );
static ConVar con_times("contimes", "8", FCVAR_MATERIAL_SYSTEM_THREAD, "Number of console lines to overlay for debugging." );
static ConVar con_drawnotify( "con_drawnotify", "1", 0, "Disables drawing of notification area (for taking screenshots)." );
static ConVar con_enable("con_enable", "0", FCVAR_ARCHIVE, "Allows the console to be activated.");
static ConVar con_filter_enable ( "con_filter_enable","0", FCVAR_MATERIAL_SYSTEM_THREAD, "Filters console output based on the setting of con_filter_text. 1 filters completely, 2 displays filtered text brighter than other text." );
static ConVar con_filter_text ( "con_filter_text","", FCVAR_MATERIAL_SYSTEM_THREAD, "Text with which to filter console spew. Set con_filter_enable 1 or 2 to activate." );
static ConVar con_filter_text_out ( "con_filter_text_out","", FCVAR_MATERIAL_SYSTEM_THREAD, "Text with which to filter OUT of console spew. Set con_filter_enable 1 or 2 to activate." );



//-----------------------------------------------------------------------------
// Purpose: Implements the console using VGUI
//-----------------------------------------------------------------------------
class CConPanel : public CBasePanel
{
	typedef CBasePanel BaseClass;

public:
	enum
	{
		MAX_NOTIFY_TEXT_LINE = 256
	};

					CConPanel( vgui::Panel *parent );
	virtual			~CConPanel( void );

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	// Draws the text
	virtual void	Paint();
	// Draws the background image
	virtual void	PaintBackground();

	// Draw notify area
	virtual void	DrawNotify( void );
	// Draws debug ( Con_NXPrintf ) areas
	virtual void	DrawDebugAreas( void );
	
	int				ProcessNotifyLines( int &left, int &top, int &right, int &bottom, bool bDraw );

	// Draw helpers
	virtual int		DrawText( vgui::HFont font, int x, int y, wchar_t *data );

	virtual bool	ShouldDraw( void );

	void			Con_NPrintf( int idx, const char *msg );
	void			Con_NXPrintf( const struct con_nprint_s *info, const char *msg );

	void			AddToNotify( const Color& clr, char const *msg );
	void			ClearNotify();

private:
	// Console font
	vgui::HFont		m_hFont;
	vgui::HFont		m_hFontFixed;

	struct CNotifyText
	{
		Color	clr;
		float		liferemaining;
		wchar_t		text[MAX_NOTIFY_TEXT_LINE];
	};

	CCopyableUtlVector< CNotifyText >	m_NotifyText;

	enum
	{
		MAX_DBG_NOTIFY = 128,
		DBG_NOTIFY_TIMEOUT = 4,
	};

	float da_default_color[3];

	typedef struct
	{
		wchar_t	szNotify[MAX_NOTIFY_TEXT_LINE];
		float	expire;
		float	color[3];
		bool	fixed_width_font;
	} da_notify_t;

	da_notify_t da_notify[MAX_DBG_NOTIFY];
	bool m_bDrawDebugAreas;
};

static CConPanel *g_pConPanel = NULL;

/*
================
Con_HideConsole_f

================
*/
void Con_HideConsole_f( void )
{
	if ( IsX360() )
		return;

	if ( EngineVGui()->IsConsoleVisible() )
	{
		// hide the console
		EngineVGui()->HideConsole();
	}
}

/*
================
Con_ShowConsole_f
================
*/
void Con_ShowConsole_f( void )
{
	if ( IsX360() )
		return;

	if ( vgui::input()->GetAppModalSurface() )
	{
		// If a dialog has modal, it probably has grabbed keyboard focus, so showing
		// the console would be a bad idea.
		return;
	}

	if ( !g_ClientDLL->ShouldAllowConsole() )
		return;

	// make sure we're allowed to see the console
	if ( con_enable.GetBool() || developer.GetInt() || CommandLine()->CheckParm("-console") || CommandLine()->CheckParm("-rpt") )
	{
		// show the console
		EngineVGui()->ShowConsole();

		// remove any loading screen
		SCR_EndLoadingPlaque();
	}
}

//-----------------------------------------------------------------------------
// Purpose: toggles the console
//-----------------------------------------------------------------------------
void Con_ToggleConsole_f( void )
{
	if ( IsX360() )
		return;

	if (EngineVGui()->IsConsoleVisible())
	{
		Con_HideConsole_f();

		// If we hide the console, we also hide the game UI
		EngineVGui()->HideGameUI();
	}
	else
	{
		Con_ShowConsole_f();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clears the console
//-----------------------------------------------------------------------------
void Con_Clear_f( void )
{	
	if ( IsX360() )
		return;

	EngineVGui()->ClearConsole();
	Con_ClearNotify();
}
						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	if ( g_pConPanel )
	{
		g_pConPanel->ClearNotify();
	}
}

#endif // SWDS												


ConsoleLogManager::ConsoleLogManager()
{
	m_fh = FILESYSTEM_INVALID_HANDLE;
}

ConsoleLogManager::~ConsoleLogManager()
{
	// This fails because of destructor order problems. The file
	// system has already been shut down by the time this runs.
	// We'll have to count on the OS to close the file for us.
	//CloseFileIfOpen();
}

void ConsoleLogManager::RemoveConsoleLogFile()
{
	// Make sure the log file is closed before we try deleting it.
	CloseFileIfOpen();
	g_pFileSystem->RemoveFile( GetConsoleLogFilename(), "GAME" );
}

bool ConsoleLogManager::ReadConsoleLogFile( CUtlBuffer& buf )
{
	// Make sure the log file is closed before we try reading it.
	CloseFileIfOpen();
	const char *pLogFile = GetConsoleLogFilename();
	if ( g_pFullFileSystem->ReadFile( pLogFile, "GAME", buf ) )
		return true;

	return false;
}

FileHandle_t ConsoleLogManager::GetConsoleLogFileHandleForAppend()
{
	if ( m_fh == FILESYSTEM_INVALID_HANDLE )
	{
		const char* file = GetConsoleLogFilename();
		m_fh = g_pFileSystem->Open( file, "a" );
	}

	return m_fh;
}

void ConsoleLogManager::CloseFileIfOpen()
{
	if ( m_fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( m_fh );
		m_fh = FILESYSTEM_INVALID_HANDLE;
	}
}

const char *ConsoleLogManager::GetConsoleLogFilename() const
{
	const char *logFile = con_logfile.GetString();
	if ( !COM_IsValidPath( logFile ) || !COM_IsValidLogFilename( logFile ) )
	{
		return "console.log";
	}
	return logFile;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
#ifdef DEDICATED
	con_debuglog = false; // the dedicated server's console will handle this
	con_debuglogmapprefixed = false;

	// Check -consolelog arg and set con_logfile if it's present. This gets some messages logged
	//  that we would otherwise miss due to con_logfile being set in the .cfg file.
	const char *filename = NULL;
	if ( CommandLine()->CheckParm( "-consolelog", &filename ) && filename && filename[ 0 ] )
	{
		con_logfile.SetValue( filename );
	}
#else
	bool bRPTClient = ( CommandLine()->FindParm( "-rpt" ) != 0 );
	con_debuglog = bRPTClient || ( CommandLine()->FindParm( "-condebug" ) != 0 );
	con_debuglogmapprefixed = CommandLine()->FindParm( "-makereslists" ) != 0;
	if ( con_debuglog )
	{
		con_logfile.SetValue( "console.log" );
		if ( bRPTClient || ( CommandLine()->FindParm( "-conclearlog" ) ) )
		{
			GetConsoleLogManager().RemoveConsoleLogFile();
		}
	}
#endif // !DEDICATED

	con_initialized = true;
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown (void)
{
	con_initialized = false;
}

/*
================
Read the console log from disk and return it in 'buf'. Buf should come
in as an empty TEXT_BUFFER CUtlBuffer.
Returns true if the log file is successfully read.
================
*/
bool GetConsoleLogFileData( CUtlBuffer& buf )
{
	return GetConsoleLogManager().ReadConsoleLogFile( buf );
}

/*
================
Con_DebugLog
================
*/
void Con_DebugLog( const char *fmt, ...)
{
    va_list argptr; 
	char data[MAXPRINTMSG];
    
    va_start(argptr, fmt);
    Q_vsnprintf(data, sizeof(data), fmt, argptr);
    va_end(argptr);

	FileHandle_t fh = GetConsoleLogManager().GetConsoleLogFileHandleForAppend();
	if (fh != FILESYSTEM_INVALID_HANDLE )
	{
		if ( con_debuglogmapprefixed )
		{
			char const *prefix = MapReslistGenerator().LogPrefix();
			if ( prefix )
			{
				g_pFileSystem->Write( prefix, strlen(prefix), fh );
			}
		}

		if ( con_timestamp.GetBool() )
		{
			static bool needTimestamp = true; // Start the first line with a timestamp
			if ( needTimestamp )
			{
				const char *timestamp = GetTimestampString();
				g_pFileSystem->Write( timestamp, strlen( timestamp ), fh );
				g_pFileSystem->Write( ": ", 2, fh );
			}
			needTimestamp = V_stristr( data, "\n" ) != 0;   
		}

		g_pFileSystem->Write( data, strlen(data), fh );
		// Now that we don't close the file we need to flush it in order
		// to make sure that the data makes it to the file system.
		g_pFileSystem->Flush( fh );
	}
}

static bool g_fIsDebugPrint = false;

#ifndef SWDS
/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
static bool g_fColorPrintf = false;
static bool g_bInColorPrint = false;
extern CThreadLocalInt<> g_bInSpew;

void Con_Printf( const char *fmt, ... );

extern ConVar spew_consolelog_to_debugstring;

void Con_ColorPrint( const Color& clr, char const *msg )
{
	if ( IsPC() )
	{
		if ( g_bInColorPrint )
			return;

		int nCon_Filter_Enable = con_filter_enable.GetInt();
		if ( nCon_Filter_Enable > 0 )
		{
			const char *pszText = con_filter_text.GetString();
			const char *pszIgnoreText = con_filter_text_out.GetString();

			switch( nCon_Filter_Enable )
			{
			case 1:
				// if line does not contain keyword do not print the line
				if ( pszText && ( *pszText != '\0' ) && ( Q_stristr( msg, pszText ) == NULL ))
					return;
				if ( pszIgnoreText && *pszIgnoreText && ( Q_stristr( msg, pszIgnoreText ) != NULL ) )
					return;
				break;

			case 2:
				if ( pszIgnoreText && *pszIgnoreText && ( Q_stristr( msg, pszIgnoreText ) != NULL ) )
					return;
				// if line does not contain keyword print it in a darker color
				if ( pszText && ( *pszText != '\0' ) && ( Q_stristr( msg, pszText ) == NULL ))
				{
					Color mycolor(200, 200, 200, 150 );
					g_pCVar->ConsoleColorPrintf( mycolor, "%s", msg );
					return;
				}
				break;

			default:
				// by default do no filtering
				break;
			}
		}

		g_bInColorPrint = true;

		// also echo to debugging console
		if ( Plat_IsInDebugSession() && !con_trace.GetInt() && !spew_consolelog_to_debugstring.GetBool() )
		{
			Sys_OutputDebugString(msg);
		}
			
		if ( sv.IsDedicated() )
		{
			g_bInColorPrint = false;
			return;		// no graphics mode
		}

		bool convisible = Con_IsVisible();
		bool indeveloper = ( developer.GetInt() > 0 );
		bool debugprint = g_fIsDebugPrint;

		if ( g_fColorPrintf )
		{
			g_pCVar->ConsoleColorPrintf( clr, "%s", msg );
		}
		else
		{
			// write it out to the vgui console no matter what
			if ( g_fIsDebugPrint )
			{
				// Don't spew debug stuff to actual console once in game, unless console isn't up
				if ( !cl.IsActive() || !convisible )
				{
					g_pCVar->ConsoleDPrintf( "%s", msg );
				}
			}
			else
			{
				g_pCVar->ConsolePrintf( "%s", msg );
			}
		}

		// Make sure we "spew" if this wan't generated from the spew system
		if ( !g_bInSpew )
		{
			Msg( "%s", msg );
		}

		// Only write to notify if it's non-debug or we are running with developer set > 0
		// Buf it it's debug then make sure we don't have the console down
		if ( ( !debugprint || indeveloper ) && !( debugprint && convisible ) )
		{
			if ( g_pConPanel )
			{
				g_pConPanel->AddToNotify( clr, msg );
			}
		}
		g_bInColorPrint = false;
	}

#if defined( _X360 )
	int			r,g,b,a;
	char		buffer[MAXPRINTMSG];
	const char	*pFrom;
	char		*pTo;

	clr.GetColor(r, g, b, a);

	// fixup percent printers
	pFrom = msg;
	pTo   = buffer;
	while ( *pFrom && pTo < buffer+sizeof(buffer)-1 )
	{
		*pTo = *pFrom++;
		if ( *pTo++ == '%' )
			*pTo++ = '%';
	}
	*pTo = '\0';

	XBX_DebugString( XMAKECOLOR(r,g,b), buffer );
#endif
}
#endif

// returns false if the print function shouldn't continue
bool HandleRedirectAndDebugLog( const char *msg )
{
	// Add to redirected message
	if ( SV_RedirectActive() )
	{
		SV_RedirectAddText( msg );
		return false;
	}

	// log all messages to file
	if ( con_debuglog )
		Con_DebugLog( "%s", msg );

	if (!con_initialized)
	{
		return false;
	}
	return true;
}

void Con_Print( const char *msg )
{
	if ( !msg || !msg[0] )
		return;

	if ( !HandleRedirectAndDebugLog( msg ) )
	{
		return;
	}

#ifdef SWDS
	Msg( "%s", msg );
#else
	if ( sv.IsDedicated() )
	{
		Msg( "%s", msg );
	}
	else
	{
#if !defined( _X360 )
		Color clr( 255, 255, 255, 255 );
#else
		Color clr( 0, 0, 0, 255 );
#endif
		Con_ColorPrint( clr, msg );
	}
#endif
}

void Con_Printf( const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static bool	inupdate;
	
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

#ifndef NO_VCR
	// Normally, we shouldn't need to write this data to the file, but it can help catch
	// out-of-sync errors earlier.
	if ( vcr_verbose.GetInt() )
	{
		VCRGenericString( "Con_Printf", msg );
	}
#endif

	if ( !HandleRedirectAndDebugLog( msg ) )
	{
		return;
	}

#ifdef SWDS
	Msg( "%s", msg );
#else
	if ( sv.IsDedicated() )
	{
		Msg( "%s", msg );
	}
	else
	{
#if !defined( _X360 )
		Color clr( 255, 255, 255, 255 );
#else
		Color clr( 0, 0, 0, 255 );
#endif
		Con_ColorPrint( clr, msg );
	}
#endif
}

#ifndef SWDS
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clr - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void Con_ColorPrintf( const Color& clr, const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg,sizeof( msg ), fmt,argptr);
	va_end (argptr);

	AUTO_LOCK( g_AsyncNotifyTextMutex );
	if ( !HandleRedirectAndDebugLog( msg ) )
	{
		return;
	}

	g_fColorPrintf = true;
	Con_ColorPrint( clr, msg );
	g_fColorPrintf = false;
}
#endif

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf(msg,sizeof( msg ), fmt,argptr);
	va_end (argptr);
	
	g_fIsDebugPrint = true;

#ifdef SWDS
	DevMsg( "%s", msg );
#else
	if ( sv.IsDedicated() )
	{
		DevMsg( "%s", msg );
	}
	else
	{
		Color clr( 196, 181, 80, 255 );
		Con_ColorPrint ( clr, msg );
	}
#endif

	g_fIsDebugPrint = false;
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	va_start (argptr,fmt);
	Q_vsnprintf(msg,sizeof( msg ), fmt,argptr);
	va_end (argptr);

#ifndef SWDS
	bool		temp;
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
#endif
	g_fIsDebugPrint = true;
	Con_Printf ("%s", msg);
	g_fIsDebugPrint = false;
#ifndef SWDS
	scr_disabled_for_loading = temp;
#endif
}

#ifndef SWDS
bool Con_IsVisible()
{
	return (EngineVGui()->IsConsoleVisible());	
}

void Con_NPrintf( int idx, const char *fmt, ... )
{
	va_list argptr; 
	char outtext[MAXPRINTMSG];

	va_start(argptr, fmt);
    Q_vsnprintf( outtext, sizeof( outtext ), fmt, argptr);
    va_end(argptr);

	if ( IsPC() )
	{
		g_pConPanel->Con_NPrintf( idx, outtext );
	}
	else
	{
		Con_Printf( outtext );
	}
}

void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
{
	va_list argptr; 
	char outtext[MAXPRINTMSG];

	va_start(argptr, fmt);
    Q_vsnprintf( outtext, sizeof( outtext ), fmt, argptr);
    va_end(argptr);

	if ( IsPC() )
	{
		g_pConPanel->Con_NXPrintf( info, outtext );
	}
	else
	{
		Con_Printf( outtext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates the console panel
// Input  : *parent - 
//-----------------------------------------------------------------------------
CConPanel::CConPanel( vgui::Panel *parent ) : CBasePanel( parent, "CConPanel" )
{
	// Full screen assumed
	SetSize( videomode->GetModeStereoWidth(), videomode->GetModeStereoHeight() );
	SetPos( 0, 0 );
	SetVisible( true );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	da_default_color[0] = 1.0;
	da_default_color[1] = 1.0;
	da_default_color[2] = 1.0;

	m_bDrawDebugAreas = false;

	g_pConPanel = this;
	memset( da_notify, 0, sizeof(da_notify) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CConPanel::~CConPanel( void )
{
}

void CConPanel::Con_NPrintf( int idx, const char *msg )
{
	if ( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

#ifdef WIN32
    _snwprintf( da_notify[idx].szNotify, sizeof( da_notify[idx].szNotify ) / sizeof( wchar_t ) - 1, L"%S", msg );
#else
    _snwprintf( da_notify[idx].szNotify, sizeof( da_notify[idx].szNotify ) / sizeof( wchar_t ) - 1, L"%s", msg );
#endif
	da_notify[idx].szNotify[ sizeof( da_notify[idx].szNotify ) / sizeof( wchar_t ) - 1 ] = L'\0';

	// Reset values
	da_notify[idx].expire = realtime + DBG_NOTIFY_TIMEOUT;
	VectorCopy( da_default_color, da_notify[idx].color );
	da_notify[idx].fixed_width_font = false;
	m_bDrawDebugAreas = true;
}

void CConPanel::Con_NXPrintf( const struct con_nprint_s *info, const char *msg )
{
	if ( !info )
		return;

	if ( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

#ifdef WIN32
	_snwprintf( da_notify[info->index].szNotify, sizeof( da_notify[info->index].szNotify ) / sizeof( wchar_t ) - 1, L"%S", msg );
#else
	_snwprintf( da_notify[info->index].szNotify, sizeof( da_notify[info->index].szNotify ) / sizeof( wchar_t ) - 1, L"%s", msg );
#endif
	da_notify[info->index].szNotify[ sizeof( da_notify[info->index].szNotify ) / sizeof( wchar_t ) - 1 ] = L'\0';

	// Reset values
	if ( info->time_to_live == -1 )
		da_notify[ info->index ].expire = -1; // special marker means to just draw it once
	else
		da_notify[ info->index ].expire = realtime + info->time_to_live;
	VectorCopy( info->color, da_notify[ info->index ].color );
	da_notify[ info->index ].fixed_width_font = info->fixed_width_font;
	m_bDrawDebugAreas = true;
}

static void safestrncat( wchar_t *text, int maxCharactersWithNullTerminator, wchar_t const *add, int addchars )
{
	int maxCharactersWithoutTerminator = maxCharactersWithNullTerminator - 1;

	int curlen = wcslen( text );
	if ( curlen >= maxCharactersWithoutTerminator )
		return;

	wchar_t *p = text + curlen;
	while ( curlen++ < maxCharactersWithoutTerminator && 
		--addchars >= 0 )
	{
		*p++ = *add++;
	}
	*p = 0;
}

void CConPanel::AddToNotify( const Color& clr, char const *msg )
{
	if ( !host_initialized )
		return;

	// notify area only ever draws in developer mode - it should never be used for game messages
	if ( !developer.GetBool() )
		return;

	// skip any special characters
	if ( msg[0] == 1 || 
		 msg[0] == 2 )
	{
		msg++;
	}

	// Nothing left
	if ( !msg[0] )
		return;

	// Protect against background modifications to m_NotifyText.
	AUTO_LOCK( g_AsyncNotifyTextMutex );

	CNotifyText *current = NULL;

	int slot = m_NotifyText.Count() - 1;
	if ( slot < 0 )
	{
		slot = m_NotifyText.AddToTail();
		current = &m_NotifyText[ slot ];
		current->clr = clr;
		current->text[ 0 ] = 0;
		current->liferemaining = con_notifytime.GetFloat();;
	}
	else
	{
		current = &m_NotifyText[ slot ];
		current->clr = clr;
	}

	Assert( current );

	wchar_t unicode[ 1024 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( msg, unicode, sizeof( unicode ) );

	wchar_t const *p = unicode;
	while ( *p )
	{
		const wchar_t *nextreturn = wcsstr( p, L"\n" );
		if ( nextreturn != NULL )
		{
			int copysize = nextreturn - p + 1;
			safestrncat( current->text, MAX_NOTIFY_TEXT_LINE, p, copysize );

			// Add a new notify, but don't add a new one if the previous one was empty...
			if ( current->text[0] && current->text[0] != L'\n' )
			{
				slot = m_NotifyText.AddToTail();
				current = &m_NotifyText[ slot ];
			}
			// Clear it
			current->clr = clr;
			current->text[ 0 ] = 0;
			current->liferemaining = con_notifytime.GetFloat();
			// Skip return character
			p += copysize;
			continue;
		}

		// Append it
		safestrncat( current->text, MAX_NOTIFY_TEXT_LINE, p, wcslen( p ) );
		current->clr = clr;
		current->liferemaining = con_notifytime.GetFloat();
		break;
	}

	while ( m_NotifyText.Count() > 0 &&
		( m_NotifyText.Count() >= con_times.GetInt() ) )
	{
		m_NotifyText.Remove( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CConPanel::ClearNotify()
{
	// Protect against background modifications to m_NotifyText.
	AUTO_LOCK( g_AsyncNotifyTextMutex );

	m_NotifyText.RemoveAll();
}

void CConPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Console font
	m_hFont = pScheme->GetFont( "DefaultSmallDropShadow", false );
	m_hFontFixed = pScheme->GetFont( "DefaultFixedDropShadow", false );
}

int CConPanel::DrawText( vgui::HFont font, int x, int y, wchar_t *data )
{
	int len = DrawColoredText( font,
	                           x,
	                           y,
	                           255,
	                           255,
	                           255,
	                           255,
	                           data );

	return len;
}


//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
bool CConPanel::ShouldDraw()
{
	bool bVisible = false;

	if ( m_bDrawDebugAreas )
	{
		bVisible = true;
	}

	// Should be invisible if there's no notifys and the console is up.
	// and if the launcher isn't active
	if ( !Con_IsVisible() )
	{
		// Protect against background modifications to m_NotifyText.
		AUTO_LOCK( g_AsyncNotifyTextMutex );

		int i;
		int c = m_NotifyText.Count();
		for ( i = c - 1; i >= 0; i-- )
		{
			CNotifyText *notify = &m_NotifyText[ i ];

			notify->liferemaining -= host_frametime;

			if ( notify->liferemaining <= 0.0f )
			{
				m_NotifyText.Remove( i );
				continue;
			}
			
			bVisible = true;
		}
	}
	else
	{
		bVisible = true;
	}

	return bVisible;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CConPanel::DrawNotify( void )
{
	int x = 8;
	int y = 5;

	if ( !m_hFontFixed )
		return;

	// notify area only draws in developer mode
	if ( !developer.GetBool() )
		return;

	// don't render notify area into movies, either
	if ( cl_movieinfo.IsRecording( ) )
	{
		return;
	}

	vgui::surface()->DrawSetTextFont( m_hFontFixed );

	int fontTall = vgui::surface()->GetFontTall( m_hFontFixed ) + 1;

	// Protect against background modifications to m_NotifyText.
	// DEADLOCK WARNING: Cannot call DrawColoredText while holding g_AsyncNotifyTextMutex or
	// deadlock can occur while MatQueue0 holds material system lock and attempts to add text
	// to m_NotifyText.
	CUtlVector< CNotifyText > textToDraw;
	{
		AUTO_LOCK( g_AsyncNotifyTextMutex );
		textToDraw = m_NotifyText;
	}

	int c = textToDraw.Count();
	for ( int i = 0; i < c; i++ )
	{
		CNotifyText *notify = &textToDraw[ i ];

		float timeleft = notify->liferemaining;
	
		Color clr = notify->clr;

		if ( timeleft < .5f )
		{
			float f = clamp( timeleft, 0.0f, .5f ) / .5f;

			clr[3] = (int)( f * 255.0f );

			if ( i == 0 && f < 0.2f )
			{
				y -= fontTall * ( 1.0f - f / 0.2f );
			}
		}
		else
		{
			clr[3] = 255;
		}

		DrawColoredText( m_hFontFixed, x, y, clr[0], clr[1], clr[2], clr[3], notify->text );

		y += fontTall;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar con_nprint_bgalpha( "con_nprint_bgalpha", "50", 0, "Con_NPrint background alpha." );
ConVar con_nprint_bgborder( "con_nprint_bgborder", "5", 0, "Con_NPrint border size." );

void CConPanel::DrawDebugAreas( void )
{
	if ( !m_bDrawDebugAreas )
		return;

	// Find the top and bottom of all the nprint text so we can draw a box behind it.
	int left=99999, top=99999, right=-99999, bottom=-99999;
	if ( con_nprint_bgalpha.GetInt() )
	{
		// First, figure out the bounds of all the con_nprint text.
		if ( ProcessNotifyLines( left, top, right, bottom, false ) )
		{
			int b = con_nprint_bgborder.GetInt();

			// Now draw a box behind it.
			vgui::surface()->DrawSetColor( 0, 0, 0, con_nprint_bgalpha.GetInt() );
			vgui::surface()->DrawFilledRect( left-b, top-b, right+b, bottom+b );
		}
	}
	
	// Now draw the text.
	if ( ProcessNotifyLines( left, top, right, bottom, true ) == 0 )
	{
		// Have all notifies expired?
		m_bDrawDebugAreas = false;
	}
}

int CConPanel::ProcessNotifyLines( int &left, int &top, int &right, int &bottom, bool bDraw )
{
	int count = 0;
	int y = 20;

	for ( int i = 0; i < MAX_DBG_NOTIFY; i++ )
	{
		if ( realtime < da_notify[i].expire || da_notify[i].expire == -1 )
		{
			// If it's marked this way, only draw it once.
			if ( da_notify[i].expire == -1 && bDraw )
			{
				da_notify[i].expire = realtime - 1;
			}
			
			int len;
			int x;

			vgui::HFont font = da_notify[i].fixed_width_font ? m_hFontFixed : m_hFont ;

			int fontTall = vgui::surface()->GetFontTall( m_hFontFixed ) + 1;

			len = DrawTextLen( font, da_notify[i].szNotify );
			x = videomode->GetModeStereoWidth() - 10 - len;

			if ( y + fontTall > videomode->GetModeStereoHeight() - 20 )
				return count;

			count++;
			y = 20 + 10 * i;

			if ( bDraw )
			{
				DrawColoredText( font, x, y, 
					da_notify[i].color[0] * 255, 
					da_notify[i].color[1] * 255, 
					da_notify[i].color[2] * 255,
					255,
					da_notify[i].szNotify );
			}

			if ( da_notify[i].szNotify[0] )
			{
				// Extend the bounds.
				left = min( left, x );
				top = min( top, y );
				right = max( right, x+len );
				bottom = max( bottom, y+fontTall );
			}

			y += fontTall;
		}
	}

	return count;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CConPanel::Paint()
{
	VPROF( "CConPanel::Paint" );

#if !defined( SWDS ) && !defined( DEDICATED )
	if ( IsPC() && !g_ClientDLL->ShouldDrawDropdownConsole() )
		return;
#endif
	
	DrawDebugAreas();

	DrawNotify();	// only draw notify in game
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CConPanel::PaintBackground()
{
	if ( !Con_IsVisible() )
		return;

	int wide = GetWide();
	char ver[ 100 ];
	Q_snprintf(ver, sizeof( ver ), "Source Engine %i (build %d)", PROTOCOL_VERSION, build_number() );
	wchar_t unicode[ 200 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( ver, unicode, sizeof( unicode ) );

	vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
	int x = wide - DrawTextLen( m_hFont, unicode ) - 2;
	DrawText( m_hFont, x, 0, unicode );

	if ( cl.IsActive() )
	{
		if ( cl.m_NetChannel->IsLoopback() )
		{
			Q_snprintf(ver, sizeof( ver ), "Map '%s'", cl.m_szLevelBaseName );
		}
		else
		{
			Q_snprintf(ver, sizeof( ver ), "Server '%s' Map '%s'", cl.m_NetChannel->GetRemoteAddress().ToString(), cl.m_szLevelBaseName );
		}
		wchar_t wUnicode[ 200 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( ver, wUnicode, sizeof( wUnicode ) );

		int tall = vgui::surface()->GetFontTall( m_hFont );

		x = wide - DrawTextLen( m_hFont, wUnicode ) - 2;
		DrawText( m_hFont, x, tall + 1, wUnicode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates the Console VGUI object
//-----------------------------------------------------------------------------
static CConPanel *conPanel = NULL;

void Con_CreateConsolePanel( vgui::Panel *parent )
{
	conPanel = new CConPanel( parent );
	if (conPanel)
	{
		conPanel->SetVisible(false);
	}
}

vgui::Panel* Con_GetConsolePanel()
{
	return conPanel;
}

static ConCommand toggleconsole("toggleconsole", Con_ToggleConsole_f, "Show/hide the console.", FCVAR_DONTRECORD );
static ConCommand hideconsole("hideconsole", Con_HideConsole_f, "Hide the console.", FCVAR_DONTRECORD );
static ConCommand showconsole("showconsole", Con_ShowConsole_f, "Show the console.", FCVAR_DONTRECORD );
static ConCommand clear("clear", Con_Clear_f, "Clear all console output.", FCVAR_DONTRECORD );

#endif // SWDS
