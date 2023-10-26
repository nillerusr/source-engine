#ifndef CLIENT_DLL
#include "props.h"
#include "asw_prop_physics.h"
#else
#include "c_props.h"
#include "c_asw_prop_physics.h"
#endif

// This is a physics prop that can only be damaged effectively with the mining laser
// (used for the rocks in Timor Station)

#if defined( CLIENT_DLL )
#define CASW_Prop_Laserable C_ASW_Prop_Laserable
#define CASW_Prop_Physics C_ASW_Prop_Physics
#endif

class CASW_Prop_Laserable : public CASW_Prop_Physics
{
	DECLARE_CLASS( CASW_Prop_Laserable, CASW_Prop_Physics );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();
public:
	CASW_Prop_Laserable();
#ifndef CLIENT_DLL
	void			Spawn( void );
	void			Precache( void );
	void			Activate( void );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );

	string_t m_Key_BreakEffect;
	string_t m_Key_BreakSound;
	CNetworkString( m_iszBreakEffect, 255 );
	CNetworkString( m_iszBreakSound, 255 );

	float m_fNextLaseredEventTime;
#else
	virtual void UpdateOnRemove( void );

	char m_iszBreakEffect[255];
	char m_iszBreakSound[255];
#endif
};
