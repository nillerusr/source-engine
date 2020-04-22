//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//==================================================================================================

#ifndef POSIX_WIN32STUBS_H
#define POSIX_WIN32STUBS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"
#include "tier0/platform.h"

typedef int32 LRESULT;
typedef void* HWND;
typedef uint32 UINT;
typedef uintp WPARAM;
typedef uintp LPARAM;

typedef uint8 BYTE;
typedef int16 SHORT;

typedef void* WNDPROC;
typedef void* HANDLE;

typedef char xKey_t;

#define XUSER_MAX_COUNT 2
#define XK_MAX_KEYS 5

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


typedef struct _XINPUT_GAMEPAD
{
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
    DWORD                               dwPacketNumber;
    XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
    WORD                                wLeftMotorSpeed;
    WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;


#endif // POSIX_WIN32STUBS_H
