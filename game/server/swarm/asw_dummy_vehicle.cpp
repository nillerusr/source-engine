#include "cbase.h"
#include "props.h"
#include "asw_dummy_vehicle.h"
#include "asw_player.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VEHICLE_MODEL "models/buggy.mdl"

IMPLEMENT_SERVERCLASS_ST( CASW_Dummy_Vehicle, DT_ASW_Dummy_Vehicle )
	SendPropEHandle( SENDINFO( m_hDriver ) ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( asw_dummy_vehicle, CASW_Dummy_Vehicle );
PRECACHE_REGISTER( asw_dummy_vehicle );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Dummy_Vehicle )
	DEFINE_FIELD( m_hDriver, FIELD_EHANDLE ),
END_DATADESC()


CASW_Dummy_Vehicle::CASW_Dummy_Vehicle()
{
	UseClientSideAnimation();
}


CASW_Dummy_Vehicle::~CASW_Dummy_Vehicle()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Dummy_Vehicle::SelectModel()
{
	SetModelName( AllocPooledString( VEHICLE_MODEL ) );
}

CASW_Dummy_Vehicle* g_pDummyVehicle = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Dummy_Vehicle::Spawn( void )
{
	//SetSolid( SOLID_BBOX );
	///AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	
	PrecacheModel(VEHICLE_MODEL);
	Precache();
	SetModel(VEHICLE_MODEL);

	BaseClass::Spawn();

	SetCollisionGroup( COLLISION_GROUP_VEHICLE );

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );	

	SetNextThink( TICK_NEVER_THINK );

	Msg("* Spawned CASW_Dummy_Vehicle\n");

	g_pDummyVehicle = this;
}


int CASW_Dummy_Vehicle::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Dummy_Vehicle::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CASW_Dummy_Vehicle::SetNormalizedPoseParameter(int iParam, float fValue)
{
	CStudioHdr *pStudioHdr = GetModelPtr();	
	if (!pStudioHdr)
		return;

	if (iParam < 0 || iParam >= pStudioHdr->GetNumPoseParameters())
		return;

	const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( iParam );

	float diff = Pose.end - Pose.start;

	SetPoseParameter(iParam, Pose.start + diff * fValue);
}

// implement driver interface
CASW_Marine* CASW_Dummy_Vehicle::ASWGetDriver()
{
	return dynamic_cast<CASW_Marine*>(m_hDriver.Get());
}

void CASW_Dummy_Vehicle::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	if ( pMarine )
	{
		if ( pMarine->IsInVehicle() )
			pMarine->StopDriving(this);
		else
			pMarine->StartDriving(this);
	}
}

bool CASW_Dummy_Vehicle::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

void asw_move_dummy_f()
{
	if (g_pDummyVehicle)
	{
		Vector v = g_pDummyVehicle->GetAbsOrigin();
		static bool bLeft = false;
		if (bLeft)
			v.x += 100;
		else
			v.x -= 100;

		bLeft = !bLeft;

		g_pDummyVehicle->SetAbsOrigin(v);
		Msg("g_pDummyVehicle new origin = %f %f %f\n",
			g_pDummyVehicle->GetAbsOrigin().x,
			g_pDummyVehicle->GetAbsOrigin().y,
			g_pDummyVehicle->GetAbsOrigin().z);
	}
}

ConCommand asw_move_dummy("asw_move_dummy", asw_move_dummy_f, 0, FCVAR_CHEAT);