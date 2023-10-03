#ifndef _INCLUDED_C_ASW_SCANNER_INFO_H
#define _INCLUDED_C_ASW_SCANNER_INFO_H
#pragma once

#include "asw_shareddefs.h"

// holds info about the location of scanner blips

class C_ASW_Scanner_Info : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Scanner_Info, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_ASW_Scanner_Info();
	virtual			~C_ASW_Scanner_Info();

	void CopyServerBlips();	

	CNetworkArray( int, m_iBlipX, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_iBlipY, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_index, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_BlipType, ASW_SCANNER_MAX_BLIPS );
	float m_fClientBlipX[ASW_SCANNER_MAX_BLIPS];
	float m_fClientBlipY[ASW_SCANNER_MAX_BLIPS];
	int m_ClientBlipIndex[ASW_SCANNER_MAX_BLIPS];
	int m_ClientBlipType[ASW_SCANNER_MAX_BLIPS];
	bool m_bClientNewBlip[ASW_SCANNER_MAX_BLIPS];
	float m_fBlipStrength[ASW_SCANNER_MAX_BLIPS];	
	void FadeBlips();

private:
	C_ASW_Scanner_Info( const C_ASW_Scanner_Info & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_SCANNER_INFO_H */