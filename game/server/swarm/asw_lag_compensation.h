#ifndef _INCLUDED_ASW_LAG_COMPENSATION_H
#define _INCLUDED_ASW_LAG_COMPENSATION_H

// this class deals with storing an entity's position regularly
//  and can rewind the entity to some time (approx.) so lagged player's shots will still hit without them having to 'aim in front' to make up for their lag

// how many samples of history we store
#define ASW_LAG_NUM_POSITION_HISTORY_SAMPLES 5
// minimum amount of lag a player has to be under to trigger lag compensation
#define	ASW_MIN_LAG_TIME 0.01
// minimum amount of time between sample histories (extra samples within this time aren't stored)
#define ASW_LAG_MIN_SAMPLE_TIME_DIFFERENCE 0.05

class CBasePlayer;
class CASW_Player;
class CUserCmd;

class CASW_Lag_Compensation
{
public:
	CASW_Lag_Compensation();	
	virtual ~CASW_Lag_Compensation();
	void Init(CBaseAnimating *pOwner);
	
	// adds a sample to the history
	void StorePositionHistory();

	// pass in a time to rewind back to
	void MoveToLaggedPosition( const float fLaggedTime );
	const Vector& GetLaggedPosition( const float fLaggedTime );
	void UndoLaggedPosition();
	Vector GetLagCompensationOffset();

	// position history
	Vector m_vecPositionHistory[ASW_LAG_NUM_POSITION_HISTORY_SAMPLES];
	float m_fPositionHistoryTime[ASW_LAG_NUM_POSITION_HISTORY_SAMPLES];
	int m_iPositionHistoryTail;	// which index in the above array is the current 

	// real position
	Vector m_vecRealPosition;
	bool m_bSetRealPosition;
	float m_fRealSimulationTime;

	// handle to the entity we're doing lag compensation for
	CHandle<CBaseAnimating> m_hOwnerEntity;

	CAI_BaseNPC *GetOwnerNPC();

	// moves all lag compensating entities
	static void AllowLagCompensation(CBasePlayer *player);
	static void RequestLagCompensation(CASW_Player *player, const CUserCmd *cmd );
	static void FinishLagCompensation();
	static bool IsInLagCompensation() { return s_bInLagCompensation; }

	static bool s_bInLagCompensation;
	static CBasePlayer *s_pLagCompensatingPlayer;
};

#endif // _INCLUDED_ASW_LAG_COMPENSATION_H