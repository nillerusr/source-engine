#include "cbase.h"
#include "asw_objective_triggered.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// An objective controlled by mapper inputs

LINK_ENTITY_TO_CLASS( asw_objective_triggered, CASW_Objective_Triggered );

BEGIN_DATADESC( CASW_Objective_Triggered )
	DEFINE_INPUTFUNC( FIELD_VOID, "SetComplete", InputSetComplete ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetIncomplete", InputSetIncomplete ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetFailed", InputSetFailed ),
END_DATADESC()


CASW_Objective_Triggered::CASW_Objective_Triggered() : CASW_Objective()
{

}


CASW_Objective_Triggered::~CASW_Objective_Triggered()
{
}

void CASW_Objective_Triggered::InputSetComplete( inputdata_t &inputdata )
{
	SetComplete(true);
}

void CASW_Objective_Triggered::InputSetIncomplete( inputdata_t &inputdata )
{
	SetComplete(false);
	SetFailed(false);
}

void CASW_Objective_Triggered::InputSetFailed( inputdata_t &inputdata )
{
	SetFailed(true);
}