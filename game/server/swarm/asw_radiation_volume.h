#ifndef _INCLUDED_ASW_RADIATION_VOLUME_H
#define _INCLUDED_ASW_RADIATION_VOLUME_H

class CASW_Radiation_Volume : public CBaseEntity
{
	DECLARE_CLASS( CASW_Radiation_Volume, CBaseEntity );
public:
	CASW_Radiation_Volume();
	void Spawn( void );
	void RadTouch( CBaseEntity *pOther );
	void RadThink();
	void RadHurt(CBaseEntity *pEnt);
	bool RadTouching(CBaseEntity *pEnt);
	bool IsValidRadTarget( CBaseEntity *pOther );
	
	DECLARE_DATADESC();
	CUtlVector<EHANDLE> m_hRadTouching;	
	EHANDLE m_hCreator;
};

#endif /* _INCLUDED_ASW_RADIATION_VOLUME_H */