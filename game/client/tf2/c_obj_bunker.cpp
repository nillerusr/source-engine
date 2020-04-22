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
class C_ObjectBunker : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectBunker, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectBunker();

private:
	C_ObjectBunker( const C_ObjectBunker & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectBunker, DT_ObjectBunker, CObjectBunker)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectBunker::C_ObjectBunker()
{
}


//=============================================================================
// Bunker Ladder
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectBunkerLadder : public C_BaseAnimating
{
	DECLARE_CLASS( C_ObjectBunkerLadder, C_BaseAnimating );
public:

	DECLARE_CLIENTCLASS();

	C_ObjectBunkerLadder();

private:
	C_ObjectBunkerLadder( const C_ObjectBunkerLadder& ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectBunkerLadder, DT_ObjectBunkerLadder, CObjectBunkerLadder )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_ObjectBunkerLadder::C_ObjectBunkerLadder()
{
}

