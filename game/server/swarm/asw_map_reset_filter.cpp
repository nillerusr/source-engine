#include "cbase.h"
#include "asw_map_reset_filter.h"
 
CASW_Map_Reset_Filter::CASW_Map_Reset_Filter() 
{
	m_pKeepEntities = new CUtlVector<const char*>;
}

CASW_Map_Reset_Filter::~CASW_Map_Reset_Filter() 
{
	delete m_pKeepEntities;
}
 
bool CASW_Map_Reset_Filter::ShouldCreateEntity(const char *pszClassname) 
{
	// Returns true if the entity isn't in our list of entities to keep around
	return (m_pKeepEntities->Find(pszClassname)<0);
}
 
CBaseEntity* CASW_Map_Reset_Filter::CreateNextEntity(const char *pszClassname) 
{
	return CreateEntityByName(pszClassname);
}
 

void CASW_Map_Reset_Filter::AddKeepEntity(const char *pszClassname) 
{
	m_pKeepEntities->AddToTail(pszClassname);
}