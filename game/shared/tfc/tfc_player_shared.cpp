//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tfc_gamerules.h"
#include "tfc_player_shared.h"
#include "takedamageinfo.h"
#include "weapon_tfcbase.h"


#ifdef CLIENT_DLL
	
	#include "c_tfc_player.h"

	BEGIN_RECV_TABLE_NOBASE( CTFCPlayerShared, DT_TFCPlayerShared )
		RecvPropInt( RECVINFO( m_iPlayerClass ) ),
		RecvPropInt( RECVINFO( m_iPlayerState ) ),
		RecvPropInt( RECVINFO( m_StateFlags ) ),
		RecvPropInt( RECVINFO( m_ItemFlags ) )
	END_RECV_TABLE()

	BEGIN_PREDICTION_DATA_NO_BASE( CTFCPlayerShared )
		DEFINE_PRED_FIELD( m_iPlayerState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE )
	END_PREDICTION_DATA()

#else

	#include "tfc_player.h"

	BEGIN_SEND_TABLE_NOBASE( CTFCPlayerShared, DT_TFCPlayerShared )
		SendPropInt( SENDINFO(m_iPlayerClass), NumBitsForCount( PC_LASTCLASS+1 ), SPROP_UNSIGNED ),
		SendPropInt( SENDINFO(m_iPlayerState), NumBitsForCount( TFC_NUM_PLAYER_STATES ), SPROP_UNSIGNED ),
		SendPropInt( SENDINFO(m_StateFlags)  , NumBitsForCount( TFSTATE_HIGHEST_VALUE+1 ), SPROP_UNSIGNED ),
		SendPropInt( SENDINFO(m_StateFlags)  , NumBitsForCount( IT_LAST_ITEM+1 ), SPROP_UNSIGNED )
	END_SEND_TABLE()

#endif


// --------------------------------------------------------------------------------------------------- //
// Shared CTFCPlayer implementation.
// --------------------------------------------------------------------------------------------------- //



// --------------------------------------------------------------------------------------------------- //
// CTFCPlayerShared implementation.
// --------------------------------------------------------------------------------------------------- //

CTFCPlayerShared::CTFCPlayerShared()
{
	m_iPlayerClass = PC_UNDEFINED;
	m_iPlayerState = STATE_WELCOME;
}


void CTFCPlayerShared::Init( CTFCPlayer *pPlayer )
{
	m_pOuter = pPlayer;
}


void CTFCPlayerShared::SetPlayerClass( int playerclass )
{
	m_iPlayerClass = playerclass;
}

int CTFCPlayerShared::GetPlayerClass() const
{
	return m_iPlayerClass;
}


TFCPlayerState CTFCPlayerShared::State_Get() const
{
	return m_iPlayerState;
}


CWeaponTFCBase*	CTFCPlayerShared::GetActiveTFCWeapon() const
{
	CBaseCombatWeapon *pRet = m_pOuter->GetActiveWeapon();
	if ( pRet )
	{
		Assert( dynamic_cast< CWeaponTFCBase* >( pRet ) != NULL );
		return static_cast< CWeaponTFCBase * >( pRet );
	}
	else
	{
		return NULL;
	}
}


