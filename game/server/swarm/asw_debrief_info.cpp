#include "cbase.h"
#include "asw_debrief_info.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_debrief_info, CASW_Debrief_Info );

BEGIN_DATADESC( CASW_Debrief_Info )
	DEFINE_KEYFIELD( m_DebriefText1, FIELD_STRING, "DebriefText1" ),
	DEFINE_KEYFIELD( m_DebriefText2, FIELD_STRING, "DebriefText2" ),
	DEFINE_KEYFIELD( m_DebriefText3, FIELD_STRING, "DebriefText3" ),
END_DATADESC()

// This class simply holds the 3 debrief strings that can be entered by a mapper in Hammer.
//  During the actual debrief, an CASW_Debrief_Stats is created to actually network these strings
//  (and other debrief data) down to the client.