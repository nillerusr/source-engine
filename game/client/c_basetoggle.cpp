//--------------------------------------------------------------------------------------------------------
// Copyright (c) 2007 Turtle Rock Studios, Inc.

#include "cbase.h"
#include "c_basetoggle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_BaseToggle, DT_BaseToggle, CBaseToggle )
END_RECV_TABLE()


//--------------------------------------------------------------------------------------------------------
// Returns the velocity imparted to players standing on us.
void C_BaseToggle::GetGroundVelocityToApply( Vector &vecGroundVel )
{
	vecGroundVel = GetLocalVelocity();
	vecGroundVel.z = 0.0f; // don't give upward velocity, or it could predict players into the air.
}


//--------------------------------------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_BaseButton, DT_BaseButton, CBaseButton )
	RecvPropBool( RECVINFO( m_usable ) ),
END_RECV_TABLE()


//--------------------------------------------------------------------------------------------------------
bool C_BaseButton::IsPotentiallyUsable( void )
{
	return true;
}
