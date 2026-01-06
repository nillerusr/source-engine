#include "cbase.h"

class CPointHurt : public CPointEntity
{
	DECLARE_CLASS( CPointHurt, CPointEntity );

public:
	void	Spawn( void );
	void	Precache( void );
	void	Activate( void );
	void	HurtThink( void );
	void	RadiationThink(void);
#ifndef EZ
	void	TurnOn( CBaseEntity * activator );
#else
	virtual void	TurnOn(CBaseEntity * activator);
#endif

	// Input handlers
	void InputTurnOn(inputdata_t &inputdata);
	void InputTurnOff(inputdata_t &inputdata);
	void InputToggle(inputdata_t &inputdata);
	void InputHurt(inputdata_t &inputdata);
	
#ifdef MAPBASE
	bool KeyValue( const char *szKeyName, const char *szValue );
#endif

	DECLARE_DATADESC();

	int			m_nDamage;
	int			m_bitsDamageType;
	float		m_flRadius;
	float		m_flDelay;
	bool		m_bDisabled = true;
	string_t	m_strTarget;
	EHANDLE		m_pActivator;

	// Fields for expiration
	float	m_flLifetime;
	float	m_flExpirationTime;
};

#ifdef EZ
class CPointHurtGoo : public CPointHurt
{
	DECLARE_CLASS( CPointHurtGoo, CPointHurt );

public:
	void	Precache( void );

	void	TurnOn( CBaseEntity * activator );

	void	GooParticleThink( void );
};
#endif
