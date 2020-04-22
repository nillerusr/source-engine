//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_INFO_ACT_H
#define C_INFO_ACT_H
#ifdef _WIN32
#pragma once
#endif

#define ACT_NONE_SPECIFIED		-1

//-----------------------------------------------------------------------------
// Purpose: Map entity that defines an act
//-----------------------------------------------------------------------------
class C_InfoAct : public C_BaseEntity
{
	DECLARE_CLASS( C_InfoAct, C_BaseEntity );
public:
	C_InfoAct();
	~C_InfoAct();

	DECLARE_CLIENTCLASS();

	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void	StartAct( float flStartTime );

	int		GetActNumber( void ) { return m_iActNumber; }
	bool	IsAWaitingAct( void );

	float	RespawnTimeRemaining( int nTeam, int nTimer ) const;

private:
	int		m_iActNumber;
	int		m_spawnflags;
	float	m_flActTimeLimit;
	int		m_nRespawn1Team1Time;
	int		m_nRespawn1Team2Time;
	int		m_nRespawn2Team1Time;
	int		m_nRespawn2Team2Time;
	int		m_nRespawnTeam1Delay;
	int		m_nRespawnTeam2Delay;
	float	m_flStartTime;
	float	m_flPreviousTimeLimit;
};

extern CHandle<C_InfoAct>	g_hCurrentAct;
void StartAct( int iActNumber, float flStartTime );
int	 GetCurrentActNumber( void );
bool CurrentActIsAWaitingAct( void );

#endif // C_INFO_ACT_H
