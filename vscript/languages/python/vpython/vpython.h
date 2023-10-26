//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VPYTHON_H
#define VPYTHON_H

#if defined( _WIN32 )
#pragma once
#endif

IScriptVM *ScriptCreatePythonVM();
void ScriptDestroyPythonVM( IScriptVM *pVM );

#endif // VPYTHON_H
