//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#if !defined( C_BASETFPLAYER_H )
#define C_BASETFPLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_vehicleshared.h"
#include "c_baseplayer.h"
#include "CommanderOverlay.h"
#include "hud_minimap.h"
#include "hud_targetreticle.h"
#include "c_tfplayerlocaldata.h"
#include "particlemgr.h"
#include "particle_prototype.h"
#include "particle_util.h"
#include "tf_playeranimstate.h"

class C_VehicleTeleportStation;
class CViewSetup;
class IMaterial;
class C_Order;
class C_BaseObject;
class C_PlayerClass;
class CPersonalShieldEffect;
class CBasePredictedWeapon;
class C_BaseViewModel;
class CUserCmd;
class C_WeaponCombatShield;

//-----------------------------------------------------------------------------
// Purpose: Client Side TF Player entity
//-----------------------------------------------------------------------------
class C_BaseTFPlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_BaseTFPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL();
	DECLARE_PREDICTABLE();

					C_BaseTFPlayer();
	virtual			~C_BaseTFPlayer();

private:
	void			Clear();	// Clear all elements.

public:
	bool			IsHidden() const;
	bool			IsDamageBoosted() const;

	bool			HasNamedTechnology( const char *name );

	float			LastAttackTime() const { return m_flLastAttackTime; }
	void			SetLastAttackTime( float flTime ) { m_flLastAttackTime = flTime; }

	// Return this client's C_BaseTFPlayer pointer
	static C_BaseTFPlayer* GetLocalPlayer( void )
	{
		return ( static_cast< C_BaseTFPlayer * >( C_BasePlayer::GetLocalPlayer() ) );
	}

	virtual void		ClientThink( void );
	virtual void		OnPreDataChanged( DataUpdateType_t updateType );
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		PreDataUpdate( DataUpdateType_t updateType );
	virtual void		PostDataUpdate( DataUpdateType_t updateType );
	virtual void		ReceiveMessage( int classID, bf_read &msg );
	virtual void		Release( void );

	virtual void		ItemPostFrame( void );

	virtual bool		ShouldDraw();
	virtual int			DrawModel( int flags );

	virtual void		SetDormant( bool bDormant );

	virtual void		GetBoneControllers(float controllers[MAXSTUDIOBONES]);

	virtual int			GetRenderTeamNumber( void );
	virtual void		ComputeFxBlend( void );
	virtual bool		IsTransparent( void );

	// Called by the view model if its rendering is being overridden.
	virtual bool		ViewModel_IsTransparent( void );

	virtual bool		IsOverridingViewmodel( void );
	virtual int			DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags );
	virtual void		CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual float		GetDefaultAnimSpeed( void );

	int					GetClass( void );

	// Called when not in tactical mode. Allows view to be overriden for things like driving a tank.
	void				OverrideView( CViewSetup *pSetup );

	// Called when not in tactical mode. Allows view model drawing to be disabled for things like driving a tank.
	bool				ShouldDrawViewModel();

	void				GetTargetDescription( char *pDest, int bufferSize );

	// Orders
	void				SetPersonalOrder( C_Order *pOrder );
	void				RemoveOrderTarget();

	// Resources
	int					GetBankResources( void );

	// Objects
	void				SetSelectedObject( C_BaseObject *pObject );
	C_BaseObject		*GetSelectedObject( void );
	int					GetNumObjects( int iObjectType );
	int					GetObjectCount( void );
	C_BaseObject		*GetObject( int index );

	// Targets
	void				Add_Target( C_BaseEntity *pTarget, const char *sName );
	void				Remove_Target( C_BaseEntity *pTarget );
	void				Remove_Target( CTargetReticle *pTargetReticle );

	void				UpdateTargetReticles( void );

	bool				IsUsingThermalVision( void ) const;

	// Weapon handling
	virtual bool				IsAllowedToSwitchWeapons( void );
	virtual bool				Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon );
	virtual bool				Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon );
	virtual	C_BaseCombatWeapon	*GetActiveWeaponForSelection( void );
	virtual C_BaseCombatWeapon  *GetLastWeaponBeforeObject( void ) { return m_hLastWeaponBeforeObject; }

	virtual C_BaseAnimating* GetRenderedWeaponModel();

	// ID Target
	void				SetIDEnt( C_BaseEntity *pEntity );
	int					GetIDTarget( void ) const;
	void				UpdateIDTarget( void );

	bool				IsKnockedDown( void ) const;
	void				CheckKnockdownState( void );
	bool				CheckKnockdownAngleOverride( void ) const;
	void				SetKnockdownAngles( const QAngle& ang );
	void				GetKnockdownAngles( QAngle& outAngles );
	float				GetKnockdownViewheightAdjust( void ) const;

	// Team handling
	virtual void		TeamChange( int iNewTeam );

	// Camouflage effect
	virtual bool		IsCamouflaged( void );
	virtual float		GetCamouflageAmount( void );
	virtual float		ComputeCamoEffectAmount( void );	// 0 - visible, 1 = invisible
	virtual int			ComputeCamoAlpha( void );
	virtual void		CheckCamoDampening( void );
	virtual void		SetCamoDampening( float amount );
	virtual float		GetDampeningAmount( void );
	virtual void		CheckCameraMovement( void );

	IMaterial			*GetCamoMaterial( void );
	virtual float		GetMovementCamoSuppression( void );
	virtual void		CheckMovementCamoSuppression( void );
	virtual void		SetMovementCamoSuppression( float amount );

	// Adrenalin
	void				CheckAdrenalin( void );

	void				CheckLastMovement( void );
	float				GetLastMoveTime( void );
	float				GetOverlayAlpha( void );

	float				GetLastDamageTime( void ) const;
	float				GetLastGainHealthTime( void ) const;
	
	// Powerups
	virtual void		PowerupStart( int iPowerup, bool bInitial );
	virtual void		PowerupEnd( int iPowerup );

	// Sniper
	bool				IsDeployed( void );
	bool				IsDeploying( void );
	bool				IsUnDeploying( void );

	virtual void		AddEntity( void );

	// Vertification
	inline bool HasClass( void ) { return GetPlayerClass() != NULL; }

	bool	IsClass( TFClass iClass );


	virtual int	GetMaxHealth() const { return m_iMaxHealth; }

	bool ClassProxyUpdate( int nClassID );

	// Should this object cast shadows?
	virtual ShadowType_t		ShadowCastType();

	// Prediction stuff
	virtual void				PreThink( void );
	virtual void				PostThink( void );

	// Combat prototyping
	bool		IsBlocking( void ) const { return m_bIsBlocking; }
	bool		IsParrying( void ) const { return m_bIsParrying; }
	void		SetBlocking( bool bBlocking ) { m_bIsBlocking = bBlocking; }
	void		SetParrying( bool bParrying ) { m_bIsParrying = bParrying; }

	// Vehicles
	void				SetVehicleRole( int nRole );
	bool				CanGetInVehicle( void );
	
	// Returns true if we're in a vehicle and it's mounted on another vehicle
	// (ie: are we in a manned gun that's mounted on a tank).
	bool IsVehicleMounted() const;

	virtual bool		IsUseableEntity( CBaseEntity *pEntity );

// Shared Client / Server code
public:
	bool					IsHittingShield( const Vector &vecVelocity, float *flDamage );
	C_WeaponCombatShield	*GetCombatShield( void );
	virtual void			PainSound( void );

public:
	// Data for only the local player
	CTFPlayerLocalData		m_TFLocal;

	// Accessors...
	const QAngle& DeployedAngles() const	{ return m_vecDeployedAngles; }
	int ZoneState() const					{ return m_iCurrentZoneState; }
	float CamouflageAmount() const			{ return m_flCamouflageAmount; }
	int	PlayerClass() const					{ return m_iPlayerClass; }
	C_PlayerClass *GetPlayerClass();
	C_Order *PersonalOrder()				{ return m_hPersonalOrder; }
	C_BaseEntity *SpawnPoint()				{ return m_hSpawnPoint.Get(); }

	C_VehicleTeleportStation* GetSelectedMCV() const;

	// Object sapper placement handling
	//float							m_flSapperAttachmentFinishTime;
	//float							m_flSapperAttachmentStartTime;
	//CHandle< CGrenadeObjectSapper >	m_hSapper;
	//CHandle< CBaseObject >			m_hSappedObject;
	CHealthBarPanel					*m_pSapperAttachmentStatus;

private:
	C_BaseTFPlayer( const C_BaseTFPlayer & );

	// Client-side obstacle avoidance
	void PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );

	float		m_flLastAttackTime;

	EHANDLE m_hSelectedMCV;

	// Weapon used before switching to an object placement
	CHandle<C_BaseCombatWeapon>	m_hLastWeaponBeforeObject;

	float		m_flCamouflageAmount;

	// Movement.
	Vector		m_vecPosDelta;

	enum { MOMENTUM_MAXSIZE = 10 };
	float		m_aMomentum[MOMENTUM_MAXSIZE];
	int			m_iMomentumHead;

	// Player Class
	int							m_iPlayerClass;
	C_AllPlayerClasses			m_PlayerClasses;

	// Spawn location...
	EHANDLE				m_hSpawnPoint;
	
	// Orders
	CHandle< C_Order >	m_hSelectedOrder;
	CHandle< C_Order >	m_hPersonalOrder;
	int					m_iSelectedTarget;
	int					m_iPersonalTarget;
	CUtlVector< CTargetReticle * > m_aTargetReticles;	// A list of entities to show target reticles for

	// Objects
	CHandle< C_BaseObject > m_hSelectedObject;

	int					m_iLastHealth;

	bool				m_bIsBlocking;
	bool				m_bIsParrying;

	bool				m_bUnderAttack;
	int					m_iMaxHealth;
	bool				m_bDeployed;
	bool				m_bDeploying;
	bool				m_bUnDeploying;
	QAngle				m_vecDeployedAngles;
	int					m_iCurrentZoneState;
	int					m_TFPlayerFlags;

	int					m_nOldTacticalView;
	int					m_nOldPlayerClass;

	float				m_flNextUseCheck;

	bool				m_bOldThermalVision;
	bool				m_bOldAttachingSapper;

	bool				m_bOldKnockDownState;
	float				m_flStartKnockdown;
	float				m_flEndKnockdown;
	QAngle				m_vecOriginalViewAngles;
	QAngle				m_vecCurrentKnockdownAngles;
	QAngle				m_vecKnockDownGoalAngles;
	bool				m_bKnockdownOverrideAngles;
	float				m_flKnockdownViewheightAdjust;

	CPlayerAnimState	m_PlayerAnimState;

	// For sniper hiding
	float				m_flLastMoveTime;
	Vector				m_vecLastOrigin;

	// For material proxies
	float				m_flLastDamageTime;
	float				m_flLastGainHealthTime;

	IMaterial			*m_pThermalMaterial;
	IMaterial			*m_pCamoEffectMaterial;

	// Camouflage
	float				m_flDampeningAmount;
	float				m_flGoalDampeningAmount;
	float				m_flDampeningStayoutTime;

	// Suppression of camo based on movement
	float				m_flMovementCamoSuppression;
	float				m_flGoalMovementCamoSuppressionAmount;
	float				m_flMovementCamoSuppressionStayoutTime;

	// Adrenalin
	float				m_flNextAdrenalinEffect;
	bool				m_bFadingIn;

	// ID Target
	int					m_iIDEntIndex;

	CMaterialReference	m_BoostMaterial;
	CMaterialReference	m_EMPMaterial;
	float				m_BoostModelAngles[3];

	// Personal shield effects.
	CUtlLinkedList<CPersonalShieldEffect*, int>	m_PersonalShieldEffects;
		
	CHandle< C_WeaponCombatShield > m_hWeaponCombatShield;

	// No one should call this
	C_BaseTFPlayer& operator=( const C_BaseTFPlayer& src );

	friend void RecvProxy_PlayerClass( const CRecvProxyData *pData, void *pStruct, void *pOut );


	friend class CTFPrediction;
};

int GetLocalPlayerClass( void );
bool IsLocalPlayerClass( int iClass );
bool IsLocalPlayerInTactical( );
inline C_BaseTFPlayer *ToBaseTFPlayer( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#if _DEBUG
	return dynamic_cast<C_BaseTFPlayer *>( pEntity );
#else
	return static_cast<C_BaseTFPlayer *>( pEntity );
#endif
}

#endif // C_BASETFPLAYER_H
