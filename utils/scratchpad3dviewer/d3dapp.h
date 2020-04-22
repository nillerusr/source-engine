//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef D3DAPP_H
#define D3DAPP_H
#ifdef _WIN32
#pragma once
#endif


#include <d3d8.h>


// Globals for the app to access. These are valid whenever the app is called.
extern IDirect3D8		*g_pDirect3D;
extern IDirect3DDevice8	*g_pDevice;


// Special keys.
#define APPKEY_LBUTTON	-1
#define APPKEY_RBUTTON	-2
#define APPKEY_SPACE	-3


// ------------------------------------------------------------------------------------ //
// Functions for the app to provide.
// ------------------------------------------------------------------------------------ //
void AppInit();

// Called to update or render the view as often as possible. If bInvalidRect is true,
// then you should always render, so the window's contents are valid.
void AppRender( float frametime, float mouseDeltaX, float mouseDeltaY, bool bInvalidRect );

// Release any D3D objects you have here - a resize will happen.
void AppPreResize();
void AppPostResize();

void AppExit( );
void AppKey( int key, int down );
void AppChar( int key );



// ------------------------------------------------------------------------------------ //
// Functions the app can call.
// ------------------------------------------------------------------------------------ //

// Show an error dialog and quit.
bool Sys_Error(const char *pMsg, ...);

// Shutdown the app. AppExit() will be called.
void Sys_Quit();

void Sys_SetWindowText( char const *pMsg, ... );

// Key can be an ASCII key code or an APPKEY_ define.
bool Sys_GetKeyState( int key );

void Sys_Sleep( int ms );

// Does the app have the focus?
bool Sys_HasFocus();

char const* Sys_FindArg( char const *pArg, char const *pDefault=NULL );
int Sys_FindArgInt( char const *pArg, int defaultVal = -1 );

int Sys_ScreenWidth();
int Sys_ScreenHeight();


#endif // D3DAPP_H
