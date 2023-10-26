#ifndef _INCLUDED_ASW_HOLOSCAN_H
#define _INCLUDED_ASW_HOLOSCAN_H
#ifdef _WIN32
#pragma once
#endif

class CASW_HoloScan : public CPointEntity
{
public:
	DECLARE_CLASS( CASW_HoloScan, CPointEntity );	
	DECLARE_DATADESC();
		
	virtual void Spawn();
	virtual void Precache();
		
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual int UpdateTransmitState();

	void ScanThink();
	void Scan();
	void ProjectBeam( const Vector &vecStart, const Vector &vecDir, int width, int brightness, float duration );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	bool m_bEnabled;
	bool m_bStartDisabled;

	float m_flTimeNextScanPing;
};


#endif // _INCLUDED_ASW_HOLOSCAN_H