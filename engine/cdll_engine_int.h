//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CDLL_ENGINE_INT_H
#define CDLL_ENGINE_INT_H
#ifdef _WIN32
#pragma once
#endif


#include "cdll_int.h"

class IVModelRender;
class IClientLeafSystemEngine;
class ClientClass;
class IClientReplay;

bool ClientDLL_Load( void );
void ClientDLL_Unload ( void );
void ClientDLL_Init( void );
void ClientDLL_Shutdown( void );
void ClientDLL_HudVidInit( void );
void ClientDLL_ProcessInput( void );
void ClientDLL_Update( void );
void ClientDLL_VoiceStatus( int entindex, bool bTalking );
void ClientDLL_FrameStageNotify( ClientFrameStage_t frameStage );
ClientClass *ClientDLL_GetAllClasses( void );
CreateInterfaceFn ClientDLL_GetFactory( void );

//-----------------------------------------------------------------------------
// slow routine to draw a physics model
//-----------------------------------------------------------------------------
void DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, matrix3x4_t& transform, const color32 &color, bool drawAxes );

#ifndef SWDS
extern IBaseClientDLL *g_ClientDLL;
#endif

extern IVModelRender* modelrender;
extern IClientLeafSystemEngine* clientleafsystem;
extern bool g_bClientLeafSystemV1;
extern ClientClass *g_pClientClasses;
extern bool scr_drawloading;
extern IClientReplay *g_pClientReplay;

#endif // CDLL_ENGINE_INT_H
