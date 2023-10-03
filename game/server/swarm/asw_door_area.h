#ifndef _DEFINED_ASW_DOOR_AREA_H
#define _DEFINED_ASW_DOOR_AREA_H

#include "triggers.h"
#include "asw_use_area.h"

class CASW_Door;
class CASW_Marine;

class CASW_Door_Area : public CASW_Use_Area
{
	DECLARE_CLASS( CASW_Door_Area, CASW_Use_Area );
public:
	CASW_Door_Area();	
	virtual void ActivateMultiTrigger(CBaseEntity *pActivator);
	CASW_Door* GetASWDoor();

	bool HasWelder(CASW_Marine *pMarine);

	virtual Class_T	Classify( void ) { return (Class_T) CLASS_ASW_DOOR_AREA; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	float m_fNextCutCheck;	
};

#endif /* _DEFINED_ASW_DOOR_AREA_H */