//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"
#include "gcsdk/gcsdk_auto.h"
#include "tf_lobby_server.h"

using namespace GCSDK;

const CTFLobbyMember* CTFGSLobby::GetMemberDetails( CSteamID steamID ) const
{
	for ( int i = 0; i < Obj().members_size(); i++ )
	{
		if ( Obj().members( i ).id() == steamID.ConvertToUint64() )
			return &Obj().members( i );
	}
	return NULL;
}

const CTFLobbyMember* CTFGSLobby::GetMemberDetails( int i ) const
{
	if ( !BAssertValidMemberIndex( i ) )
		return NULL;

	return &Obj().members( i );
}

const CSteamID CTFGSLobby::GetMember( int i ) const
{
	Assert( i >= 0 && i < Obj().members_size() );
	if ( i < 0 || i >= Obj().members_size() )
		return k_steamIDNil;

	return Obj().members( i ).id();
}

CTFLobbyMember_ConnectState CTFGSLobby::GetMemberConnectState( int iMemberIndex ) const
{
	if ( !BAssertValidMemberIndex( iMemberIndex ) )
		return CTFLobbyMember_ConnectState_INVALID;
	return Obj().members( iMemberIndex ).connect_state();
}

bool CTFGSLobby::BAssertValidMemberIndex( int iMemberIndex ) const
{
	bool bValidMemberIndex = iMemberIndex >= 0 && iMemberIndex < Obj().members_size();
	Assert( bValidMemberIndex );
	return bValidMemberIndex;
}

void CTFGSLobby::SpewDebug()
{
	Msg( "CTFGSLobby: ID:%016llx  %d member(s) allow_spectators: %d\n", GetGroupID(), GetNumMembers(), Obj().allow_spectating() );
	for ( int i = 0; i < GetNumMembers(); i++ )
	{
		Msg( "  Member[%d] %s  team = %d\n", i, GetMember( i ).Render(), GetMemberDetails( i )->team() );
	}
	Msg(" Dump:\n" );
	Dump();
}

#ifdef USE_MVM_TOUR
const char *CTFGSLobby::GetMannUpTourName() const
{
	if ( !IsMannUpGroup( GetMatchGroup() ) )
		return NULL;
	return Obj().mannup_tour_name().c_str();
}
#endif // USE_MVM_TOUR
