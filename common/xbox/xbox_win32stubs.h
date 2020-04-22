//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox win32 replacements - Mocks trivial windows flow
//
//=============================================================================
#pragma once

typedef unsigned long REGSAM;

#define DSBCAPS_LOCSOFTWARE		0

#define DSERR_BUFFERLOST		0

#define DSBSTATUS_BUFFERLOST	0x02

#define DSSPEAKER_GEOMETRY(x)	(((x)>>16) & 0xFFFF)
#define DSSPEAKER_CONFIG(x)		((x) & 0xFFFF)

#define DSSPEAKER_HEADPHONE		-1
#define DSSPEAKER_QUAD			-2
#define DSSPEAKER_5POINT1		-3
#define DSSPEAKER_7POINT1		-4 

#define DISP_CHANGE_SUCCESSFUL	0

#define HKEY_CURRENT_USER		NULL
#define HKEY_LOCAL_MACHINE		NULL
#define KEY_QUERY_VALUE			0

#define KEY_READ		0
#define KEY_WRITE		1
#define KEY_ALL_ACCESS	((ULONG)-1)

#define SMTO_ABORTIFHUNG		0

#define JOY_RETURNX	0x01
#define JOY_RETURNY	0x02
#define JOY_RETURNZ	0x04
#define JOY_RETURNR	0x08
#define JOY_RETURNU	0x10
#define JOY_RETURNV	0x20

#define JOYCAPS_HASPOV		0x01
#define JOYCAPS_HASU		0x01
#define JOYCAPS_HASV		0x01
#define JOYCAPS_HASR		0x01
#define JOYCAPS_HASZ		0x01

#define MMSYSERR_NODRIVER	1
#define JOYERR_NOERROR		0
#define	JOY_RETURNCENTERED	0
#define JOY_RETURNBUTTONS	0
#define	JOY_RETURNPOV		0
#define JOY_POVCENTERED		0
#define JOY_POVFORWARD		0
#define JOY_POVRIGHT		0
#define JOY_POVBACKWARD		0
#define JOY_POVLEFT			0

#define CCHDEVICENAME		32
#define CCHFORMNAME			32

typedef WCHAR BCHAR;

typedef UINT MMRESULT;

#define IDLE_PRIORITY_CLASS	1
#define HIGH_PRIORITY_CLASS 2

typedef struct _devicemode { 
  BCHAR  dmDeviceName[CCHDEVICENAME]; 
  WORD   dmSpecVersion; 
  WORD   dmDriverVersion; 
  WORD   dmSize; 
  WORD   dmDriverExtra; 
  DWORD  dmFields; 
  union u1 {
    struct s {
      short dmOrientation;
      short dmPaperSize;
      short dmPaperLength;
      short dmPaperWidth;
      short dmScale; 
      short dmCopies; 
      short dmDefaultSource; 
      short dmPrintQuality; 
    };
    POINTL dmPosition;
    DWORD  dmDisplayOrientation;
    DWORD  dmDisplayFixedOutput;
  };
  short  dmColor; 
  short  dmDuplex; 
  short  dmYResolution; 
  short  dmTTOption; 
  short  dmCollate; 
  BYTE  dmFormName[CCHFORMNAME]; 
  WORD  dmLogPixels; 
  DWORD  dmBitsPerPel; 
  DWORD  dmPelsWidth; 
  DWORD  dmPelsHeight; 
  union u2 {
    DWORD  dmDisplayFlags; 
    DWORD  dmNup;
  };
  DWORD  dmDisplayFrequency; 
  DWORD  dmICMMethod;
  DWORD  dmICMIntent;
  DWORD  dmMediaType;
  DWORD  dmDitherType;
  DWORD  dmReserved1;
  DWORD  dmReserved2;
  DWORD  dmPanningWidth;
  DWORD  dmPanningHeight;
} DEVMODE, *LPDEVMODE; 

typedef DWORD				MCIERROR;
typedef UINT				MCIDEVICEID;

typedef struct {
    DWORD_PTR dwCallback;  
} MCI_GENERIC_PARMS;

typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwReturn; 
    DWORD     dwItem; 
    DWORD     dwTrack; 
} MCI_STATUS_PARMS;
 
typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwFrom; 
    DWORD     dwTo; 
} MCI_PLAY_PARMS;

typedef struct {
    DWORD_PTR    dwCallback; 
    MCIDEVICEID  wDeviceID; 
    LPCSTR       lpstrDeviceType; 
    LPCSTR       lpstrElementName; 
    LPCSTR       lpstrAlias; 
} MCI_OPEN_PARMS; 

typedef struct {
    DWORD_PTR dwCallback; 
    DWORD     dwTimeFormat; 
    DWORD     dwAudio; 
} MCI_SET_PARMS;

#define MCI_MAKE_TMSF(t, m, s, f)	((DWORD)(((BYTE)(t) | ((WORD)(m) << 8)) | ((DWORD)(BYTE)(s) | ((WORD)(f)<<8)) << 16)) 
#define MCI_MSF_MINUTE(msf)			((BYTE)(msf)) 
#define MCI_MSF_SECOND(msf)			((BYTE)(((WORD)(msf)) >> 8)) 

#define MCI_OPEN					0
#define MCI_OPEN_TYPE				0
#define MCI_OPEN_SHAREABLE			0
#define MCI_FORMAT_TMSF				0
#define MCI_SET_TIME_FORMAT			0
#define MCI_CLOSE					0
#define MCI_STOP					0
#define MCI_PAUSE					0
#define MCI_PLAY					0
#define MCI_SET						0
#define MCI_SET_DOOR_OPEN			0
#define MCI_SET_DOOR_CLOSED			0
#define MCI_STATUS_READY			0
#define MCI_STATUS					0
#define MCI_STATUS_ITEM				0
#define MCI_STATUS_WAIT				0
#define MCI_STATUS_NUMBER_OF_TRACKS	0
#define MCI_CDA_STATUS_TYPE_TRACK	0
#define MCI_TRACK					0
#define MCI_WAIT					0
#define MCI_CDA_TRACK_AUDIO			0
#define MCI_STATUS_LENGTH			0
#define MCI_NOTIFY					0
#define MCI_FROM					0
#define MCI_TO						0
#define MCIERR_DRIVER				-1

#define	DSERR_ALLOCATED				0

typedef struct _STARTUPINFOW {
    DWORD   cb;
    LPWSTR  lpReserved;
    LPWSTR  lpDesktop;
    LPWSTR  lpTitle;
    DWORD   dwX;
    DWORD   dwY;
    DWORD   dwXSize;
    DWORD   dwYSize;
    DWORD   dwXCountChars;
    DWORD   dwYCountChars;
    DWORD   dwFillAttribute;
    DWORD   dwFlags;
    WORD    wShowWindow;
    WORD    cbReserved2;
    LPBYTE  lpReserved2;
    HANDLE  hStdInput;
    HANDLE  hStdOutput;
    HANDLE  hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;
typedef STARTUPINFOW STARTUPINFO;
typedef LPSTARTUPINFOW LPSTARTUPINFO;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef DWORD HWAVEOUT, *LPHWAVEOUT;

typedef struct { 
    LPSTR      lpData; 
    DWORD      dwBufferLength; 
    DWORD      dwBytesRecorded; 
    DWORD_PTR  dwUser; 
    DWORD      dwFlags; 
    DWORD      dwLoops; 
    struct wavehdr_tag * lpNext; 
    DWORD_PTR reserved; 
} WAVEHDR, *LPWAVEHDR; 

typedef struct { 
  DWORD  dwSize; 
  DWORD  dwFlags; 
  DWORD  dwBufferBytes; 
  DWORD  dwUnlockTransferRate; 
  DWORD  dwPlayCpuOverhead; 
} DSBCAPS, *LPDSBCAPS; 

typedef struct _DSCEFFECTDESC
{
    DWORD       dwSize;
    DWORD       dwFlags;
    GUID        guidDSCFXClass;
    GUID        guidDSCFXInstance;
    DWORD       dwReserved1;
    DWORD       dwReserved2;
} DSCEFFECTDESC, *LPDSCEFFECTDESC;

typedef struct _DSCBUFFERDESC
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    LPWAVEFORMATEX  lpwfxFormat;
    DWORD           dwFXCount;
    LPDSCEFFECTDESC lpDSCFXDesc;
} DSCBUFFERDESC, *LPDSCBUFFERDESC;


#define RGB(r,g,b)	((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (CALLBACK* WNDENUMPROC)(HWND, LPARAM);

typedef char* HPSTR;

typedef struct tagPAINTSTRUCT {
   HDC  hdc;
   BOOL fErase;
   RECT rcPaint;
   BOOL fRestore;
   BOOL fIncUpdate;
   BYTE rgbReserved[16];
} PAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagMSG {
    HWND        hwnd;
    UINT        message;
    WPARAM      wParam;
    LPARAM      lParam;
    DWORD       time;
    POINT       pt;
} MSG, *PMSG, NEAR *NPMSG, FAR *LPMSG;

typedef struct {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCTSTR lpszMenuName;
    LPCTSTR lpszClassName;
} WNDCLASS, *PWNDCLASS;

typedef struct {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
} WNDCLASSW, *PWNDCLASSW;

typedef struct {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCTSTR lpszMenuName;
    LPCTSTR lpszClassName;
    HICON hIconSm;
} WNDCLASSEX, *PWNDCLASSEX;

typedef struct tagTEXTMETRIC { 
  LONG tmHeight; 
  LONG tmAscent; 
  LONG tmDescent; 
  LONG tmInternalLeading; 
  LONG tmExternalLeading; 
  LONG tmAveCharWidth; 
  LONG tmMaxCharWidth; 
  LONG tmWeight; 
  LONG tmOverhang; 
  LONG tmDigitizedAspectX; 
  LONG tmDigitizedAspectY; 
  TCHAR tmFirstChar; 
  TCHAR tmLastChar; 
  TCHAR tmDefaultChar; 
  TCHAR tmBreakChar; 
  BYTE tmItalic; 
  BYTE tmUnderlined; 
  BYTE tmStruckOut; 
  BYTE tmPitchAndFamily; 
  BYTE tmCharSet; 
} TEXTMETRIC, *PTEXTMETRIC; 

typedef struct _ABC { 
  int     abcA; 
  UINT    abcB; 
  int     abcC; 
} ABC, *PABC; 

typedef struct tagRGBQUAD {
  BYTE    rgbBlue; 
  BYTE    rgbGreen; 
  BYTE    rgbRed; 
  BYTE    rgbReserved; 
} RGBQUAD; 

typedef struct tagBITMAP
  {
    LONG        bmType;
    LONG        bmWidth;
    LONG        bmHeight;
    LONG        bmWidthBytes;
    WORD        bmPlanes;
    WORD        bmBitsPixel;
    LPVOID      bmBits;
  } BITMAP, *PBITMAP, NEAR *NPBITMAP, FAR *LPBITMAP;

typedef struct tagBITMAPINFOHEADER{
  DWORD  biSize; 
  LONG   biWidth; 
  LONG   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount; 
  DWORD  biCompression; 
  DWORD  biSizeImage; 
  LONG   biXPelsPerMeter; 
  LONG   biYPelsPerMeter; 
  DWORD  biClrUsed; 
  DWORD  biClrImportant; 
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER; 

typedef struct tagBITMAPINFO { 
  BITMAPINFOHEADER bmiHeader; 
  RGBQUAD          bmiColors[1]; 
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO; 

typedef struct tagBITMAPFILEHEADER { 
  WORD    bfType; 
  DWORD   bfSize; 
  WORD    bfReserved1; 
  WORD    bfReserved2; 
  DWORD   bfOffBits; 
} BITMAPFILEHEADER, *PBITMAPFILEHEADER; 

typedef struct tagRGBTRIPLE { 
  BYTE rgbtBlue; 
  BYTE rgbtGreen; 
  BYTE rgbtRed; 
} RGBTRIPLE; 

typedef struct tagBITMAPCOREHEADER {
  DWORD   bcSize; 
  WORD    bcWidth; 
  WORD    bcHeight; 
  WORD    bcPlanes; 
  WORD    bcBitCount; 
} BITMAPCOREHEADER, *PBITMAPCOREHEADER; 

typedef struct _BITMAPCOREINFO { 
  BITMAPCOREHEADER  bmciHeader; 
  RGBTRIPLE         bmciColors[1]; 
} BITMAPCOREINFO, *PBITMAPCOREINFO, *LPBITMAPCOREINFO; 

typedef struct _OSVERSIONINFO 
{  
	DWORD dwOSVersionInfoSize;  
	DWORD dwMajorVersion;  
	DWORD dwMinorVersion;  
	DWORD dwBuildNumber;  
	DWORD dwPlatformId;  
	TCHAR szCSDVersion[128];
} OSVERSIONINFO, *LPOSVERSIONINFO;

typedef struct _OSVERSIONINFOEX 
{  
	DWORD dwOSVersionInfoSize;  
	DWORD dwMajorVersion;  
	DWORD dwMinorVersion;  
	DWORD dwBuildNumber;  
	DWORD dwPlatformId;  
	TCHAR szCSDVersion[128];  
	WORD wServicePackMajor;  
	WORD wServicePackMinor;  
	WORD wSuiteMask;  
	BYTE wProductType;  
	BYTE wReserved;
} OSVERSIONINFOEX,  *POSVERSIONINFOEX,  *LPOSVERSIONINFOEX;

typedef enum {
  INTERNET_SCHEME_PARTIAL = -2,
  INTERNET_SCHEME_UNKNOWN = -1,
  INTERNET_SCHEME_DEFAULT = 0,
  INTERNET_SCHEME_FTP,
  INTERNET_SCHEME_GOPHER,
  INTERNET_SCHEME_HTTP,
  INTERNET_SCHEME_HTTPS,
  INTERNET_SCHEME_FILE,
  INTERNET_SCHEME_NEWS,
  INTERNET_SCHEME_MAILTO,
  INTERNET_SCHEME_SOCKS,
  INTERNET_SCHEME_JAVASCRIPT,
  INTERNET_SCHEME_VBSCRIPT,
  INTERNET_SCHEME_ABOUT,
  INTERNET_SCHEME_RES,
  INTERNET_SCHEME_FIRST = INTERNET_SCHEME_FTP,
  INTERNET_SCHEME_LAST = INTERNET_SCHEME_VBSCRIPT
} INTERNET_SCHEME, * LPINTERNET_SCHEME;

typedef struct {
  DWORD dwStructSize;
  LPTSTR lpszScheme;
  DWORD dwSchemeLength;
  INTERNET_SCHEME nScheme;
  LPTSTR lpszHostName;
  DWORD dwHostNameLength;
  UINT nPort;
  LPTSTR lpszUserName;
  DWORD dwUserNameLength;
  LPTSTR lpszPassword;
  DWORD dwPasswordLength;
  LPTSTR lpszUrlPath;
  DWORD dwUrlPathLength;
  LPTSTR lpszExtraInfo;
  DWORD dwExtraInfoLength;
} URL_COMPONENTS, *LPURL_COMPONENTS;

typedef struct _COORD 
{  
	SHORT X;  
	SHORT Y;
} COORD,  *PCOORD;

typedef struct _SMALL_RECT 
{  
	SHORT Left;  
	SHORT Top;  
	SHORT Right;  
	SHORT Bottom;
} SMALL_RECT;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO 
{  
	COORD dwSize;  
	COORD dwCursorPosition;  
	WORD wAttributes;  
	SMALL_RECT srWindow;  
	COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;

typedef struct _WINDOW_BUFFER_SIZE_RECORD 
{
    COORD dwSize;
} WINDOW_BUFFER_SIZE_RECORD, *PWINDOW_BUFFER_SIZE_RECORD;

typedef struct _MENU_EVENT_RECORD 
{
    UINT dwCommandId;
} MENU_EVENT_RECORD, *PMENU_EVENT_RECORD;

typedef struct _FOCUS_EVENT_RECORD 
{
    BOOL bSetFocus;
} FOCUS_EVENT_RECORD, *PFOCUS_EVENT_RECORD;

typedef struct _KEY_EVENT_RECORD 
{
    BOOL bKeyDown;
    WORD wRepeatCount;
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
    union {
        WCHAR UnicodeChar;
        CHAR   AsciiChar;
    } uChar;
    DWORD dwControlKeyState;
} KEY_EVENT_RECORD, *PKEY_EVENT_RECORD;

typedef struct _MOUSE_EVENT_RECORD 
{
    COORD dwMousePosition;
    DWORD dwButtonState;
    DWORD dwControlKeyState;
    DWORD dwEventFlags;
} MOUSE_EVENT_RECORD, *PMOUSE_EVENT_RECORD;

typedef struct _INPUT_RECORD 
{
    WORD EventType;
    union {
        KEY_EVENT_RECORD KeyEvent;
        MOUSE_EVENT_RECORD MouseEvent;
        WINDOW_BUFFER_SIZE_RECORD WindowBufferSizeEvent;
        MENU_EVENT_RECORD MenuEvent;
        FOCUS_EVENT_RECORD FocusEvent;
    } Event;
} INPUT_RECORD, *PINPUT_RECORD;

typedef GUID UUID;

#define MAXPNAMELEN 32
#define MAX_JOYSTICKOEMVXDNAME 260

typedef struct 
{ 
    WORD wMid; 
    WORD wPid; 
    CHAR szPname[MAXPNAMELEN]; 
    UINT wXmin; 
    UINT wXmax; 
    UINT wYmin; 
    UINT wYmax; 
    UINT wZmin; 
    UINT wZmax; 
    UINT wNumButtons; 
    UINT wPeriodMin; 
    UINT wPeriodMax; 
    UINT wRmin; 
    UINT wRmax; 
    UINT wUmin; 
    UINT wUmax; 
    UINT wVmin; 
    UINT wVmax; 
    UINT wCaps; 
    UINT wMaxAxes; 
    UINT wNumAxes; 
    UINT wMaxButtons; 
    CHAR szRegKey[MAXPNAMELEN]; 
    CHAR szOEMVxD[MAX_JOYSTICKOEMVXDNAME]; 
} JOYCAPS, *LPJOYCAPS; 

typedef struct joyinfoex_tag 
{ 
    DWORD dwSize; 
    DWORD dwFlags; 
    DWORD dwXpos; 
    DWORD dwYpos; 
    DWORD dwZpos; 
    DWORD dwRpos; 
    DWORD dwUpos; 
    DWORD dwVpos; 
    DWORD dwButtons; 
    DWORD dwButtonNumber; 
    DWORD dwPOV; 
    DWORD dwReserved1; 
    DWORD dwReserved2; 
} JOYINFOEX, *LPJOYINFOEX; 

typedef struct _MEMORYSTATUSEX 
{
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;

typedef struct tagCOPYDATASTRUCT 
{
    ULONG_PTR dwData;
    DWORD cbData;
    PVOID lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT;

typedef LPVOID	HINTERNET;

typedef VOID (CALLBACK * INTERNET_STATUS_CALLBACK)(
    IN HINTERNET hInternet,
    IN DWORD_PTR dwContext,
    IN DWORD dwInternetStatus,
    IN LPVOID lpvStatusInformation OPTIONAL,
    IN DWORD dwStatusInformationLength
    );

typedef struct 
{
    DWORD   dwStructSize;       // size of this structure. Used in version check
    LPSTR   lpszScheme;         // pointer to scheme name
    DWORD   dwSchemeLength;     // length of scheme name
    INTERNET_SCHEME nScheme;    // enumerated scheme type (if known)
    LPSTR   lpszHostName;       // pointer to host name
    DWORD   dwHostNameLength;   // length of host name
    UINT	nPort;        // converted port number
    LPSTR   lpszUserName;       // pointer to user name
    DWORD   dwUserNameLength;   // length of user name
    LPSTR   lpszPassword;       // pointer to password
    DWORD   dwPasswordLength;   // length of password
    LPSTR   lpszUrlPath;        // pointer to URL-path
    DWORD   dwUrlPathLength;    // length of URL-path
    LPSTR   lpszExtraInfo;      // pointer to extra information (e.g. ?foo or #foo)
    DWORD   dwExtraInfoLength;  // length of extra information
} URL_COMPONENTSA, * LPURL_COMPONENTSA;

struct  hostent 
{
        char    FAR * h_name;           /* official name of host */
        char    FAR * FAR * h_aliases;  /* alias list */
        short   h_addrtype;             /* host address type */
        short   h_length;               /* length of address */
        char    FAR * FAR * h_addr_list; /* list of addresses */
#define h_addr  h_addr_list[0]          /* address, for backward compat */
};

#define WHEEL_DELTA				120

#define ANSI_CHARSET			0
#define SYMBOL_CHARSET			1

#define NONANTIALIASED_QUALITY	0
#define ANTIALIASED_QUALITY		4

#define SPI_SETMOUSE			1
#define SPI_GETMOUSE			2

#define SC_SCREENSAVE			0
#define SC_CLOSE				1
#define SC_KEYMENU				2
#define SC_MONITORPOWER			3

#define SIZE_MINIMIZED			0

#define DM_PELSWIDTH			0
#define DM_PELSHEIGHT			0
#define DM_BITSPERPEL			0
#define DM_DISPLAYFREQUENCY		0

#define CDS_FULLSCREEN			0

#define FILE_TYPE_UNKNOWN		0
#define FILE_TYPE_DISK			1

#define HORZRES					1
#define VERTRES					2
#define VREFRESH				3

#define FILE_MAP_ALL_ACCESS		0
#define FILE_MAP_COPY			1
#define FILE_MAP_WRITE			2
#define FILE_MAP_READ			3

#define PBT_APMQUERYSUSPEND		0
#define BROADCAST_QUERY_DENY	0x424D5144

#define IDOK					0
#define IDCANCEL				1

#define IMAGE_ICON				0
#define MB_ICONEXCLAMATION		1
#define MB_OKCANCEL				2
#define MB_SYSTEMMODAL			3
#define MB_ICONERROR			4

#define LR_DEFAULTCOLOR     0x0000
#define LR_MONOCHROME       0x0001
#define LR_COLOR            0x0002
#define LR_COPYRETURNORG    0x0004
#define LR_COPYDELETEORG    0x0008
#define LR_LOADFROMFILE     0x0010
#define LR_LOADTRANSPARENT  0x0020
#define LR_DEFAULTSIZE      0x0040
#define LR_VGACOLOR         0x0080
#define LR_LOADMAP3DCOLORS  0x1000
#define LR_CREATEDIBSECTION 0x2000
#define LR_COPYFROMRESOURCE 0x4000
#define LR_SHARED           0x8000

#define MAKEINTRESOURCE( res )	((ULONG_PTR) (USHORT) res)
#define CREATE_NEW_CONSOLE		0x00000010

// registry
#define REG_OPTION_NON_VOLATILE		0ul
#define REG_CREATED_NEW_KEY			1
#define HKEY_CLASSES_ROOT			(HKEY)0

// winsock
#define MSG_NOSIGNAL			0
#define MSG_PEEK				2

// show styles
#define SW_SHOWNORMAL			0
#define SW_SHOWDEFAULT			1
#define SW_SHOW					2
#define SW_MINIMIZE				3

#define SWP_NOZORDER			0x00000001
#define SWP_NOREDRAW			0x00000002
#define SWP_NOSIZE				0x00000004
#define SWP_NOMOVE				0x00000008
#define SWP_SHOWWINDOW			0x00000010
#define SWP_DRAWFRAME			0x00000020

// platform versions
#define VER_PLATFORM_WIN32s			0
#define VER_PLATFORM_WIN32_WINDOWS	1
#define VER_PLATFORM_WIN32_NT		2

// windows messages
#define WM_CHAR						1
#define WM_CLOSE					2
#define WM_DESTROY					3
#define WM_MOUSEMOVE				4
#define WM_LBUTTONUP				5
#define WM_LBUTTONDOWN				6
#define WM_RBUTTONUP				7
#define WM_RBUTTONDOWN				8
#define WM_SETFOCUS					9
#define WM_SETCURSOR				10
#define WM_MBUTTONDOWN				11
#define WM_MBUTTONUP				12
#define WM_LBUTTONDBLCLK			13
#define WM_RBUTTONDBLCLK			14
#define WM_MBUTTONDBLCLK			15
#define WM_MOUSEWHEEL				16
#define WM_KEYDOWN					17
#define WM_SYSKEYDOWN				18
#define WM_SYSCHAR					19
#define WM_KEYUP					20
#define WM_SYSKEYUP					21
#define WM_PAINT					23
#define WM_COPYDATA					24
#define WM_MOVE						25
#define WM_ACTIVATEAPP				26
#define WM_QUIT						27
#define WM_CREATE					28
#define WM_SYSCOMMAND				29
#define WM_SIZE						30
#define WM_SETTINGCHANGE			31
#define WM_USER						32
#define WM_POWERBROADCAST			33
#define WM_IME_CHAR					34
#define WM_IME_NOTIFY				35
#define WM_IME_STARTCOMPOSITION		36
#define WM_IME_COMPOSITION			37
#define WM_IME_ENDCOMPOSITION		38
#define	WM_IME_SETCONTEXT			39
#define WM_INPUTLANGCHANGE			40

#define IMN_OPENCANDIDATE			0
#define IMN_SETOPENSTATUS			1
#define IMN_CHANGECANDIDATE			2
#define IMN_CLOSECANDIDATE			3
#define IMN_SETCONVERSIONMODE		4
#define	IMN_SETSENTENCEMODE			5
#define IMN_CLOSESTATUSWINDOW		6
#define IMN_GUIDELINE				7
#define IMN_OPENSTATUSWINDOW		8
#define IMN_SETCANDIDATEPOS			9
#define IMN_SETCOMPOSITIONFONT		10
#define IMN_SETCOMPOSITIONWINDOW	11
#define IMN_SETSTATUSWINDOWPOS		12

#define ISC_SHOWUICOMPOSITIONWINDOW		0
#define ISC_SHOWUIGUIDELINE				0
#define ISC_SHOWUIALLCANDIDATEWINDOW	0

// message box
#define MB_OK					0
#define MB_ICONINFORMATION		0
#define	MB_TOPMOST				0
#define SEM_NOGPFAULTERRORBOX   2

// class styles
#define CS_OWNDC				0
#define CS_DBLCLKS				0	
#define CS_CLASSDC				0
#define CS_HREDRAW				0
#define CS_VREDRAW				0

#define IDC_ARROW				0

#define STD_INPUT_HANDLE		((DWORD)-10)
#define STD_OUTPUT_HANDLE		((DWORD)-11)

#define COLOR_GRAYTEXT			0
#define WHITE_BRUSH				0
#define SRCCOPY					0

/* Font Weights */
#define FW_DONTCARE				0
#define FW_THIN				    100
#define FW_EXTRALIGHT			200
#define FW_LIGHT				300
#define FW_NORMAL				400
#define FW_MEDIUM				500
#define FW_SEMIBOLD				600
#define FW_BOLD					700
#define FW_EXTRABOLD			800
#define FW_HEAVY				900

#define CLIP_DEFAULT_PRECIS		0
#define DEFAULT_PITCH           0
#define TRANSPARENT				1
#define OUT_TT_PRECIS           4
#define BI_RGB					0L
#define IMAGE_BITMAP			0

#define DT_NOPREFIX             0x00000000
#define DT_VCENTER              0x00000000
#define DT_CENTER               0x00000000
#define DT_LEFT					0x00000000
#define DT_RIGHT				0x00000000
#define DT_SINGLELINE           0x00000000

#define DIB_RGB_COLORS			0

// window styles
#define WS_OVERLAPPEDWINDOW		0
#define WS_POPUP				0
#define WS_CLIPSIBLINGS			0
#define WS_THICKFRAME			0
#define WS_MAXIMIZEBOX			0
#define WS_VISIBLE				0
#define WS_EX_TOOLWINDOW		0
#define WS_EX_TOPMOST			0
#define WS_CAPTION				0
#define WS_SYSMENU				0
#define WS_CLIPCHILDREN			0

// cursors
#define OCR_NORMAL				1
#define OCR_IBEAM				2
#define OCR_WAIT				3
#define OCR_CROSS				4
#define OCR_UP					5
#define OCR_SIZENWSE			6
#define OCR_SIZENESW			7
#define OCR_SIZEWE				8
#define OCR_SIZENS				9
#define OCR_SIZEALL				10
#define OCR_NO					11
#define OCR_HAND				12

// system metrics
#define SM_CXFIXEDFRAME			1
#define SM_CYFIXEDFRAME			2
#define SM_CYSIZE				3
#define SM_CXSCREEN				4
#define SM_CYSCREEN				5

// window longs
#define GWLP_WNDPROC			(-4)
#define GWLP_HINSTANCE			(-6)
#define GWLP_HWNDPARENT			(-8)
#define GWLP_USERDATA			(-21)
#define GWLP_ID					(-12)

#define GWL_WNDPROC				0
#define GWL_USERDATA			1
#define GWL_STYLE				2
#define GWL_EXSTYLE				3
#define GWL_MAX					4

#define HWND_TOP        ((HWND)0)
#define HWND_BOTTOM     ((HWND)1)
#define HWND_TOPMOST    ((HWND)-1)
#define HWND_NOTOPMOST  ((HWND)-2)
#define HWND_BROADCAST	0

// PeekMessage
#define PM_NOREMOVE         0x0000
#define PM_REMOVE           0x0001
#define PM_NOYIELD          0x0002

#define MK_LBUTTON				0x0001
#define MK_RBUTTON				0x0002
#define MK_MBUTTON				0x0004

// File attributes
#define FILE_ATTRIBUTE_COMPRESSED	0x00000800  

#define QS_INPUT		0
#define QS_ALLEVENTS	5
#define KEY_EVENT		0

// sockets
#define	SO_KEEPALIVE		8

// Download status cases
#define INTERNET_STATUS_RESOLVING_NAME			0
#define INTERNET_STATUS_NAME_RESOLVED			1
#define INTERNET_STATUS_CONNECTING_TO_SERVER	2
#define INTERNET_STATUS_CONNECTED_TO_SERVER		3
#define INTERNET_STATUS_SENDING_REQUEST			4
#define INTERNET_STATUS_REQUEST_SENT			5
#define INTERNET_STATUS_REQUEST_COMPLETE		6
#define INTERNET_STATUS_CLOSING_CONNECTION		7
#define INTERNET_STATUS_CONNECTION_CLOSED		8
#define INTERNET_STATUS_RECEIVING_RESPONSE		9
#define INTERNET_STATUS_RESPONSE_RECEIVED		10
#define INTERNET_STATUS_HANDLE_CLOSING			11
#define INTERNET_STATUS_HANDLE_CREATED			12
#define INTERNET_STATUS_INTERMEDIATE_RESPONSE	13
#define INTERNET_STATUS_REDIRECT				14
#define INTERNET_STATUS_STATE_CHANGE			15

#define INTERNET_FLAG_RELOAD            0x80000000  // retrieve the original item
#define INTERNET_FLAG_RAW_DATA          0x40000000  // FTP/gopher find: receive the item as raw (structured) data
#define INTERNET_FLAG_EXISTING_CONNECT  0x20000000  // FTP: use existing InternetConnect handle for server if possible
#define INTERNET_FLAG_ASYNC             0x10000000  // this request is asynchronous (where supported)
#define INTERNET_FLAG_PASSIVE           0x08000000  // used for FTP connections
#define INTERNET_FLAG_NO_CACHE_WRITE    0x04000000  // don't write this item to the cache
#define INTERNET_FLAG_DONT_CACHE        INTERNET_FLAG_NO_CACHE_WRITE
#define INTERNET_FLAG_MAKE_PERSISTENT   0x02000000  // make this item persistent in cache
#define INTERNET_FLAG_FROM_CACHE        0x01000000  // use offline semantics
#define INTERNET_FLAG_OFFLINE           INTERNET_FLAG_FROM_CACHE
#define INTERNET_FLAG_SECURE            0x00800000  // use PCT/SSL if applicable (HTTP)
#define INTERNET_FLAG_KEEP_CONNECTION   0x00400000  // use keep-alive semantics
#define INTERNET_FLAG_NO_AUTO_REDIRECT  0x00200000  // don't handle redirections automatically
#define INTERNET_FLAG_READ_PREFETCH     0x00100000  // do background read prefetch
#define INTERNET_FLAG_NO_COOKIES        0x00080000  // no automatic cookie handling
#define INTERNET_FLAG_NO_AUTH           0x00040000  // no automatic authentication handling
#define INTERNET_FLAG_RESTRICTED_ZONE   0x00020000  // apply restricted zone policies for cookies, auth
#define INTERNET_FLAG_CACHE_IF_NET_FAIL 0x00010000  // return cache file if net request fails
#define INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP   0x00008000 // ex: https:// to http://
#define INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS  0x00004000 // ex: http:// to https://
#define INTERNET_FLAG_IGNORE_CERT_DATE_INVALID  0x00002000 // expired X509 Cert.
#define INTERNET_FLAG_IGNORE_CERT_CN_INVALID    0x00001000 // bad common name in X509 Cert.

#define INTERNET_OPEN_TYPE_PRECONFIG                    0   // use registry configuration
#define INTERNET_OPEN_TYPE_DIRECT                       1   // direct to net
#define INTERNET_OPEN_TYPE_PROXY                        3   // via named proxy
#define INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY  4   // prevent using java/script/INS

#define HTTP_QUERY_CONTENT_LENGTH               5
#define HTTP_QUERY_LAST_MODIFIED                11
#define HTTP_QUERY_STATUS_CODE                  19  // special: part of status line
#define HTTP_QUERY_RAW_HEADERS_CRLF             22  // special: all headers
#define HTTP_QUERY_FLAG_NUMBER                  0x20000000
#define HTTP_STATUS_OK							200 // request completed
#define HTTP_STATUS_PARTIAL_CONTENT				206 // partial GET furfilled

// Virtual Keys, Standard Set
#define VK_LBUTTON				0x01
#define VK_RBUTTON				0x02
#define VK_CANCEL				0x03
#define VK_MBUTTON				0x04    /* NOT contiguous with L & RBUTTON */
#define VK_XBUTTON1				0x05    /* NOT contiguous with L & RBUTTON */
#define VK_XBUTTON2				0x06    /* NOT contiguous with L & RBUTTON */
#define VK_BACK					0x08
#define VK_TAB					0x09
#define VK_CLEAR				0x0C
#define VK_RETURN				0x0D
#define VK_SHIFT				0x10
#define VK_CONTROL				0x11
#define VK_MENU					0x12
#define VK_PAUSE				0x13
#define VK_CAPITAL				0x14
#define VK_KANA					0x15
#define VK_HANGEUL				0x15  /* old name - should be here for compatibility */
#define VK_HANGUL				0x15
#define VK_JUNJA				0x17
#define VK_FINAL				0x18
#define VK_HANJA				0x19
#define VK_KANJI				0x19
#define VK_ESCAPE				0x1B
#define VK_CONVERT				0x1C
#define VK_NONCONVERT			0x1D
#define VK_ACCEPT				0x1E
#define VK_MODECHANGE			0x1F
#define VK_SPACE				0x20
#define VK_PRIOR				0x21
#define VK_NEXT					0x22
#define VK_END					0x23
#define VK_HOME					0x24
#define VK_LEFT					0x25
#define VK_UP					0x26
#define VK_RIGHT				0x27
#define VK_DOWN					0x28
#define VK_SELECT				0x29
#define VK_PRINT				0x2A
#define VK_EXECUTE				0x2B
#define VK_SNAPSHOT				0x2C
#define VK_INSERT				0x2D
#define VK_DELETE				0x2E
#define VK_HELP					0x2F
#define VK_LWIN					0x5B
#define VK_RWIN					0x5C
#define VK_APPS					0x5D
#define VK_SLEEP				0x5F
#define VK_NUMPAD0				0x60
#define VK_NUMPAD1				0x61
#define VK_NUMPAD2				0x62
#define VK_NUMPAD3				0x63
#define VK_NUMPAD4				0x64
#define VK_NUMPAD5				0x65
#define VK_NUMPAD6				0x66
#define VK_NUMPAD7				0x67
#define VK_NUMPAD8				0x68
#define VK_NUMPAD9				0x69
#define VK_MULTIPLY				0x6A
#define VK_ADD					0x6B
#define VK_SEPARATOR			0x6C
#define VK_SUBTRACT				0x6D
#define VK_DECIMAL				0x6E
#define VK_DIVIDE				0x6F
#define VK_F1					0x70
#define VK_F2					0x71
#define VK_F3					0x72
#define VK_F4					0x73
#define VK_F5					0x74
#define VK_F6					0x75
#define VK_F7					0x76
#define VK_F8					0x77
#define VK_F9					0x78
#define VK_F10					0x79
#define VK_F11					0x7A
#define VK_F12					0x7B
#define VK_F13					0x7C
#define VK_F14					0x7D
#define VK_F15					0x7E
#define VK_F16					0x7F
#define VK_F17					0x80
#define VK_F18					0x81
#define VK_F19					0x82
#define VK_F20					0x83
#define VK_F21					0x84
#define VK_F22					0x85
#define VK_F23					0x86
#define VK_F24					0x87
#define VK_NUMLOCK				0x90
#define VK_SCROLL				0x91
#define VK_OEM_NEC_EQUAL		0x92   // '=' key on numpad
#define VK_OEM_FJ_JISHO			0x92   // 'Dictionary' key
#define VK_OEM_FJ_MASSHOU		0x93   // 'Unregister word' key
#define VK_OEM_FJ_TOUROKU		0x94   // 'Register word' key
#define VK_OEM_FJ_LOYA			0x95   // 'Left OYAYUBI' key
#define VK_OEM_FJ_ROYA			0x96   // 'Right OYAYUBI' key
#define VK_LSHIFT				0xA0
#define VK_RSHIFT				0xA1
#define VK_LCONTROL				0xA2
#define VK_RCONTROL				0xA3
#define VK_LMENU				0xA4
#define VK_RMENU				0xA5
#define VK_BROWSER_BACK			0xA6
#define VK_BROWSER_FORWARD		0xA7
#define VK_BROWSER_REFRESH		0xA8
#define VK_BROWSER_STOP			0xA9
#define VK_BROWSER_SEARCH		0xAA
#define VK_BROWSER_FAVORITES	0xAB
#define VK_BROWSER_HOME			0xAC
#define VK_VOLUME_MUTE			0xAD
#define VK_VOLUME_DOWN			0xAE
#define VK_VOLUME_UP			0xAF
#define VK_MEDIA_NEXT_TRACK		0xB0
#define VK_MEDIA_PREV_TRACK		0xB1
#define VK_MEDIA_STOP			0xB2
#define VK_MEDIA_PLAY_PAUSE		0xB3
#define VK_LAUNCH_MAIL			0xB4
#define VK_LAUNCH_MEDIA_SELECT	0xB5
#define VK_LAUNCH_APP1			0xB6
#define VK_LAUNCH_APP2			0xB7
#define VK_OEM_1				0xBA   // ';:' for US
#define VK_OEM_PLUS				0xBB   // '+' any country
#define VK_OEM_COMMA			0xBC   // ',' any country
#define VK_OEM_MINUS			0xBD   // '-' any country
#define VK_OEM_PERIOD			0xBE   // '.' any country
#define VK_OEM_2				0xBF   // '/?' for US
#define VK_OEM_3				0xC0   // '`~' for US
#define VK_OEM_4				0xDB  //  '[{' for US
#define VK_OEM_5				0xDC  //  '\|' for US
#define VK_OEM_6				0xDD  //  ']}' for US
#define VK_OEM_7				0xDE  //  ''"' for US
#define VK_OEM_8				0xDF
#define VK_OEM_AX				0xE1  //  'AX' key on Japanese AX kbd
#define VK_OEM_102				0xE2  //  "<>" or "\|" on RT 102-key kbd.
#define VK_ICO_HELP				0xE3  //  Help key on ICO
#define VK_ICO_00				0xE4  //  00 key on ICO
#define VK_PROCESSKEY			0xE5
#define VK_ICO_CLEAR			0xE6
#define VK_PACKET				0xE7
#define VK_OEM_RESET			0xE9
#define VK_OEM_JUMP				0xEA
#define VK_OEM_PA1				0xEB
#define VK_OEM_PA2				0xEC
#define VK_OEM_PA3				0xED
#define VK_OEM_WSCTRL			0xEE
#define VK_OEM_CUSEL			0xEF
#define VK_OEM_ATTN				0xF0
#define VK_OEM_FINISH			0xF1
#define VK_OEM_COPY				0xF2
#define VK_OEM_AUTO				0xF3
#define VK_OEM_ENLW				0xF4
#define VK_OEM_BACKTAB			0xF5
#define VK_ATTN					0xF6
#define VK_CRSEL				0xF7
#define VK_EXSEL				0xF8
#define VK_EREOF				0xF9
#define VK_PLAY					0xFA
#define VK_ZOOM					0xFB
#define VK_NONAME				0xFC
#define VK_PA1					0xFD
#define VK_OEM_CLEAR			0xFE

// Error codes used in the Secure CRT functions 
#define EINVAL          22


#ifdef getenv
	#undef getenv
#endif
#ifdef _getenv
	#undef _getenv
#endif
#define getenv		XBX_getenv
#define _getenv		XBX_getenv
FORCEINLINE char *XBX_getenv(const char *name) { return NULL; }

#ifdef _putenv
	#undef _putenv
#endif
#define _putenv		XBX_putenv
FORCEINLINE int XBX_putenv(const char *name) { return -1; }

#ifdef GetEnvironmentVariable
	#undef GetEnvironmentVariable
#endif
#define GetEnvironmentVariable XBX_GetEnvironmentVariable
FORCEINLINE DWORD XBX_GetEnvironmentVariable( LPCTSTR lpName, LPTSTR lpBuffer, DWORD nSize ) { return 0; }

#ifdef unlink
	#undef unlink
#endif
#define unlink XBX_unlink
PLATFORM_INTERFACE int XBX_unlink( const char* filename );

#ifdef mkdir
	#undef mkdir
#endif
#ifdef _mkdir
	#undef _mkdir
#endif
#define mkdir XBX_mkdir
#define _mkdir XBX_mkdir
PLATFORM_INTERFACE int XBX_mkdir( const char *pszDir );

#ifdef getcwd
	#undef getcwd
#endif
#ifdef _getcwd
	#undef _getcwd
#endif
#define getcwd XBX_getcwd
#define _getcwd XBX_getcwd
PLATFORM_INTERFACE char *XBX_getcwd( char *buf, size_t size );

#ifdef GetCurrentDirectory
	#undef GetCurrentDirectory
#endif
#define GetCurrentDirectory XBX_GetCurrentDirectory
PLATFORM_INTERFACE DWORD XBX_GetCurrentDirectory( DWORD nBufferLength, LPTSTR lpBuffer );

#ifdef _access
	#undef _access
#endif
#define _access XBX_access
PLATFORM_INTERFACE int XBX_access( const char *path, int mode );

#ifdef _chdir
	#undef _chdir
#endif
#define _chdir XBX_chdir
FORCEINLINE int XBX_chdir(  const char *dirname ) { return -1; }

FORCEINLINE BOOL SetPriorityClass( HANDLE hProcess, DWORD dwPriorityClass ) { return FALSE; }

PLATFORM_INTERFACE int MessageBox( HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType );

#ifdef GetModuleFileName
	#undef GetModuleFileName
#endif
#define GetModuleFileName XBX_GetModuleFileName
PLATFORM_INTERFACE DWORD XBX_GetModuleFileName( HMODULE hModule, LPTSTR lpFilename, DWORD nSize );

//FORCEINLINE int WSAStartup( WORD wVersionRequested, LPWSADATA lpWSAData ) { return WSASYSNOTREADY; }
//FORCEINLINE int WSACleanup(void) { return WSANOTINITIALISED; }

FORCEINLINE HRESULT CoInitialize( LPVOID pvReserved ) { return S_OK; }
FORCEINLINE void CoUninitialize( void ) { }

FORCEINLINE LRESULT	DefWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) { return 0L; }
FORCEINLINE void PostQuitMessage(int nExitCode) { }

FORCEINLINE HANDLE GetStdHandle( DWORD ) { return 0; }

FORCEINLINE BOOL GetConsoleScreenBufferInfo( HANDLE, PCONSOLE_SCREEN_BUFFER_INFO ) { return false; }

FORCEINLINE COORD GetLargestConsoleWindowSize( HANDLE ) { COORD c = { 0, 0 }; return c; }

FORCEINLINE BOOL SetConsoleWindowInfo( HANDLE, BOOL, SMALL_RECT* ) { return false; }

FORCEINLINE BOOL SetConsoleScreenBufferSize( HANDLE, COORD ) { return false; }

FORCEINLINE BOOL ReadConsoleOutputCharacter( HANDLE, LPTSTR, DWORD, COORD, LPDWORD ) { return false; }

FORCEINLINE BOOL WriteConsoleInput( HANDLE, CONST INPUT_RECORD*, DWORD, LPDWORD ) { return false; }

FORCEINLINE HWND GetDesktopWindow(VOID) { return (HWND)0; }

FORCEINLINE int GetWindowText( HWND, LPTSTR, int ) { return 0; }

FORCEINLINE UINT RegisterWindowMessage(LPCTSTR lpString) { return 0xC000; }

FORCEINLINE HWND FindWindow(LPCTSTR lpClassName, LPCTSTR lpWindowName) { return NULL; }

FORCEINLINE BOOL EnumWindows(WNDENUMPROC lpEnumFunc, LPARAM lParam) { return FALSE; }

FORCEINLINE BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return FALSE; }

FORCEINLINE BOOL SystemParametersInfo(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) { return FALSE; }

FORCEINLINE BOOL SetForegroundWindow(HWND hWnd) { return TRUE; }

FORCEINLINE HDC	BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) { return NULL; }

FORCEINLINE BOOL EndPaint(HWND hWnd, CONST PAINTSTRUCT *lpPaint) { return TRUE; }

FORCEINLINE BOOL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle) { return TRUE; }

FORCEINLINE LONG ChangeDisplaySettings(LPDEVMODE lpDevMode, DWORD dwflags) { return DISP_CHANGE_SUCCESSFUL; }

FORCEINLINE DWORD GetFileType(HANDLE hFile) { return FILE_TYPE_DISK; }

FORCEINLINE BOOL FileTimeToDosDateTime(const FILETIME* lpFileTime, LPWORD lpFatDate, LPWORD lpFatTime)
{
	*lpFatDate = 0;
	*lpFatTime = 0;
	return TRUE;
}

FORCEINLINE BOOL DosDateTimeToFileTime(const WORD wFatDate, const WORD wFatTime, LPFILETIME lpFileTime )
{
	lpFileTime->dwHighDateTime = 0;
	lpFileTime->dwLowDateTime = 0;
	return TRUE;
}

FORCEINLINE BOOL SetViewportOrgEx( HDC, int, int, LPPOINT ) { return false; }

FORCEINLINE BOOL MoveWindow( HWND, int, int, int, int, BOOL ) { return false; }

FORCEINLINE int ShowCursor( BOOL ) { return 0; }

FORCEINLINE HFONT CreateFontA( int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return 0; }

FORCEINLINE int DrawText( HDC, LPCTSTR, int, LPRECT, UINT ) { return 0; }

FORCEINLINE int SetBkMode( HDC, int ) { return 0; }

FORCEINLINE COLORREF SetTextColor( HDC, COLORREF col ) { return col; }

FORCEINLINE HBRUSH CreateSolidBrush( COLORREF ) { return 0; }

FORCEINLINE BOOL Rectangle( HDC, int, int, int, int ) { return false; }

FORCEINLINE HANDLE LoadImage( HINSTANCE, LPCSTR, UINT, int, int, UINT) { return 0; }
FORCEINLINE HANDLE LoadImageA( HINSTANCE, LPCSTR, UINT, int, int, UINT) { return 0; }

FORCEINLINE HICON LoadIcon( HINSTANCE hInstance, ULONG_PTR lpIconName) { return 0; }

FORCEINLINE COLORREF SetPixel( HDC, int, int, COLORREF col ) { return col; }

FORCEINLINE BOOL BitBlt( HDC, int, int, int, int, HDC, int, int, DWORD ) { return false; }

FORCEINLINE HGDIOBJ GetStockObject( int ) { return 0; }

FORCEINLINE int GetObject( HGDIOBJ, int, LPVOID ) { return 0; }

FORCEINLINE int GetDIBits( HDC, HBITMAP, UINT, UINT, LPVOID, LPBITMAPINFO, UINT ) { return 0; }

FORCEINLINE HDC GetDC(HWND hWnd) { return (HDC)0x12345678; }

FORCEINLINE void ReleaseDC(HWND hWnd, HDC hDC) { }

FORCEINLINE HDC CreateCompatibleDC( HDC ) { return 0; }

FORCEINLINE HBITMAP CreateCompatibleBitmap( HDC, int, int ) { return 0; }

FORCEINLINE HBITMAP CreateDIBSection( HDC, CONST BITMAPINFO *, UINT, VOID **ppBits, HANDLE, DWORD) { ppBits = NULL; return 0; }

FORCEINLINE BOOL InvalidateRect( HWND, const RECT*, bool ) { return false; }

FORCEINLINE UINT joyGetDevCaps( UINT uJoyID, JOYCAPS* pjc, UINT cbjc) {	return 0; }

FORCEINLINE UINT joyGetPosEx( UINT uJoyID, LPJOYINFOEX pji) { return 0; }

FORCEINLINE UINT joyGetNumDevs(void) { return 0; }

FORCEINLINE HKL GetKeyboardLayout( DWORD ) { return NULL; }

FORCEINLINE HKL LoadKeyboardLayout( LPCTSTR, UINT ) { return NULL; }

FORCEINLINE UINT MapVirtualKeyEx( UINT, UINT, HKL ) { return 0; }

FORCEINLINE SHORT GetKeyState( int nVirtKey ) { return 0; }

FORCEINLINE BOOL PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {	return FALSE; }

FORCEINLINE BOOL TranslateMessage(CONST MSG *lpMsg) { return FALSE; }

FORCEINLINE BOOL DispatchMessage(CONST MSG *lpMsg) { return FALSE; }

FORCEINLINE BOOL UpdateWindow(HWND hWnd) { return FALSE; }

FORCEINLINE LONG RegOpenKeyEx( HKEY hKey, LPCTSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult ) {	return -1; }

FORCEINLINE LONG RegQueryValueEx( HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData ) { return -1; }

FORCEINLINE LONG RegCreateKeyEx( HKEY hKey, LPCTSTR lpSubKey, DWORD Reserved, LPTSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition ) { return -1; }

FORCEINLINE LONG RegSetValueEx( HKEY hKey, LPCTSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData ) { return -1; }

FORCEINLINE LONG RegDeleteValue( HKEY hKey, LPCTSTR lpValueName ) { return -1; }

FORCEINLINE LONG RegCloseKey( HKEY hKey ) {	return -1; }

FORCEINLINE BOOL ClientToScreen( HWND hwnd, LPPOINT lpPoint )
{
	lpPoint->x = 0;
	lpPoint->y = 0;
	return TRUE;
}

FORCEINLINE BOOL SetCursorPos( int x, int y ) { return FALSE; }

FORCEINLINE BOOL UnregisterClass( LPCTSTR lpClassNAme, HINSTANCE hInstance ) { return TRUE; }
FORCEINLINE BOOL UnregisterClassW( LPCWSTR lpClassNAme, HINSTANCE hInstance ) { return TRUE; }

FORCEINLINE	HCURSOR LoadCursor( HINSTANCE, LPCTSTR lpCursorName ) { return NULL; }

FORCEINLINE HWND GetParent( HWND hWnd ) { return NULL; }

FORCEINLINE BOOL EnumChildWindows( HWND hWndParent, WNDENUMPROC lpEnumFunc, LPARAM lParam ) { return FALSE; }

FORCEINLINE BOOL IsIconic( HWND hWnd ) { return FALSE; }

FORCEINLINE BOOL DestroyCursor( HCURSOR hCursor ) { return TRUE; }

FORCEINLINE HCURSOR LoadCursorFromFile( LPCTSTR lpFileName ) { return NULL; }

FORCEINLINE HCURSOR SetCursor( HCURSOR hCursor ) { return NULL; }

FORCEINLINE BOOL GetCursorPos( LPPOINT lpPoint ) { return TRUE; }

FORCEINLINE BOOL ScreenToClient( HWND hWnd, LPPOINT lpPoint ) { return TRUE; }

FORCEINLINE HWND SetCapture( HWND hWnd ) { return NULL; }

FORCEINLINE BOOL ReleaseCapture() { return TRUE; }

FORCEINLINE BOOL DeleteObject( HGDIOBJ hObject ) { return TRUE; }

FORCEINLINE BOOL DeleteDC( HDC hdc ) { return TRUE; }

FORCEINLINE HGDIOBJ SelectObject( HDC hdc, HGDIOBJ hgdiobj ) { return NULL; }

FORCEINLINE BOOL GetComputerName( LPTSTR lpBuffer, LPDWORD nSize ) { return FALSE; }

FORCEINLINE BOOL GetUserName( LPTSTR lpBuffer, LPDWORD nSize ) { return FALSE; }

FORCEINLINE UINT SetErrorMode( UINT mode ) { return 0; }

FORCEINLINE MCIERROR mciGetDeviceID( LPCTSTR ) { return 0; }

FORCEINLINE MCIERROR mciSendString( LPCTSTR lpszCommand, LPTSTR lpszReturnString, UINT cchReturn, HANDLE hwndCallback ) { return 0; }

FORCEINLINE MCIERROR mciSendCommand( MCIDEVICEID, UINT, DWORD, DWORD ) { return (UINT)MCIERR_DRIVER; } 

FORCEINLINE BOOL mciGetErrorString( MCIERROR, LPTSTR, UINT ) { return false; };

FORCEINLINE int UuidCreate( UUID *newId ) { return 0; };

FORCEINLINE HANDLE CreateFileMapping( HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCTSTR ) { return NULL; }

FORCEINLINE LPVOID MapViewOfFile( HANDLE, DWORD, DWORD, DWORD, SIZE_T ) { return NULL; }

FORCEINLINE BOOL UnmapViewOfFile( LPCVOID ) { return false; }

FORCEINLINE BOOL GetVersionEx( LPOSVERSIONINFO lpVersionInfo ) { lpVersionInfo->dwPlatformId = VER_PLATFORM_WIN32_NT; return true; }

FORCEINLINE BOOL GlobalMemoryStatusEx( LPMEMORYSTATUSEX lpBuffer ) { return false; }

FORCEINLINE BOOL TerminateThread( HANDLE hThread, DWORD dwExitCode ) { return false; }

FORCEINLINE BOOL HttpQueryInfo( HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex ) { return false; }

FORCEINLINE struct hostent FAR * _stdcall FAR gethostbyname( const char FAR * name ) { return NULL; }

FORCEINLINE BOOL InternetReadFile(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead ) { return false; }

FORCEINLINE BOOL InternetCloseHandle( HINTERNET hInternet ) { return false; }

FORCEINLINE BOOL InternetCrackUrl( LPCSTR lpszUrl, DWORD dwUrlLength, DWORD dwFlags, LPURL_COMPONENTS lpUrlComponents ) { return false; }

FORCEINLINE HINTERNET InternetOpen( LPCSTR lpszAgent, DWORD dwAccessType, LPCSTR lpszProxy OPTIONAL, LPCSTR lpszProxyBypass OPTIONAL, DWORD dwFlags ) { return 0; }

FORCEINLINE INTERNET_STATUS_CALLBACK InternetSetStatusCallback( HINTERNET hInternet, INTERNET_STATUS_CALLBACK lpfnInternetCallback ) { return NULL; }

FORCEINLINE HINTERNET InternetOpenUrl( HINTERNET hInternet, LPCSTR lpszUrl,LPCSTR lpszHeaders OPTIONAL, DWORD dwHeadersLength, DWORD dwFlags, DWORD_PTR dwContext ) { return 0; }

FORCEINLINE BOOL TerminateProcess( HANDLE, UINT ) { return false; }

FORCEINLINE DWORD MsgWaitForMultipleObjects( DWORD, CONST HANDLE*, BOOL, DWORD, DWORD ) { return 0; }

FORCEINLINE int gethostname( char *dest, int len ) { strncpy( dest, "localhost", len ); return 0; }

FORCEINLINE BOOL GetProcessTimes( HANDLE, LPFILETIME ft1, LPFILETIME ft2, LPFILETIME ft3, LPFILETIME ft4 ) { return false; }

//FORCEINLINE time_t Time(time_t *today) {  ULARGE_INTEGER ul; GetSystemTimeAsFileTime( (FILETIME*)&ul ); if(today) *today=ul.QuadPart; return ul.QuadPart; }
FORCEINLINE long Time(long *today) {  ULARGE_INTEGER ul; GetSystemTimeAsFileTime( (FILETIME*)&ul ); if(today) *today=ul.QuadPart; return ul.QuadPart; }

FORCEINLINE BOOL CreateProcess( LPCSTR lpApplicationName, 
					 LPSTR lpCommandLine, 
					 LPSECURITY_ATTRIBUTES lpProcessAttributes, 
					 LPSECURITY_ATTRIBUTES lpThreadAttributes, 
					 BOOL bInheritHandles, DWORD dwCreationFlags, 
					 LPVOID lpEnvironment, LPCSTR lpCurrentDirectory, 
					 LPSTARTUPINFO lpStartupInfo, 
					 LPPROCESS_INFORMATION lpProcessInformation )
{
	return false;
}

PLATFORM_INTERFACE DWORD g_dwProcessAffinityMask;

FORCEINLINE bool GetProcessAffinityMask( HANDLE hProcess, DWORD *lpProcessAffinityMask, DWORD *lpSystemAffinityMask )
{
	*lpProcessAffinityMask = g_dwProcessAffinityMask;
	*lpSystemAffinityMask = 0x01;
	return true;
}

FORCEINLINE bool SetProcessAffinityMask( HANDLE hProcess, DWORD dwProcessAffinityMask )
{
	if ( dwProcessAffinityMask == 0 )
	{
		// invalid
		return false;
	}

	g_dwProcessAffinityMask = dwProcessAffinityMask;
	return true;
}

FORCEINLINE DWORD_PTR SetThreadAffinityMask( HANDLE hThread, DWORD_PTR dwThreadAffinityMask ) 
{
	if ( dwThreadAffinityMask == 0 )
	{
		// invalid
		return 0;
	}

	// must select a specific processor, even if affinity mask specifies multiple.
	// bit mask to enum will choose last valid processor
	DWORD dwProcessor = 0;
	while ( dwThreadAffinityMask >>= 1 )
		dwProcessor++;

	dwProcessor = XSetThreadProcessor( hThread, dwProcessor ); 
	if ( dwProcessor == (DWORD)-1 )
	{
		// failed
		return 0;
	}

	// back to bit mask
	return ( 1<<dwProcessor );
}

// snd_win.cpp
class IAudioDevice;
class CAudioSource;
FORCEINLINE IAudioDevice *Audio_CreateWaveDevice( void ) { return NULL; }
FORCEINLINE IAudioDevice *Audio_CreateDirectSoundDevice( void ) { return NULL; }
FORCEINLINE CAudioSource* Voice_SetupAudioSource( int soundsource, int entchannel ) { return NULL; }

// snd_wave_source.cpp
FORCEINLINE float GetMP3Duration_Helper( char const *filename ) { return 0.f; }

// host.cpp
FORCEINLINE void InitPME() {}
FORCEINLINE void ShutdownPME() {}

// Steam API stubs
class ISteamClient;
class ISteamGameServer;
class ISteamUtils;
class ISteamUser;
class ISteamUserStats;
class ISteamFriends;
class ISteamMasterServerUpdater;
enum EServerMode;
FORCEINLINE ISteamClient				*SteamClient() { return NULL; }
FORCEINLINE ISteamGameServer			*SteamGameServer() { return NULL; }
FORCEINLINE ISteamUtils					*SteamGameServerUtils() { return NULL; }
FORCEINLINE ISteamUtils					*SteamUtils() { return NULL; }
FORCEINLINE ISteamUser					*SteamUser() { return NULL; }
FORCEINLINE ISteamUserStats				*SteamUserStats() { return NULL; }
FORCEINLINE ISteamFriends				*SteamFriends() { return NULL; }
FORCEINLINE int32						GetHSteamPipe()	{ return 0; }
FORCEINLINE int32						GetHSteamUser()	{ return 0; }
FORCEINLINE int32						SteamAPI_GetHSteamPipe()	{ return 0; }
FORCEINLINE int32						SteamAPI_GetHSteamUser()	{ return 0; }
FORCEINLINE ISteamMasterServerUpdater	*SteamMasterServerUpdater() { return NULL; }
FORCEINLINE bool SteamGameServer_Init( uint32 unIP, uint16 usPort, uint16 usGamePort, uint16 usSpectatorPort, uint16 usQueryPort, EServerMode eServerMode, int nGameAppId, const char *pchGameDir, const char *pchVersionString ) { return false; }
FORCEINLINE void SteamGameServer_Shutdown() {}
FORCEINLINE void SteamGameServer_RunCallbacks() {}

FORCEINLINE int getch( void ) { return 0; }

PLATFORM_INTERFACE LONG			GetWindowLong( HWND hWnd, int nIndex );
PLATFORM_INTERFACE LONG			SetWindowLong( HWND hWnd, int nIndex, LONG dwNewLong );
PLATFORM_INTERFACE LONG_PTR		GetWindowLongPtr( HWND hWnd, int nIndex );
PLATFORM_INTERFACE LONG_PTR		SetWindowLongPtr( HWND hWnd, int nIndex, LONG dwNewLong );
PLATFORM_INTERFACE LONG_PTR		GetWindowLongPtrW( HWND hWnd, int nIndex );  // false stub -- no work has been done to make unicode version of this function on 360
PLATFORM_INTERFACE LONG_PTR		SetWindowLongPtrW( HWND hWnd, int nIndex, LONG dwNewLong ); // false stub -- no work has been done to make unicode version of this function on 360
PLATFORM_INTERFACE HWND			CreateWindow( LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam );
PLATFORM_INTERFACE HWND			CreateWindowEx( DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam );
PLATFORM_INTERFACE BOOL			DestroyWindow( HWND hWnd );
PLATFORM_INTERFACE ATOM			RegisterClassEx( CONST WNDCLASSEX *lpwcx );
PLATFORM_INTERFACE ATOM			RegisterClass( CONST WNDCLASS *lpwc );
PLATFORM_INTERFACE HWND			GetFocus( VOID );
PLATFORM_INTERFACE HWND			SetFocus( HWND hWnd );
PLATFORM_INTERFACE int			GetSystemMetrics( int nIndex );
PLATFORM_INTERFACE BOOL			ShowWindow( HWND hWnd, int nCmdShow );
PLATFORM_INTERFACE LRESULT		SendMessage( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam );
PLATFORM_INTERFACE LRESULT		CallWindowProc( WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam );
PLATFORM_INTERFACE BOOL			GetClientRect( HWND hwnd, LPRECT lpRect );
PLATFORM_INTERFACE int			GetDeviceCaps( HDC hdc, int nIndex );
PLATFORM_INTERFACE LRESULT		SendMessageTimeout( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult );
PLATFORM_INTERFACE BOOL			SetWindowPos( HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags );








