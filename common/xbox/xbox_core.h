//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox Core definitions
//
//=============================================================================
#pragma once

#define XBOX_DONTCARE					0		// for functions with don't care params

#define XBX_MAX_DPORTS					4
#define XBX_MAX_BUTTONSAMPLE			32768
#define XBX_MAX_ANALOGSAMPLE			255
#define XBX_MAX_MESSAGE					2048
#define XBX_MAX_PATH					MAX_PATH
#define XBX_MAX_RCMDLENGTH				256
#define XBX_MAX_RCMDNAMELEN				32
#define XBX_HDD_CLUSTERSIZE				16384

// could be dvd or hdd, actual device depends on source of xex launch
#define XBX_DVD_DRIVE					"D:\\"
#define XBX_BOOT_DRIVE					"D:\\"

#define XBX_IOTHREAD_STACKSIZE			32768
#define XBX_IOTHREAD_PRIORITY			THREAD_PRIORITY_HIGHEST

// scale by screen dimension to get an inset
#define XBOX_MINBORDERSAFE				0.05f
#define XBOX_MAXBORDERSAFE				0.075f

#define	XBX_CALCSIG_TYPE				XCALCSIG_FLAG_NON_ROAMABLE
#define XBX_INVALID_STORAGE_ID			((DWORD)-1)
#define XBX_STORAGE_DECLINED			((DWORD)-2)
#define XBX_INVALID_USER_ID				((DWORD)-1)

#define XBX_USER_SETTINGS_CONTAINER_DRIVE	"CFG"

// Path to our running executable
#define XBX_XEX_BASE_FILENAME			"default.xex"
#define XBX_XEX_PATH					XBX_BOOT_DRIVE XBX_XEX_BASE_FILENAME

#define XBX_CLR_DEFAULT					0xFF000000
#define XBX_CLR_WARNING					0x0000FFFF
#define XBX_CLR_ERROR					0x000000FF

// disk space requirements
#define XBX_SAVEGAME_BYTES				( 1024 * 1024 * 2 )
#define XBX_CONFIGFILE_BYTES			( 1024 * 100 )
#define XBX_USER_STATS_BYTES			( 1024 * 28 )
#define XBX_USER_SETTINGS_BYTES			( XBX_CONFIGFILE_BYTES + XBX_USER_STATS_BYTES )

#define XBX_PERSISTENT_BYTES_NEEDED		( XBX_SAVEGAME_BYTES * 10 )	// 8 save games, 1 autosave, 1 autosavedangerous

#define XMAKECOLOR( r, g, b )			((unsigned int)(((unsigned char)(r)|((unsigned int)((unsigned char)(g))<<8))|(((unsigned int)(unsigned char)(b))<<16)))

#define MAKE_NON_SRGB_FMT(x)			((D3DFORMAT)( ((unsigned int)(x)) & ~(D3DFORMAT_SIGNX_MASK | D3DFORMAT_SIGNY_MASK | D3DFORMAT_SIGNZ_MASK)))
#define IS_D3DFORMAT_SRGB( x )			( MAKESRGBFMT(x) == (x) )

typedef enum
{
	XEV_NULL,
	XEV_REMOTECMD,
	XEV_QUIT,
	XEV_LISTENER_NOTIFICATION,
} xevent_e;

typedef struct xevent_s
{
	xevent_e	event;
	int			arg1;
	int			arg2;
	int			arg3;
} xevent_t;

typedef enum
{
	XK_NULL,
	XK_BUTTON_UP,
	XK_BUTTON_DOWN,
	XK_BUTTON_LEFT,
	XK_BUTTON_RIGHT,
	XK_BUTTON_START,
	XK_BUTTON_BACK,
	XK_BUTTON_STICK1,
	XK_BUTTON_STICK2,
	XK_BUTTON_A,
	XK_BUTTON_B,
	XK_BUTTON_X,
	XK_BUTTON_Y,
	XK_BUTTON_LEFT_SHOULDER,
	XK_BUTTON_RIGHT_SHOULDER,
	XK_BUTTON_LTRIGGER,
	XK_BUTTON_RTRIGGER,
	XK_STICK1_UP,
	XK_STICK1_DOWN,
	XK_STICK1_LEFT,
	XK_STICK1_RIGHT,
	XK_STICK2_UP,
	XK_STICK2_DOWN,
	XK_STICK2_LEFT,
	XK_STICK2_RIGHT,
	XK_MAX_KEYS,
} xKey_t;

typedef struct
{
	const char	*pName;
	const char	*pGroupName;
	const char	*pFormatName;
	int			size;
	int			width;
	int			height;
	int			depth;
	int			numLevels;
	int			binds;
	int			refCount;
	int			sRGB;
	int			edram;
	int			procedural;
	int			fallback;
	int			final;
	int			failed;
} xTextureList_t;

typedef struct
{
	const char	*pName;
	const char	*pShaderName;
	int			refCount;
} xMaterialList_t;

typedef struct
{
	char		name[MAX_PATH];
	char		formatName[32];
	int			rate;
	int			bits;
	int			channels;
	int			looped;
	int			dataSize;
	int			numSamples;
	int			streamed;
} xSoundList_t;

typedef struct
{
	float	position[3];
	float	angle[3];
	char	mapPath[256];
	char	savePath[256];
	int		build;
	int		skill;
} xMapInfo_t;

/******************************************************************************
	XBOX_SYSTEM.CPP
******************************************************************************/
#if defined( PLATFORM_H )

// redirect debugging output through xbox debug channel
#define OutputDebugStringA		XBX_OutputDebugStringA

// Messages
PLATFORM_INTERFACE	void		XBX_Error( const char* format, ... );
PLATFORM_INTERFACE	void		XBX_OutputDebugStringA( LPCSTR lpOutputString );

// Event handling
PLATFORM_INTERFACE  bool		XBX_NotifyCreateListener( ULONG64 categories );
PLATFORM_INTERFACE	void		XBX_QueueEvent( xevent_e event, int arg1, int arg2, int arg3 );
PLATFORM_INTERFACE	void		XBX_ProcessEvents( void );

// Accessors
PLATFORM_INTERFACE	const char* XBX_GetLanguageString( void );
PLATFORM_INTERFACE	bool		XBX_IsLocalized( void );
PLATFORM_INTERFACE	DWORD		XBX_GetStorageDeviceId( void );
PLATFORM_INTERFACE	void		XBX_SetStorageDeviceId( DWORD id );
PLATFORM_INTERFACE	DWORD		XBX_GetPrimaryUserId( void );
PLATFORM_INTERFACE	void		XBX_SetPrimaryUserId( DWORD id );
PLATFORM_INTERFACE  XNKID		XBX_GetInviteSessionId( void );
PLATFORM_INTERFACE	void		XBX_SetInviteSessionId( XNKID nSessionId );
PLATFORM_INTERFACE  DWORD		XBX_GetInvitedUserId( void );
PLATFORM_INTERFACE	void		XBX_SetInvitedUserId( DWORD nUserId );

#endif