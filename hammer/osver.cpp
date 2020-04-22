//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "stdafx.h"
#include "osver.h"

static eOSVersion s_OS = eUninitialized;

void initOSVersion()
{
    OSVERSIONINFO versioninfo;
    versioninfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&versioninfo))
    {
        switch(versioninfo.dwPlatformId)
        {
            case VER_PLATFORM_WIN32_WINDOWS:
            {
                s_OS = eWin9x;
                break;
            }
            case VER_PLATFORM_WIN32_NT:
            {
                s_OS = eWinNT;
                break;
            }
            default:
            {
                s_OS = eUnknown;
                break;
            }
        }
    }
}

eOSVersion getOSVersion()
{
    if (s_OS == eUninitialized)
    {
        initOSVersion();
    }
    return s_OS;
}