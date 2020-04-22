//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rising liquid that acts as a one-way portal
//
// $NoKeywords: $
//===========================================================================//

#ifndef FUNC_LIQUIDPORTAL_H
#define FUNC_LIQUIDPORTAL_H

#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"

class CFunc_LiquidPortal : public CBaseEntity
{
public:
	DECLARE_CLASS( CFunc_LiquidPortal, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFunc_LiquidPortal( void );

	virtual void Spawn( void );
	virtual void Activate( void );
	virtual void Think( void );

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	virtual int	Save( ISave &save );
	virtual int	Restore( IRestore &restore );

    void InputSetLinkedLiquidPortal( inputdata_t &inputdata );
	void InputSetFillTime( inputdata_t &inputdata ); //time it takes to fill the portal volume
	void InputStartFilling( inputdata_t &inputdata ); //start filling with portal liquid, will teleport entities as they become completely enveloped

	//add/remove teleportables to offload the selection process to triggers, each with their own filters
	void InputAddActivatorToTeleportList( inputdata_t &inputdata ); //add an activator entity to the list of entities to teleport when filling
	void InputRemoveActivatorFromTeleportList( inputdata_t &inputdata ); //remove an activator entity from the list of entities to teleport when filling

	void ComputeLinkMatrix( void );
	void SetLinkedLiquidPortal( CFunc_LiquidPortal *pLinked );

	void TeleportImmersedEntity( CBaseEntity *pEntity );

	CNetworkHandle( CFunc_LiquidPortal, m_hLinkedPortal ); //the portal this portal is linked to
	VMatrix m_matrixThisToLinked; //the matrix that will transform a point relative to this portal, to a point relative to the linked portal
	float m_fFillTime; //how long it takes to fill completely
	bool m_bFillInProgress;
	CNetworkVar( float, m_fFillStartTime ); // time started filling with portal liquid, will teleport entities as they become completely enveloped
	CNetworkVar( float, m_fFillEndTime ); // time that filling should be finished and touching entities teleport

	CUtlVector<EHANDLE> m_hTeleportList; //list of entities to teleport when filling
	CUtlVector<EHANDLE> m_hLeftToTeleportThisFill; //list of entities that have not yet teleported during this fill, they get teleported out when fully immersed in the liquid

	string_t m_strInitialLinkedPortal;
};

#endif //#ifndef FUNC_LIQUIDPORTAL_H

