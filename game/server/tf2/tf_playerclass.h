//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef TF_PLAYERCLASS_H
#define TF_PLAYERCLASS_H

#ifdef _WIN32
#pragma once
#endif


#include "tier0/fasttimer.h"
#include <crtdbg.h>

class CPlayerClassData;
class CPlayerClass;
class CBaseTFPlayer;
class COrder;
class CBaseObject;
class CBaseTechnology;
class CWeaponCombatShield; 


enum ResupplyReason_t
{
	RESUPPLY_RESPAWN = 0,
	RESUPPLY_ALL_FROM_STATION,
	RESUPPLY_AMMO_FROM_STATION,
	RESUPPLY_GRENADES_FROM_STATION,
};


//==============================================================================
// PLAYER CLASSES
//==============================================================================

class CBaseTFPlayer;
class CTFTeam;

// Base PlayerClass
// The PlayerClass classes handle all class specific weaponry / abilities / etc
class CPlayerClass
{
public:
	DECLARE_CLASS_NOBASE( CPlayerClass );
	CPlayerClass( CBaseTFPlayer *pPlayer, TFClass iClass );
	virtual ~CPlayerClass();

	// Any objects created/owned by class should be allocated and destroyed here
	virtual void			ClassActivate( void );
	virtual void			ClassDeactivate( void );

	// Class initialization
	virtual void		CreateClass( void );				// Create the class upon initial spawn
	virtual void		RespawnClass( void );				// Called upon all respawns
	virtual bool		ResupplyAmmo( float flPercentage, ResupplyReason_t reason );	// Reset the ammo counts
	virtual bool		ResupplyAmmoType( float flAmount, const char *pAmmoType ); // Purpose: Supply the player with Ammo. Return true if some ammo was given.

	virtual void		SetMaxHealth( float flMaxHealth );	// Set the player's max health
	int					GetMaxHealthCVarValue();			// Return the player class's max health cvar
	
	virtual float		GetMaxSpeed( void );				// Calculate and return the player's max speed
	virtual	float		GetMaxWalkSpeed( void );			// Calculate and return the player's max walking speed
	virtual void		SetMaxSpeed( float flMaxSpeed );	// Set the player's max speed
	
	virtual string_t	GetClassModel( int nTeam );			// Return a string containing this class's model
	virtual const char*	GetClassModelString( int nTeam );

	virtual void		SetupMoveData( void );				// Setup the default player movement data.
	virtual void		SetupSizeData( void );

	virtual bool		CanSeePlayerOnRadar( CBaseTFPlayer *pl );
	virtual void		ItemPostFrame( void );
	virtual bool		ClientCommand( const CCommand &args );

	// Class abilities
	virtual void		ClassThink( void );
	virtual void		GainedNewTechnology( CBaseTechnology *pTechnology );	// New technology has been gained

	// Deployment
	virtual float		GetDeployTime( void ) { return 0.0; };

	// Resources
	virtual int			ClassCostAdjustment( ResupplyBuyType_t nType ) { return 0; }

	// Objects
	int					GetNumObjects( int iObjectType );
	virtual int			CanBuild( int iObjectType );
	virtual int			StartedBuildingObject( int iObjectType );
	virtual void		StoppedBuilding( int iObjectType );
	virtual void		FinishedObject( CBaseObject *pObject );
	virtual void		PickupObject( CBaseObject *pObject );
	virtual void		OwnedObjectDestroyed( CBaseObject *pObject );
	virtual void		OwnedObjectChangeToTeam( CBaseObject *pObject, CBaseTFPlayer *pNewOwner );
	virtual void		OwnedObjectChangeFromTeam( CBaseObject *pObject, CBaseTFPlayer *pOldOwner );
	virtual void		CheckDeterioratingObjects( void );

	// Hooks
	virtual float		OnTakeDamage( const CTakeDamageInfo &info );
	virtual bool		ShouldApplyDamageForce( const CTakeDamageInfo &info );

	// Vehicles
	virtual void		OnVehicleStart() {}
	virtual void		OnVehicleEnd() {}
	virtual bool		CanGetInVehicle( void ) { return true; }

	virtual void		PlayerDied( CBaseEntity *pAttacker );
	virtual void		PlayerKilledPlayer( CBaseTFPlayer *pVictim );

	virtual void		SetPlayerHull( void );
	virtual void		GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax );

	// Player Physics Shadow
	virtual void		InitVCollision( void );

	// Powerups
	virtual void		PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual void		PowerupEnd( int iPowerup );

	// Camo
	virtual void		ClearCamouflage( void ) { return; };

	// Disguise
	virtual void		FinishedDisguising( void ) { return; };
	virtual void		StopDisguising( void ) { return; };

	// Orders
	virtual void		CreatePersonalOrder( void );

	// Create a high-priority order. This should be called by all player classes before
	// they try to create class-specific orders. This function returns true if an order is
	// created.
	bool				CreateInitialOrder();
	
	bool				AnyResourceZoneOrders();
	bool				AnyNonResourceZoneOrders();		// Returns true if there are any non-resource-zone orders.
														// If there are, then no class should make any overriding orders.

	// Respawn ( allow classes to override spawn points )
	virtual CBaseEntity		*SelectSpawnPoint( void );

    void *operator new( size_t stAllocateBlock )	
	{												
		Assert( stAllocateBlock != 0 );				
		void *pMem = malloc( stAllocateBlock );
		memset( pMem, 0, stAllocateBlock );
		return pMem;												
	}
	
	void* operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )  
	{ 
		Assert( stAllocateBlock != 0 );				
		void *pMem = _malloc_dbg( stAllocateBlock, nBlockUse, pFileName, nLine );
		memset( pMem, 0, stAllocateBlock );
		return pMem;												
	}

	void operator delete( void *pMem )
	{
		free( pMem );
	}

	void SetClassModel( string_t sModelName, int nTeam )	{ m_sClassModel[nTeam] = sModelName; }

	// Weapon & Tech Associations
	void AddWeaponTechAssoc( char *pWeaponTech );

	
	// Accessors.
	inline CBaseTFPlayer*	GetPlayer() { return m_pPlayer; }
	CTFTeam*				GetTeam();
	
	virtual void ResetViewOffset( void );

	virtual TFClass			GetTFClass( void );

	void AddWeaponTechAssociations( void );

	// For CNetworkVar support. Chain to the player entity.
	void NetworkStateChanged();

	TFClass					m_TFClass;

protected:
	double m_flNormalizedEngagementNextTime;	

	CBaseTFPlayer			*m_pPlayer;		// Reference to the player

	float					m_flMaxWalkingSpeed;

	string_t				m_sClassModel[ MAX_TF_TEAMS + 1 ];

	// Weapon & Tech associations
	// Used to give out all weapons the player currently has the technologies for.
	struct WeaponTechAssociation_t
	{
		char	*pWeaponTech;
	};
	WeaponTechAssociation_t		m_WeaponTechAssociations[ MAX_WEAPONS ];
	int							m_iNumWeaponTechAssociations;

	CHandle<CWeaponCombatShield> m_hWpnShield;
private:
	void ClearAllWeaponTechAssoc( void );

private:
	bool	m_bTechAssociationsSet;
};

#endif // TF_PLAYERCLASS_H