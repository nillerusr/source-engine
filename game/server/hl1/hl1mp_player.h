//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================// 

#ifndef HL1MP_PLAYER_H
#define HL1MP_PLAYER_H
#pragma once

#include "cbase.h"
#include "hl1_player_shared.h"
#include "hl1_player.h"
#include "takedamageinfo.h"


class CHL1MP_Player;


//=============================================================================
// >> HL1MP_Player
//=============================================================================
class CHL1MP_Player : public CHL1_Player
{
public:
	DECLARE_CLASS( CHL1MP_Player, CHL1_Player );
	DECLARE_SERVERCLASS();
    
	CHL1MP_Player();
	~CHL1MP_Player( void );
	
    virtual void Event_Killed( const CTakeDamageInfo &info );
    virtual void Spawn( void );
    virtual void PostThink( void );
    virtual void SetAnimation( PLAYER_ANIM playerAnim );
    void GiveDefaultItems( void );
    void CreateRagdollEntity( void );
    void UpdateOnRemove( void );
    virtual bool BecomeRagdollOnClient( const Vector &force ) { return true; };
    virtual void CreateCorpse( void );

	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );

	virtual void ChangeTeam( int iTeamNum ) OVERRIDE;

	void SetPlayerTeamModel( void );

	float GetNextModelChangeTime( void ) { return m_flNextModelChangeTime; }
	float GetNextTeamChangeTime( void ) { return m_flNextTeamChangeTime; }

    void SetPlayerModel( void );

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	virtual bool StartObserverMode (int mode) 
	{
		if ( !IsHLTV() )
			return false;	
		return BaseClass::StartObserverMode( mode );	
	}

	void DetonateSatchelCharges( void );

	CNetworkVar( int, m_iRealSequence );

private:
    CNetworkHandle( CBaseEntity, m_hRagdoll );
	CNetworkVar( int, m_iSpawnInterpCounter );
	CNetworkQAngle( m_angEyeAngles );

    IHL1MPPlayerAnimState*		m_PlayerAnimState;
	float						m_flNextModelChangeTime;
	float						m_flNextTeamChangeTime;
};

inline CHL1MP_Player *ToHL1MPPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CHL1MP_Player*>( pEntity );
}


#endif //HL1MP_PLAYER_H
