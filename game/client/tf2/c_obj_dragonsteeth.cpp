//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "commanderoverlay.h"
#include "c_baseobject.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectDragonsTeeth : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectDragonsTeeth, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectDragonsTeeth();

	// Since we have material proxies to show building amount, don't offset origin
	virtual bool	OffsetObjectOrigin( Vector& origin )
	{
		return false;
	}

private:
	C_ObjectDragonsTeeth( const C_ObjectDragonsTeeth & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectDragonsTeeth, DT_ObjectDragonsTeeth, CObjectDragonsTeeth)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectDragonsTeeth::C_ObjectDragonsTeeth()
{
}
