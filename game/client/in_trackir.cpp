//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: TrackIR handling function
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( _X360 )
#include <windows.h>
#endif

#include "string_t.h"

// These two have to be included very early
#include <predictableid.h>
#include <predictable_entity.h>

#include "cdll_util.h"
#include <util_shared.h>
#include "vphysics_interface.h"
#include <icvar.h>
#include <baseentity_shared.h>
#include "basehandle.h"
#include "ehandle.h"
#include "utlvector.h"
#include "cdll_client_int.h"
#include "kbutton.h"
#include "usercmd.h"
#include "iclientvehicle.h"
#include "input.h"
#include "iviewrender.h"
#include "convar.h"
#include "hud.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"
#include "vgui/cursor.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "math.h"
#include "tier1/convar_serverbounded.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if !defined( _X360 )
#include "npsclient.h"

#define TIR_MAX_VALUE   	    16383

QAngle	 g_angleCenter;

ConVar tir_maxyaw(   "tir_maxyaw",   "90", FCVAR_CHEAT, "TrackIR Max Yaw",   true, 0.0, true, 180.0); 
ConVar tir_maxpitch( "tir_maxpitch", "15", FCVAR_CHEAT, "TrackIR Max Pitch", true, 0.0, true, 180.0); 
ConVar tir_maxroll(  "tir_maxroll",  "90", FCVAR_CHEAT, "TrackIR Max Roll",  true, 0.0, true, 180.0); 
ConVar tir_maxx(     "tir_maxx",      "4", FCVAR_CHEAT, "TrackIR Max X",     true, 0.0, true, 50.0); 
ConVar tir_maxy(     "tir_maxy",      "6", FCVAR_CHEAT, "TrackIR Max Y",     true, 0.0, true, 50.0); 
ConVar tir_maxz(     "tir_maxz",      "1", FCVAR_CHEAT, "TrackIR Max Z",     true, 0.0, true, 50.0); 

ConVar tir_start( "tir_start", "0", 0, "TrackIR Start", true, 0.0, true, 1.0); 
ConVar tir_stop(  "tir_stop",  "0", 0, "TrackIR Stop",  true, 0.0, true, 1.0); 

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND CreateHiddenWindow()
{
	HWND hWnd;
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= 0;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= TEXT("HL2-TrackIR");
	wcex.hIconSm		= NULL;

	RegisterClassEx(&wcex);

	hWnd = CreateWindow(TEXT("HL2-TrackIR"), NULL, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (hWnd)
	{
		ShowWindow(hWnd, SW_HIDE);
		UpdateWindow(hWnd);
	}

	return hWnd;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Init_TrackIR
//-----------------------------------------------------------------------------
void CInput::Init_TrackIR( void ) 
{ 
#if !defined( _X360 )
	if ( !IsHeadTrackingEnabled() )
		return;

	ZeroMemory(&g_angleCenter, sizeof(g_angleCenter));

	HWND hWnd = CreateHiddenWindow();
	NPRESULT  result = NPS_Init(hWnd);

	// Mark the TrackIR as available and advanced initialization not completed
	// this is needed as correctly set cvars are not available this early during initialization
	// FIXME:  Is this still the case?
	Msg( "TrackIR initialized [%d]\n", result ); 
	m_fTrackIRAvailable = true; 
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Init_TrackIR
//-----------------------------------------------------------------------------
void CInput::Shutdown_TrackIR( void ) 
{
#if !defined( _X360 )
	if ( !IsHeadTrackingEnabled() )
		return;

	NPS_Shutdown();

	Msg( "TrackIR shut down\n" ); 
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Apply TrackIR to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::TrackIRMove( float frametime, CUserCmd *cmd )
{
#if !defined( _X360 )
	if ( !IsHeadTrackingEnabled() )
		return;

	// complete initialization if first time in ( needed as cvars are not available at initialization time )
	// verify TrackIR is available and that the user wants to use it
	if (!m_fTrackIRAvailable )
	{
		return; 
	}

	if (tir_start.GetFloat() == 1.0)
	{
		Init_TrackIR();
		tir_start.SetValue(0);
	}

	if (tir_stop.GetFloat() == 1.0)
	{
		Shutdown_TrackIR();
		tir_stop.SetValue(0);
	}

	// grab the data from the TrackIR
	TRACKIRDATA tid;
	NPRESULT    result;

	// Go get the latest data
	result = NPS_GetData(&tid);
	if( NP_OK == result )
	{
		QAngle viewangles;
		QAngle engineview;

		// get the current player
		C_BasePlayer * pPlayer = C_BasePlayer::GetLocalPlayer();

		// calculate the amount of rotation from TrackIR
		viewangles[YAW] = g_angleCenter[YAW] + (tid.fNPYaw / (float) TIR_MAX_VALUE) * tir_maxyaw.GetFloat();
		viewangles[PITCH] = g_angleCenter[PITCH] + (tid.fNPPitch / (float) TIR_MAX_VALUE) * tir_maxpitch.GetFloat();
		viewangles[ROLL] = g_angleCenter[ROLL] + (tid.fNPRoll / (float) TIR_MAX_VALUE) * tir_maxroll.GetFloat() * -1.0;

		// get the direction the player is facing
		QAngle eyeAngle;
		eyeAngle = pPlayer->EyeAngles();

		// add in the head rotation
		eyeAngle += viewangles;

		// get the rotation matrix for the head
		matrix3x4_t mat;
		AngleMatrix( pPlayer->EyeAngles(), mat );

		// create a normalized vector based on the TIR input
		Vector tirForward, tirEye;

		tirForward.x = (tid.fNPZ / (float) TIR_MAX_VALUE) * -1;
		tirForward.y = (tid.fNPX / (float) TIR_MAX_VALUE);
		tirForward.z = (tid.fNPY / (float) TIR_MAX_VALUE);

		// now rotate the vector based on the eye angle
		VectorRotate(tirForward, mat, tirEye);

		// scale the translation vector
		tirEye.x *= tir_maxz.GetFloat();
		tirEye.y *= tir_maxx.GetFloat();
		tirEye.z *= tir_maxy.GetFloat();

		// save the values for later
		pPlayer->SetEyeOffset(tirEye);
		pPlayer->SetEyeAngleOffset(viewangles);

		cmd->headangles = viewangles;
		cmd->headoffset = tirEye;
	}
#endif
}

