//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _DEBUG

ConVar net_synctags( "net_synctags", "0", 0, "Insert tokens into the net stream to find client/server mismatches." );

#endif


