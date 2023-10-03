//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VLUA_H
#define VLUA_H

#if defined( _WIN32 )
#pragma once
#endif

IScriptVM *ScriptCreateLuaVM();
void ScriptDestroyLuaVM( IScriptVM *pVM );

#endif // VLUA_H
