//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DETAILOBJECTS_H
#define DETAILOBJECTS_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "datamap.h"
#include "bspfile.h"
#include "builddisp.h"
#include "mapface.h"
#include "mapdisp.h"
#include "utlsymbol.h"
#include "sprite.h"
#include "studiomodel.h"

//=============================================================================
// DetailObjects:: class
//=============================================================================

class DetailObjects : public CMapPoint
{
// Comparators Functions for initialized member lists

	// Error checking.. make sure the model is valid + is a static prop
	struct StaticPropLookup_t
	{
		CUtlSymbol	m_ModelName;
		bool		m_IsValid;
	};

	// Comparator for static property sorting

	static bool StaticLess( StaticPropLookup_t const& src1, StaticPropLookup_t const& src2 )
	{
		return src1.m_ModelName < src2.m_ModelName;
	}

//-----------------------------------------------------------------------------
// Contructors / Destructors
//-----------------------------------------------------------------------------
public:
	DetailObjects() {}
	~DetailObjects();

//-----------------------------------------------------------------------------
// Internal Constants & Data Structures
//-----------------------------------------------------------------------------
protected:
	enum { DETAIL_NAME_LENGTH = 128 };

	enum DetailPropOrientation_t
	{
		DETAIL_PROP_ORIENT_NORMAL = 0,
		DETAIL_PROP_ORIENT_SCREEN_ALIGNED,
		DETAIL_PROP_ORIENT_SCREEN_ALIGNED_VERTICAL,
	};

	// NOTE: If DetailPropType_t enum changes, change CDetailModel::QuadsToDraw
	// in detailobjectsystem.cpp
	enum DetailPropType_t
	{
		DETAIL_PROP_TYPE_MODEL = 0,
		DETAIL_PROP_TYPE_SPRITE,
		DETAIL_PROP_TYPE_SHAPE_CROSS,
		DETAIL_PROP_TYPE_SHAPE_TRI,
	};

	// Information about particular detail object types
	enum
	{
		MODELFLAG_UPRIGHT = 0x1,
	};

	class DetailModel_t
	{
	public:
		DetailModel_t();
		CUtlSymbol	m_ModelName;
		float		m_Amount;
		float		m_MinCosAngle;
		float		m_MaxCosAngle;
		int			m_Flags;
		int			m_Orientation;
		int			m_Type;
		Vector2D	m_Pos[2];
		Vector2D	m_Tex[2];
		float		m_flRandomScaleStdDev;
		unsigned char m_ShapeSize;
		unsigned char m_ShapeAngle;
		unsigned char m_SwayAmount;
	};

	struct DetailObjectGroup_t
	{
		float	m_Alpha;
		CUtlVector< DetailModel_t >	m_Models;
	};

	struct DetailObject_t
	{
		CUtlSymbol m_Name;
		float	m_Density;
		CUtlVector< DetailObjectGroup_t >	m_Groups;

		bool operator==(const DetailObject_t& src ) const
		{
			return src.m_Name == m_Name;
		}
	};


//-----------------------------------------------------------------------------
// Publically callable interfaces
//-----------------------------------------------------------------------------
public:
	static void	LoadEmitDetailObjectDictionary( char const* pGameDir );
	static void	BuildAnyDetailObjects(CMapFace *);
	static void EnableBuildDetailObjects( bool bBuild );	// This is used to delay building detail objects until the
															// end of the map load. Prevents it from generating the
															// detail objects 3x more often than necessary.
	
	void Render3D( CRender3D* pRender );
	bool ShouldRenderLast(void);

//-----------------------------------------------------------------------------
// Internal Helper Functions
//-----------------------------------------------------------------------------
protected:
	static const char *FindDetailVBSPName( void );
	static void	ParseDetailObjectFile( KeyValues& keyValues );
	static void	ParseDetailGroup( int detailId, KeyValues* pGroupKeyValues );

	bool	LoadStudioModel( char const* pFileName, char const* pEntityType, CUtlBuffer& buf );
	float	ComputeDisplacementFaceArea( CMapFace *pMapFace );
	int		SelectGroup( const DetailObject_t& detail, float alpha );
	int		SelectDetail( DetailObjectGroup_t const& group );
	void	PlaceDetail( DetailModel_t const& model, const Vector& pt, const Vector& normal );
	void	EmitDetailObjectsOnFace( CMapFace *pMapFace, DetailObject_t& detail );
	void	EmitDetailObjectsOnDisplacementFace( CMapFace *pMapFace, DetailObject_t& detail );

	void	AddDetailSpriteToFace( const Vector &vecOrigin, const QAngle &vecAngles, DetailModel_t const& model, float flScale );
	void	AddDetailModelToFace( const char* pModelName, const Vector& pt, const QAngle& angles, int nOrientation );



//-----------------------------------------------------------------------------
// Protected Data Members
//-----------------------------------------------------------------------------

	static CUtlVector<DetailObject_t>			s_DetailObjectDict;		// static members?
	static bool s_bBuildDetailObjects;

	CUtlVector<CSpriteModel *>	m_DetailSprites;
	CUtlVector<StudioModel *>	m_DetailModels;
};

#endif // DETAILOBJECTS_H
