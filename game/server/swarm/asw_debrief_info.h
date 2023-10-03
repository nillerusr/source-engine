#ifndef _INCLUDED_ASW_DEBRIEF_INFO_H
#define _INCLUDED_ASW_DEBRIEF_INFO_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
// Hold debriefing messages

class CASW_Debrief_Info : public CLogicalEntity
{
public:
	DECLARE_CLASS( CASW_Debrief_Info, CLogicalEntity );
	DECLARE_DATADESC(); 

	string_t m_DebriefText1;
	string_t m_DebriefText2;
	string_t m_DebriefText3;
};

#endif /* _INCLUDED_ASW_DEBRIEF_INFO_H */