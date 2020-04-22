//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// CTextConsoleUnix.cpp: Unix implementation of the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32

#include <sys/ioctl.h>

#include "TextConsoleUnix.h"
#include "tier0/icommandline.h"
#include "tier1/utllinkedlist.h"
#include "filesystem.h"
#include "../thirdparty/libedit-3.1/src/histedit.h"
#include "tier0/vprof.h"

#define CONSOLE_LOG_FILE "console.log"

static pthread_mutex_t g_lock;
static pthread_t g_threadid = (pthread_t)-1;
CUtlLinkedList< CUtlString > g_Commands;
static volatile int g_ProcessingCommands = false;

#if defined( LINUX )

// Dynamically load the libtinfo stuff. On servers without a tty attached,
//  we get to skip all of these dependencies on servers without ttys.
#define TINFO_SYM(rc, fn, params, args, ret)	\
	typedef rc (*DYNTINFOFN_##fn) params;		\
	static DYNTINFOFN_##fn g_TINFO_##fn;		\
	extern "C" rc fn params						\
	{											\
		ret g_TINFO_##fn args;					\
	}

TINFO_SYM(char *, tgoto,    (char *string, int x, int y), (string, x, y), return);
TINFO_SYM(int,    tputs,    (const char *str, int affcnt, int (*putc)(int)), (str, affcnt, putc), return);
TINFO_SYM(int,    tgetflag, (char *id), (id), return);
TINFO_SYM(int,    tgetnum,  (char *id), (id), return);
TINFO_SYM(int,    tgetent,  (char *bufp, const char *name), (bufp, name), return);
TINFO_SYM(char *, tgetstr,  (char *id, char **area), (id, area), return);

#endif // LINUX

//----------------------------------------------------------------------------------------------------------------------
// init_tinfo_functions
//----------------------------------------------------------------------------------------------------------------------
static bool init_tinfo_functions()
{
#if !defined( LINUX )
    return true;
#else
	static void *s_ncurses_handle = NULL;

	if ( !s_ncurses_handle )
	{
		// Long time ago, ncurses was two libraries. So if libtinfo fails, try libncurses.
		static const char *names[] = { "libtinfo.so.5", "libncurses.so.5" };

		for ( int i = 0; !s_ncurses_handle && ( i < ARRAYSIZE( names ) ); i++ )
		{
			bool bFailed = true;
			s_ncurses_handle = dlopen( names[i], RTLD_NOW );

			if ( s_ncurses_handle )
			{
				bFailed = false;
#define LOADTINFOFUNC(_handle, _func, _failed) 									\
			do { 																\
				g_TINFO_##_func = ( DYNTINFOFN_##_func )dlsym(_handle, #_func); \
				if ( !g_TINFO_##_func) 											\
					_failed = true; 											\
			} while (0)

				LOADTINFOFUNC( s_ncurses_handle, tgoto, bFailed );
				LOADTINFOFUNC( s_ncurses_handle, tputs, bFailed );
				LOADTINFOFUNC( s_ncurses_handle, tgetflag, bFailed );
				LOADTINFOFUNC( s_ncurses_handle, tgetnum, bFailed );
				LOADTINFOFUNC( s_ncurses_handle, tgetent, bFailed );
				LOADTINFOFUNC( s_ncurses_handle, tgetstr, bFailed );
#undef LOADTINFOFUNC
			}

			if ( bFailed )
				s_ncurses_handle = NULL;
		}

		if ( !s_ncurses_handle )
		{
			fprintf( stderr, "\nWARNING: Failed to load 32-bit libtinfo.so.5 or libncurses.so.5.\n"
							 "  Please install (lib32tinfo5 / ncurses-libs.i686 / equivalent) to enable readline.\n\n");
		}
	}

	return !!s_ncurses_handle;
#endif // LINUX
}

static unsigned char editline_complete( EditLine *el, int ch __attribute__((__unused__)) )
{
	static const char *s_cmds[] =
	{
		"cvarlist ",
		"find ",
		"help ",
		"maps ",
		"nextlevel",
		"quit",
		"status",
		"sv_cheats ",
		"tf_bot_quota ",
		"toggle ",
		"sv_dump_edicts",
#ifdef STAGING_ONLY
		"tf_bot_use_items ",
#endif
	};

	const LineInfo *lf = el_line(el);
	const char *cmd = lf->buffer;
	size_t len = lf->cursor - cmd;

	if ( len > 0 )
	{
		for (int i = 0; i < ARRAYSIZE(s_cmds); i++)
		{
			if ( len > strlen( s_cmds[i] ) )
				continue;

			if ( !Q_strncmp( cmd, s_cmds[i], len ) )
			{ 
				if ( el_insertstr( el, s_cmds[i] + len ) == -1 )
					return CC_ERROR;
				else 
					return CC_REFRESH;
			}
		}
	}

	return CC_ERROR; 
}

static const char *editline_prompt( EditLine *e )
{
	// Something like: "\1\033[7m\1Srcds$\1\033[0m\1 "
	static const char *szPrompt = getenv( "SRCDS_PROMPT" );
	return szPrompt ? szPrompt : "";
}

static void editline_cleanup_handler( void *arg )
{
	if ( arg )
	{
		EditLine *el = (EditLine *)arg;
		el_end( el );
	}
}

static bool add_command( const char *cmd, int cmd_len )
{
	if ( cmd )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ ); 

		// Trim trailing whitespace.
		while ( ( cmd_len > 0 ) && isspace( cmd[ cmd_len - 1 ] ) )
			cmd_len--;

		if ( cmd_len > 0 )
		{
			pthread_mutex_lock( &g_lock );
			if ( g_Commands.Count() < 32 )
			{
				CUtlString szCommand( cmd, cmd_len );
				g_Commands.AddToTail( szCommand );

				g_ProcessingCommands = true;
			}
			pthread_mutex_unlock( &g_lock );

			// Wait a bit until we've processed the command we added.
			for ( int i = 0; i < 6; i++ )
			{
				while ( g_ProcessingCommands )
					usleep( 500 );
			}

			return true;
		}
	}

	return false;
}

static void *editline_threadproc( void *arg )
{
	HistEvent ev;
	EditLine *el;
	History *myhistory;
	FILE *tty = (FILE *)arg;

	ThreadSetDebugName( "libedit" );

	// Set up state
	el = el_init( "srcds_linux", stdin, tty, stderr );
	el_set( el, EL_PROMPT, &editline_prompt );
	el_set( el, EL_EDITOR, "emacs" ); // or "vi"

	// Hitting Ctrl+R will reset prompt.
	el_set( el, EL_BIND, "^R", "ed-redisplay", NULL );

	/* Add a user-defined function  */
	el_set( el, EL_ADDFN, "ed-complete", "Complete argument", editline_complete );
	/* Bind tab to it       */
	el_set( el, EL_BIND, "^I", "ed-complete", NULL );

	// Init history.
	myhistory = history_init();
	if (myhistory == 0)
	{
		fprintf( stderr, "history could not be initialized\n" );
		g_threadid = (pthread_t)-1;
		return (void *)-1;
	}

	// History size.
	history( myhistory, &ev, H_SETSIZE, 800 );

	// History callback.
	el_set( el, EL_HIST, history, myhistory );

	// Source user's defaults.
	el_source( el, NULL );

	pthread_cleanup_push( editline_cleanup_handler, el );

	while ( g_threadid != (pthread_t)-1 )
	{
		// count is the number of characters read.
		// line is a const char* of our command line with the tailing \n
		int count;
		const char *line = el_gets( el, &count );

		if ( add_command( line, count ) )
		{
			// Add command to history.
			history( myhistory, &ev, H_ENTER, line );
		}
	}

	pthread_cleanup_pop( 0 );

	// Clean up...
	history_end( myhistory );
	el_end( el );
	return NULL;
}

static void *fgets_threadproc( void *arg )
{
	pthread_cleanup_push( editline_cleanup_handler, NULL );

	while ( g_threadid != (pthread_t)-1 )
	{
		char cmd[ 512 ];

		if ( fgets( cmd, sizeof( cmd ), stdin ) )
		{
			cmd[ sizeof(cmd) - 1 ] = 0;
			add_command( cmd, strlen( cmd ) );
		}
	}

	pthread_cleanup_pop( 0 );
	return NULL;
}

bool CTextConsoleUnix::Init()
{
	if( g_threadid != (pthread_t)-1 )
	{
		Assert( !"CTextConsoleUnix can only handle a single thread!" );
		return false;
	}

	pthread_mutex_init( &g_lock, NULL );

	// This code is for echo-ing key presses to the connected tty
	//   (which is != STDOUT)
	if ( isatty( STDIN_FILENO ) )
	{
		const char *termid_str = ctermid( NULL );

		m_tty = fopen( termid_str, "w+" );
		if ( !m_tty )
		{
			fprintf( stderr, "WARNING: Unable to open tty(%s) for output.\n", termid_str );
			m_tty = stdout;
		}

		void *(*terminal_threadproc) (void *) = editline_threadproc;
		if ( !init_tinfo_functions() )
			terminal_threadproc = fgets_threadproc;

		if ( pthread_create( &g_threadid, NULL, terminal_threadproc, (void *)m_tty ) != 0 )
		{
			g_threadid = (pthread_t)-1;
			fprintf( stderr, "WARNING: pthread_create failed: %s.\n", strerror(errno) ); 
		}
	}
	else
	{
		m_tty = fopen( "/dev/null", "w+" );
		if ( !m_tty )
			m_tty = stdout;
	}

	m_bConDebug = CommandLine()->FindParm( "-condebug" ) != 0;
	if ( m_bConDebug && CommandLine()->FindParm( "-conclearlog" ) )
		g_pFullFileSystem->RemoveFile( CONSOLE_LOG_FILE, "GAME" );

	return CTextConsole::Init();
}

void CTextConsoleUnix::ShutDown()
{
	if ( g_threadid != (pthread_t)-1 )
	{
		void *status = NULL;
		pthread_t tid = g_threadid;

		g_threadid = (pthread_t)-1;

		pthread_cancel( tid );
		pthread_join( tid, &status );
	}

	pthread_mutex_destroy( &g_lock ); 
}

void CTextConsoleUnix::Print( char * pszMsg )
{
	int nChars = strlen( pszMsg );

	if ( nChars > 0 )
	{
		if ( m_bConDebug )
		{
			FileHandle_t fh = g_pFullFileSystem->Open( CONSOLE_LOG_FILE, "a" );
			if ( fh != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->Write( pszMsg, nChars, fh );
				g_pFullFileSystem->Close( fh );
			}
		}

		fwrite( pszMsg, 1, nChars, m_tty );
	}
}

void CTextConsoleUnix::SetTitle( char *pszTitle )
{
}

void CTextConsoleUnix::SetStatusLine( char *pszStatus )
{
}

void CTextConsoleUnix::UpdateStatus()
{
}

char *CTextConsoleUnix::GetLine( int index, char *buf, int buflen )
{
	if ( g_threadid != (pthread_t)-1 )
	{
		if ( g_Commands.Count() > 0 )
		{
			pthread_mutex_lock( &g_lock );

			const CUtlString& psCommand = g_Commands[ g_Commands.Head() ];
			V_strncpy( buf, psCommand.Get(), buflen );
			g_Commands.Remove( g_Commands.Head() );

			pthread_mutex_unlock( &g_lock );
			return buf;
		}
		else if ( index == 0 )
		{
			// We're being asked for the first command. Must be a new frame.
			//  Reset the processed commands global.
			g_ProcessingCommands = false;
		}
	}

	return NULL;
}

int CTextConsoleUnix::GetWidth()
{
	int nWidth = 0;
	struct winsize ws;

	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) == 0 )
		nWidth = (int)ws.ws_col;

	if ( nWidth <= 1 )
		nWidth = 80;

	return nWidth;
}

#endif // !_WIN32
