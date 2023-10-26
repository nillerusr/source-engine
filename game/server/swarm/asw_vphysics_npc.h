#ifndef ASW_VPHYSICS_NPC_H
#define ASW_VPHYSICS_NPC_H
#pragma once

#include "ai_playerally.h"

class CMoveData;

class CASW_VPhysics_NPC : public CAI_PlayerAlly
{
public:
	DECLARE_CLASS( CASW_VPhysics_NPC, CAI_PlayerAlly );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_VPhysics_NPC();
	virtual ~CASW_VPhysics_NPC();
	virtual void UpdateOnRemove();

	void	Spawn( void );
	int Restore( IRestore &restore );
	virtual void InhabitedPhysicsSimulate();
	virtual void UpdateVPhysicsAfterMove();
	virtual float MaxSpeed();

	void PostThink();
	// physics stuff
	bool ShouldSavePhysics();
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	void SetVCollisionState( int collisionState );
	void VPhysicsDestroyObject();
	void InitVCollision( void );
	void PostThinkVPhysics(CMoveData* pMoveData);
	void SetupVPhysicsShadow( CPhysCollide *pStandModel, const char *pStandHullName, CPhysCollide *pCrouchModel, const char *pCrouchHullName );
	void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsUpdate( IPhysicsObject *pPhysics );
	void UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival );
	void UpdatePhysicsShadowToCurrentPosition();
	IPhysicsPlayerController* GetPhysicsController() { return m_pPhysicsController; }
	virtual bool			IsFollowingPhysics( void ) { return false; }
	bool					IsRideablePhysics( IPhysicsObject *pPhysics );
	IPhysicsObject			*GetGroundVPhysics();
	// Player Physics Shadow
	IPhysicsPlayerController	*m_pPhysicsController;
	IPhysicsObject				*m_pShadowStand;
	IPhysicsObject				*m_pShadowCrouch;
	int							m_vphysicsCollisionState;
	Vector						m_oldOrigin;
	Vector						m_vecSmoothedVelocity;
	bool						m_touchedPhysObject;
	bool					HasPhysicsFlag( unsigned int flag ) { return (m_afPhysicsFlags & flag) != 0; }
	unsigned int			m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	// These are generated while running usercmds, then given to UpdateVPhysicsPosition after running all queued commands.
	Vector m_vNewVPhysicsPosition;
	Vector m_vNewVPhysicsVelocity;
	void Touch( CBaseEntity *pOther );
	void					SetTouchedPhysics( bool bTouch ) { m_touchedPhysObject = bTouch; }
	bool					TouchedPhysics( void ) { return m_touchedPhysObject; }

	// asw
	Vector m_vecLastSafePosition;
	//CUtlVector<EHANDLE> m_BlockingEntities;
	//CUtlVector<int> m_BlockingCollisionGroup;
};


#endif /* ASW_VPHYSICS_NPC_H */