#ifndef ASW_SQUADFORMATION_H
#define ASW_SQUADFORMATION_H
#pragma once

/*
#include "asw_marine.h"
#include "asw_vphysics_npc.h"
#include "asw_shareddefs.h"
*/

class CAI_Hint;
class HintData_t;

/*!  Aggregates state about all marines assigned to a squad,
	 eg, AI marines following a leader in formation.

 */
class CASW_SquadFormation : protected CAutoGameSystem
{
	typedef CAutoGameSystem BaseClass;

public:
	/// local compiler constants
	enum 
	{
		MAX_SQUAD_SIZE = 3, ///< NOT including the leader.
		INVALID_SQUADDIE,
	};

	virtual void LevelInitPostEntity();

	inline CHandle<CASW_Marine> Leader() const { return m_hLeader; }
	inline void Leader(CHandle<CASW_Marine> val) { m_hLeader = val; }
	inline int Count() const ;  ///< number of followers 


	inline CASW_Marine *Squaddie( unsigned int slotnum ) const ;
	inline CASW_Marine *operator[]( unsigned int slotnum ) const { return Squaddie(slotnum); }

	/// return the slot number for a particular marine. 
	/// Return INVALID_SQUADDIE ( or some number > MAX_SQUAD_SIZE ) if the marine is not in this squad.
	unsigned int Find( CASW_Marine *pMarine ) const;
	inline static bool IsValid( unsigned slotnum );

	float GetYaw( unsigned slotnum );
	inline const Vector &GetIdealPosition( unsigned slotnum ) const;

	unsigned int Add( CASW_Marine *pMarine );
	bool Remove( CASW_Marine *pMarine, bool bIgnoreAssert = false ); // ret true if this marine was actually found and removed
	bool Remove( unsigned int slotnum ); // ret true if this marine was actually found and removed

	void ChangeLeader( CASW_Marine *pNewLeader, bool bUpdateLeaderPos = false );

	/// reorganize the follower slots so that each follower has the least distance to move
	void RecomputeFollowerOrder( const Vector &vProjectedLeaderPos, QAngle qLeaderAim ) ; 
	
	/// recompute the array of positions that squaddies should head towards.
	void UpdateFollowPositions();

	/// should the squaddie positions be recomputed -- assumed this function is called from a marine's Think
	bool ShouldUpdateFollowPositions() const ;

	/// Current notion of "forward", is updated/cached in calls to GetLdrAnglMatrix
	inline const Vector &Forward() const;

	// disallow inadvertent construction
	explicit CASW_SquadFormation() : m_flLastSquadUpdateTime(0), CAutoGameSystem( "asw_squadformation" ) {Reset();};
	bool SanityCheck() const ;

	void FindFollowHintNodes();

	void Reset();

#ifdef HL2_HINTS
	static int FollowHintSortFunc( CAI_Hint* const *pHint1, CAI_Hint* const *pHint2 );
#else
	static int FollowHintSortFunc( HintData_t* const *pHint1, HintData_t* const *pHint2 );
#endif

	void DrawDebugGeometryOverlays();

protected:
	CHandle<CASW_Marine> m_hLeader;
	CHandle<CASW_Marine> m_hSquad[MAX_SQUAD_SIZE];

	// the location in world where each squaddie is supposed
	// to be standing this frame.
	Vector m_vFollowPositions[MAX_SQUAD_SIZE];	

	// hint nodes for use in combat
#ifdef HL2_HINTS
	CHandle<CAI_Hint> m_hFollowHint[MAX_SQUAD_SIZE];
#else
	int m_nMarineHintIndex[MAX_SQUAD_SIZE];
#endif
	bool m_bRearGuard[MAX_SQUAD_SIZE];			// is this hint the rearmost? if so, marine faces backwards
	bool m_bStandingInBeacon[MAX_SQUAD_SIZE];
	bool m_bFleeingBoomerBombs[MAX_SQUAD_SIZE];

	// clumsy holdovers from old system, should be replaced
	// with proper movement histories with prediction
	float m_flLastLeaderYaw;
	Vector m_vLastLeaderPos;

	Vector m_vLastLeaderVelocity; ///< velocity leader had at last update. If it changes radically, we reupdate immediately.
	Vector m_vCachedForward;

	float m_flCurrentForwardAbsoluteEulerYaw;
	float m_flLastSquadUpdateTime;

	int m_iLastHealBeaconCount;

#pragma region   STATICS
	const static Vector s_MarineFollowOffset[MAX_SQUAD_SIZE];
	const static float s_MarineFollowDirection[MAX_SQUAD_SIZE];
	const static Vector s_MarineBeaconOffset[MAX_SQUAD_SIZE];
	const static float s_MarineBeaconDirection[MAX_SQUAD_SIZE];
#pragma endregion

private:
	Vector GetLdrAnglMatrix( const Vector &origin, const QAngle &ang, matrix3x4_t *pout );

#pragma region From IGameSystem
protected:
	virtual void LevelInitPreEntity();
#pragma endregion 

private:
	bool m_bLevelHasFollowHints;

	// thou shalt not copy
	CASW_SquadFormation( const CASW_SquadFormation & );
};

extern CASW_SquadFormation g_ASWSquadFormation;


inline CASW_Marine *CASW_SquadFormation::Squaddie( unsigned int slotnum ) const 
{
	Assert( IsValid(slotnum) );
	return m_hSquad[slotnum];
}


inline const Vector &CASW_SquadFormation::Forward() const
{
	return m_vCachedForward;
}


inline bool CASW_SquadFormation::IsValid( unsigned slotnum )
{
	return slotnum < MAX_SQUAD_SIZE;
}


inline const Vector &CASW_SquadFormation::GetIdealPosition( unsigned slotnum ) const
{
	return m_vFollowPositions[slotnum];
}


inline int CASW_SquadFormation::Count() const
{
	int ret = 0;
	for ( int i = 0 ; i < MAX_SQUAD_SIZE; ++i )
	{
		if ( !!m_hSquad[i] )
			++ret;
	}
	return ret;
}
#endif /* ASW_SQUADFORMATION_H */
