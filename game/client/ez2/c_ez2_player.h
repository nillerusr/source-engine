#ifndef C_EZ2_PLAYER_H
#define C_EZ2_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlplayer.h"

class C_PointDetonatable : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PointDetonatable, C_BaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_PointDetonatable();
	~C_PointDetonatable();

	bool		ShouldDraw() { return false; }

	bool		m_bDisabled;
	EHANDLE		m_hThrower;
	EHANDLE		m_hGlowTarget;
};

class C_EZ2_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_EZ2_Player , C_BaseHLPlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_EZ2_Player();
	~C_EZ2_Player();

	void OnDataChanged( DataUpdateType_t updateType );

	void BonusChallengeUpdate( );

	void UpdateGlowTargetEffect( void );
	void DestroyGlowTargetEffect( void );

	void UpdateSLAMGlowEffect( void );
	void DestroySLAMGlowEffect( void );

	bool m_bBonusChallengeUpdate;
	
	EHANDLE m_hWarningTarget;
	CGlowObject *m_pGlowTargetEffect;

	CUtlVector<EHANDLE>	m_hActiveSatchels;
	CUtlVector<EHANDLE>	m_hActiveTripmines;
	CUtlVector< CHandle<C_PointDetonatable> >	m_hActiveDetonatables;
	CUtlVector<CGlowObject*>	m_pSLAMGlowEffects;
};


inline C_EZ2_Player* ToEZ2Player( CBaseEntity *pPlayer )
{
	Assert( dynamic_cast<C_EZ2_Player*>( pPlayer ) != NULL );
	return static_cast<C_EZ2_Player*>( pPlayer );
}


#endif // C_SDK_PLAYER_H