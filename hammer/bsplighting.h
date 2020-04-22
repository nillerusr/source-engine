//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BSPLIGHTING_H
#define BSPLIGHTING_H
#ifdef _WIN32
#pragma once
#endif


#include "stdafx.h"
#include "ibsplighting.h"
#include "utlvector.h"
#include "utllinkedlist.h"
#include "bspfile.h"
#include "interface.h"
#include "ivraddll.h"
#include "ibsplightingthread.h"


class CBSPLighting : public IBSPLighting
{
public:

							CBSPLighting();
	virtual					~CBSPLighting();
	virtual void			Release();

	virtual bool			Load( char const *pFilename );	
	virtual void			Term();
	virtual bool			Serialize();
	virtual void			StartLighting( char const *pVMFFileWithEnts );
	virtual float			GetPercentComplete();
	virtual void			Interrupt();
	virtual bool			CheckForNewLightmaps();
	virtual void			Draw();


private:
	class CVert
	{
	public:
		Vector		m_vPos;
		Vector2D	m_vTexCoords;
		Vector2D	m_vLightCoords;
	};

	// This is the face data we store for each face. Just enough to
	// let us update the lightmaps in memory.
	class CFaceMaterial;
	class CFace;
	class CStoredFace
	{
	public:
		int				m_iMapFace;			// index into dfaces.
		int				m_LightmapPageID;
		int				m_OffsetIntoLightmapPage[2];
		int				m_LightmapSize[2];	// This already has 1 added to it (unlike dface).
		CFaceMaterial	*m_pMaterial;
		CFace			*m_pFace; // only valid inside of Load
		float			m_BumpSTexCoordOffset;

		// Indices into CFaceMaterial::m_pMesh
		int				m_iFirstIndex;
		int				m_nIndices;
	};

	class CDrawCommand
	{
	public:
		CUtlVector<CPrimList>	m_PrimLists;
		int						m_LightmapPageID;
	};

	friend bool FindDrawCommand( CUtlVector<CDrawCommand*> &drawCommands, int lmPageID, int &index );

	class CMaterialBuf
	{
	public:
				CMaterialBuf();
				~CMaterialBuf();

		CUtlLinkedList<CStoredFace*, unsigned short> m_Faces;

		// Commands to draw everything in this material as fast as possible.
		CUtlVector<CDrawCommand*>		m_DrawCommands;
		
		int		m_nVerts;
		int		m_nIndices;

		IMesh	*m_pMesh;
	};


	class CFaceMaterial
	{
	public:
										~CFaceMaterial();

		IMaterial						*m_pMaterial;
	
		// Faces using this material.
		CUtlLinkedList<CStoredFace*, unsigned short>	m_Faces;

		// Static buffers to hold all the verts.
		CUtlLinkedList<CMaterialBuf*, unsigned short>	m_MaterialBufs;
	};

	class CFace
	{
	public:
		
		int		m_iDispInfo;
		dface_t	*m_pDFace;	// used while loading..

		CStoredFace	*m_pStoredFace;
		int		m_LightmapSortID;
		
		float	m_LightmapVecs[2][4];
		int		m_LightmapTextureMinsInLuxels[2];

		int		m_iVertStart;	// Indexes CBSPLighting::m_Verts.
		int		m_nVerts;
	};

	class CDispInfoFaces
	{
	public:
		CUtlVector<CVert>			m_Verts;
		int							m_Power;
	};

private:

	void					AssignFaceMaterialCounts(
		CBSPInfo &file,
		CUtlVector<CFace> &faces );

	VertexFormat_t			ComputeLMGroupVertexFormat( IMaterial * pMaterial );

	void					BuildLMGroups(
		CBSPInfo &file,
		CUtlVector<CFace> &faces,
		CUtlVector<CVert> &verts,
		CUtlVector<CDispInfoFaces> &dispInfos
		);

	void					BuildDrawCommands();

	void					ReloadLightmaps();
	bool					LoadVRADDLL( char const *pFilename );
	void					CreateDisplacements( CBSPInfo &file, CUtlVector<CFace> &faces, CUtlVector<CDispInfoFaces> &dispInfos );
	
	// Fast material ID to CFaceMaterial lookups..
	void					InitMaterialLUT( CBSPInfo &file );
	CFaceMaterial*			FindOrAddMaterial( CBSPInfo &file, int stringTableID );


private:
	CUtlVector<CStoredFace>			m_StoredFaces;
	CUtlLinkedList<CFaceMaterial*, unsigned short> m_FaceMaterials;

	int								m_nTotalTris;

	// The VRAD DLL. This holds the level file.
	CSysModule						*m_hVRadDLL;
	IVRadDLL						*m_pVRadDLL;

	// The lighting thread.
	IBSPLightingThread				*m_pBSPLightingThread;

	// Used to detect when lighting is finished so it can update the lightmaps
	// in the material system.
	bool							m_bLightingInProgress;

	// Maps string table IDs to materials.
	CUtlVector<CFaceMaterial*>		m_StringTableIDToMaterial;
};



#endif // BSPLIGHTING_H
