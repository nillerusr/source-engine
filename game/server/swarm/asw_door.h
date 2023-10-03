#ifndef _DEFINED_ASW_DOOR_H
#define _DEFINED_ASW_DOOR_H

#include "entityblocker.h"
#include "BasePropDoor.h"
#include "asw_shareddefs.h"

class CASW_Player;
class CASW_Marine;
class CASW_Door_Padding;

enum ASW_DoorSpawnPos_t
{
	DOOR_SPAWN_CLOSED = 0,
	DOOR_SPAWN_OPEN,	
};

// how dented the door is
enum ASW_DoorDent_t
{
	ASWDD_NONE,			// not dented at all, it's fine
	ASWDD_PARTIAL,		// partially dented, can open, jerkily, with a grinding noise
	ASWDD_COMPLETE,		// very dented, unable to open
	ASWDD_PARTIAL_PREFLIP,
	ASWDD_COMPLETE_PREFLIP,
};

// This is our sliding door class

class CASW_Door : public CBasePropDoor
{
	DECLARE_CLASS( CASW_Door, CBasePropDoor );	
public:

	virtual ~CASW_Door();

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	int		DrawDebugTextOverlays(void);

	void	Spawn( void );

	static bool DestroyVismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );
	static bool DestroyVismonCallback( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );
	static bool WeldedVismonCallback( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

	int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual void Precache();
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_DOOR; }
	void	MoveDone( void );
	void	BeginOpening(CBaseEntity *pOpenAwayFrom);
	void	BeginClosing( void );
	void	OnRestore( void );
	virtual void ComputeDoorExtent( Extent *extent, unsigned int extentType );

	void	DoorTeleportToSpawnPosition();

	void	GetNPCOpenData(CAI_BaseNPC *pNPC, opendata_t &opendata);

	void	DoorClose( void );
	bool	DoorCanClose( bool bAutoClose );
	void	DoorOpen( CBaseEntity *pOpenAwayFrom );

	void	OnDoorOpened();
	void	OnDoorClosed();
	virtual bool IsDoorLocked();

	void	DoorResume( void );
	void	DoorStop( void );

	float	GetOpenInterval();

	bool	OverridePropdata() { return true; }

	// input
	void InputNPCNear( inputdata_t &inputdata );
	void InputEnableAutoOpen( inputdata_t &inputdata );
	void InputDisableAutoOpen( inputdata_t &inputdata );
	void InputRecommendWeld( inputdata_t &inputdata );
	
	// auto opening
	bool IsAutoOpen() { return m_bAutoOpen; }
	void AutoOpen(CBaseEntity* pMarine);
	
	// welding shut/cutting open
	float GetSealAmount();	// returns how sealed this door is, from 0 to 1.0
	float GetCurrentSealTime() { return m_flCurrentSealTime; }
	void SetCurrentSealTime(float fTime);
	float GetTotalSealTime() { return m_flTotalSealTime; }
	void SetTotalSealTime(float fTime);
	void WeldDoor(bool bSeal, float fAmount, CASW_Marine* pMarine);	// welder weapon calls this repeatedly when the marine is sealing/cutting the door
	Vector GetWeldFacingPoint(CBaseEntity* pOther); // the point a marine should look to weld this door

	bool CloseForWeld(CASW_Marine* PMarine);		// player requests the door to shut so he can weld it
	bool IsRecommendedSeal( void ) { return m_bRecommendedSeal; }
	bool CanWeld( void ) { return m_bCanCloseToWeld; }
	float m_fClosingToWeldTime;	// door won't autoopen before this curtime
	bool m_bHasBeenWelded;
	bool m_bDoCutShout;	// should a marine shout out 'cut this door!' when he encounters this door and a marine with a welder is nearby?	
	virtual void CheckForDoorShootChatter( const CTakeDamageInfo &info );
	virtual void DoAutoDoorShootChatter(CASW_Marine *pMarine);
	float m_fLastMarineShootTime;
	float m_fMarineShootCounter;
	bool m_bDoneDoorShout;
	bool m_bDoBreachedShout;
	bool m_bDoAutoShootChatter;
	bool m_bRotateOnFlip;
	// SCARY NOTE: a float defined here was taking on strange values
	float m_fSkillMarineHelping;	// last time an engineering marine was nearby helping a weld
	CNetworkVar(bool, m_bSkillMarineHelping);		// is an engineer helping a weld on this door currently?

	// player has hit a use icon which manipulates this door
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );

	bool	IsOpen( void );
	bool	IsMoving();

	int OnTakeDamage( const CTakeDamageInfo &info );
	void DoorSmoke();
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void SetDentSequence();	
	virtual void SetDoorDamage();
	virtual bool DoorNeedsFlip( void );
	inline const Vector &GetClosedPosition(); ///< the door's origin when closed (coz it slides back and forth)
	CNetworkVar( float, m_fLastMomentFlipDamage );
	Vector m_vLastDamageDir;

	virtual void RunAnimation();
	virtual void HandleAnimEvent(animevent_t *pEvent);
	virtual void Event_Killed( const CTakeDamageInfo &info );

	// physics stuff

	bool ASWCreateVPhysics();
	void VPhysicsUpdate( IPhysicsObject *pPhysics );
	void GetMassCenter( Vector *pMassCenter );
	float GetMass() const;
	// toggles the door between normal mesh and the flipped one
	void FlipDoor();
	// kill anyone in front of the falling door
	void FallCrush();
	int m_iFallingStage;

	IPhysicsObject *VPhysicsInitFallenShadow( bool allowPhysicsMovement, bool allowPhysicsRotation, solid_t *pSolid = NULL);

	// is the door currently using the flipped mesh?
	bool m_bFlipped;

	// should this door show as a blip on the marine's scanner?
	bool m_bShowsOnScanner;
	bool	m_bDoorFallen;

private:

	float m_fLastFullyWeldedSound;

	void	SlideMove(const Vector &vecDestPosition, float flSpeed);
	void	CalculateDoorVolume( Vector OpenPosition, Vector ClosedPosition, Vector *destMins, Vector *destMaxs );

	bool	CheckDoorClear();		

	float	m_flDistance;				// How far to slide
	QAngle	m_angSlideAngle;				// The angle the door slides in relative to its own angle

	ASW_DoorSpawnPos_t m_eSpawnPosition;

	Vector	m_vecOpenPosition;
	CNetworkVar( Vector, m_vecClosedPosition );
	
	Vector	m_vecGoal;

	Vector	m_vecBoundsMin;
	Vector	m_vecBoundsMax;

	CNetworkVar( float,  m_flTotalSealTime );
	CNetworkVar( float,  m_flCurrentSealTime );
	CNetworkVar( int, m_iDoorType );
	CNetworkVar( int,  m_iDoorStrength );

	//CNetworkVar( bool,  m_bShowsOnScanner );	
	CNetworkVar( bool,  m_bAutoOpen );
	CNetworkVar( bool,  m_bBashable );
	CNetworkVar( bool,  m_bShootable );
	CNetworkVar( bool,  m_bCanCloseToWeld );
	CNetworkVar( bool,  m_bRecommendedSeal );
	CNetworkVar( bool,  m_bWasWeldedByMarine );
	
	ASW_DoorDent_t m_DentAmount;
	bool m_bSetSide;

	bool m_bDoneChatter;
	float m_fChatterCounter;

	CHandle<CEntityBlocker>	m_hDoorBlocker;
	CHandle<CASW_Door_Padding> m_hDoorPadding;

	COutputEvent m_OnFullySealed;
	COutputEvent m_OnFullyCut;
	COutputEvent m_OnDestroyed;

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
};

inline const Vector &CASW_Door::GetClosedPosition()
{
	return m_vecClosedPosition.Get();
}


#endif /* _DEFINED_ASW_DOOR_H */
