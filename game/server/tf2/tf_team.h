//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_TEAM_H
#define TF_TEAM_H

#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "tf_shareddefs.h"
#include "techtree.h"
#include "team.h"
#include "order_events.h"

class CBaseTFPlayer;
class CResourceZone;
class CTeamSpawnPoint;
class CBaseTFPlayer;
class CResourceDrop;
class CTechnologyTree;
class CBaseTechnology;
class CObjectResupply;
class CBaseObject;
class COrder;
class CTeamMessage;
class CObjectPowerPack;
class CObjectBuffStation;


enum
{
	COUNTORDERS_TYPE	=	(1<<0),
	COUNTORDERS_TARGET	=	(1<<1),
	COUNTORDERS_OWNER	=	(1<<2)
};


// Maximum total number of objects a team can have.
#define MAX_TEAM_OBJECTS 1024


//-----------------------------------------------------------------------------
// Purpose: Team Manager
//-----------------------------------------------------------------------------
class CTFTeam : public CTeam
{
	DECLARE_CLASS( CTFTeam, CTeam );
public:
	virtual ~CTFTeam( void );

	DECLARE_SERVERCLASS();

	// Initialization
	virtual void Init( const char *pName, int iNumber );
	
	virtual void Precache( void );
	virtual void PrecacheTechnology( CBaseTechnology *pTech );
	virtual void Think( void );

	//-----------------------------------------------------------------------------
	// Data Handling
	//-----------------------------------------------------------------------------
	virtual void UpdateClientData( CBasePlayer *pPlayer );
	virtual void UpdateClientTechnology( int iTechID, CBaseTFPlayer *pPlayer );
	virtual void UpdateTechnologyData( void );
	virtual bool ShouldTransmitToPlayer( CBasePlayer *pRecipient, CBaseEntity* pEntity );
	virtual bool IsEntityVisibleToTactical( CBaseEntity *pEntity );

	//-----------------------------------------------------------------------------
	// Resources
	//-----------------------------------------------------------------------------
	virtual void UpdatePotentialResources( void );

	virtual void AddResourceZone( CResourceZone *pResource );
	virtual void RemoveResourceZone( CResourceZone *pResource );

	// Handling for players joining the team during the game
	float		 GetJoiningPlayerResources( void );
	void		 SetRecentBankSet( float flResources );

	//-----------------------------------------------------------------------------
	// Technology Tree
	//-----------------------------------------------------------------------------
	virtual void InitializeTechTree( void );
	virtual CTechnologyTree *GetTechnologyTree( void );
	virtual void EnableTechnology( CBaseTechnology *technology, bool bStolen = false );		// Give this team a technology
	virtual void EnableAllTechnologies( void );
	virtual void RecomputeTeamResources( void );
	virtual void RecomputePreferences( void );
	virtual void RecomputePurchases( void );
	virtual bool HasNamedTechnology( const char *name );
	virtual void GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual void UpdateTechnologies( void );
	
	//-----------------------------------------------------------------------------
	// Players
	//-----------------------------------------------------------------------------
	virtual void AddPlayer( CBasePlayer *pPlayer );
	virtual void RemovePlayer( CBasePlayer *pPlayer );
	int		GetNumOfClass( TFClass iClass );

	//-----------------------------------------------------------------------------
	// Resource Bank
	//-----------------------------------------------------------------------------
	void	InitializeTeamResources( void );
	float	GetTeamResources( void );
	int		AddTeamResources( float fAmount, int nStat = -1 );
	void	ResourceLoadDeposited( void );
	void	DonateResources( CBaseTFPlayer *pPlayer );

	//-----------------------------------------------------------------------------
	// Objects
	//-----------------------------------------------------------------------------
	void	AddObject( CBaseObject *pObject );
	void	RemoveObject( CBaseObject *pObject );
	bool	IsObjectOnTeam( CBaseObject *pObject ) const;
	void	AddResupply( CObjectResupply *pResupply );
	void	RemoveResupply( CObjectResupply *pResupply );
	int		GetNumObjects( int iObjectType = -1 );
	CBaseObject		*GetObject( int num );
	int		GetNumResupplies( void );
	CObjectResupply *GetResupply( int num );

	// Returns true if the position is covered by a sentry gun.
	bool	IsCoveredBySentryGun( const Vector &vPos );

	int		GetNumShieldWallsCoveringPosition( const Vector &vPos );
	int		GetNumResuppliesCoveringPosition( const Vector &vPos );
	int		GetNumRespawnStationsCoveringPosition( const Vector &vPos );


	//-----------------------------------------------------------------------------
	// Orders
	//-----------------------------------------------------------------------------
	void	InitializeOrders( void );
	
	COrder*	AddOrder( 
		int iOrderType, 
		CBaseEntity *pTarget, 
		CBaseTFPlayer *pPlayer = NULL, 
		float flDistanceToRemove = 1e24, 
		float flLifetime = 60,
		COrder *pDefaultOrder = NULL	// If this is specified, then it is used instead of 
										// asking COrder to allocate an order.
		);
	void	RemoveOrder( COrder *pOrder );
	void	RecalcOrders( void );
	void	UpdateOrders( void );
	void	UpdateOrdersOnEvent( COrderEvent_Base *pEvent );
	
	// Flags is a combination of COUNTORDERS_ flags telling which fields to check.
	int		CountOrders( int flags, int iOrderType, CBaseEntity *pTarget=0, CBaseTFPlayer *pOwner=0 );
	int		CountOrdersOwnedByPlayer( CBaseTFPlayer *pPlayer );

	void	CreatePersonalOrders( void );
	void	CreatePersonalOrder( CBaseTFPlayer *pPlayer );
	void	RemoveOrdersToPlayer( CBaseTFPlayer *pPlayer );

	//-----------------------------------------------------------------------------
	// Messages
	//-----------------------------------------------------------------------------
	void	ClearMessages( void );
	void	PostMessage( int iMessageID, CBaseEntity *pEntity = NULL, char *sData = NULL );
	void	UpdateMessages( void );

	void	UpdatePowerpacks( CObjectPowerPack *pPackToIgnore, CBaseObject *pObjectToTarget );
	void	UpdateBuffStations( CObjectBuffStation *pBuffStationToIgnore, CBaseObject *pObjectToTarget, bool bPlacing );

	//-----------------------------------------------------------------------------
	// Utility funcs
	//-----------------------------------------------------------------------------
	CTFTeam*		GetEnemyTeam();

	// Technology Tree
	CTechnologyTree *m_pTechnologyTree;

	// TEST CODE
	// Remove after Resource Experiment!
	float	m_flTotalResourcesSoFar;

private:
	typedef CHandle<COrder>	OrderHandle;

	// Resource UI data
	CNetworkVar( bool, m_bHaveZone );

	// Resources
	CNetworkVar( float, m_fResources );									// Current amounts of resource
	CNetworkVar( float, m_fPotentialResources );							// Amounts of resource when all collectors have returned
	float	m_flLastBankSetAmount;							// Most recent amount of resources our players had their banks set to
	float	m_flLastBankSetTime;							// Time at which our players last had their banks set

	// Orders
	float	m_flPersonalOrderUpdateTime;

	// Used to distribute resources to a team
	float	m_flNextResourceTime;

	int		m_iLastUpdateSentAt;

	CUtlVector< CResourceZone * >		m_aResourcesBeingCollected;
	CUtlVector< CObjectResupply * >		m_aResupplyBeacons;
	CUtlVector< OrderHandle >			m_aOrders;				// Stored in order of priority
	CUtlVector< CTeamMessage* >			m_aMessages;
	CUtlVector< CBaseObject* >			m_aObjects;
};


extern CTFTeam *GetGlobalTFTeam( int iIndex );


#endif // TF_TEAM_H
