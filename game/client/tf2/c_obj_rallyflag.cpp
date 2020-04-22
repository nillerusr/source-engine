//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_baseobject.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectRallyFlag : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectRallyFlag, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectRallyFlag();

private:
	C_ObjectRallyFlag( const C_ObjectRallyFlag & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectRallyFlag, DT_ObjectRallyFlag, CObjectRallyFlag)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectRallyFlag::C_ObjectRallyFlag()
{
}

