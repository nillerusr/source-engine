#include "cbase.h"
#include "asw_tutorial_spawning.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(info_tutorial_start, CASW_TutorialStartPoint);

ConVar asw_tutorial_save_stage("asw_tutorial_save_stage", "0", FCVAR_ARCHIVE, "How far through the tutorial the player has got");

BEGIN_DATADESC( CASW_TutorialStartPoint )
	DEFINE_KEYFIELD( m_iMarineSlot,			FIELD_INTEGER,		"MarineSlot" ),	
	DEFINE_KEYFIELD( m_iSaveStage,			FIELD_INTEGER,		"SaveStage" ),	
END_DATADESC()

// static
CASW_TutorialStartPoint* CASW_TutorialStartPoint::GetTutorialStartPoint(int iMarineSlot)
{
	if (iMarineSlot<0 || iMarineSlot>=8)
		return NULL;

	CASW_TutorialStartPoint* pStartEntity = (CASW_TutorialStartPoint*) gEntList.FindEntityByClassname( NULL, "info_tutorial_start");
	while (pStartEntity != NULL)
	{
		if (pStartEntity->m_iMarineSlot == iMarineSlot
			&& pStartEntity->m_iSaveStage == GetTutorialSaveStage())
			return pStartEntity;
		pStartEntity = (CASW_TutorialStartPoint*) gEntList.FindEntityByClassname( pStartEntity, "info_tutorial_start");
	}
	return NULL;
}

int CASW_TutorialStartPoint::GetTutorialSaveStage()
{
	return asw_tutorial_save_stage.GetInt();
}