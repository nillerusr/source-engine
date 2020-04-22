//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An trigger that teleports its contents to a target when triggered.
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_FUNC_MASS_TELEPORT_H
#define TF_FUNC_MASS_TELEPORT_H

#ifdef _WIN32
#pragma once
#endif

class CBaseObject;
class Vector;
class CTFTeam;
class CVehicleTeleportStation;

//-----------------------------------------------------------------------------
// Purpose: Defines an area from which resources can be collected
//-----------------------------------------------------------------------------
class CFuncMassTeleport : public CBaseEntity
{
	DECLARE_CLASS( CFuncMassTeleport, CBaseEntity );
public:
	CFuncMassTeleport();

	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate(void);

	void	DoTeleport( CTFTeam *pTeam, Vector vSourceMins, Vector vSourceMaxs, Vector vDest, bool bSwap, bool bPlayersOnly, EHANDLE hMCV );

	// Inputs
	void	InputDoTeleport( inputdata_t &inputdata );

	bool	PointIsWithin( const Vector &vecPoint );

	// need to transmit to players who are in commander mode
	int		UpdateTransmitState() { return SetTransmitState( FL_EDICT_FULLCHECK); }
	int 	ShouldTransmit( const CCheckTransmitInfo *pInfo );

	// Return true if this teleport's for use by MCVs in the field
	bool	IsMCVTeleport( void ) { return m_bMCV; }
	void	AddMCV( CBaseEntity *pMCV );
	void	RemoveMCV( CBaseEntity *pMCV );

private:

	void MassTeleport( CTFTeam *pTeam, Vector vSourceMins, Vector vSourceMaxs, Vector vDest, bool bSwap, bool bPlayersOnly, EHANDLE hMCV, bool bTelefragDestination );
	void MassTelefrag( Vector vMins, Vector vMaxs );


	// Entity to teleport to.
	EHANDLE		m_hTargetEntity;		

	// If true, this teleport's designed to teleport players to MCVs in the field
	bool		m_bMCV;
	typedef CHandle<CVehicleTeleportStation> hMCVHandle;
	CUtlVector<hMCVHandle> m_MCVs;

	// Outputs
	COutputEvent m_OnTeleport;
	COutputEvent m_OnActive;
	COutputEvent m_OnInactive;
};


#endif // TF_FUNC_MASS_TELEPORT_H
