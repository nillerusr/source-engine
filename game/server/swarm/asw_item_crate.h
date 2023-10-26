#ifndef _INCLUDED_ASW_BARREL_RADIOACTIVE_H
#define _INCLUDED_ASW_BARREL_RADIOACTIVE_H
#pragma once

#include "props.h"
#include "asw_prop_physics.h"

class CASW_Emitter;
class CASW_Radiation_Volume;

class CASW_Item_Crate : public CASW_Prop_Physics
{
public:
	DECLARE_CLASS( CASW_Item_Crate, CASW_Prop_Physics );
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Item_Crate();
	virtual ~CASW_Item_Crate();
	void	Spawn( void );
	
	// damage related
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );	
};


#endif /* _INCLUDED_ASW_BARREL_RADIOACTIVE_H */