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
#include "hl1_c_player.h"
#include "hl1_player_shared.h"


class C_HL1MP_Player : public C_HL1_Player
{
public:
    DECLARE_CLASS( C_HL1MP_Player, C_HL1_Player );
    DECLARE_CLIENTCLASS();
    DECLARE_PREDICTABLE();
    DECLARE_INTERPOLATION();

    C_HL1MP_Player( void );
	~C_HL1MP_Player();

	virtual const QAngle& GetRenderAngles();
	virtual void UpdateClientSideAnimation();
	virtual void ProcessMuzzleFlashEvent();

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	virtual bool ShouldPredict( void );
	virtual void CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	IRagdoll* GetRepresentativeRagdoll() const;

    virtual void Spawn( void );
	virtual void AddEntity( void );
	virtual bool ShouldDraw( void );
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
    virtual void ClientThink( void );
	virtual C_BaseAnimating *BecomeRagdollOnClient();    
    //QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles; }
	const QAngle& EyeAngles( void )
	{
		if ( IsLocalPlayer() )
		{
			return BaseClass::EyeAngles();
		}
		else
		{
			return m_angEyeAngles;
		}
	}

	virtual ShadowType_t ShadowCastType()
	{
		if ( !IsVisible() )
			 return SHADOWS_NONE;
	
		return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
	}

	void PreThink( void );

	int	m_iRealSequence;

private:
	C_HL1MP_Player( const C_HL1MP_Player & );

    EHANDLE m_hRagdoll;

    QAngle m_angEyeAngles;

	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

    IHL1MPPlayerAnimState* m_PlayerAnimState;
    
    int m_iSpawnInterpCounter;
    int m_iSpawnInterpCounterCache;

	float m_fLastPredFreeze;
};


class C_HL1MPRagdoll : public C_BaseAnimatingOverlay
{
public:
	DECLARE_CLASS( C_HL1MPRagdoll, C_BaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();
	
	C_HL1MPRagdoll();
	~C_HL1MPRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	void UpdateOnRemove( void );
	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	
private:
	
	C_HL1MPRagdoll( const C_HL1MPRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );
	void CreateHL1MPRagdoll( void );

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};


inline C_HL1MP_Player *ToHL1MPPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_HL1MP_Player*>( pEntity );
}



#endif
