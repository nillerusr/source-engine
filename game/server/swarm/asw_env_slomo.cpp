// Activates the slow motion part of Alien Swarm

#include "cbase.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CEnvSlomo : public CPointEntity
{
private:	
	float m_Duration;
	float m_Radius;			// radius of 0 means all players


	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CEnvSlomo, CPointEntity );

	~CEnvSlomo( void );
	virtual void	Spawn( void );	

	inline	float	Duration( void ) { return m_Duration; }
	inline	void	SetDuration( float duration ) { m_Duration = duration; }

	// Input handlers
	void InputStartSlomo( inputdata_t &inputdata );
	void InputStopSlomo( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( env_slomo, CEnvSlomo );

BEGIN_DATADESC( CEnvSlomo )
	DEFINE_KEYFIELD( m_Duration,		FIELD_FLOAT, "duration" ),	
	DEFINE_INPUTFUNC( FIELD_VOID, "StartSlomo", InputStartSlomo ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopSlomo", InputStopSlomo ),
END_DATADESC()

CEnvSlomo::~CEnvSlomo( void )
{
	//Msg("env_slomo destroyed\n");

}

void CEnvSlomo::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	//Msg("env_slomo spawned\n");
}


void CEnvSlomo::InputStartSlomo( inputdata_t &inputdata )
{
	//Msg("Start slomo dur = %f\n", m_Duration);
	CAlienSwarm* game = ASWGameRules();
	if (game)
	{
		if (gpGlobals->curtime + m_Duration > ASWGameRules()->m_fPreventStimMusicTime.Get())
		{
			Msg("env_slomo setting prevent stim time to %f\n", gpGlobals->curtime + m_Duration);
			ASWGameRules()->m_fPreventStimMusicTime = gpGlobals->curtime + m_Duration;
		}
		else
		{
			Msg("env_slomo not setting prevent stimtime as its alreayd further ahead\n");
		}
		game->StartStim( m_Duration, inputdata.pActivator );
	}
}


void CEnvSlomo::InputStopSlomo( inputdata_t &inputdata )
{
	CAlienSwarm* game = ASWGameRules();
	if (game)
		game->StartStim( 4.0f, inputdata.pActivator );
}

