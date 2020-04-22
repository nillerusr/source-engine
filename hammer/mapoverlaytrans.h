//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================

#ifndef OVERLAYTRANS_H
#define OVERLAYTRANS_H
#pragma once

#include "utlvector.h"
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "MapHelper.h"

class IEditorTexture;
class CHelperInfo;
class CMapEntity;
class CMapFace;

struct ShoreEntityData_t
{
	IEditorTexture	*m_pTexture;
	Vector2D		m_vecLengthTexcoord;
	Vector2D		m_vecWidthTexcoord;
	float			m_flWidths[2];
};

//=============================================================================
//
// Overlay Transition Entity Helper
//
class CMapOverlayTransition : public CMapHelper
{
public:

	DECLARE_MAPCLASS(CMapOverlayTransition,CMapHelper)

	CMapOverlayTransition();
	~CMapOverlayTransition();

	// Factory for building from a list of string parameters.
	static CMapClass *Create( CHelperInfo *pInfo, CMapEntity *pParent );

	// Virtual/Interface Implementation.
	virtual void PostloadWorld( CMapWorld *pWorld );

	void CalcBounds( BOOL bFullUpdate = FALSE );

	virtual CMapClass *Copy( bool bUpdateDependencies );
	virtual CMapClass *CopyFrom( CMapClass *pObject, bool bUpdateDependencies );

	virtual void OnParentKeyChanged( const char* szKey, const char* szValue );
	virtual void OnNotifyDependent( CMapClass *pObject, Notify_Dependent_t eNotifyType );

	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

	void DoTransform( const VMatrix& matrix );
	
	void OnPaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, 
				  const CMapObjectList &OriginalList, CMapObjectList &NewList);
	void OnClone( CMapClass *pClone, CMapWorld *pWorld, 
				  const CMapObjectList &OriginalList, CMapObjectList &NewList );
	void OnUndoRedo( void );

	bool OnApply( void );

	void Render3D( CRender3D *pRender );

	inline virtual bool IsVisualElement( void ) { return true; }
	inline virtual bool ShouldRenderLast( void ) { return true; }
	inline const char* GetDescription() { return ( "Overlay Transition" ); }

	ChunkFileResult_t LoadVMF( CChunkFile *pFile );
	ChunkFileResult_t SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo );
	bool ShouldSerialize( void ) { return true; }

private:

	bool BuildFaceCaches( void );

	static ChunkFileResult_t OverlayDataCallback( CChunkFile *pFile, CMapDisp *pDisp );
	static ChunkFileResult_t OverlayDataKeyCallback( const char *szKey, const char *szValue, CMapDisp *pDisp );

private:

	bool					m_bIsWater;
	ShoreEntityData_t		m_ShoreData;

	CUtlVector<CMapFace*>	m_aFaceCache1;
	CUtlVector<CMapFace*>	m_aFaceCache2;

	int						m_nShorelineId;
	CUtlVector<CMapEntity*>	m_aOverlayChildren;

	bool					m_bDebugDraw;
};


#endif // OVERLAYTRANS_H