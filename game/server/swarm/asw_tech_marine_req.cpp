#include "cbase.h"
#include "asw_tech_marine_req.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_tech_marine_req, CASW_Tech_Marine_Req );

BEGIN_DATADESC( CASW_Tech_Marine_Req )
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableTechMarineReq",	InputDisableTechMarineReq ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"EnableTechMarineReq",	InputEnableTechMarineReq ),
END_DATADESC()


void CASW_Tech_Marine_Req::Spawn()
{
	BaseClass::Spawn();

	//Msg("CASW_Tech_Marine_Req::Spawn setting mission requires tech to true\n");
	if (ASWGameRules())
	{
		ASWGameRules()->m_bMissionRequiresTech = true;
	}
}

void CASW_Tech_Marine_Req::InputDisableTechMarineReq( inputdata_t &inputdata )
{
	//Msg("CASW_Tech_Marine_Req::InputDisableTechMarineReq setting mission requires tech to false\n");
	if (ASWGameRules())
	{
		ASWGameRules()->m_bMissionRequiresTech = false;
	}
}

void CASW_Tech_Marine_Req::InputEnableTechMarineReq( inputdata_t &inputdata )
{
	//Msg("CASW_Tech_Marine_Req::InputEnableTechMarineReq setting mission requires tech to true\n");
	if (ASWGameRules())
	{
		ASWGameRules()->m_bMissionRequiresTech = true;
	}
}