#include "cbase.h"
#include "c_asw_boomer.h"
#include "c_asw_clientragdoll.h"
#include "asw_fx_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Boomer, DT_ASW_Boomer, CASW_Boomer)
	RecvPropBool( RECVINFO( m_bInflated ) ),
	RecvPropBool( RECVINFO( m_bInflating ) )
END_NETWORK_TABLE()

C_ASW_Boomer::C_ASW_Boomer()
{
}


C_ASW_Boomer::~C_ASW_Boomer()
{
}

/*
void C_ASW_Boomer::SpawnClientSideEffects()
{
	//was i inflated?
	if ( m_bInflated )
	{   
		ParticleProp()->Create( "boomer_explode", PATTACH_POINT, "attach_explosion" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "leg_1_explode" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "leg_2_explode" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "leg_3_explode" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "up_leg_1_explode" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "up_leg_2_explode" );
		ParticleProp()->Create( "joint_goo", PATTACH_POINT, "up_leg_3_explode" );
	}
}
*/

C_BaseAnimating * C_ASW_Boomer::BecomeRagdollOnClient( void )
{
	// effects get spawned in C_ASW_Alien::BecomeRagdollOnClient
	//SpawnClientSideEffects();

	return BaseClass::BecomeRagdollOnClient();
}

C_ClientRagdoll *C_ASW_Boomer::CreateClientRagdoll( bool bRestoring )
{
	return new C_ASW_ClientRagdoll( bRestoring );
}
