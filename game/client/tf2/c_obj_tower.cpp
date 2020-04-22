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
class C_ObjectTower : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectTower, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectTower();

private:
	C_ObjectTower( const C_ObjectTower & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectTower, DT_ObjectTower, CObjectTower)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectTower::C_ObjectTower()
{
}

//=============================================================================
// Tower Ladder
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectTowerLadder : public C_BaseAnimating
{
	DECLARE_CLASS( C_ObjectTowerLadder, C_BaseAnimating );
public:

	DECLARE_CLIENTCLASS();

	C_ObjectTowerLadder();

private:
	C_ObjectTowerLadder( const C_ObjectTowerLadder& ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectTowerLadder, DT_ObjectTowerLadder, CObjectTowerLadder )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_ObjectTowerLadder::C_ObjectTowerLadder()
{
}

