#ifndef _INCLUDED_ASW_SPAWNABLE_NPC_H
#define _INCLUDED_ASW_SPAWNABLE_NPC_H

class CASW_Base_Spawner;
class CAI_BaseNPC;

enum AlienOrder_t
{		
	AOT_SpreadThenHibernate,
	AOT_MoveTo,
	AOT_MoveToIgnoringMarines,
	AOT_MoveToNearestMarine,
	AOT_None,
};

// Spawnable interface for all spawnable NPC types to implement
abstract_class IASW_Spawnable_NPC
{
public:	

	virtual void SetSpawner(CASW_Base_Spawner* spawner) = 0;		// inform our spawnable about the spawner, so the spawn can be notified of the spawnable's death
	virtual CAI_BaseNPC* GetNPC() = 0;
	virtual bool CanStartBurrowed() = 0;					// spawnables should return true if they can spawn underground
	virtual void StartBurrowed() = 0;						// should make the spawnable start underground
	virtual void SetUnburrowActivity( string_t iszActivityName ) = 0;
	virtual void SetUnburrowIdleActivity( string_t iszActivityName ) = 0;
	virtual void SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject) = 0;
	virtual AlienOrder_t GetAlienOrders() = 0;
	virtual void MoveAside() = 0;							// this means the spawnable is blocking a spawn point and should try to move out of the way
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL ) = 0;	
	virtual void ElectroStun( float flStuntime ) = 0;
	virtual void OnSwarmSensed(int iDistance) = 0;		// sensing for asw_ai_senses
	virtual void OnSwarmSenseEntity(CBaseEntity* pEnt) = 0;
	virtual void SetHealthByDifficultyLevel() = 0;	// sets the alien's health to its default 
	virtual void SetHoldoutAlien() = 0;				// this alien was spawned as part of a holdout wave
	virtual bool IsHoldoutAlien() = 0;
};

#endif // _INCLUDED_ASW_SPAWNABLE_NPC_H