//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "shadersystem.h"
#include <malloc.h>
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_FrameNum;
IShaderAPI *g_pShaderAPI = 0;
IShaderDeviceMgr* g_pShaderDeviceMgr = 0;
IShaderDevice *g_pShaderDevice = 0;
IShaderShadow* g_pShaderShadow = 0;
