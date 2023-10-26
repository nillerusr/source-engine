//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VSQUIRREL_H
#define VSQUIRREL_H

#if defined( _WIN32 )
#pragma once
#endif

IScriptVM *ScriptCreateSquirrelVM();
void ScriptDestroySquirrelVM( IScriptVM *pVM );

#endif // VSQUIRREL_H
