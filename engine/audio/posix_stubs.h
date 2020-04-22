//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Posix win32 replacements - Mocks trivial windows flow
//
//=============================================================================
#ifndef POSIX_AUDIO_STUBS_H
#define POSIX_AUDIO_STUBS_H

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

typedef wchar_t BCHAR;

typedef uint MMRESULT;
typedef uint32 *DWORD_PTR;
typedef char *LPCSTR;
typedef uint POINTL;

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

typedef uint32				MCIERROR;
typedef uint				MCIDEVICEID;

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

#pragma pack(push, 1)
typedef struct tWAVEFORMATEX
{
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
    WORD    wBitsPerSample;
    WORD    cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;

typedef const WAVEFORMATEX *LPCWAVEFORMATEX;


typedef struct waveformat_tag
{
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *NPWAVEFORMAT, *LPWAVEFORMAT;

typedef const WAVEFORMAT *LPCWAVEFORMAT;

typedef struct pcmwaveformat_tag
{
    WAVEFORMAT  wf;
    WORD        wBitsPerSample;
} PCMWAVEFORMAT, *PPCMWAVEFORMAT, *NPPCMWAVEFORMAT, *LPPCMWAVEFORMAT;

typedef const PCMWAVEFORMAT *LPCPCMWAVEFORMAT;

typedef struct adpcmcoef_tag {
	short	iCoef1;
	short	iCoef2;
} ADPCMCOEFSET;

typedef struct adpcmwaveformat_tag {
	WAVEFORMATEX	wfx;
	WORD			wSamplesPerBlock;
	WORD			wNumCoef;
	ADPCMCOEFSET	aCoef[1];
} ADPCMWAVEFORMAT;

#pragma pack(pop)
#endif

