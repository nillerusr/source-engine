#include "cbase.h"
#include "asw_door_padding.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC(CASW_Door_Padding)

END_DATADESC()

LINK_ENTITY_TO_CLASS(asw_door_padding, CASW_Door_Padding);
PRECACHE_REGISTER( asw_door_padding );

#define SINGLE_DOOR_PADDING "models/swarm/doors/swarm_singledoor_padding.mdl"


CASW_Door_Padding::~CASW_Door_Padding( void )
{

}



void CASW_Door_Padding::Spawn()
{	
	BaseClass::Spawn();

	SetCollisionGroup(ASW_COLLISION_GROUP_BLOCK_DRONES);
	SetModel( SINGLE_DOOR_PADDING );
	AddEffects( EF_NODRAW );

	SetSolid(SOLID_VPHYSICS);		
	VPhysicsInitShadow(false, false);	
}

void CASW_Door_Padding::Precache()
{		
	PrecacheModel(SINGLE_DOOR_PADDING);

	BaseClass::Precache();
}

void CASW_Door_Padding::OnRestore( void )
{
	BaseClass::OnRestore();

}

CASW_Door_Padding* CASW_Door_Padding::CreateDoorPadding( CASW_Door *pDoor )
{
	if (!pDoor || !pDoor->CollisionProp())
		return NULL;

	CASW_Door_Padding *pPadding = (CASW_Door_Padding *)CreateEntityByName( "asw_door_padding" );	
	//pPadding->SetAbsAngles( pDoor->GetAbsAngles() );
	//pPadding->SetAbsOrigin( pDoor->GetAbsOrigin() );
	pPadding->Spawn();
	pPadding->SetOwnerEntity( pDoor );	
	pPadding->SetParent(pDoor);
	pPadding->SetLocalOrigin(vec3_origin);
	pPadding->SetLocalAngles(QAngle(0,90,0));
	UTIL_SetOrigin( pDoor, pDoor->GetAbsOrigin() );

	return pPadding;
}

// ==========================================================================

BEGIN_DATADESC(CASW_Fallen_Door_Padding)

END_DATADESC()

LINK_ENTITY_TO_CLASS(asw_fallen_door_padding, CASW_Fallen_Door_Padding);
PRECACHE_REGISTER( asw_fallen_door_padding );

#define SINGLE_DOOR_FALLEN "models/swarm/doors/swarm_singledoor_fallen_padding.mdl"

void CASW_Fallen_Door_Padding::Spawn()
{	
	BaseClass::BaseClass::Spawn();

	SetCollisionGroup(COLLISION_GROUP_NONE);
	SetModel( SINGLE_DOOR_FALLEN );
	AddEffects( EF_NODRAW );
	SetSolid(SOLID_VPHYSICS);		
	VPhysicsInitShadow(false, false);
}

void CASW_Fallen_Door_Padding::Precache()
{		
	PrecacheModel(SINGLE_DOOR_FALLEN);

	BaseClass::Precache();
}

CASW_Fallen_Door_Padding* CASW_Fallen_Door_Padding::CreateFallenDoorPadding( CASW_Door *pDoor )
{
	if (!pDoor || !pDoor->CollisionProp())
		return NULL;

	CASW_Fallen_Door_Padding *pPadding = (CASW_Fallen_Door_Padding *)CreateEntityByName( "asw_fallen_door_padding" );	
	QAngle ang = pDoor->GetAbsAngles();
	if ( pDoor->DoorNeedsFlip())
		ang[YAW] -= 180;
	if (pDoor->m_bFlipped)
		ang[YAW] -= 90;
	else
		ang[YAW] += 90;
	pPadding->SetAbsAngles( ang );
	Vector vecPos = pDoor->GetAbsOrigin();
	//vecPos.z -= 15;
	pPadding->SetAbsOrigin( vecPos );
	pPadding->Spawn();
	//pPadding->SetOwnerEntity( pDoor );
	//UTIL_SetOrigin( pPadding, pDoor->GetAbsOrigin() );

	return pPadding;
}

int CASW_Fallen_Door_Padding::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Fallen_Door_Padding::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );	
}
