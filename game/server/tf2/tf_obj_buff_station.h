//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's portable power generator (Buff Station)
//
//=============================================================================//

#include "tf_obj.h"

//=============================================================================
//
// Portable Power Generator Class (Buff Station)
//
class CObjectBuffStation : public CBaseObject  
{

DECLARE_CLASS( CObjectBuffStation, CBaseObject );

public:

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CObjectBuffStation();

	static CObjectBuffStation *Create(const Vector &vOrigin, const QAngle &vAngles);

	void	Spawn();
	void	Precache();
	void	SetupTeamModel( void ); 
	void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void	DestroyObject( void );
	void	OnGoInactive( void );
	bool	CalculatePlacement( CBaseTFPlayer *pPlayer );
	void	FinishedBuilding( void );

	// Attach/Detach 
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );

	// EMP
	bool	CanTakeEMPDamage( void ) { return true; }

	// Buff
	void	DeBuffObject( CBaseObject *pObject );
	void	BuffNearbyObjects( CBaseObject *pObjectToTarget, bool bPlacing );
	void	CheckBuffConnection( CBaseObject *pObject );

	virtual void	OnActivityChanged( Activity act );
private:

	void			InitAttachmentData( void );
	CRopeKeyframe	*CreateRope( CBaseTFPlayer *pPlayer, int iAttachPoint );
	CRopeKeyframe	*CreateRope( CBaseObject *pObject, int iAttachPoint, int iObjectAttachPoint );

	// Attach/Detach
	bool			IsPlayerAttached( CBaseTFPlayer *pPlayer );
	bool			IsObjectAttached( CBaseObject *pObject );

	void			AttachPlayer( CBaseTFPlayer *pPlayer );
	void			DetachPlayer( CBaseTFPlayer *pPlayer );
	void			DetachPlayerByIndex( int nIndex );
	void			AttachObject( CBaseObject *pObject, bool bPlacing );
	void			DetachObject( CBaseObject *pObject );
	void			DetachObjectByIndex( int nIndex );

	void			UpdatePlayerAttachment( CBaseTFPlayer *pPlayer );

	void			SwapObjectAttachment( int nIndex );

	// Input handlers
	void			InputPlayerSpawned( inputdata_t &inputdata );
	void			InputPlayerAttachedToGenerator( inputdata_t &inputdata );

	// Buff Helpers
	bool			IsWithinBuffRange( CBaseObject *pObject );
	CBaseObject		*GetBuffedObject( int iIndex );

	// Think
	void			BoostPlayerThink( void );
	void			BoostObjectThink( void );

private:

	struct AttachInfo_t
	{
		CDamageModifier			m_DamageModifier;
		CHandle<CRopeKeyframe>	m_hRope;
		int						m_iAttachPoint;
	};

	typedef CHandle<CBaseTFPlayer> CPlayerHandle;
	CNetworkArray( CPlayerHandle, m_hPlayers, BUFF_STATION_MAX_PLAYERS );

	CNetworkVar( int, m_nPlayerCount );
	AttachInfo_t	m_aPlayerAttachInfo[BUFF_STATION_MAX_PLAYERS];

	typedef CHandle<CBaseObject> CObjectHandle;
	CNetworkArray( CObjectHandle, m_hObjects, BUFF_STATION_MAX_OBJECTS );

	CNetworkVar( int, m_nObjectCount );
	AttachInfo_t	m_aObjectAttachInfo[BUFF_STATION_MAX_OBJECTS];

	bool			m_bBuilding;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CBaseObject *CObjectBuffStation::GetBuffedObject( int iIndex )
{
	if ( ( iIndex >= 0 ) && ( iIndex < m_nObjectCount ) )
	{
		return m_hObjects[iIndex].Get();
	}

	return NULL;
}