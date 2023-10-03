#ifndef _INCLUDED_ASW_BARREL_RADIOACTIVE_H
#define _INCLUDED_ASW_BARREL_RADIOACTIVE_H
#pragma once

#include "props.h"
#include "asw_prop_physics.h"

class CASW_Emitter;
class CASW_Radiation_Volume;

class CASW_Barrel_Radioactive : public CASW_Prop_Physics
{
public:
	DECLARE_CLASS( CASW_Barrel_Radioactive, CASW_Prop_Physics );
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Barrel_Radioactive();
	virtual ~CASW_Barrel_Radioactive();
	void	Spawn( void );
	
	// damage related
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );	
	virtual void Burst( const CTakeDamageInfo &info );
	
	void InputBurst( inputdata_t &inputdata );
	
	bool m_bBurst;
	CHandle<CASW_Emitter> m_hRadJet;
	CHandle<CASW_Emitter> m_hRadCloud;
	CHandle<CASW_Radiation_Volume> m_hRadVolume;

	// sound
	virtual void			StopLoopingSounds();
	virtual void			StartRadLoopSound();
	
	CSoundPatch	*m_pRadSound;
};


#endif /* _INCLUDED_ASW_BARREL_RADIOACTIVE_H */