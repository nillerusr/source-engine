#ifndef _INCLUDED_ASW_MAP_RESET_FILTER_H
#define _INCLUDED_ASW_MAP_RESET_FILTER_H
 
#include "mapentities.h"
#include "UtlSortVector.h"

// Define the comparison function for sorting

typedef const char* StringPointer_t;

static bool StringLessThan(const StringPointer_t &string1,  const StringPointer_t &string2, void *pCtx) 
{
	return (strcmp(string1, string2) < 0);
}
 
class CASW_Map_Reset_Filter : public IMapEntityFilter
{
public:
	CASW_Map_Reset_Filter();
	virtual ~CASW_Map_Reset_Filter();

	virtual bool ShouldCreateEntity(const char *pClassname);	
	virtual CBaseEntity* CreateNextEntity(const char *pClassname);
	void AddKeepEntity(const char*);
 
private:
	CUtlVector<const char*> *m_pKeepEntities;
};
 
#endif // _INCLUDED_ASW_MAP_RESET_FILTER_H