//=============================================================================//
//
// Purpose: 	Bad Cop's humble friend, a pacifist turret who could open doors for some reason.
// 
// Author:		Blixibon
//
//=============================================================================//

#include "npc_turret_floor.h"
#include "ai_speech.h"
#include "ai_playerally.h"
#include "ai_baseactor.h"
#include "ai_sensorydummy.h"

#include "Sprite.h"
#include "filters.h"

//-----------------------------------------------------------------------------

// This is triggered by the maps right now
#define TLK_WILLE_BEAST_DANGER "TLK_BEAST_DANGER"
//-----------------------------------------------------------------------------

// This was CAI_ExpresserHost<CNPCBaseInteractive<CAI_BaseActor>>, but then I remembered we don't need CAI_ExpresserHost because CAI_BaseActor already has it.
// This was then just CNPCBaseInteractive<CAI_BaseActor>, but then I remembered CNPCBaseInteractive is just the Alyx-hackable stuff, which Wilson doesn't need.
// Then this was just CAI_BaseActor, but then I realized CAI_PlayerAlly has a lot of code Wilson could use.
// Then this was just CAI_PlayerAlly, but then I started using CAI_SensingDummy for some Bad Cop code and it uses things I used for Will-E.
// So now it's CAI_SensingDummy<CAI_PlayerAlly>.
typedef CAI_SensingDummy<CAI_PlayerAlly> CAI_WilsonBase;

class CWilsonCamera;

//-----------------------------------------------------------------------------
// Purpose: Wilson, Willie, Will-E
// 
// NPC created by Blixibon.
// 
// Will-E is a floor turret with no desire for combat and the ability to use the Response System.
// 
//-----------------------------------------------------------------------------
class CNPC_Wilson : public CAI_WilsonBase, public CDefaultPlayerPickupVPhysics, public ITippableTurret
{
	DECLARE_CLASS(CNPC_Wilson, CAI_WilsonBase);
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_SERVERCLASS();
public:
	CNPC_Wilson();
	~CNPC_Wilson();

	static CNPC_Wilson *GetWilson( void );
	static CNPC_Wilson *GetBestWilson( float &flBestDistSqr, const Vector *vecOrigin );
	CNPC_Wilson *m_pNext;

	void	Precache();
	void	Spawn();
	void	Activate( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );
	void	NPCInit( void );
	bool	CreateVPhysics( void );
	void	UpdateOnRemove( void );
	float	MaxYawSpeed( void ){ return 0; }

	void	AutoSetLocatorThink();

	bool	ShouldSavePhysics() { return true; }
	unsigned int	PhysicsSolidMaskForEntity( void ) const;

	void	Touch(	CBaseEntity *pOther );

	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void	PlayerPenetratingVPhysics( void ) {}
	bool	CanBecomeServerRagdoll( void ) { return false; }

	//int		GetSensingFlags( void ) { return SENSING_FLAGS_DONT_LOOK | SENSING_FLAGS_DONT_LISTEN; }

	int		OnTakeDamage( const CTakeDamageInfo &info );
	void	TeslaThink();
	void	Event_Killed( const CTakeDamageInfo &info );

	void	Event_KilledOther( CBaseEntity * pVictim, const CTakeDamageInfo & info );

	// We use our own regen code
	bool	ShouldRegenerateHealth( void ) { return false; }

	// Wilson should only hear combat-related sounds, ironically.
	int		GetSoundInterests( void ) { return SOUND_COMBAT | SOUND_DANGER | SOUND_BULLET_IMPACT; }

	void			NPCThink();
	void			PrescheduleThink( void );
	void			GatherConditions( void );
	void			GatherEnemyConditions( CBaseEntity *pEnemy );

	int 			TranslateSchedule( int scheduleType );

	void			OnSeeEntity( CBaseEntity *pEntity );
	bool			Remark( AI_CriteriaSet &modifiers, CBaseEntity *pRemarkable ) { return SpeakIfAllowed( TLK_REMARK, modifiers ); }
	
	void 			OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttacker );

	void			AimGun();

	bool			FInViewCone( CBaseEntity *pEntity ) { return BaseClass::FInViewCone( pEntity ); }
	bool			FInViewCone( const Vector &vecSpot );
	bool			FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );
	bool			FVisible( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );
	CWilsonCamera*	GetCameraForTarget( CBaseEntity *pTarget );

	bool			QueryHearSound( CSound *pSound );

	// Wilson hardly cares about his NPC state because he's just a vessel for choreography and player attachment, not a useful combat ally.
	// This means we redirect SelectIdleSpeech() and SelectAlertSpeech() to the same function. Sure, we could've marked SelectNonCombatSpeech() virtual
	// and overrode that instead, but this makes things simpler to understand and avoids changing all allies just for Will-E to say this nonsense.
	bool			DoCustomSpeechAI();
	bool			DoIdleSpeechAI( AISpeechSelection_t *pSelection, int iState );
	bool			SelectIdleSpeech( AISpeechSelection_t *pSelection ) { return DoIdleSpeechAI(pSelection, NPC_STATE_IDLE); }
	bool			SelectAlertSpeech( AISpeechSelection_t *pSelection ) { return DoIdleSpeechAI(pSelection, NPC_STATE_ALERT); }

	void			HandlePrescheduleIdleSpeech();

	void			InputSelfDestruct( inputdata_t &inputdata );

	void			InputTurnOnEyeLight( inputdata_t &inputdata ) { m_bEyeLightEnabled = true; }
	void			InputTurnOffEyeLight( inputdata_t &inputdata ) { m_bEyeLightEnabled = false; }

	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );

	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	virtual bool	OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	CBasePlayer		*HasPhysicsAttacker( float dt );

	bool			IsBeingCarriedByPlayer( ) { return m_flCarryTime != -1.0f; }
	bool			WasJustDroppedByPlayer( ) { return m_flPlayerDropTime > gpGlobals->curtime; }
	bool			IsHeldByPhyscannon( )	{ return VPhysicsGetObject() && (VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD); }

	bool			OnSide( void );

	// Needed to prevent weird binding crash
	/*__declspec(noinline)*/ bool	ScriptIsBeingCarriedByPlayer() { return IsBeingCarriedByPlayer(); }
	/*__declspec(noinline)*/ bool	ScriptWasJustDroppedByPlayer() { return WasJustDroppedByPlayer(); }

	bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	bool	PreThink( turretState_e state );
	void	SetEyeState( eyeState_t state );

	virtual void    ModifyOrAppendCriteria(AI_CriteriaSet& criteriaSet);
	void			ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, bool bRespondingToPlayer = false, char *pszOutResponseChosen = NULL, size_t bufsize = 0);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, bool bRespondingToPlayer = false, char *pszOutResponseChosen = NULL, size_t bufsize = 0);
	bool			IsOkToSpeak( ConceptCategory_t category, bool fRespondingToPlayer = false );
	void			PostSpeakDispatchResponse( AIConcept_t concept, AI_Response *response );
	virtual void    PostConstructor(const char *szClassname);
	virtual CAI_Expresser *CreateExpresser(void);
	virtual CAI_Expresser *GetExpresser() { return m_pExpresser;  }

	bool			GetGameTextSpeechParams( hudtextparms_t &params );

	bool			IsOmniscient() { return m_bOmniscient; }

	void			RefreshCameraTargets();
	const Vector&	GetSpeechTargetSearchOrigin();

	void			InputEnableMotion( inputdata_t &inputdata );
	void			InputDisableMotion( inputdata_t &inputdata );

	int		BloodColor( void ) { return DONT_BLEED; }

	bool	IsValidEnemy( CBaseEntity *pEnemy );

	// By default, Will-E doesn't attack anyone and nobody attacks him. (although he does see enemies for BC, see IRelationType)
	// You can make NPCs attack him with m_bCanBeEnemy, but Will-E is literally incapable of combat.
	bool	CanBeAnEnemyOf( CBaseEntity *pEnemy );
	Class_T		Classify( void ) { return CLASS_ARBEIT_TECH; }

	Disposition_t		IRelationType( CBaseEntity *pTarget );

	bool		IsSilentSquadMember() const;

	// Always think if we're the main Wilson NPC
	bool		ShouldAlwaysThink() { return CNPC_Wilson::GetWilson() == this || BaseClass::ShouldAlwaysThink(); }

	void		SetDamaged( bool bDamaged );
	void		SetPlayingDead( bool bPlayingDead );

	void			InputTurnOnDamagedMode( inputdata_t &inputdata ) { SetDamaged( true ); }
	void			InputTurnOffDamagedMode( inputdata_t &inputdata ) { SetDamaged( false ); }

	void			InputTurnOnDeadMode( inputdata_t &inputdata ) { SetPlayingDead( true ); }
	void			InputTurnOffDeadMode( inputdata_t &inputdata ) { SetPlayingDead( false ); }

	void			InputSetCameraTargets( inputdata_t &inputdata ) { m_iszCameraTargets = inputdata.value.StringID(); RefreshCameraTargets(); }
	void			InputClearCameraTargets( inputdata_t &inputdata ) { m_iszCameraTargets = NULL_STRING; RefreshCameraTargets(); }

	void			InputEnableSeeThroughPlayer( inputdata_t &inputdata ) { m_bSeeThroughPlayer = true; }
	void			InputDisableSeeThroughPlayer( inputdata_t &inputdata ) { m_bSeeThroughPlayer = false; }

	void			InputEnableOmnipresence( inputdata_t &inputdata ) { m_bOmniscient = true; }
	void			InputDisableOmnipresence( inputdata_t &inputdata ) { m_bOmniscient = false; }


protected:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_EXAMPLE = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,

		SCHED_WILSON_ALERT_STAND = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		//TASK_EXAMPLE = BaseClass::NEXT_TASK,
		//NEXT_TASK,

		//AE_EXAMPLE = LAST_SHARED_ANIMEVENT

	};

	COutputEvent m_OnTipped;
	COutputEvent m_OnPlayerUse;
	COutputEvent m_OnPhysGunPickup;
	COutputEvent m_OnPhysGunDrop;
	COutputEvent m_OnDestroyed;

	int						m_iEyeAttachment;
	eyeState_t				m_iEyeState;
	CHandle<CSprite>		m_hEyeGlow;
	CHandle<CTurretTipController>	m_pMotionController;

	bool	m_bBlinkState;
	bool	m_bTipped;
	float	m_flCarryTime;
	bool	m_bUseCarryAngles;
	float	m_flPlayerDropTime;
	float	m_flTeslaStopTime;

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	EHANDLE		m_hAttachedVehicle;
	float		m_flNextClearanceCheck;

	float		m_fNextFidgetSpeechTime;

	// See CNPC_Wilson::NPCThink()
	bool	m_bPlayerLeftPVS;

	// How long the player has spent around Wilson
	// (for goodbyes)
	float	m_flPlayerNearbyTime;

	// For when Wilson is meant to be non-interactive (e.g. background maps, dev commentary)
	// This makes Wilson unmovable and deactivates/doesn't precache a few miscellaneous things.
	bool	m_bStatic;

	// For when Wilson is in 'damaged mode'. He can still respond and interact as normal, but he has a different model and effects.
	bool	m_bDamaged;

	// For when Wilson is 'playing dead', as in the final map ez2_c6_3. The NPC technically isn't dead but he will not respond and appears dead 
	bool	m_bDead;

	// Makes Will-E always available as a speech target, even when out of regular range.
	// (e.g. Will-E on monitor in ez2_c3_3)
	// 
	// MISNOMER: Actually means "omnipresent". This should be corrected next time we can break saves.
	bool	m_bOmniscient;

	string_t								m_iszCameraTargets;
	CUtlVector<CHandle<CWilsonCamera>>		m_hCameraTargets;	// Refreshed every restore

	bool	m_bSeeThroughPlayer;

	// See CNPC_Wilson::CanBeAnEnemyOf().
	bool	m_bCanBeEnemy;
 
	// Sets the player's target locator to this Wilson automatically.
	bool	m_bAutoSetLocator;

	// Enables a projected texture spotlight on the client.
	CNetworkVar( bool, m_bEyeLightEnabled );
	CNetworkVar( int, m_iEyeLightBrightness );

	CAI_Expresser *m_pExpresser;

	DEFINE_CUSTOM_AI;
};

#define SF_ARBEIT_SCANNER_STAY_SCANNING ( 1 << 0 )

//-----------------------------------------------------------------------------
// Purpose: Arbeit scanner that Wilson uses.
//-----------------------------------------------------------------------------
class CArbeitScanner : public CBaseAnimating
{
	DECLARE_CLASS(CArbeitScanner, CBaseAnimating);
	DECLARE_DATADESC();
public:
	CArbeitScanner();
	~CArbeitScanner();

	void	Precache();
	void	Spawn();

	bool	IsScannable( CAI_BaseNPC *pNPC );

	// Checks if they're in front and then tests visibility
	inline bool	IsInScannablePosition( CAI_BaseNPC *pNPC, Vector &vecToNPC, Vector &vecForward ) { return DotProduct( vecToNPC, vecForward ) < -0.10f && FVisible( pNPC, MASK_SOLID_BRUSHONLY ); }

	// Just hijack the damage filter
	inline bool	CanPassScan( CAI_BaseNPC *pNPC ) { return m_hDamageFilter ? static_cast<CBaseFilter*>(m_hDamageFilter.Get())->PassesFilter(this, pNPC) : true; }

	CAI_BaseNPC *FindMatchingNPC( float flRadiusSqr );

	// For now, just use our skin
	inline void	SetScanState( int iState ) { m_nSkin = iState; }
	inline int	GetScanState() { return m_nSkin; }

	void	IdleThink();
	void	AwaitScanThink();
	void	WaitForReturnThink();

	void	ScanThink();
	bool	FinishScan();
	void	CleanupScan(bool dispatchInteraction);

	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputFinishScan( inputdata_t &inputdata );
	void	InputForceScanNPC( inputdata_t &inputdata );
	void	InputSetInnerRadius( inputdata_t &inputdata );
	void	InputSetOuterRadius( inputdata_t &inputdata );
	void	InputSetScanFilter( inputdata_t &inputdata );

	enum
	{
		SCAN_IDLE,		// Default
		SCAN_WAITING,	// Spotted NPC, waiting for them to get close
		SCAN_SCANNING,	// Scan in progress
		SCAN_DONE,		// Scanning done, displays a checkmark or something
		SCAN_REJECT,	// Scanning rejected, displays an X or something
		SCAN_OFF,		// Scanner is disabled
	};

	COutputEvent m_OnScanDone;
	COutputEvent m_OnScanReject;

	COutputEvent m_OnScanStart;
	COutputEvent m_OnScanInterrupt;

private:

	bool m_bDisabled;

	// -1 = Don't cool down
	float m_flCooldown;

	float m_flOuterRadius;
	float m_flInnerRadius;

	string_t m_iszScanSound;
	string_t m_iszScanDeploySound;
	string_t m_iszScanDoneSound;
	string_t m_iszScanRejectSound;

	// How long to wait
	float m_flScanTime;

	// Actual curtime
	float m_flScanEndTime;

	// Don't finish scanning until the target's scene is done
	bool m_bWaitForScene;

	AIHANDLE	m_hScanning;
	CSprite		*m_pSprite;

	string_t m_iszScanFilter;
	CHandle<CBaseFilter> m_hScanFilter;

	int m_iScanAttachment;

};

//-----------------------------------------------------------------------------
// Purpose: Camera entity that Wilson can use to see outside of his body.
//-----------------------------------------------------------------------------
class CWilsonCamera : public CBaseEntity
{
	DECLARE_CLASS( CWilsonCamera, CBaseEntity );
	DECLARE_DATADESC();
public:
	CWilsonCamera();

	void Spawn();
	bool FInViewCone( const Vector &vecSpot );

	inline bool IsEnabled() const { return !m_bDisabled; }
	inline float GetFOV() const { return m_flFieldOfView; }
	inline float GetLookDistSqr() const { return m_flLookDistSqr; }
	inline bool Using3DViewCone() const { return m_b3DViewCone; }

	void	InputEnable( inputdata_t &inputdata ) { m_bDisabled = false; }
	void	InputDisable( inputdata_t &inputdata ) { m_bDisabled = true; }

	void	InputSetFOV( inputdata_t &inputdata ) { m_flFieldOfView = cos( DEG2RAD( inputdata.value.Float() / 2 ) ); }
	void	InputSetLookDist( inputdata_t &inputdata ) { m_flLookDistSqr = Square( inputdata.value.Float() ); }

private:

	bool m_bDisabled;
	float m_flFieldOfView;
	float m_flLookDistSqr;
	bool m_b3DViewCone;
};
