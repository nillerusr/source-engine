// *******************************************************************************
// *
// * Module Name:
// *   NPSClient.h
// *
// * Abstract:
// *   Header for NaturalPoint Simple Game Client API.
// *
// * Environment:
// *   Microsoft Windows -- User mode
// *
// *******************************************************************************

#ifndef _NPSCLIENT_H_DEFINED_
#define _NPSCLIENT_H_DEFINED_

#pragma pack( push, npsclient_h ) // Save current pack value
#pragma pack(1)

#ifdef __cplusplus
extern "C"{
#endif 

//////////////////
/// Typedefs /////////////////////////////////////////////////////////////////////
/////////////////

#ifndef _NPCLIENT_H_DEFINED_

// NPESULT values are returned from the Game Client API functions.
//
typedef enum tagNPResult
{
    NP_OK = 0,
    NP_ERR_DEVICE_NOT_PRESENT,
    NP_ERR_UNSUPPORTED_OS,
    NP_ERR_INVALID_ARG,
    NP_ERR_DLL_NOT_FOUND,
    NP_ERR_NO_DATA,
    NP_ERR_INTERNAL_DATA,
    NP_ERR_ALREADY_REGISTERED,  // a window handle or game ID is already registered
    NP_ERR_UNKNOWN_ID,          // unknown game ID registered
    NP_ERR_READ_ONLY,           // parameter is read only
    
} NPRESULT;

typedef struct tagTrackIRData
{
    unsigned short wNPStatus;
    unsigned short wPFrameSignature;
    unsigned long  dwNPIOData;

    float fNPRoll;
    float fNPPitch;
    float fNPYaw;
    float fNPX;
    float fNPY;
    float fNPZ;
    float fNPRawX;
    float fNPRawY;
    float fNPRawZ;
    float fNPDeltaX;
    float fNPDeltaY;
    float fNPDeltaZ;
    float fNPSmoothX;
    float fNPSmoothY;
    float fNPSmoothZ;

} TRACKIRDATA, *LPTRACKIRDATA;

#endif

typedef NPRESULT (__stdcall *PF_NPS_INIT)( HWND );
typedef NPRESULT (__stdcall *PF_NPS_SHUTDOWN)( void );
typedef NPRESULT (__stdcall *PF_NPS_GETDATA)( LPTRACKIRDATA );

//// Function Prototypes ///////////////////////////////////////////////
//
// Functions exported from game client API DLL ( note __stdcall calling convention
// is used for ease of interface to clients of differing implementations including
// C, C++, Pascal (Delphi) and VB. )
//
NPRESULT __stdcall NPS_Init( HWND hWnd  );
NPRESULT __stdcall NPS_Shutdown( void );
NPRESULT __stdcall NPS_GetData( LPTRACKIRDATA pTID );

/////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#pragma pack( pop, npsclient_h ) // Ensure previous pack value is restored

#endif // #ifdef NPCLIENT_H_DEFINED_

//
// *** End of file: NPSClient.h ***
//


