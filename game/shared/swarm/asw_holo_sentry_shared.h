#ifndef _INCLUDED_ASW_HOLO_SENTRY_H
#define _INCLUDED_ASW_HOLO_SENTRY_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Holo_Sentry C_ASW_Holo_Sentry
#endif

// This class is used to hint where sentries should be placed

class CASW_Holo_Sentry : public CBaseEntity
#ifndef CLIENT_DLL
	, public CGameEventListener
#endif
{
public:
	DECLARE_CLASS( CASW_Holo_Sentry, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CASW_Holo_Sentry();
	virtual ~CASW_Holo_Sentry();

	virtual void UpdateOnRemove();

#ifndef CLIENT_DLL
public:
	DECLARE_DATADESC();

	static bool VismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

	virtual void Precache();
	virtual void Spawn();
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }

	virtual void FireGameEvent( IGameEvent *event );

	void CASW_Holo_Sentry::InputEnable( inputdata_t &inputdata );
	void CASW_Holo_Sentry::InputDisable( inputdata_t &inputdata );

private:
	COutputEvent m_OnSentryPlaced;

	bool m_bStartDisabled;
#else
public:
	virtual void OnDataChanged( DataUpdateType_t type );

private:
	void CreateSentryBuildDisplay();
	void DestroySentryBuildDisplay();

private:
	CUtlReference<CNewParticleEffect> m_hSentryBuildDisplay;
#endif

private:
	CNetworkVar( bool, m_bEnabled );
};

#endif /* _INCLUDED_ASW_HOLO_SENTRY_H */
