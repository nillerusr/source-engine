//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_PLAYER_SHARED_H
#define TFC_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#include "networkvar.h"
#include "tfc_shareddefs.h"
#include "weapon_tfcbase.h"


#ifdef CLIENT_DLL
	class C_TFCPlayer;
	EXTERN_RECV_TABLE( DT_TFCPlayerShared );
#else
	class CTFCPlayer;
	EXTERN_SEND_TABLE( DT_TFCPlayerShared );
#endif



// Data in the DoD player that is accessed by shared code.
// This data isn't necessarily transmitted between client and server.
class CTFCPlayerShared
{
public:

#ifdef CLIENT_DLL
	friend class C_TFCPlayer;
	typedef C_TFCPlayer OuterClass;
	DECLARE_PREDICTABLE();
#else
	friend class CTFCPlayer;
	typedef CTFCPlayer OuterClass;
#endif
	
	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CTFCPlayerShared );


	CTFCPlayerShared();
	
	void Init( OuterClass *pOuter );

	void SetPlayerClass( int playerclass );
	int GetPlayerClass() const;
	const CTFCPlayerClassInfo* GetClassInfo() const;

	// State.
	TFCPlayerState State_Get() const;

	// State flags (TFSTATE_).
	int GetStateFlags() const;
	void SetStateFlags( int val );
	void AddStateFlags( int flags );
	void RemoveStateFlags( int flags );

	// Item flags (IT_).
	int GetItemFlags() const;
	void SetItemFlags( int val );
	void AddItemFlags( int val );
	void RemoveItemFlags( int val );

	CWeaponTFCBase*	GetActiveTFCWeapon() const;
	
// Vars that are networked.
private:

	CNetworkVar( int, m_StateFlags ); // Combination of the TFSTATE_ flags.
	CNetworkVar( int, m_ItemFlags );
	CNetworkVar( int, m_iPlayerClass );
	CNetworkVar( TFCPlayerState, m_iPlayerState );


// Vars that aren't networked.
public:


private:
	
	OuterClass *m_pOuter;
};			   


inline int CTFCPlayerShared::GetStateFlags() const
{
	return m_StateFlags;
}

inline void CTFCPlayerShared::SetStateFlags( int val )
{
	m_StateFlags = val;
}

inline void CTFCPlayerShared::AddStateFlags( int flags )
{
	m_StateFlags |= flags;
}

inline void CTFCPlayerShared::RemoveStateFlags( int flags )
{
	m_StateFlags &= ~flags;
}

inline int CTFCPlayerShared::GetItemFlags() const
{
	return m_ItemFlags;
}

inline void CTFCPlayerShared::SetItemFlags( int val )
{
	m_ItemFlags = val;
}

inline void CTFCPlayerShared::AddItemFlags( int val )
{
	m_ItemFlags |= val;
}

inline void CTFCPlayerShared::RemoveItemFlags( int val )
{
	m_ItemFlags &= ~val;
}

inline const CTFCPlayerClassInfo* CTFCPlayerShared::GetClassInfo() const
{
	return GetTFCClassInfo( GetPlayerClass() );
}


#endif // TFC_PLAYER_SHARED_H
