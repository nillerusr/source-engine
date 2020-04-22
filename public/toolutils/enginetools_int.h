//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ENGINETOOLS_INT_H
#define ENGINETOOLS_INT_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IEngineTool;
class IEngineVGui;
class IServerTools;
class IClientTools;
class IFileSystem;
class IP4;
class IVDebugOverlay;
class IDmSerializers;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern IEngineTool	*enginetools;
extern IEngineVGui	*enginevgui;
extern IServerTools	*servertools;
extern IClientTools	*clienttools;
extern IFileSystem	*g_pFileSystem;
extern IP4			*p4;
extern IVDebugOverlay *debugoverlay;
extern IDmSerializers *dmserializers;


#endif // ENGINETOOLS_INT_H
