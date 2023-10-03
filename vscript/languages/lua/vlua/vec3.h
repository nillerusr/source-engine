//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef VEC3_H
#define VEC3_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

extern int luaopen_vec3(lua_State *L);
extern Vector *lua_getvec3(lua_State *L, int i);
extern Vector *lua_newvec3( lua_State *L, const Vector *Value );

#endif // VEC3_H
