//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPSTUDIOMODEL_H
#define MAPSTUDIOMODEL_H
#ifdef _WIN32
#pragma once
#endif


#include "MapHelper.h"
#include "StudioModel.h"


class CRender2D;
class CRender3D;


class CMapStudioModel : public CMapHelper
{
	public:

		//
		// Factories.
		//
		static CMapClass *CreateMapStudioModel(CHelperInfo *pHelperInfo, CMapEntity *pParent);
		static CMapStudioModel *CreateMapStudioModel(const char *pszModelPath, bool bOrientedBBox, bool bReversePitch);

		static void AdvanceAnimation(float flInterval);

		//
		// Construction/destruction:
		//
		CMapStudioModel(void);
		~CMapStudioModel(void);

		DECLARE_MAPCLASS(CMapStudioModel,CMapHelper)

		void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		void Initialize(void);

		void Render2D(CRender2D *pRender);
		void Render3D(CRender3D *pRender);

		void GetAngles(QAngle& pfAngles);
		void SetAngles(QAngle& fAngles);

		void OnParentKeyChanged(const char* szKey, const char* szValue);

		bool RenderPreload(CRender3D *pRender, bool bNewContext);

		int SerializeRMF(std::fstream &File, BOOL bRMF);
		int SerializeMAP(std::fstream &File, BOOL bRMF);

		static void SetRenderDistance(float fRenderDistance);
		static void EnableAnimation(BOOL bEnable);

		bool IsVisualElement(void) { return(true); }
		
		bool ShouldRenderLast();

		const char* GetDescription() { return("Studio model"); }

		int GetFrame(void);
		void SetFrame(int nFrame);

		int GetSequence(void);
		int GetSequenceCount(void);
		void GetSequenceName(int nIndex, char *szName);
		void SetSequence(int nIndex);
		
		// Returns the index of the sequence (does a case-insensitive search).
		// Returns -1 if the sequence doesn't exist.
		int GetSequenceIndex( const char *pSequenceName ) const;

	protected:

		inline float ComputeFade( CRender3D *pRender ) const;
		inline float ComputeDistanceFade( CRender3D *pRender ) const;
		inline float ComputeScreenFade( CRender3D *pRender ) const;

		void GetRenderAngles(QAngle &Angles);
		
		//
		// Implements CMapAtom transformation functions.
		//
		void DoTransform(const VMatrix &matrix);
		
		inline void ReversePitch(bool bReversePitch);
		inline void SetOrientedBounds(bool bOrientedBounds);

		StudioModel *m_pStudioModel;		// Pointer to a studio model in the model cache.
		QAngle m_Angles;					// Euler angles of this studio model.
		float m_flPitch;					// Pitch (stored separately for lights -- yuck!)
		bool m_bPitchSet;

		int	m_Skin;							// the model skin

		bool m_bOrientedBounds;				// Whether the bounding box should consider the orientation of the model.
											// Note that this is not a true oriented bounding box, but an axial box
											// indicating the extents of the oriented model.

		bool m_bReversePitch;				// Lights negate pitch, so models representing light sources in Hammer
											// must do so as well.

		bool m_bScreenSpaceFade;			// If true, min & max dist are pixel size in screen space.
		float m_flFadeScale;				// Multiplied by distance to camera before calculating fade.
		float m_flFadeMinDist;				// The distance/pixels at which this model is fully visible.
		float m_flFadeMaxDist;				// The distance/pixels at which this model is fully invisible.
		int m_iSolid;						// The collision setting of this model: 0 = not solid, 2 = bounding box, 6 = vphysics

		//
		// Data that is common to all studio models.
		//
		static float m_fRenderDistance;		// Distance beyond which studio models render as bounding boxes.
		static BOOL m_bAnimateModels;		// Whether to animate studio models.
};


//-----------------------------------------------------------------------------
// Purpose: Sets whether this object has an oriented or axial bounding box.
//			Note that this is not a true oriented bounding box, but an axial box
//			indicating the extents of the oriented model.
//-----------------------------------------------------------------------------
void CMapStudioModel::SetOrientedBounds(bool bOrientedBounds)
{
	m_bOrientedBounds = bOrientedBounds;
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether this object negates pitch.
//-----------------------------------------------------------------------------
void CMapStudioModel::ReversePitch(bool bReversePitch)
{
	m_bReversePitch = bReversePitch;
}


#endif // MAPSTUDIOMODEL_H
