//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SYS_UTILS_H
#define SYS_UTILS_H
#ifdef _WIN32
#pragma once
#endif

typedef int WHANDLE;

// ERROR HANDLING
extern const unsigned int SYS_NO_ERROR;
extern const unsigned int SYS_ERROR_INVALID_HANDLE;

void Sys_SetLastError(unsigned long error);
unsigned long Sys_GetLastError();

// MUTEX HANDLING
WHANDLE Sys_CreateMutex(const char *mutexName);
void Sys_ReleaseMutex(WHANDLE mutexHandle);

// MUTEX SYNCHRONIZATION
extern const unsigned int SYS_WAIT_OBJECT_0;
extern const unsigned int SYS_WAIT_ABANDONED;
unsigned int Sys_WaitForSingleObject(WHANDLE mutexHandle, int milliseconds);

// window handling
unsigned int Sys_RegisterWindowMessage(const char *msgName);
WHANDLE Sys_FindWindow(const char *className, const char *windowName);
void Sys_EnumWindows(void *callbackFunction, int lparam);
void Sys_GetWindowText(WHANDLE wnd, char *buffer, int bufferSize);
void Sys_PostMessage(WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam);
WHANDLE Sys_CreateWindowEx(const char *windowName);
void Sys_DestroyWindow(WHANDLE wnd);

// mouse
void Sys_SetCursorPos(int x, int y);






#endif // SYS_UTILS_H
