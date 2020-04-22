//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_OBJ_MAPDEFINED_H
#define C_OBJ_MAPDEFINED_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseobject.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectMapDefined : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectMapDefined, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectMapDefined();

	virtual const char	*GetTargetDescription( void ) const;

private:
	C_ObjectMapDefined( const C_ObjectMapDefined & ); // not defined, not accessible

	char	m_szCustomName[ MAX_OBJ_CUSTOMNAME_SIZE ];
};

#endif // C_OBJ_MAPDEFINED_H
