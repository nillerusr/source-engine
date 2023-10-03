#ifndef _DEFINED_C_ASW_DOOR_AREA_H
#define _DEFINED_C_ASW_DOOR_AREA_H

#include "c_asw_use_area.h"

class C_ASW_Door;

class C_ASW_Door_Area : public C_ASW_Use_Area
{
	DECLARE_CLASS( C_ASW_Door_Area, C_ASW_Use_Area );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Door_Area();	
	
	C_ASW_Door* GetASWDoor();	

	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) { }
	virtual bool ShouldPaintBoxAround() { return false; }

	virtual Class_T	Classify( void ) { return (Class_T) CLASS_ASW_DOOR_AREA; }

protected:
	C_ASW_Door_Area( const C_ASW_Door_Area & ); // not defined, not accessible		
};

#endif /* _DEFINED_C_ASW_DOOR_AREA_H */