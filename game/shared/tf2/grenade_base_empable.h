//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_BASE_EMPABLE_H
#define GRENADE_BASE_EMPABLE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"

#if defined( CLIENT_DLL )
#define CBaseEMPableGrenade C_BaseEMPableGrenade
#endif

//-----------------------------------------------------------------------------
// Purpose: EMP grenade
//-----------------------------------------------------------------------------
class CBaseEMPableGrenade : public CBaseGrenade
{
	DECLARE_CLASS( CBaseEMPableGrenade, CBaseGrenade );
public:
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	CBaseEMPableGrenade();

	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual bool	TakeEMPDamage( float duration );
	
	void	FizzleThink( void );

private:
	CNetworkVar( float, m_flFizzleDuration );

private:
	CBaseEMPableGrenade( const CBaseEMPableGrenade & ); // not defined, not accessible

};


#endif // GRENADE_BASE_EMPABLE_H
