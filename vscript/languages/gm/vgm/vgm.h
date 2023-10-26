//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VGM_H
#define VGM_H

#if defined( _WIN32 )
#pragma once
#endif

IScriptVM *ScriptCreateGameMonkeyVM();
void ScriptDestroyGameMonkeyVM( IScriptVM *pVM );

#endif // VGM_H
