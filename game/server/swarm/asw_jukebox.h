#ifndef _DEFINED_ASW_JUKEBOX_H
#define _DEFINED_ASW_JUKEBOX_H

class CASW_Jukebox : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Jukebox, CBaseEntity );
	DECLARE_DATADESC();

	void InputMusicStart( inputdata_t &inputdata );
	void InputMusicStop( inputdata_t &inputdata );
protected:
	float m_fFadeInTime;
	float m_fFadeOutTime;
	char *m_szDefaultMusic;
};

#endif // #ifndef _DEFINED_ASW_JUKEBOX_H