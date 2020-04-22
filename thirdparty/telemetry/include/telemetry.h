#ifndef TELEMETRY_H
#define TELEMETRY_H

//-------------------------------------------------------------------
// Documentation stuff
//-------------------------------------------------------------------
#ifndef EXPAPI
#define EXPAPI
#endif
#ifndef EXPMACRO
#define EXPMACRO
#endif
#ifndef EXPCONST
#define EXPCONST
#endif
#ifndef EXPOUT
#define EXPOUT
#endif
#ifndef EXPGROUP
#define EXPGROUP(GroupName)
#endif
#ifndef EXPTYPEBEGIN
#define EXPTYPEBEGIN
#endif
#ifndef EXPTYPEEND
#define EXPTYPEEND
#endif
#ifndef EXPTYPE
#define EXPTYPE
#endif

EXPGROUP(_NullGroup)

#define TelemetryRunTimeVersion   "2.0P"   EXPMACRO
#define LTelemetryRunTimeVersion L"2.0P"   EXPMACRO
#define Telemetry2Version   "2.0P"   EXPMACRO
#define LTelemetry2Version L"2.0P"   EXPMACRO

#define TelemetryMajorVersion    2  EXPMACRO    
#define TelemetryMinorVersion    0  EXPMACRO    
#define TelemetryBuildNumber    56  EXPMACRO    
#define TelemetryCustomization   0  EXPMACRO    

#ifndef TelemetryToolsVersion
#define TelemetryToolsVersion  TelemetryRunTimeVersion
#define LTelemetryToolsVersion LTelemetryRunTimeVersion
#endif

#define TelemetrySalesEMailAddress "sales3@radgametools.com" EXPMACRO 
#define TelemetrySalesPhoneNumber "425-893-4300" EXPMACRO             
#define TelemetryCompanyName "RAD Game Tools, Inc." EXPMACRO
#define TelemetryCopyright "Copyright (C) 2009-2014, RAD Game Tools, Inc."  EXPMACRO 
#define TelemetryTrademarks "Telemetry is a registered trademark of RAD Game Tools" EXPMACRO
#define TelemetrySupportAddress "telemetry@radgametools.com" EXPMACRO 
#define TelemetrySupportPage "www.radgametools.com/telemetry.htm" EXPMACRO
#define TelemetryCompanyPage "http://www.radgametools.com" EXPMACRO
#define TelemetryOnLineChangelogAddress "www.radgametools.com/telemetry/changelog.htm" EXPMACRO

#ifndef __RADRES__

EXPGROUP(TMAPI)

//-------------------------------------------------------------------
// Important manifest constants:
//
// --- The following should _not_ be set by the end user! ---
//
// TM_INLINE_FASTTIME - DO NOT CHANGE THIS!!
// TM_DLL - is set to 1 if building the library as a DLL
// TM_API_STATIC - functions are statically linked directly and not going through a function pointer table
// TM_BUILD_LIB - should be set to 1 when building the library
// TM_HARDLINK  - we're statically linking (not using a DLL)
//
// TM_IPC_CLIENT - set to 1 if building the library as an IPC client
//    * assumes communication is NOT over a network
//    * assumes single threading
// TM_IPC_HOST - set to 1 if building the library as an IPC server
//    * assumes subprocesses use IPC for communication
//    * IPC mechanism not assumed
//
// --- The following can be set by the enduser ---
//
// NTELEMETRY - set to 1 if you want to compile away the library
//
//-------------------------------------------------------------------
#define TM_INLINE_FASTTIME 1

#define TM_MAX_OBJECT_PATH 64 EXPMACRO // Maximum length of a path string
#define TM2_MAX_OBJECT_PATH (1024*8) EXPMACRO // Maximum length of a path string
#define TM_MAX_STRING   (1024*8) EXPMACRO // Maximum length of a string that you can pass to Telemetry.
#define TM_MAX_EXTRA_DATA 256 EXPMACRO          // Maximum amount of variable length string data
#define TM_MAX_EVENT_FILENAME 320 EXPMACRO // Longest length for a filename used with a Telemetry event

#define TM_VA_LIST ((char const *)1) EXPMACRO // Specifies that a va_list is being passed to the function.  See $ug_valist for more information.

#define TM_LOCK_MIN_TIME_BUFSIZE 512 EXPMACRO // Size of buffer necessary for tmSetLockStateMinTime

#ifndef TM_CAT
#define TM_CAT( s1, s2 ) s1##s2
#endif 

#ifndef TM_FUNCTION_TYPE
#define TM_FUNCTION_TYPE(name) TM_CAT(name,type)
#endif

//-----------------------------------------------------------------------------
// Base types
//----------------------------------------------------------------------------- 
#if defined __RADARM__ || defined __RADWIIU__
#define TM_SLOW 1 EXPMACRO // If defined then reading timestamps is slow on this platforms, so adjust your markup accordingly
#endif

#ifdef TM_DLL
#ifndef __RADINDLL__
#define __RADINDLL__
#endif
#endif

// NOTE: Need to include this before the __RADNT__ check
//#ifndef NTELEMETRY
#include "tmtypes.h"
//#endif

#if !defined __RADQNX__ && !defined __RADSPU__ && !defined __RADNT__ && !defined __RADXENON__ && !defined __RADANDROID__ && !defined __RADSEKRIT__ && !defined __RADPS4__ && !defined __RADPS3__ && !defined __RADIPHONE__ && !defined __RADWIIU__ && !defined __RADLINUX__ && !(defined __RADMAC__ && !defined __RADPPC__) && !defined __RADPSP2__ && !defined NTELEMETRY
#define NTELEMETRY 1
#endif

#if defined __RADSPU__ && !defined TM_IPC_CLIENT
#define TM_IPC_CLIENT 1
#endif

#if defined __RADPS3__ && !defined TM_IPC_HOST
#define TM_IPC_HOST 1
#endif

#if defined __RADSPU__ || defined __RADPS3__
#define TM_SPU_CONTEXT_SIZE     (2*1024) EXPMACRO // Required size for a Telemetry context on the SPU
#endif

#ifdef TM_IPC_CLIENT
#define TM_SINGLE_THREADED 1
#endif

#if defined __RADXENON__ || defined __RADSEKRIT__ || defined __RADPS4__ || defined __RADIPHONE__ || defined __RADANDROID__ || defined __RADPS3__ || defined __RADWIIU__ || defined __RADPSP2__ || defined __RADQNX__
#define TM_HARDLINK 1
#endif

#if !defined RADEXPLINK && !defined NTELEMETRY
#define RADEXPLINK
#endif

#ifndef RADDEFSTART
#define RADDEFEND }
#define RADDEFSTART extern "C" {
#endif

#if defined __RADSPU__
#define TM_API_STATIC 1
#endif

#ifndef TM_API
#ifdef TM_API_STATIC
#define TM_API( ret, name, params ) ret RADEXPLINK name params
#else
#define TM_API( ret, name, params ) typedef ret (RADEXPLINK *TM_FUNCTION_TYPE(name))params
#endif
#endif

#ifndef TMCHECKCONTEXT
#ifdef TM_API_STATIC
#define TMCHECKCONTEXTR(c,n,f,z) ( (c) ? ( n f ) : (z) )
#define TMCHECKCONTEXTV(c,n,f) { if (c) { n f ; } }
#else
#define TMCHECKCONTEXTR(c,n,f,z) ( (c) ? ( ((TM_API_STRUCT*)(c))->n f) : (z) )
#define TMCHECKCONTEXTV(c,n,f) { if (c) { ( ((TM_API_STRUCT*)(c))->n f); } }
#endif

#define TMCHECKCONTEXT_VA(c,n,f) { static TmFormatCode tm_fmt; TMCHECKCONTEXTV(c,n,f); }
#define TMCHECKCONTEXT(c,n,f)  TMCHECKCONTEXTR(c,n,f,TMERR_INVALID_CONTEXT)
#endif

//-----------------------------------------------------------------------------
// Call stack
//-----------------------------------------------------------------------------
#define TM_MAX_CALLSTACK_DEPTH 32 //This value cannot change without affecting the server protocol!!
EXPTYPE typedef struct _TmCallStack
{
    int    cs_depth;
    void  *cs_stack[ TM_MAX_CALLSTACK_DEPTH ];
} TmCallStack;

//-----------------------------------------------------------------------------
// Error codes
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmErrorCode
{ 
    TM_OK                   = 0x0000, // Everything is okay
    TMERR_DISABLED          = 0x0001, // Telemetry has been compiled away with $NTELEMETRY 
    TMERR_INVALID_CONTEXT   = 0x0002, // The context passed to Telemetry was invalid
    TMERR_INVALID_PARAM     = 0x0003, // Out of range, null pointer, etc.
    TMERR_OUT_OF_RESOURCES  = 0x0004, // Typically out of available memory, string space, etc.
    TMERR_UNINITIALIZED     = 0x0006, // A Telemetry API was called before $tmStartup
    TMERR_BAD_HOSTNAME      = 0x0007, // Could not resolve hostname
    TMERR_COULD_NOT_CONNECT = 0x0008, // Could not connect to the server
    TMERR_UNKNOWN_NETWORK   = 0x0009, // Unknown error in the networking system 
    TMERR_ALREADY_SHUTDOWN  = 0x000A, // $tmShutdown called more than once 
    TMERR_ARENA_TOO_SMALL   = 0x000B, // buffer passed to $tmInitializeContext was too small
    TMERR_BAD_HANDSHAKE     = 0x000C, // handshake with server failed (protocol error)
    TMERR_UNALIGNED         = 0x000D, // One more more parameters were not aligned correctly
    TMERR_NETWORK_NOT_INITIALIZED = 0x000E, // Network startup functions were not called, e.g. WSAStartup
    TMERR_BAD_VERSION             = 0x000F, // You're using an out of date version of the Telemetry libraries
    TMERR_BAD_TIMER               = 0x00A0, // The provided user timer is too coarse for Telemetry
  TMERR_UNKNOWN           = 0xFFFF  // Unknown error occurred
} TmErrorCode;
/*
Standard errors returned by Telemetry.

Standard errors returned by Telemetry.  Most Telemetry functions return void, so the
standard Telemetry method for checking errors is to call $tmGetLastError.  In some cases, e.g.
when a context is unavailable or unnecessary, a Telemetry API may return an error code directly.
$-
See also $ug_errors
*/

//-----------------------------------------------------------------------------
// Printf-style tokens, reserved for Telemetry internal use only
//-----------------------------------------------------------------------------
typedef enum TmPrintfToken
{
    TMPRINTF_TOKEN_NONE = 0,
    TMPRINTF_TOKEN_CALLSTACK = 1
} TmPrintfToken;

//-----------------------------------------------------------------------------
// Connection status
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmConnectionStatus
{
    TMCS_DISCONNECTED = 0, //Not connected to a server
    TMCS_CONNECTING   = 1, //Attempting a connection to a server
    TMCS_CONNECTED    = 2  //Connected to a server
} TmConnectionStatus;
/*
Connection status returned by $tmGetConnectionStatus.
*/

#ifndef NTELEMETRY
//-----------------------------------------------------------------------------
// Zone flags
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmZoneFlag
{
    TMZF_NONE                      = 0x0000,      // Zone is normal.
    TMZF_STALL                     = 0x0001,      // Zone is stalling while waiting on a lock, etc.  Implies thread is busy but can't progress.
    TMZF_IDLE                      = 0x0002,      // Zone is sleeping/idle until something new happens.  Implies thread is idle. 
    TMZF_PROFILER_ONLY             = 0x0004,      // Telemetry 1 only: Only show this zone in the profiler view, not in the zone view

    TMZF_MEM_REGION_EXPERIMENTAL   = 0x0010,      // Only show this zone in the profiler view, not in the zone view

    TMZF_PLOT_TIME                 = 0x0100,      // Generate plot of the zone's duration on the server
    TMZF_PLOT_TIME_EXPERIMENTAL = TMZF_PLOT_TIME, // Retained for backward compatibility

    // Everything from here on out is for Telemetry internal use only, do not use!!!!!
    TMZF_INTERNAL_ACCUMULATOR     = 0x10000, //For Telemetry internal use only
    TMZF_INTERNAL_LOCK_STALL      = 0x20000, //For Telemetry internal use only
    TMZF_INTERNAL_OPEN_ENDED      = 0x40000, //For Telemetry internal use only
    TMZF_INTERNAL_CONTINUATION    = 0x80000, //For Telemetry internal use only

    TMZF_INTERNAL_DISCARD          = 0x100000, //For Telemetry internal use only; this constant is used by the run-time and server for different things
    TMZF_INTERNAL_DISCARD_SERVER   = 0x100000, //For Telemetry internal use only; this constant is shared but used differently by run time and server
    TMZF_INTERNAL_NO_INSERT_SERVER = 0x200000, //For Telemetry internal use only (server only)
    TMZF_INTERNAL_LOD_ZONE         = 0x400000, //For Telemetry internal use only (server only)

    // Not really flags...
    TMZF_INTERNAL_LOCK_FAILED    = 0x10000000, //For Telemetry internal use only
    TMZF_INTERNAL_LOCK_SUCCESS   = 0x20000000, //For Telemetry internal use only
    TMZF_INTERNAL_LOCK_TIMEOUT   = 0x30000000, //For Telemetry internal use only

} TmZoneFlag;
/*
Bitmasks that can be ORed together when calling tmZone, tmCoreEnter, tmCoreLeave, tmEnter or tmLeave.
*/
#define TMZF_INTERNAL_ANY_MASK ( TMZF_INTERNAL_ACCUMULATOR | TMZF_INTERNAL_LOCK_STALL | TMZF_INTERNAL_OPEN_ENDED | TMZF_INTERNAL_CONTINUATION | TMZF_INTERNAL_LOCK_FAILED | TMZF_INTERNAL_LOCK_TIMEOUT )

//-----------------------------------------------------------------------------
// Timespan flags
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmTimeSpanFlag
{
    TMTSF_NONE  = 0x0000,               // Timespan is normal.
} TmTimeSpanFlags;
/*
Bitmasks that can be ORed together when calling tmBeginTimeSpan and tmEndTimeSpan
*/

//-----------------------------------------------------------------------------
// Plot type
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmPlotType
{
    TMPT_NONE   = 0,               // Display as raw float data
    TMPT_MEMORY = 1,               // Display as contextually relevant memory, i.e. bytes, kb, MB, GB, TB, etc.
    TMPT_HEX    = 2,               // Display as hex value based on range
    TMPT_INTEGER = 3,              // Display as integer, no decimal points
    TMPT_UNTYPED = 3,              // Data is untyped and should be displayed as an integer (this is the same as TMPT_INTEGER)
    TMPT_PERCENTAGE_COMPUTED = 4,  // Display as a percentage of the max, i.e. as (value-min)/(max-min), computed by the client
    TMPT_PERCENTAGE_DIRECT = 5,    // Display as a percentage (i.e. 0.2187 => 21.87%).  For I32/U32 it expects 0..100
    TMPT_TIME = 6,                 // Display in timecode format, i.e. hh:mm:dd:ss.xxx 
    TMPT_TIME_MS = 7,              // Floating point milliseconds
    TMPT_TIME_US = 8,              // Floating point microseconds
    TMPT_TIME_CLOCKS = 9,          // I64 clock cycles
    TMPT_TIME_INTERVAL = 10,       // This plot's value should be the delta between this time and the last call to tmPlot for this plot
    TMPT_USER = 128,               // Reserved for user types
    TMPT_MAX = 255
} TmPlotType;
/*
Passed to $(tmPlot) to specify how the Visualizer should present the data.
*/

//-----------------------------------------------------------------------------
// Plot flags
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmPlotFlag
{
    TMPF_NONE           = 0x0000,       // No additional flags
    TMPF_MINVAL_ZERO    = 0x0001,       // Set plot's minval to 0 instead of actual minval
    TMPF_FRAMETIME      = 0x0002,       // This plot is being used to measure frame times

    TMPF_INTERNAL_SYNTHETIC = 0x10000   // Internal use only!
} TmPlotFlag;
/*
Passed to $(tmPlot) to specify how the Visualizer should present the data.  These are available in Telemetry 2 only!
*/

#endif // NTELEMETRY


EXPTYPE typedef struct TmContext*        HTELEMETRY;
/*
Opaque handle to a Telemetry context.

Telemetry binds the majority of its state to a context.  This context is allocated and
passed in by the application, and must be available and valid through the life of the
Telemetry session.  The context is initialized with $tmInitializeContext and shut down
with $tmShutdownContext.

An application can have multiple contexts, but each context will open a separate
connection to the server and create its own individual session.  This should rarely
be required, but in some instances is unavoidable, for example you are writing a 
middleware component and need to be able to instrument your code without necessarily
having access to the code base in which you're embedded (which, in turn, may have its
own Telemetry context).
*/

RADDEFSTART

//-----------------------------------------------------------------------------
// Message flags
//-----------------------------------------------------------------------------
#ifndef NTELEMETRY
EXPTYPE typedef enum TmMessageFlag
{
    TMMF_SEVERITY_LOG         = 0x0001,         // Regular log message
    TMMF_SEVERITY_WARNING     = 0x0002,         // Warning
    TMMF_SEVERITY_ERROR       = 0x0004,         // Error
    TMMF_SEVERITY_RESERVED    = 0x0008,         // For future expansion
    TMMF_SEVERITY_MASK        =    0xF,         // Low 3-bits determine the message type

    TMMF_ZONE_LABEL           = 0x0010,         // Replace zone label with this text
    TMMF_ZONE_SUBLABEL        = 0x0020,         // Print secondary zone label with text
    TMMF_ZONE_SHOW_IN_PARENTS = 0x0040,         // Show this not only in the zone's tooltip, but also in the tooltips of any parents
    TMMF_ZONE_RESERVED01      = 0x0080,
    TMMF_ZONE_MASK            = 0x00F0,

    // We have up to 15 icon types
    TMMF_ICON_EXCLAMATION     =  0x1000,         // Show exclamation marker in the zone view for this message
    TMMF_ICON_NOTE            =  0x2000,         // Show note marker in the zone view for this message
    TMMF_ICON_WTF             =  0x3000,         // Show "question mark/WTF?" marker in the zone view for this message
    TMMF_ICON_QUESTION_MARK   = TMMF_ICON_WTF,
    TMMF_ICON_EXTRA00         =  0x4000,         // These are placeholders for now until we get some nicer icons
    TMMF_ICON_EXTRA01         =  0x5000,         // 
    TMMF_ICON_EXTRA02         =  0x6000,         // 
    TMMF_ICON_EXTRA03         =  0x7000,         // 
    TMMF_ICON_EXTRA04         =  0x8000,         // 
    TMMF_ICON_MASK            =  0xF000,

    // Special message tags
    TMMF_UNUSED00     = 0x10000,
    TMMF_UNUSED01     = 0x20000,         // Used by tmSetTimelineSectionName

    // Internal use only
    // We can expand these later and make them either incremental like icons or do
    // an expansion mask and overflow into another variable...
    TMMF_INTERNAL_PLOT     = 0x10000000,
    TMMF_INTERNAL_MEMORY   = 0x20000000,
    TMMF_INTERNAL_TIMESPAN = 0x30000000,
    TMMF_INTERNAL_LOCK     = 0x40000000,

    TMMF_INTERNAL_ANY_MASK = 0x70000000
} TmMessageFlag;
/*
Passed to $tmMessage to determine how to present the message to the end user.
*/

//-----------------------------------------------------------------------------
// Open flags
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmOpenFlag
{
    TMOF_INIT_NETWORKING           = 0x0001,      // Initialize operating system networking layer.  Specify this if you do not already call the platform specific network startup functions (e.g. WSAStartup) prior to $tmOpen.
    TMOF_DONT_KILL_OTHER_SESSIONS  = 0x0002,      // If set the server should NOT kill any other sessions from this machine with the same app name.

    // This is an enumeration
    TMOF_DISABLE_CONTEXT_SWITCHES  = 0x0000,      // Disable context switch event tracking entirely.  None are sent to server.  See $ug_cswitches_overview for more information.
    TMOF_MODERATE_CONTEXT_SWITCHES = 0x0010,      // Send relevant context switches only (e.g. on PS4 it will not send context switches for the two system cores).  See $ug_cswitches_overview for more information.
    TMOF_MINIMAL_CONTEXT_SWITCHES  = 0x0020,      // Currently the same as "moderate" context switches.
    TMOF_MAXIMUM_CONTEXT_SWITCHES  = 0x0030,      // Send all context switches. See $ug_cswitches_overview for more information.
    TMOF_CONTEXT_SWITCH_MASK       = 0x0030,

    TMOF_NO_PDB_LOOKUP             = 0x0040,      // If specified then don't search all loaded DLLs for PDB signatures
} TmOpenFlag;
/*
Passed to $tmOpen.
*/

#ifdef __RADXENON__
#define TMOF_DEFAULT TMOF_MODERATE_CONTEXT_SWITCHES  // Default open flags
#else
#define TMOF_DEFAULT 0  // Default open flags
#endif

/*
Flags passed to $tmOpen
*/

#define TELEMETRY_DEFAULT_PORT 0 EXPMACRO  //Constant used to specify the default Telemetry port when calling $tmOpen
//-----------------------------------------------------------------------------
// Connection type
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmConnectionType
{
    TMCT_TCP,           // Connect to Telemetry server over TCP/IP
    TMCT_IPC,           // Use an IPC connection (not supported on all platforms)
    TMCT_FILE,          // Store all data to local disk (not supported on all platforms)

    TMCT_USER_PROVIDED, // Use the user defined callbacks for all file i/o
} TmConnectionType;
/* 
Determines how the application will connect to the Telemetry server.

Specified in $tmOpen.  In the future we may support additional communication methods such
as interprocess communication, USB connections, shared files, etc.
$-
For more information on recording-to-disk (instead of recording over a network) see $localfiles.
$-
*/

//-----------------------------------------------------------------------------
// Telemetry options
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmOption
{
    TMO_OUTPUT_DEBUG_INFO       = 0x0001,  // Print debug output to the debugger window if possible
    TMO_RECORD_TELEMETRY_STALLS = 0x0002,  // Record stalls within Telemetry itself

    TMO_RECORD_CALLSTACKS       = 0x0004,  // Record callstacks if requested

    TMO_BOOST_PRIORITY          = 0x0008,  // Temporarily boost priority for Telemetry events to help with priority inversion issues

    TMO_SUPPORT_ZONES       = 0x0100,      // Send zones
    TMO_SUPPORT_MESSAGES    = 0x0200,      // Send messages

    TMO_SUPPORT_LOCK_STATES = 0x0400,      // Send lock states

    TMO_SUPPORT_MEMORY      = 0x0800,      // Send memory events
    TMO_SUPPORT_PLOT        = 0x1000,      // Send plots
    TMO_SUPPORT_BLOB         = 0x2000,     // Send user blobs

    TMO_PPU_SYNCHRONIZE_SPUS     = 0x4000,  // Lockstep PPU-to-SPU communication
    TMO_SUPPORT_CONTEXT_SWITCHES = 0x8000,  // Support context switch recording on supported platforms

    TMO_NULL_NETWORK        = 0x10000,       // Disable network traffic

    TMO_SUPPORT_TIMESPANS   = 0x20000,       // Disable timespans

} TmOption;
/*
Option values that can be toggled during recording using $tmEnable.
*/

//-----------------------------------------------------------------------------
// Telemetry variables
//-----------------------------------------------------------------------------
EXPAPI typedef void TmDebugPrinter(char const *msg);
/*
User defined debug print function.
*/

typedef int (*TMFNC_USERCB_OPEN)(void* user, char const *fname );
typedef int (*TMFNC_USERCB_CLOSE)(void* user);
typedef int (*TMFNC_USERCB_WRITE)(void* user,void const* data,int datasize);

EXPTYPE typedef enum TmParameter
{
    TMP_NONE              = 0x0000,

    TMP_LOCK_MIN_TIME    = 1,         // pass in a pointer to TmI32, >= 0 in microseconds  
    TMP_DEBUG_PRINTER    = 2,         // pass in a pointer to a $TmDebugPrinter
    TMP_TELEMETRY_PREFERRED_CPU = 3,  // pass in a pointer to a TmI32 with the preferred CPU for Telemetry's background thread

    TMP_USERCB_OPEN = 4,
    TMP_USERCB_CLOSE = 5,
    TMP_USERCB_WRITE = 6,
    TMP_USERCB_DATA = 7,

    TMP_EMULATED_SERVER_VERSION = 8,

    TMP_SOCKET_BUFFER_SIZE = 9,

} TmParameter;
/*
Parameter values that can be set using $tmSetParameter
*/

EXPTYPE typedef enum TmServerConfiguration
{
    TMSC_MEM_PLOT_THRESHOLD    = 0,  // Delta memory usage that triggers a plot for memory usage on the server.  Value passed to $tmSetServerConfiguration should be a 64-bit integer.
} TmServerConfiguration;
/*
Parameter values that can be set using $tmSetServerConfiguration
*/

EXPTYPE typedef enum TmPlatformInformation
{
    TMPI_TELEMETRY_THREAD          = 0,  // Thread handle of the telemetry processing background thread.  Windows/360/XB1/PS3/PS4 only.  pIn should be your Telemetry context.  pOut should be a pointer to a HANDLE/sys_ppu_thread_t/ScePthread
    TMPI_CONTEXT_SWITCH_THREAD     = 1,  // Thread handle of the context switch processing background thread.  Windows Vista/7/360 only.  pIn should be your Telemetry context.  pOut sould be a pointer to a HANDLE.
    TMPI_START_TIME                = 2,  // Initial start time (in cycles) that Telemetry is basing its relative times from.  pIn should be your Telemetry context.  pOut should be a pointer to a U64.
} TmPlatformInformation;

/*
Platform information values that can be passed to $tmGetPlatformInformation
*/

//-----------------------------------------------------------------------------
// Lock state
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmLockState
{
    TMLS_RELEASED            = 0x00000001,        // Lock was released
    TMLS_LOCKED              = 0x00000002,        // Lock was acquired
    TMLS_DESTROYED           = 0x00000004,        // Lock was destroyed

    TMLS_INTERNAL_MASK         = 0xF0000000,
    TMLS_INTERNAL_TIMESPAN     = 0x10000000,     // This is a timespan, not a lock
    TMLS_INTERNAL_LOD_LOCK     = 0x20000000,     // Internally generated by Telemetry, DO NOT USE.
    TMLS_INTERNAL_FAKE         = 0x40000000,     // Internally generated by Telemetry, DO NOT USE.
    TMLS_INTERNAL_COUNT        = 0x80000000,     // Internally generated by Telemetry, DO NOT USE.
} TmLockState;
/*
Lock state value passed to $tmSetLockState

See also $ug_mutexes
*/

//-----------------------------------------------------------------------------
// Lock attempt result
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmLockResult
{
    TMLR_SUCCESS  = 0,        // Lock attempt succeeded
    TMLR_FAILED   = 1,        // Lock attempt failed
    TMLR_TIMEOUT  = 2,        // Lock attempt timed out
} TmLockResult;
/*
Passed to $tmEndTryLock to specify the result of the lock action.

See also $ug_mutexes
*/

//-----------------------------------------------------------------------------
// Telemetry statistics
//-----------------------------------------------------------------------------
EXPTYPE typedef enum TmStat
{
    TMSTAT_NUM_ALLOCS_DEPRECATED,     // Number of allocs this tick
    TMSTAT_NUM_FREES_DEPRECATED,      // Number of frees this tick

    TMSTAT_NUM_TICKS_DEPRECATED,      // Number of ticks so far
} TmStat;
/*
Passed to $tmGetStati
*/
#endif

#ifndef NTELEMETRY

#if defined __RADPS3__
#define TM_NUM_SPUS 6
TM_API( TmErrorCode, tmPPUCoreGetListener, ( HTELEMETRY cx, int const kNdx, TmU32 *pListener ) );
TM_API( TmErrorCode, tmPPUCoreRegisterSPUProgram, ( HTELEMETRY cx, TmU64 const kGuid, void const *imagebase, unsigned int const kImageSize, int const kRdOnlyOffset ) );
#endif

#if defined __RADSPU__
TM_API( TmErrorCode, tmSPUCoreBindContextToListener, ( HTELEMETRY *pcx, void * mem, TmU32 kPPUListener, char const *imagename, ...) );
TM_API( TmErrorCode, tmSPUCoreUpdateTime, ( HTELEMETRY cx ) );
TM_API( TmErrorCode, tmSPUCoreFlushImage, ( HTELEMETRY cx ) );
#endif
 
TM_API( TmU32, tmCoreGetVersion, ( void ) );
TM_API( TmErrorCode, tmCoreCheckVersion, ( HTELEMETRY cx, TmU32 const major, TmU32 const minor, TmU32 const build, TmU32 const cust ) );
TM_API( TmErrorCode, tmCoreGetPlatformInformation, ( void* obj, TmPlatformInformation const kInfo, void* dst, TmU32 const kDstSize ) );
TM_API( TmErrorCode, tmCoreGetLastError, ( HTELEMETRY cx ) );
TM_API( TmErrorCode, tmCoreGetSessionName, ( HTELEMETRY cx, char *dst, int const kDstSize ) );
TM_API( TmConnectionStatus, tmCoreGetConnectionStatus, ( HTELEMETRY cx ) );

#ifndef __RADSPU__

RADDEFFUNC typedef TmU64 TMUSERTIMERFNC( void ); // User defined timing function

TM_API( TmErrorCode, tmCoreStartup, ( void ) );
TM_API( TmErrorCode, tmCoreStartupEx, ( TMUSERTIMERFNC *fnc ) );
TM_API( TmErrorCode, tmCoreInitializeContext, ( EXPOUT HTELEMETRY * pcx, void * pArena, TmU32 const kArenaSize ) );
TM_API( void, tmCoreShutdownContext, ( HTELEMETRY cx ) );
TM_API( void, tmCoreShutdown, ( void ) ); 
TM_API( TmErrorCode, tmCoreOpen, ( HTELEMETRY cx, char const * kpAppName,  
       char const * kpBuildInfo,
       char const * kpServerAddress, 
       TmConnectionType const kConnection,
       TmU16 const kServerPort,
       TmU32 const kFlags,
       int const kTimeoutMS ) );
#endif

TM_API( void, tmCoreClose, ( HTELEMETRY cx ) );

TM_API( void , tmCoreSetDebugZoneLevel, ( HTELEMETRY cx, int const v ) );
TM_API( void , tmCoreCheckDebugZoneLevel, ( HTELEMETRY cx, int const v ) );
TM_API( void , tmCoreUnwindToDebugZoneLevel, ( HTELEMETRY cx, int const v ) );

TM_API( char const *, tmCoreDynamicString, ( HTELEMETRY cx, char const * s ) );
TM_API( void, tmCoreClearStaticString, ( HTELEMETRY cx, char const * s ) );

TM_API( void, tmCoreSetVariable, ( HTELEMETRY cx, char const *kpKey, TmFormatCode* pFormatCode, char const *kpValueFmt, ... ) );
TM_API( void, tmCoreSetTimelineSectionName, ( HTELEMETRY cx, TmFormatCode *pFormatCode, char const * kpFmt, ... ) );
TM_API( void, tmCoreThreadName, ( HTELEMETRY cx, TmU32 const kThreadID, TmFormatCode *pFormatCode, char const * kpFmt, ... ) ); 
TM_API( void, tmCoreGetFormatCode, ( TmFormatCode* pCode, char const * kpFmt ) );

TM_API( void, tmCoreEnable, ( HTELEMETRY cx, TmOption const kOption, int const kValue ) );
TM_API( int , tmCoreIsEnabled, ( HTELEMETRY cx, TmOption const kOption ) );

TM_API( void, tmCoreSetParameter, ( HTELEMETRY cx, TmParameter const kParam, void const *kpValue ) );
TM_API( void, tmCoreSetServerConfiguration, ( HTELEMETRY cx, TmServerConfiguration const kConfiguration, void const *kpValue ) );

TM_API( void, tmCoreTick, ( HTELEMETRY cx ) );
TM_API( void, tmCoreFlush, ( HTELEMETRY cx ) );
TM_API( void, tmCorePause, ( HTELEMETRY cx, int const kPause ) );
TM_API( int, tmCoreIsPaused, ( HTELEMETRY cx ) );
TM_API( void, tmCoreEnter, (HTELEMETRY cx, TmU64 *matchid, TmU32 const kThreadId, TmU64 const kThreshold, TmU32 const kFlags, char const *kpLocation, TmU32 const kLine, TmFormatCode* pFmtCode, char const *kpFmt, ... ) );
TM_API( void, tmCoreLeave, ( HTELEMETRY cx, TmU64 const kMatchID, TmU32 const kThreadId, char const *kpLocation, int const kLine ) ); 

TM_API( void,  tmCoreEmitAccumulationZone, ( HTELEMETRY cx, TmU64 * pAccum, TmU64 const kZoneTotal, TmU32 const kCount, TmU32 const kZoneFlags, char const *kpLocation, TmU32 const kLine, TmFormatCode *pFmtCode, char const * kpFmt, ... ) );

TM_API( TmU64, tmCoreGetLastContextSwitchTime, (HTELEMETRY cx) );

TM_API( void, tmCoreLockName, ( HTELEMETRY cx, void const *kpPtr, TmFormatCode* pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCoreSetLockState, ( HTELEMETRY cx, void const *kpPtr, TmLockState const kState, char const * kLocation, TmU32 const kLine, TmFormatCode *pFormatCode, char const *kpFmt, ... ) );
TM_API( int,  tmCoreSetLockStateMinTime, ( HTELEMETRY cx, void* buf, void const *kpPtr, TmLockState const kState, char const * kLocation, TmU32 const kLine, TmFormatCode *pFormatCode, char const *kpFmt, ... ) );

TM_API( void, tmCoreBeginTimeSpan, ( HTELEMETRY cx, TmU64 const kId, TmU32 const kFlags, TmU64 const kTime, char const *kpLocation, TmU32 const kLine, TmFormatCode *pFmtCode, char const *kpFmt, ... ) );
TM_API( void, tmCoreEndTimeSpan, ( HTELEMETRY cx, TmU64 const kId, TmU32 const kFlags, TmU64 const kTime, char const *kpLocation, TmU32 const kLine, TmFormatCode *pFmtCode, char const *kpFmt, ... ) );

TM_API( void, tmCoreSignalLockCount, ( HTELEMETRY cx, char const *kpLocation, TmU32 const kLine, void const * kPtr, TmU32 const kCount, TmFormatCode* pFmtCode, char const *kpName, ... ) );
TM_API( void, tmCoreTryLock,   ( HTELEMETRY cx, TmU64 *matchid, TmU64 const kThreshold, char const *kpLocation, TmU32 const kLine, void const * kPtr, TmFormatCode* pFmtCode, char const *kpFmt, ... ) );
TM_API( void, tmCoreEndTryLock, ( HTELEMETRY cx, TmU64 const kMatchId, char const *kpLocation, int const kLine, TmFormatCode* pFmt, void const * kPtr, TmLockResult const kResult ) );

TM_API( TmI32, tmCoreGetStati, ( HTELEMETRY cx, TmStat const kStat ) );

TM_API( void, tmCoreMessage, ( HTELEMETRY cx, TmU32 const kThreadId, TmU32 const kFlags, TmFormatCode* pFmtCode, char const * kpFmt, ... ) );

TM_API( void, tmCoreAlloc, ( HTELEMETRY cx, void const * kPtr, TmU64 const kSize, char const *kpLocation, TmU32 const kLine, TmFormatCode *pFmtCode, char const *kpFmt, ... ) );
TM_API( void, tmCoreFree, ( HTELEMETRY cx, void const * kpPtr, char const *kpLocation, int const kLine, TmFormatCode *pFmtCode ) );

TM_API( void, tmCorePlot, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, float const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCorePlotI32, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, TmI32 const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCorePlotU32, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, TmU32 const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCorePlotI64, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, TmI64 const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCorePlotU64, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, TmU64 const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCorePlotF64, ( HTELEMETRY cx, TmI64 const kTSC, TmPlotType const kType, TmU32 const kFlags, double const kValue,  TmFormatCode *pFmtCode, char const * kpFmt, ... ) );

TM_API( void, tmCoreBlob, ( HTELEMETRY cx,   void const * kpData, int const kDataSize, char const *kpPluginIdentifier, TmFormatCode* pFmtCode, char const * kpFmt, ... ) );
TM_API( void, tmCoreDisjointBlob, ( HTELEMETRY cx, int const kNumPieces, void const ** kpData, int const *kDataSize, char const *kpPluginIdentifier, TmFormatCode* pFmtCode, char const * kpFmt, ... ) );

TM_API( void, tmCoreUpdateSymbolData, ( HTELEMETRY cx ) );

TM_API( int, tmCoreSendCallStack, ( HTELEMETRY cx, TmCallStack const * kpCallStack, int const kSkip ) );
TM_API( int, tmCoreGetCallStack, ( HTELEMETRY cx, TmCallStack * pCallStack ) );

TM_API( int, tmCoreSendCallStackR, ( HTELEMETRY cx, TmCallStack const * kpCallStack, int const kSkip ) );
TM_API( int, tmCoreGetCallStackR, ( HTELEMETRY cx, TmCallStack * pCallStack ) );

#ifndef TM_API_STATIC
typedef struct TM_API_STRUCT
{
  // NOTE: Order of this must be preserved, new functions should be added to the end
  #define TM_API_S(name) TM_FUNCTION_TYPE(name) name

  TM_API_S( tmCoreCheckVersion );
  TM_API_S( tmCoreUpdateSymbolData );
  TM_API_S( tmCoreGetLastContextSwitchTime );    
#ifndef __RADSPU__
  TM_API_S( tmCoreTick );
#endif
  TM_API_S( tmCoreFlush );
  TM_API_S( tmCoreDynamicString );
  TM_API_S( tmCoreClearStaticString );
  TM_API_S( tmCoreSetVariable );
  TM_API_S( tmCoreGetFormatCode );
  TM_API_S( tmCoreGetSessionName );
  TM_API_S( tmCoreGetLastError );
  TM_API_S( tmCoreShutdownContext );
  TM_API_S( tmCoreGetConnectionStatus );
  TM_API_S( tmCoreSetTimelineSectionName );
  TM_API_S( tmCoreEnable );
  TM_API_S( tmCoreIsEnabled );
#ifndef __RADSPU__
  TM_API_S( tmCoreOpen );
#endif
  TM_API_S( tmCoreClose );
  TM_API_S( tmCorePause );
  TM_API_S( tmCoreIsPaused );
  TM_API_S( tmCoreEnter );
  TM_API_S( tmCoreLeave );

  TM_API_S( tmCoreThreadName );
  TM_API_S( tmCoreLockName );
  TM_API_S( tmCoreTryLock );
  TM_API_S( tmCoreEndTryLock );
  TM_API_S( tmCoreSignalLockCount );
  TM_API_S( tmCoreSetLockState );

  TM_API_S( tmCoreAlloc );
  TM_API_S( tmCoreFree );
  TM_API_S( tmCoreGetStati );

  TM_API_S( tmCoreBeginTimeSpan );
  TM_API_S( tmCoreEndTimeSpan );

  TM_API_S( tmCorePlot );
  TM_API_S( tmCorePlotI32 );
  TM_API_S( tmCorePlotU32 );
  TM_API_S( tmCorePlotI64 );
  TM_API_S( tmCorePlotU64 );
  TM_API_S( tmCorePlotF64 );

  TM_API_S( tmCoreBlob );
  TM_API_S( tmCoreDisjointBlob );
  TM_API_S( tmCoreMessage );

  TM_API_S( tmCoreSendCallStack );
  TM_API_S( tmCoreGetCallStack );

  TM_API_S( tmCoreSetDebugZoneLevel );
  TM_API_S( tmCoreCheckDebugZoneLevel );
  TM_API_S( tmCoreUnwindToDebugZoneLevel );

  TM_API_S( tmCoreEmitAccumulationZone );

  TM_API_S( tmCoreSetLockStateMinTime );
  TM_API_S( tmCoreSetParameter );

#if defined __RADPS3__ && !defined TM_API_STATIC
  TM_API_S( tmPPUCoreGetListener );
  TM_API_S( tmPPUCoreRegisterSPUProgram );
#endif

  // If adding anything after here make sure to update stubs!
  TM_API_S( tmCoreSendCallStackR );
  TM_API_S( tmCoreGetCallStackR );
  TM_API_S( tmCoreSetServerConfiguration );


} TM_API_STRUCT;
#endif // !TM_API_STATIC


#endif //NTELEMETRY

RADDEFEND

#if !defined TM_API_STATIC && !defined NTELEMETRY
// snag API function
typedef void (RADEXPLINK *tmGetAPIproc)( void * buffer );
#endif
//----------------------------------------------------------------------------
// If NTELEMETRY is defined then all Telemetry calls are compiled away by
// the preprocessor.
//----------------------------------------------------------------------------
#ifdef NTELEMETRY

#define TM_VAR(x)

#define tmUpdateSymbolData(cx)
#define tmSetDebugZoneLevel(cx,n)
#define tmCheckDebugZoneLevel(cx,n)
#define tmUnwindToDebugZoneLevel(cx,n)

#define tmGetLastError(cx) TM_OK
#define tmShutdownContext(cx) 

#define tmGetLastContextSwitchTime(cx) 0
#define tmGetAccumulationStart(cx) 0

#define tmEnterAccumulationZone(cx,z)
#define tmLeaveAccumulationZone(cx,z)

#define tmGetFormatCode(cx,s,t)
#define tmDynamicString(cx,s) 0

#define tmTick(cx)
#define tmFlush(cx)
#define tmPause(cx,p)
#define tmIsPaused(cx) 0

#define tmClearStaticString(cx,s)

#define tmEnable(cx,flag,value)
#define tmIsEnabled(cx,flag) 0

#define tmSetParameter(cx,param,v)
#define tmSetServerConfiguration(cx,param,v)

#define tmOpen(cx,appname,buildinfo,servername,contype,serverport,flags,timeout) TMERR_DISABLED
#define tmClose(cx)
#define tmGetConnectionStatus(cx) TMCS_DISCONNECTED

#define tmFree(cx,ptr)
#define tmLeave(cx)

#ifndef __RADNOVARARGMACROS__

#define tmGetSessionName(...)
#define tmEndTryLock(...)
#define tmEndTryLockEx(...)
#define tmSetLockState(...)
#define tmSetLockStateEx(...)
#define tmSetLockStateMinTime(...) 0
#define tmSetLockStateMinTimeEx(...) 0
#define tmSignalLockCount(...)

#define tmCheckVersion(...) 0
#define tmGetCallStack(...) 0
#define tmSendCallStack( ... ) TMPRINTF_TOKEN_NONE

#define tmGetVersion(...) 0
#define tmStartup(...)  TMERR_DISABLED
#define tmStartupEx(...)  TMERR_DISABLED
#define tmGetPlatformInformation(...) TMERR_DISABLED
#define tmInitializeContext(...) TMERR_DISABLED
#define tmShutdown(...) TMERR_DISABLED

#define tmEnter(...)
#define tmEnterEx(...)
#define tmZone(...) 
#define tmZoneFiltered(...)
#define tmLeaveEx(...)

#define tmBeginTimeSpan(...)
#define tmEndTimeSpan(...)

#define tmEmitAccumulationZone(...)

#define tmGetStati(...) 0

#define tmSetVariable(...)

#define tmBlob(...)
#define tmDisjointBlob(...)
#define tmSetTimelineSectionName(...)
#define tmThreadName(...)
#define tmLockName(...)
#define tmMessage(...)
#define tmMessageEx(...)
#define tmAlloc(...)
#define tmAllocEx(...)

#define tmTryLock(...)
#define tmTryLockEx(...)
    
#define tmPlot(...)
#define tmPlotF32(...)
#define tmPlotF64(...)
#define tmPlotI32(...)
#define tmPlotU32(...)
#define tmPlotS32(...)
#define tmPlotI64(...)
#define tmPlotU64(...)
#define tmPlotS64(...)

#define tmPlotAt(...)
#define tmPlotF32At(...)
#define tmPlotF64At(...)
#define tmPlotI32At(...)
#define tmPlotU32At(...)
#define tmPlotS32At(...)
#define tmPlotI64At(...)
#define tmPlotU64At(...)
#define tmPlotS64At(...)

#define tmPPUGetListener(...)
#define tmPPURegisterSPUProgram(...)
#define tmSPUBindContextToListener(...)
#define tmSPUUpdateTime(...)
#define tmSPUFlushImage(...)

#endif // __RADNOVARARGMACROS__

#else // !NTELEMETRY

//----------------------------------------------------------------------------
// Variable declaration macro so that if you compile with NTELEMETRY
// the Telemetry specific variable declarations go away (helps when compiling
// with warnings-as-errors).
//----------------------------------------------------------------------------
#define TM_VAR(x) x 

//----------------------------------------------------------------------------
// C++ helper for entering/exiting zones
//----------------------------------------------------------------------------
#if defined(__cplusplus) && !defined(TM_NO_ZCLASS)
#define TMLINENAME3(name,line) name##line
#define TMLINENAME2( name, line ) TMLINENAME3(name,line)
#define TMLINENAME( name ) TMLINENAME2(name,__LINE__)

#define tmZoneFiltered(cx,mintime,flags,name,...) TmU64 TMLINENAME(tm_id) = 0; \
    TMCHECKCONTEXT_VA(cx,tmCoreEnter,(cx,&(TMLINENAME(tm_id)),0,mintime,(flags),__FILE__, __LINE__, &tm_fmt, name,##__VA_ARGS__));\
    Tm_Zone TMLINENAME(tm_ZoneProfiler)(cx,TMLINENAME(tm_id));

#define tmZone(cx,flags,name,...) tmZoneFiltered(cx,0,flags,name,##__VA_ARGS__)

//Zone helper
class Tm_Zone 
{
private:
    TmU64 z_id;
    HTELEMETRY z_cx; 

public:
    inline Tm_Zone(HTELEMETRY cx, TmU64 const kMatchID )
    {
        z_cx = cx;
        z_id = kMatchID;
    }

    inline ~Tm_Zone()
    {
        TMCHECKCONTEXTV(z_cx,tmCoreLeave,(z_cx,z_id,0,0,0));
    }
};

#endif //__cplusplus

#if (!(defined(TM_DLL) && (TM_DLL==1)))

// startup and shutdown functions (part of loading lib, not DLL)
#ifndef __RADINDLL__
RADDEFSTART
#endif

#if defined TM_API_STATIC
#define tmStartup tmCoreStartup
#define tmStartupEx tmCoreStartupEx
#define tmShutdown tmCoreShutdown
#define tmGetVersion tmCoreGetVersion
#define tmInitializeContext tmCoreInitializeContext
#define tmGetPlatformInformation tmCoreGetPlatformInformation
#else
RADDEFFUNC TmErrorCode RADEXPLINK tmStartup( void );
RADDEFFUNC TmErrorCode RADEXPLINK tmStartupEx( TMUSERTIMERFNC *fnc );
RADDEFFUNC TmErrorCode RADEXPLINK tmShutdown( void ); 
RADDEFFUNC TmU32 RADEXPLINK tmGetVersion( void );
RADDEFFUNC TmErrorCode RADEXPLINK tmInitializeContext( EXPOUT HTELEMETRY * pcx, void * pArena, TmU32 const kArenaSize );
RADDEFFUNC TmErrorCode RADEXPLINK tmGetPlatformInformation( void* obj, TmPlatformInformation const kInfo, void *dst, TmU32 const kSize );
#endif

#ifndef __RADINDLL__
RADDEFEND
#endif

#define tmCheckVersion( cx,major,minor,build,cust) TMCHECKCONTEXTR(cx,tmCoreCheckVersion,(cx,major,minor,build,cust),0)

#ifdef __RADPS3__
#define tmPPUGetListener(cx,n,p) TMCHECKCONTEXTR(cx,tmPPUCoreGetListener,(cx,n,p),TMERR_DISABLED)
#define tmPPURegisterSPUProgram(cx,guid,address,s,ro_offset) TMCHECKCONTEXTR(cx,tmPPUCoreRegisterSPUProgram,(cx,guid,address,s,ro_offset),TMERR_DISABLED)
#endif

#ifdef __RADSPU__
#define tmSPUUpdateTime(cx) TMCHECKCONTEXTV(cx,tmSPUCoreUpdateTime,(cx))
#define tmSPUFlushImage(cx) TMCHECKCONTEXTV(cx,tmSPUCoreFlushImage,(cx))
#endif

#define tmUpdateSymbolData(cx) TMCHECKCONTEXTV(cx,tmCoreUpdateSymbolData,(cx))

#define tmGetSessionName(cx,a,b) TMCHECKCONTEXTR(cx,tmCoreGetSessionName,(cx,a,b),TMERR_DISABLED)

#define tmUnwindToDebugZoneLevel(cx,n) TMCHECKCONTEXTV(cx,tmCoreUnwindToDebugZoneLevel,(cx,n))
#define tmSetDebugZoneLevel(cx,n) TMCHECKCONTEXTV(cx,tmCoreSetDebugZoneLevel,(cx,n))
#define tmCheckDebugZoneLevel(cx,n) TMCHECKCONTEXTV(cx,tmCoreCheckDebugZoneLevel,(cx,n))

#define tmGetCallStack(cx,p) TMCHECKCONTEXTR(cx,tmCoreGetCallStack,(cx,p),0)
#define tmSendCallStack(cx,p) TMCHECKCONTEXTR(cx,tmCoreSendCallStack,(cx,p,0),TMPRINTF_TOKEN_NONE)
#define tmSendCallStackWithSkip(cx,p,s) TMCHECKCONTEXTR(cx,tmCoreSendCallStack,(cx,p,s),TMPRINTF_TOKEN_NONE)

#define tmGetCallStackR(cx,p) TMCHECKCONTEXTR(cx,tmCoreGetCallStackR,(cx,p),0)
#define tmSendCallStackR(cx,p) TMCHECKCONTEXTR(cx,tmCoreSendCallStackR,(cx,p,0),TMPRINTF_TOKEN_NONE)
#define tmSendCallStackWithSkipR(cx,p,s) TMCHECKCONTEXTR(cx,tmCoreSendCallStackR,(cx,p,s),TMPRINTF_TOKEN_NONE)

#define tmGetLastError(cx) TMCHECKCONTEXT(cx,tmCoreGetLastError,(cx))
#define tmShutdownContext(cx) TMCHECKCONTEXTV(cx,tmCoreShutdownContext,(cx))

#define tmGetLastContextSwitchTime(cx) TMCHECKCONTEXTR(cx,tmCoreGetLastContextSwitchTime,(cx),0)

#define tmGetAccumulationStart(cx) ((cx) ? tmFastTime() : 0)
#define tmEnterAccumulationZone(cx,z) { if (cx) (z) -= tmFastTime(); }
#define tmLeaveAccumulationZone(cx,z) { if (cx) (z) += tmFastTime(); }

#define tmGetFormatCode(cx,s,t) TMCHECKCONTEXTV(cx,tmCoreGetFormatCode,(s,t))
#define tmDynamicString(cx,s) TMCHECKCONTEXTR(cx,tmCoreDynamicString,(cx,s),0)

#define tmClearStaticString(cx,s) TMCHECKCONTEXTV(cx,tmCoreClearStaticString,(cx,s))

#define tmEnable(cx,flag,value) TMCHECKCONTEXTV(cx,tmCoreEnable,(cx,flag,value))
#define tmIsEnabled(cx,flag) TMCHECKCONTEXT(cx,tmCoreIsEnabled,(cx,flag))
#define tmSetParameter(cx,p,v) TMCHECKCONTEXTV(cx,tmCoreSetParameter,(cx,p,v))
#define tmSetServerConfiguration(cx,p,v) TMCHECKCONTEXTV(cx,tmCoreSetServerConfiguration,(cx,p,v))

#define tmOpen(cx,appname,buildinfo,servername,contype,serverport,flags,timeout) TMCHECKCONTEXT(cx,tmCoreOpen,(cx,appname,buildinfo,servername,contype,serverport,flags,timeout))
#define tmClose(cx) TMCHECKCONTEXTV(cx,tmCoreClose,(cx))
#define tmTick(cx) TMCHECKCONTEXTV(cx,tmCoreTick,(cx))

#define tmFlush(cx) TMCHECKCONTEXTV(cx,tmCoreFlush,(cx))
#define tmPause(cx,pause) TMCHECKCONTEXTV(cx,tmCorePause,(cx,pause))
#define tmIsPaused(cx) TMCHECKCONTEXTR(cx,tmCoreIsPaused,(cx),0)
#define tmGetConnectionStatus(cx) TMCHECKCONTEXTR(cx,tmCoreGetConnectionStatus,(cx),TMCS_DISCONNECTED)

#define tmGetStati( c, stat ) TMCHECKCONTEXT(cx,tmCoreGetStati,( cx, stat ))
#define tmLeave(cx) TMCHECKCONTEXTV(cx,tmCoreLeave,(cx,0,0,__FILE__,__LINE__))
#define tmLeaveEx(cx,match,tid,f,l) TMCHECKCONTEXTV(cx,tmCoreLeave,(cx,match,tid,f,l))
#define tmFree(cx,ptr) TMCHECKCONTEXT_VA(cx,tmCoreFree,(cx,ptr,__FILE__, __LINE__, &tm_fmt))

#ifndef __RADNOVARARGMACROS__

#ifdef __RADSPU__
#define tmSPUBindContextToListener(pcx,mem,pptr,imagename,...) TMCHECKCONTEXTV(pcx,tmSPUCoreBindContextToListener,(pcx,mem,pptr,imagename,##__VA_ARGS__))
#endif

#define tmTryLock(cx,ptr,name,...) TMCHECKCONTEXT_VA(cx,tmCoreTryLock,(cx,0,0,__FILE__,__LINE__,ptr,&tm_fmt,name,##__VA_ARGS__))
#define tmTryLockEx(cx,matchid,threshold,file,line,ptr,name,...) TMCHECKCONTEXT_VA(cx,tmCoreTryLock,(cx,matchid,threshold,file,line,ptr,&tm_fmt,name,##__VA_ARGS__))
#define tmEndTryLock(cx,m,result)  TMCHECKCONTEXT_VA(cx,tmCoreEndTryLock,(cx,0,__FILE__,__LINE__,&tm_fmt, m,result))
#define tmEndTryLockEx(cx,matchid,file,line,m,result)  TMCHECKCONTEXT_VA(cx,tmCoreEndTryLock,(cx,matchid,file,line,&tm_fmt,m,result))

#define tmBeginTimeSpan(cx,id,flags,name,...) TMCHECKCONTEXT_VA(cx,tmCoreBeginTimeSpan,(cx,id,flags,0,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))
#define tmEndTimeSpan(cx,id,flags,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEndTimeSpan,(cx,id,flags,0,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))
#define tmBeginTimeSpanEx(cx,id,flags,t,file,line,name,...) TMCHECKCONTEXT_VA(cx,tmCoreBeginTimeSpan,(cx,id,flags,t,file,line,&tm_fmt,name,##__VA_ARGS__))
#define tmEndTimeSpanEx(cx,id,flags,t,file,line,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEndTimeSpan,(cx,id,flags,t,file,line,&tm_fmt,name,##__VA_ARGS__))
#define tmBeginTimeSpanAt(cx,id,flags,t,name,...) TMCHECKCONTEXT_VA(cx,tmCoreBeginTimeSpan,(cx,id,flags,t,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))
#define tmEndTimeSpanAt(cx,id,flags,t,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEndTimeSpan,(cx,id,flags,t,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))

#define tmSignalLockCount(cx,m,cnt,name,...) TMCHECKCONTEXT_VA(cx,tmCoreSignalLockCount,(cx,__FILE__,__LINE__,m,cnt,&tm_fmt,name,##__VA_ARGS__))
#define tmSetLockState(cx,lockptr,state,name,...)  TMCHECKCONTEXT_VA(cx,tmCoreSetLockState,(cx,lockptr,state,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__ ))
#define tmSetLockStateEx(cx,file,line,lockptr,state,name,...)  TMCHECKCONTEXT_VA(cx,tmCoreSetLockState,(cx,lockptr,state,file,line,&tm_fmt,name,##__VA_ARGS__ ))
#define tmSetLockStateMinTime(cx,buf,lockptr,state,name,...)  TMCHECKCONTEXT_VA(cx,tmCoreSetLockStateMinTime,(cx,buf,lockptr,state,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__ ))
#define tmSetLockStateMinTimeEx(cx,buf,file,line,lockptr,state,name,...)  TMCHECKCONTEXT_VA(cx,tmCoreSetLockStateMinTime,(cx,buf,lockptr,state,file,line,&tm_fmt,name,##__VA_ARGS__ ))

#define tmThreadName(cx,threadid,name,...) TMCHECKCONTEXT_VA(cx,tmCoreThreadName,(cx,threadid,&tm_fmt,name,##__VA_ARGS__))
#define tmLockName(cx,ptr,name,...) TMCHECKCONTEXT_VA(cx,tmCoreLockName,(cx,ptr,&tm_fmt,name,##__VA_ARGS__))

#define tmEmitAccumulationZone(cx,flags,acc,count,ztotal,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEmitAccumulationZone,(cx,acc,ztotal,count,flags,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))
#define tmSetVariable(cx,key,v,...) TMCHECKCONTEXT_VA(cx,tmCoreSetVariable,(cx,key,&tm_fmt,v,##__VA_ARGS__))
#define tmSetTimelineSectionName(cx,name,...) TMCHECKCONTEXT_VA(cx,tmCoreSetTimelineSectionName,(cx,&tm_fmt,name,##__VA_ARGS__))
#define tmEnter(cx,flags,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEnter,(cx,0,0,0,flags,__FILE__,__LINE__,&tm_fmt,name,##__VA_ARGS__))
#define tmEnterEx(cx,matchid,tid,threshold,file,line,flags,name,...) TMCHECKCONTEXT_VA(cx,tmCoreEnter,(cx,matchid,tid,threshold,flags,file,line,&tm_fmt,name,##__VA_ARGS__))

#define tmAlloc(cx,ptr,size,name,...) TMCHECKCONTEXT_VA(cx,tmCoreAlloc,(cx,ptr,size,__FILE__,__LINE__, &tm_fmt, name,##__VA_ARGS__))
#define tmAllocEx(cx,file,line,ptr,size, name,...) TMCHECKCONTEXT_VA(cx,tmCoreAlloc,(cx,ptr,size,file,line, &tm_fmt,name,##__VA_ARGS__))
#define tmMessage(cx,flags,s,...) TMCHECKCONTEXT_VA(cx,tmCoreMessage,(cx,0,flags,&tm_fmt,s,##__VA_ARGS__))
#define tmMessageEx(cx,tid,flags,s,...) TMCHECKCONTEXT_VA(cx,tmCoreMessage,(cx,tid,flags,&tm_fmt,s,##__VA_ARGS__))

#define tmPlot(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlot,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotF32(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlot,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotF64(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotF64,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotI32(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotI32,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotU32(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotU32,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotI64(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotI64,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotU64(cx,type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotU64,(cx,0,type,flags,val,&tm_fmt,name,##__VA_ARGS__))

#define tmPlotAt(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlot,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotF32At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlot,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotF64At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotF64,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotI32At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotI32,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotU32At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotU32,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotI64At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotI64,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))
#define tmPlotU64At(cx,tsc, type,flags,val,name,...) TMCHECKCONTEXT_VA(cx,tmCorePlotU64,(cx,tsc,type,flags,val,&tm_fmt,name,##__VA_ARGS__))

#define tmBlob(cx,chunk,size,pluginid,name,...) TMCHECKCONTEXT_VA(cx,tmCoreBlob,(cx,chunk,size,pluginid,&tm_fmt,name,##__VA_ARGS__))
#define tmDisjointBlob(cx,num_args,chunks,sizes,plugind,name,...) TMCHECKCONTEXT_VA(cx,tmCoreDisjointBlob,(cx,num_args,chunks,sizes,pluginid,&tm_fmt,name,##__VA_ARGS__))

#endif

#endif

#endif // !NTELEMETRY


#if ( defined __RADNT__ || defined __RADLINUX__ || defined __RADMAC__ ) && !defined TM_API_STATIC && !defined __RADDURANGO__

RADDEFSTART

RADDEFFUNC int RADEXPLINK tmLoadTelemetryEx( int const major, int const minor, int const build, int const cust, int const kUseCheckedDLL );

#define TM_LOAD_CHECKED_LIBRARY 1
#define TM_LOAD_RELEASE_LIBRARY 0

#ifndef NTELEMETRY
#define tmLoadTelemetry( kUseCheckedDLL ) tmLoadTelemetryEx( TelemetryMajorVersion, TelemetryMinorVersion, TelemetryBuildNumber, TelemetryCustomization, kUseCheckedDLL )
#else
#define tmLoadTelemetry(...) 1
#endif

RADDEFEND

#else
#define tmLoadTelemetry(...) 1
#endif

#ifdef EXPGROUP
#undef EXPGROUP
#endif
#define EXPGROUP()
EXPGROUP() 
#undef EXPGROUP
 
// ----------------------------------------------------------------------------
// Define a fast time retrieval macro for this platform
// ----------------------------------------------------------------------------
#ifdef NTELEMETRY

#define tmFastTime() 0

#elif defined TM_INLINE_FASTTIME

    // Enable intrinsic RDTSC if available
    #if defined _MSC_VER  && _MSC_VER >= 1400 && defined __RADX86__ 
        #pragma warning(disable:4505) 
        #pragma warning(disable:4995)

        RADDEFFUNC unsigned __int64 __rdtsc(void);
        #pragma intrinsic(__rdtsc)
    #endif

    // SPU
    #if defined __RADSPU__
        #ifndef _SPU_INTRINSICS_H
        #include <spu_intrinsics.h>
        #endif

        #ifdef __cplusplus
            extern "C" TmU64 tm_spu_ppu_time;
            extern "C" TmU64 tm_spu_last_time;
        #else
            extern TmU64 tm_spu_ppu_time;
            extern TmU64 tm_spu_last_time;
        #endif

        static RADINLINE U32 __tmFastTimeSPU()
        {
            U32 dec = spu_readch(SPU_RdDec);
            U32 tsc = - dec;
            return tsc;
        }

        static RADINLINE U64 tmFastTime()
        {
            U64 const now = __tmFastTimeSPU();
            U64 t;

            if ( ( now & 0xFFFFFFFF ) < ( tm_spu_ppu_time & 0xFFFFFFFF ) )
            {
                t = now | ( ( tm_spu_ppu_time + 0x100000000LL ) & (~0xFFFFFFFFLL) );
            }
            else
            {
                t = now | ( tm_spu_ppu_time & (~0xFFFFFFFFLL ));
            }

            // Check to see if our time has wrapped/reversed
            if ( tm_spu_last_time > t )
            {
                t += 0x100000000LL;
            }

            tm_spu_last_time = t;

            return t;
        }

    // 32-bit Windows uses RDTSC
    #elif defined __RADNT__ && !defined __RADX64__

       // If intrinsics are available, use them
       #if defined _MSC_VER  && _MSC_VER >= 1400
          #define tmFastTime() __rdtsc() // available as intrinsic on Visual Studio
       // Otherwise resort to inline assembly
       #else
          static __declspec(naked) unsigned __int64 tmFastTime()
          {
            __asm rdtsc;
            __asm ret;
          }
       #endif
    // On 64-bit windows we can use __rdtsc directly
    #elif defined __RADWIN__ && defined __RADX64__ && defined _MSC_VER
        #define tmFastTime() __rdtsc() 
    // Mac and Linux imply GCC inline asm available  
    #elif defined __RADMAC__ || ( ( defined __RADLINUX__ || defined __RADQNX__ ) && ( defined __RADX86__ || defined __RADX64__) ) || defined __RADPS4__
        static inline unsigned long long tmFastTime() 
        {
            TmU32 lo, hi;
            asm volatile ("rdtsc" : "=a" (lo), "=d" (hi) );
            return (TmU64) hi << 32 | lo;
        } 
    // On QNX this is the best we can hope for, unfortunately it may not be synced across
    // processors so we need to watch out for that
    #elif defined ( __RADQNX__ ) 
        #include <sys/neutrino.h>
        #include <inttypes.h>
        static RADINLINE U64 tmFastTime() 
        {
            return ClockCycles();
        }
    // On ARM Linux platforms (including Android) we have to hope that clock_gettime
    // will be of sufficient accuracy
    #elif defined __RADANDROID__
        #include <time.h>
        static inline unsigned long long tmFastTime()
        {
            // We're scaling by 1000 to get closer to a nanosecondish scale.  Keep in mind
            // that clock() is 32-bits and will wrap every ~72 minutes, so if you want higher
            // precision and/or longer duration you should implement this with user timers
            return clock() * 1000ULL;
        }

    #elif ( defined __RADLINUX__  && defined __RADARM__ )
        #include <time.h>
        static inline unsigned long long tmFastTime()
        {
            struct timespec t;

            clock_gettime( CLOCK_MONOTONIC_RAW, &t );
            return t.tv_sec * 1000000000ULL + t.tv_nsec;
        }
    // Xenon or PS3 we assume __mftb is available  
    // PowerPC platforms can use MFTB
    #elif defined __RADPS3__
        // PS3 we assume __mftb is available  
        #define tmFastTime() __extension__          \
          ({ unsigned long long __macro_result;         \
          __asm__ volatile ("1: mftb %[current_tb]\n"     \
              "\tcmpwi 7, %[current_tb], 0\n"       \
              "\tbeq-  7, 1b"           \
              : [current_tb] "=r" (__macro_result):       \
              :"cr7");              \
          __macro_result; })

    #elif defined __RADXENON__ 
        // Xenon we assume __mftb is available  
        #if defined __RADXENON__
            RADDEFFUNC unsigned __int64 __mftb(); //From <PPCIntrinsics.h>
            #pragma intrinsic(__mftb)
        #endif
  
        // check for weird sample and re-read
        static RADINLINE U64 tmFastTime()
        {
            U64 a;
            a = __mftb();
            if ( ( a & 0xffffffff ) < 500 )
                a = __mftb();
            return a;
        }

    #elif defined __RADIPHONE__
        RADDEFFUNC U64 mach_absolute_time();
        static RADINLINE U64 tmFastTime()
        {
            return mach_absolute_time();
        }
    #elif defined __RADWIIU__
        RADDEFFUNC unsigned int __MFTB(void); // in case aren't previously defined
        RADDEFFUNC unsigned int __MFTBU(void);
        static RADINLINE U64 tmFastTime()
        {
            U64 a = (U64) __MFTB() | ((U64) __MFTBU() << 32);
            if ( ( a & 0xffffffff ) < 500 )
                a = (U64) __MFTB() | ((U64) __MFTBU() << 32);
            return a;
        }
    #elif defined __RADPSP2__
        #include <libperf.h>
        #define tmFastTime() scePerfGetTimebaseValue()
    #else
        #error tmFastTime not defined for this platform
    #endif
#endif // NTELEMETRY

#endif //__RADRES__

#endif //TELEMETRY_H
