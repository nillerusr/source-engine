//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPINSTANCE_H
#define MAPINSTANCE_H
#pragma once


#include "MapHelper.h"
#include "MapDoc.h"


class CRender3D;
class CManifestMap;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CMapInstance : public CMapHelper
{
	public:
		DECLARE_MAPCLASS(CMapInstance, CMapHelper)

		//
		// Factory for building from a list of string parameters.
		//
		static CMapClass	*Create( CHelperInfo *pInfo, CMapEntity *pParent );
		static void			SetInstancePath( const char *pszInstancePath );
		static const char	*GetInstancePath( void ) { return m_InstancePath; }
		static bool			DeterminePath( const char *pszBaseFileName, const char *pszInstanceFileName, char *pszOutFileName );

		//
		// Construction/destruction:
		//
		CMapInstance( void );
		CMapInstance( const char *pszBaseFileName, const char *pszInstanceFileName );
		~CMapInstance(void);

		GDIV_TYPE	GetFieldType( const char *pszValue );

		virtual void FindTargetNames( CUtlVector< const char * > &Names );
		virtual void ReplaceTargetname( const char *szOldName, const char *szNewName );

		virtual bool OnApply( void );
		virtual void CalcBounds(BOOL bFullUpdate = FALSE);
		virtual void UpdateChild(CMapClass *pChild);
		virtual CMapEntity *FindChildByKeyValue( const char* key, const char* value, bool *bIsInInstance = NULL, VMatrix *InstanceMatrix = NULL );
		virtual void InstanceMoved( void );

		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

		void Initialize(void);
		void SetManifest( CManifestMap *pManifestMap );

		void Render2D(CRender2D *pRender);
		void Render3D(CRender3D *pRender);

		void SwitchTo( void );

		// Called by entity code to render sprites
		void RenderLogicalAt(CRender2D *pRender, const Vector2D &vecMins, const Vector2D &vecMaxs );

		void GetAngles(QAngle &Angles);

		int SerializeRMF(std::fstream &File, BOOL bRMF);
		int SerializeMAP(std::fstream &File, BOOL bRMF);

		bool ShouldRenderLast(void);

		bool IsVisualElement(void) { return(true); }
		
		virtual bool	IsEditable( void );
				bool	IsInstanceVisible( void );
		
		const char		*GetDescription( void ) { return( "Instance" ); }
		CMapDoc			*GetInstancedMap( void ) { return m_pInstancedMap; }
		CManifestMap	*GetManifestMap( void ) { return m_pManifestMap; }
		bool			IsInstance( void ) { return ( m_pManifestMap == NULL ); }
		void			UpdateInstanceMap( void );

		void OnParentKeyChanged(const char* szKey, const char* szValue);

	protected:

		//
		// Implements CMapAtom transformation functions.
		//
		void DoTransform(const VMatrix &matrix);
		
		QAngle			m_Angles;

		char			m_FileName[ MAX_PATH ];
		CMapDoc			*m_pInstancedMap;
		CManifestMap	*m_pManifestMap;

		static char		m_InstancePath[ MAX_PATH ];
};

#endif // MAPINSTANCE_H
