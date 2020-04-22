//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPLIGHTCONE_H
#define MAPLIGHTCONE_H
#ifdef _WIN32
#pragma once
#endif


#include "MapHelper.h"
#include "MapFace.h"
#include "fgdlib/WCKeyValues.h"


class CHelperInfo;
class CRender3D;


class CMapLightCone : public CMapHelper
{
public:

	DECLARE_MAPCLASS(CMapLightCone,CMapHelper);
	
	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);
	
	//
	// Construction/destruction:
	//
	CMapLightCone(void);
	~CMapLightCone(void);

	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	void Render3D(CRender3D *pRender);

	int SerializeRMF(std::fstream &File, BOOL bRMF);
	int SerializeMAP(std::fstream &File, BOOL bRMF);

	virtual void PostloadWorld(CMapWorld *pWorld);

	virtual bool IsVisualElement(void) { return(false); } // Only visible when parent entity is selected.
	virtual bool IsClutter(void) { return true; }
	virtual bool IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs) { return false; } // We don't hide unless our parent hides.

	virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);
		
	const char* GetDescription() { return("Light cone helper"); }

	void OnParentKeyChanged( const char* key, const char* value );
	bool ShouldRenderLast(void) { return(true); }
	void GetAngles(QAngle& fAngles);

	float GetInnerConeAngle(void) const
	{
		return m_fInnerConeAngle;
	}

	float GetOuterConeAngle(void) const
	{
		return m_fOuterConeAngle;
	}

	Vector GetColor(void) const
	{
		float multiplier=m_fBrightness/256.0;
		Vector ret;
		ret.x=GammaToLinear(m_LightColor.x/255.0)*multiplier;
		ret.y=GammaToLinear(m_LightColor.y/255.0)*multiplier;
		ret.z=GammaToLinear(m_LightColor.z/255.0)*multiplier;
		return ret;
	}

	float m_fQuadraticAttn;
	float m_fLinearAttn;
	float m_fConstantAttn;

	float m_fFiftyPercentDistance;							// "_fifty_percent_distance" <0 = not
															// using this mode
	float m_fZeroPercentDistance;							// "_zero_percent_distance"


protected:

	void BuildCone(void);
	float GetBrightnessAtDist(float fDistance);
	float GetLightDist(float fBrightness);
	bool SolveQuadratic(float &x, float y, float A, float B, float C);

	Vector m_LightColor;
	float m_fBrightness;


	float m_fInnerConeAngle;
	float m_fOuterConeAngle;

	QAngle m_Angles;

	bool m_bPitchSet;
	float m_fPitch;

	float m_fFocus;

	CMapFaceList m_Faces;

	char m_szColorKeyName[KEYVALUE_MAX_KEY_LENGTH];
	char m_szInnerConeKeyName[KEYVALUE_MAX_KEY_LENGTH];
	char m_szOuterConeKeyName[KEYVALUE_MAX_KEY_LENGTH];
	float m_flPitchScale;
};

#endif // MAPLIGHTCONE_H
