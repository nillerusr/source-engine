#ifndef _INCLUDED_ASW_SCANNER_INFO_H
#define _INCLUDED_ASW_SCANNER_INFO_H
#pragma once

#include "asw_shareddefs.h"

// This class maintains data about blip positions on the scanner
class CASW_Scanner_Info : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Scanner_Info, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Scanner_Info();
	virtual ~CASW_Scanner_Info();	

	void Spawn();
	void UpdateBlips();
	void AddBlips(Vector vecScannerCenter, float fDist);

	// info about each blip
	CNetworkArray( int, m_iBlipX, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_iBlipY, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_index, ASW_SCANNER_MAX_BLIPS );
	CNetworkArray( int, m_BlipType, ASW_SCANNER_MAX_BLIPS);
	CNetworkArray( EHANDLE, m_hBlipEntity, ASW_SCANNER_MAX_BLIPS);
	bool m_bActiveBlip[ASW_SCANNER_MAX_BLIPS];

	virtual int	ShouldTransmit( const CCheckTransmitInfo *pInfo );
};

#endif /* _INCLUDED_ASW_SCANNER_INFO_H */