#include "cbase.h"
#include "asw_holdout_spawner.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_holdout_spawner, CASW_Holdout_Spawner );

BEGIN_DATADESC( CASW_Holdout_Spawner )
END_DATADESC()

CASW_Holdout_Spawner::CASW_Holdout_Spawner()
{
}

CASW_Holdout_Spawner::~CASW_Holdout_Spawner()
{

}

void CASW_Holdout_Spawner::Spawn()
{
	BaseClass::Spawn();
}