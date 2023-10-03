#ifndef _INCLUDED_C_ASW_SCANNER_NOISE_H
#define _INCLUDED_C_ASW_SCANNER_NOISE_H
#ifdef _WIN32
#pragma once
#endif

// A clientside only object used to make the scanner visually noisy when the player is near certain spots

struct CASW_NoiseSpot
{
	CASW_NoiseSpot()
	{
		m_vecPosition = vec3_origin;
		m_fCoreRadius = 100.0f;
		m_fMaximumDistance = 200.0f;
	}
	Vector m_vecPosition;
	float m_fCoreRadius;
	float m_fMaximumDistance;
};

struct CASW_EntNoiseSpot
{
	CASW_EntNoiseSpot()
	{
		m_hEnt = NULL;
		m_fCoreRadius = 100.0f;
		m_fMaximumDistance = 200.0f;
	}
	EHANDLE m_hEnt;
	float m_fCoreRadius;
	float m_fMaximumDistance;
};

// one of these is created and holds a list of the noisy spots

class C_ASW_Scanner_Noise
{
public:
	C_ASW_Scanner_Noise();
	virtual ~C_ASW_Scanner_Noise();

	void AddScannerNoise(const Vector& vecPos, float fCoreRadius, float fMaximumDistance);
	void AddScannerEntNoise(C_BaseEntity *pEnt, float fCoreRadius, float fMaximumDistance);
	float GetScannerNoiseAt(const Vector& vecPos);
	void ListScannerNoises();

	CUtlVector<CASW_NoiseSpot*> m_NoiseSpots;
	CUtlVector<CASW_EntNoiseSpot*> m_NoiseEnts;

	static void RecreateAll();
	static void DestroyAll();

protected:
	static const char * ParseEntity( const char *pEntData );
	static void ParseAllEntities(const char *pMapData);
	void ParseMapData( CEntityMapData *mapData );
	bool KeyValue( const char *szKeyName, const char *szValue );
	float m_fCurrentRadius;
	float m_fCurrentCore;
	Vector m_vecCurrentPos;
};

extern C_ASW_Scanner_Noise* g_pScannerNoise;

#endif // _INCLUDED_C_ASW_SCANNER_NOISE_H