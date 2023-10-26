#ifndef _INCLUDED_ASW_BROADCAST_CAMERA_H
#define _INCLUDED_ASW_BROADCAST_CAMERA_H
#ifdef _WIN32
#pragma once
#endif

// like a normal CTriggerCamera, but changes the view of all players connected

class CASW_Broadcast_Camera : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Broadcast_Camera, CBaseEntity );

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Enable( void );
	void Disable( void );
	void FollowTarget( void );
	void Move(void);

	// asw
	void UpdateAllPlayers();
	void RestoreAllPlayerViews();

	// Always transmit to clients so they know where to move the view to
	virtual int UpdateTransmitState();
	
	DECLARE_DATADESC();

	// Input handlers
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

private:
	EHANDLE m_hTarget;
	CBaseEntity *m_pPath;
	string_t m_sPath;
	float m_flWait;
	float m_flReturnTime;
	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	int	  m_state;
	Vector m_vecMoveDir;
	string_t m_iszTargetAttachment;
	int	  m_iAttachmentIndex;
	bool  m_bSnapToGoal;

	Vector m_vecLastPos;

private:
	COutputEvent m_OnEndFollow;
};

#endif //_INCLUDED_ASW_BROADCAST_CAMERA_H