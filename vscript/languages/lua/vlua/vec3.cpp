#include <stdio.h>

#if !defined(OSX) && !defined (LINUX)
extern "C"
{
#endif

#include "lua.h"
#include "lauxlib.h"

#if !defined(OSX) && !defined (LINUX)
}
#endif

#include "vec3.h"


#define VEC3_TYPE		"Vec3"
#define VEC3_NAME		"Vec3"


Vector *lua_getvec3( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, VEC3_TYPE ) == NULL )  
	{
		luaL_typerror( pState, i, VEC3_TYPE );
	}

	return ( Vector * )lua_touserdata( pState, i );
}


Vector lua_getvec3ByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		float flValue = lua_tonumber( pState, i );

		return Vector( flValue, flValue, flValue  );
	}
	if ( luaL_checkudata( pState, i, VEC3_TYPE ) == NULL )  
	{
		luaL_typerror( pState, i, VEC3_TYPE );
	}

	return *( Vector * )lua_touserdata( pState, i );
}


static Vector *lua_allocvec3( lua_State *pState )
{
	Vector *v =( Vector *)lua_newuserdata( pState, sizeof( Vector ) );
	luaL_getmetatable( pState, VEC3_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


Vector *lua_newvec3( lua_State *pState, const Vector *Value )
{
	Vector *v;

	v = lua_allocvec3( pState );
	v->x = Value->x;
	v->y = Value->y;
	v->z = Value->z;

	return v;
}


static int vec3_new( lua_State *pState )
{
	Vector *v;

	lua_settop( pState, 3 );
	v = lua_allocvec3( pState );
	v->x = luaL_optnumber( pState, 1, 0 );
	v->y = luaL_optnumber( pState, 2, 0 );
	v->z = luaL_optnumber( pState, 3, 0 );

	return 1;
}


static int vec3_index( lua_State *pState )
{
	const char	*pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey[ 1 ] == '\0' )
	{
		Vector	*v = lua_getvec3( pState, 1 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r': 
				lua_pushnumber( pState, v->x ); 
				return 1;

			case '2': case 'y': case 'g': 
				lua_pushnumber( pState, v->y ); 
				return 1;

			case '3': case 'z': case 'b': 
				lua_pushnumber( pState, v->z );
				return 1;
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, VEC3_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


static int vec3_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey[ 1 ] == '\0' )
	{
		Vector	*v = lua_getvec3( pState, 1 );
		float	flValue = luaL_checknumber( pState, 3 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r': 
				v->x = flValue; 
				break;

			case '2': case 'y': case 'g': 
				v->y = flValue; 
				break;

			case '3': case 'z': case 'b': 
				v->z = flValue; 
				break;

			default: 
				break;
		}
	}

	return 1;
}

static int vec3_tostring( lua_State *pState )
{
	char	s[ 64 ];
	Vector	*v = lua_getvec3( pState, 1 );

	sprintf( s, "%s %p", VEC3_TYPE, ( void * )v );

	lua_pushstring( pState, s );

	return 1;
}


static int vec3_add(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	Vector vResult = v1 + v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


static int vec3_subtract(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	Vector vResult = v1 - v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


static int vec3_multiply(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	Vector vResult = v1 * v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


static int vec3_divide(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	Vector vResult = v1 / v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


static int vec3_length(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );

	float flResult = v1.Length();

	lua_pushnumber( pState, flResult );

	return 1;
}


static int vec3_equal(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	return ( v1 == v2 ? 1 : 0 );
}


static int vec3_dot(  lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState, 2 );

	float flResult = v1.Dot( v2 );

	lua_pushnumber( pState, flResult );

	return 1;
}


static int vec3_cross( lua_State *pState )
{
	Vector v1 = lua_getvec3ByValue( pState, 1 );
	Vector v2 = lua_getvec3ByValue( pState,2 );

	Vector vResult = v1,Cross( v2 );

	lua_newvec3( pState, &vResult );

	return 1;
}


static const luaL_reg Registrations[] =
{
	{ "__index",	vec3_index		},
	{ "__newindex",	vec3_newindex	},
	{ "__tostring",	vec3_tostring	},
	{ "__add",		vec3_add		},
	{ "__sub",		vec3_subtract	},
	{ "__mul",		vec3_multiply	},
	{ "__div",		vec3_divide		},
	{ "__len",		vec3_length		},
	{ "__eq",		vec3_equal		},
	{ "dot",		vec3_dot		},
	{ "cross",		vec3_cross		},
	{ NULL,		NULL				}
};


LUALIB_API int luaopen_vec3( lua_State *pState )
{
	luaL_newmetatable( pState, VEC3_TYPE );
	luaL_register( pState, NULL, Registrations );
	lua_register( pState, VEC3_NAME, vec3_new );

	return 1;
}
