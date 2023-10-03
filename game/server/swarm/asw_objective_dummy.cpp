#include "cbase.h"
#include "asw_objective_dummy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_dummy, CASW_Objective_Dummy );

BEGIN_DATADESC( CASW_Objective_Dummy )
	
END_DATADESC()


// dummy objectives are complete automatically
CASW_Objective_Dummy::CASW_Objective_Dummy() : CASW_Objective()
{
	m_bDummy = true;
	m_bComplete = true;
}


CASW_Objective_Dummy::~CASW_Objective_Dummy()
{
}