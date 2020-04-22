//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "particles_simple.h"

//-----------------------------------------------------------------------------
// Purpose: Client side entity for the harpoon
//-----------------------------------------------------------------------------
class C_Harpoon : public C_BaseAnimating
{
	DECLARE_CLASS( C_Harpoon, C_BaseAnimating );
public:
	DECLARE_CLIENTCLASS();

	C_Harpoon();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pOrigin, QAngle *pAngles );

public:
	C_Harpoon( const C_Harpoon & );

private:
	// Impaling
	Vector		m_vecOffset;
	QAngle		m_angOffset;
};

IMPLEMENT_CLIENTCLASS_DT(C_Harpoon, DT_Harpoon, CHarpoon)
	RecvPropVector( RECVINFO(m_vecOffset) ),
	RecvPropVector( RECVINFO(m_angOffset) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Harpoon::C_Harpoon( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Harpoon::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
}


//-----------------------------------------------------------------------------
// Returns the attachment render origin + origin
//-----------------------------------------------------------------------------
void C_Harpoon::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pOrigin, QAngle *pAngles )
{
	C_BaseAnimating *pEnt = dynamic_cast< C_BaseAnimating * >( pAttachedTo->GetBaseEntity() );
	if (!pEnt)
		return;

	float controllers[MAXSTUDIOBONES];
	pEnt->GetBoneControllers(controllers);

	float headcontroller = controllers[ 0 ];

	// Compute angles as well, since parent uses bone controller for rotation

	// Convert 0 - 1 to angles
	float renderYaw = -180.0f + 360.0f * headcontroller;

	matrix3x4_t matrix;

	// Convert roll/pitch only to matrix
	AngleMatrix( pEnt->GetAbsAngles(), matrix );

	// Convert desired yaw to vector
	QAngle anglesRotated( 0, renderYaw, 0 );
	Vector forward;
	AngleVectors( anglesRotated, &forward );

	Vector rotatedForward;

	// Rotate desired yaw vector by roll/pitch matrix
	VectorRotate( forward, matrix, rotatedForward );

	// Convert rotated vector back to orientation
	VectorAngles( rotatedForward, *pAngles );
	//*pAngles -= m_angOffset;
	
	// HACK: Until we have a proper bone solution, hack the origin for all moving objects
	*pOrigin = pEnt->WorldSpaceCenter( );
}