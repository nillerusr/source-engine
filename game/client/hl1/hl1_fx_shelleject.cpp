//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"


void HL1ShellEjectCallback( const CEffectData &data )
{
	int iType = data.m_fFlags;	// 0 = shell, 1 = shotgun shell

	tempents->HL1EjectBrass( data.m_vOrigin, data.m_vAngles, data.m_vStart, iType );
}

DECLARE_CLIENT_EFFECT( "HL1ShellEject", HL1ShellEjectCallback );
