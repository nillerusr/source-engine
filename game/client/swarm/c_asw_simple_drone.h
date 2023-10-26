#ifndef _INCLUDED_C_ASW_SIMPLE_DRONE_H
#define _INCLUDED_C_ASW_SIMPLE_DRONE_H

#include "c_asw_simple_alien.h"

class C_ASW_Simple_Drone : public C_ASW_Simple_Alien
{
public:
	DECLARE_CLASS( C_ASW_Simple_Drone, C_ASW_Simple_Alien );
	DECLARE_CLIENTCLASS();

					C_ASW_Simple_Drone();
	virtual			~C_ASW_Simple_Drone();

	float GetRunSpeed();
	virtual void ClientThink();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	void UpdatePoseParams();

private:
	C_ASW_Simple_Drone( const C_ASW_Simple_Drone & ); // not defined, not accessible
	float m_flCurrentTravelYaw;
	float m_flCurrentTravelSpeed;
};

#endif /* _INCLUDED_C_ASW_SIMPLE_DRONE_H */