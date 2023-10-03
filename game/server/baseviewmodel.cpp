//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "animation.h"
#include "baseviewmodel.h"
#include "player.h"
#include <KeyValues.h>
#include "studio.h"
#include "vguiscreen.h"
#include "saverestore_utlvector.h"
#include "hltvdirector.h"
#include "replaydirector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void SendProxy_AnimTime( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_SequenceChanged( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: Save Data for Base Weapon object
//-----------------------------------------------------------------------------// 
BEGIN_DATADESC( CBaseViewModel )

	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),

// Client only
//	DEFINE_FIELD( m_LagAnglesHistory, CInterpolatedVar < QAngle > ),
//	DEFINE_FIELD( m_vLagAngles, FIELD_VECTOR ),

	DEFINE_FIELD( m_nViewModelIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT ),
	DEFINE_FIELD( m_nAnimationParity, FIELD_INTEGER ),

	// Client only
//	DEFINE_FIELD( m_nOldAnimationParity, FIELD_INTEGER ),

	DEFINE_FIELD( m_vecLastFacing, FIELD_VECTOR ),
	DEFINE_FIELD( m_hWeapon, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_hScreens, FIELD_EHANDLE ),

// Read from weapons file
//	DEFINE_FIELD( m_sVMName, FIELD_STRING ),
//	DEFINE_FIELD( m_sAnimationPrefix, FIELD_STRING ),

// ---------------------------------------------------------------------

// Don't save these, init to 0 and regenerate
//	DEFINE_FIELD( m_Activity, FIELD_INTEGER ),

END_DATADESC()

int CBaseViewModel::UpdateTransmitState()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

int CBaseViewModel::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Check if recipient owns this weapon viewmodel
	CBasePlayer *pOwner = ToBasePlayer( m_hOwner );

	if ( pOwner && 
		 ( pOwner->edict() == pInfo->m_pClientEnt ||
			// If we're using the other guys network connection in split screen, then also force transmit
		 pOwner->IsSplitScreenUserOnEdict( pInfo->m_pClientEnt ) ) )
	{
		return FL_EDICT_ALWAYS;
	}

	// check if recipient (or one of his splitscreen parasites) is spectating the owner of this viewmodel
	CBasePlayer *pPlayer = ToBasePlayer( CBaseEntity::Instance( pInfo->m_pClientEnt ) );
	if ( pPlayer)
	{
		// Bug 28591:  In splitscreen, when the second slot is the one in spectator mode, it wouldn't
		//  get the viewmodel for the spectatee.
		//
		// The new logic is to loop through the splitscreen parasites (as well as the host player) 
		//  and see if any of them are observing the viewmodel owner, and if so, FL_EDICT_ALWAYS the vm for them, too.

		CUtlVector< CBasePlayer * > checkList;
		checkList.AddToTail( pPlayer );
		CUtlVector< CHandle< CBasePlayer > > &vecParasites = pPlayer->GetSplitScreenPlayers();
		for ( int i = 0; i < vecParasites.Count(); ++i )
		{
			checkList.AddToTail( vecParasites[ i ] );
		}

		for ( int i = 0; i < checkList.Count(); ++i )
		{
			CBasePlayer *pPlayer = checkList[ i ];
			if ( !pPlayer )
				continue;

			if ( pPlayer->IsHLTV() || pPlayer->IsReplay() )
			{
				// if this is the HLTV or Replay client, transmit all viewmodels in our PVS
				return FL_EDICT_PVSCHECK;
			}
			if ( (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)  && (pPlayer->GetObserverTarget() == pOwner) )
			{
				return FL_EDICT_ALWAYS;
			}
		}
	}

	// Don't send to anyone else except the local player or his spectator
	return FL_EDICT_DONTSEND;
}

void CBaseViewModel::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );
	
	// Force our screens to be sent too.
	for ( int i=0; i < m_hScreens.Count(); i++ )
	{
		CVGuiScreen *pScreen = m_hScreens[i].Get();
		if ( pScreen )
			pScreen->SetTransmit( pInfo, bAlways );
	}
}
