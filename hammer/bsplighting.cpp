//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "material.h"
#include "materialsystem/imesh.h"
#include "disp_common.h"
#include "bsplighting.h"
#include "interface.h"
#include "filesystem.h"
#include "hammer.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


bool SurfHasBumpedLightmaps( int flags )
{
	return ( flags & SURF_BUMPLIGHT ) && 
		( !( flags & SURF_NOLIGHT ) );
}


void InitLMSamples( Vector4D *pSamples, int nSamples, float value )
{
	for( int i=0; i < nSamples; i++ )
	{
		pSamples[i][0] = pSamples[i][1] = pSamples[i][2] = value;
		pSamples[i][3] = 1.0f;
	}
}


void InitLMSamplesRed( Vector4D *pSamples, int nSamples )
{
	for( int i=0; i < nSamples; i++ )
	{
		pSamples[i][0] = 1;
		pSamples[i][1] = pSamples[i][2] = 0;
		pSamples[i][3] = 1.0f;
	}
}


CBSPLighting::CMaterialBuf::CMaterialBuf()
{
	m_nVerts = m_nIndices = 0;
	m_pMesh = NULL;
}


CBSPLighting::CMaterialBuf::~CMaterialBuf()
{
	if( m_pMesh )
	{
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->DestroyStaticMesh( m_pMesh );
	}

	m_DrawCommands.PurgeAndDeleteElements();
}


CBSPLighting::CFaceMaterial::~CFaceMaterial()
{
	m_MaterialBufs.PurgeAndDeleteElements();
	m_Faces.PurgeAndDeleteElements();
}


CBSPLighting::CBSPLighting()
{
	m_nTotalTris = 0;
	m_hVRadDLL = 0;
	m_pVRadDLL = 0;
	m_pBSPLightingThread = 0;
	m_bLightingInProgress = false;
}


CBSPLighting::~CBSPLighting()
{
	Term();
}


void CBSPLighting::Release()
{
	delete this;
}


bool CBSPLighting::Load( char const *pFilename )
{
	// Free everything.
	Term();


	// Load VRAD's DLL (and load the BSP file).
	if( !LoadVRADDLL( pFilename ) )
		return false;


	// Create the lighting thread.
	m_pBSPLightingThread = CreateBSPLightingThread( m_pVRadDLL );
	if( !m_pBSPLightingThread )
		return false;


	// Get the BSP file information from VRAD.
	CBSPInfo file;
	m_pVRadDLL->GetBSPInfo( &file );


	// Allocate faces and verts.
	CUtlVector<char> usedFaces;
	usedFaces.SetSize( file.numfaces );

	int nFaces = 0;
	int nVerts = 0;
	for( int iCountFace=0; iCountFace < file.numfaces; iCountFace++ )
	{
		usedFaces[iCountFace] = 0;

		// Was checking m_LightmapTextureSizeInLuxels[0] twice. Fixing but then
		// commenting out the second check to avoid changing the behavior.
		if( file.dfaces[iCountFace].m_LightmapTextureSizeInLuxels[0] != 0 /*|| 
			file.dfaces[iCountFace].m_LightmapTextureSizeInLuxels[1] != 0*/ )
		{
			texinfo_t *pTexInfo = &file.texinfo[ file.dfaces[iCountFace].texinfo ];

			if( !(pTexInfo->flags & SURF_NODRAW) )
			{
				++nFaces;
				nVerts += file.dfaces[iCountFace].numedges;
				usedFaces[iCountFace] = 1;
			}
		}
	}


	CUtlVector<CFace> faces;
	faces.SetSize( nFaces );

	CUtlVector<CVert> verts;
	verts.SetSize( nVerts );

	m_StoredFaces.SetSize( nFaces );

	
	InitMaterialLUT( file );


	// Make lightmaps and translate the map faces over..
	IMaterialSystem *pMatSys = MaterialSystemInterface();

	// Add the BSP file as a search path so our FindMaterial calls will get
	// VMFs embedded in the BSP file.
	g_pFullFileSystem->AddSearchPath( pFilename, "GAME" );

		m_nTotalTris = 0;
		int iOutVert = 0;
		int iOutFace = 0;
		for( int iFace=0; iFace < file.numfaces; iFace++ )
		{
			dface_t *pIn = &file.dfaces[iFace];

			if( !usedFaces[iFace] )
			{
				continue;
			}

			CFace *pOut = &faces[iOutFace];
			CStoredFace *pStoredFace = &m_StoredFaces[iOutFace];

			++iOutFace;
			
			pStoredFace->m_iMapFace = iFace;
			pStoredFace->m_pFace = pOut;

			pOut->m_pDFace = pIn;
			pOut->m_pStoredFace = pStoredFace;

			// Get its material.
			texinfo_t *pTexInfo = &file.texinfo[pIn->texinfo];
			dtexdata_t *pTexData = &file.dtexdata[pTexInfo->texdata];
			pStoredFace->m_pMaterial = FindOrAddMaterial( file, pTexData->nameStringTableID );
			if( pStoredFace->m_pMaterial )
				pStoredFace->m_pMaterial->m_Faces.AddToTail( pStoredFace );

			// Setup its lightmap.		
			memcpy( pOut->m_LightmapVecs, file.texinfo[pIn->texinfo].lightmapVecsLuxelsPerWorldUnits, sizeof(pOut->m_LightmapVecs) );
			memcpy( pOut->m_LightmapTextureMinsInLuxels, pIn->m_LightmapTextureMinsInLuxels, sizeof(pOut->m_LightmapTextureMinsInLuxels) );

			pStoredFace->m_LightmapSize[0] = pIn->m_LightmapTextureSizeInLuxels[0]+1;
			pStoredFace->m_LightmapSize[1] = pIn->m_LightmapTextureSizeInLuxels[1]+1;
			
			// Setup the verts.
			pOut->m_iVertStart = iOutVert;
			pOut->m_nVerts = pIn->numedges;
			for( int iEdge=0; iEdge < pIn->numedges; iEdge++ )
			{
				int edgeVal = file.dsurfedges[ pIn->firstedge + iEdge ];
				if( edgeVal < 0 )
					verts[pOut->m_iVertStart+iEdge].m_vPos = file.dvertexes[ file.dedges[-edgeVal].v[1] ].point;
				else
					verts[pOut->m_iVertStart+iEdge].m_vPos = file.dvertexes[ file.dedges[edgeVal].v[0] ].point;
			}
			m_nTotalTris += pOut->m_nVerts - 2;

			iOutVert += pOut->m_nVerts;
			pOut->m_iDispInfo = pIn->dispinfo;
		}

	g_pFullFileSystem->RemoveSearchPath( pFilename, "GAME" );

	
	// Allocate lightmaps.. must be grouped by material.
	pMatSys->ResetMaterialLightmapPageInfo();

	pMatSys->BeginLightmapAllocation();

		FOR_EACH_LL( m_FaceMaterials, iMat )
		{
			CFaceMaterial *pMat = m_FaceMaterials[iMat];
			bool bNeedsBumpmap = pMat->m_pMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS );

			FOR_EACH_LL( pMat->m_Faces, iFace )
			{
				CStoredFace *pStoredFace = pMat->m_Faces[iFace];
				CFace *pOut = pStoredFace->m_pFace;
			
				int bumpedSize = pStoredFace->m_LightmapSize[0];
				if( bNeedsBumpmap )
					bumpedSize *= 4;
				
				pOut->m_LightmapSortID = pMatSys->AllocateLightmap(
					bumpedSize,
					pStoredFace->m_LightmapSize[1],
					pStoredFace->m_OffsetIntoLightmapPage,
					pMat->m_pMaterial );
			}
		}

	pMatSys->EndLightmapAllocation();


	// Get sort IDs from the material system.
	CUtlVector<MaterialSystem_SortInfo_t> sortInfos;
	sortInfos.SetSize( pMatSys->GetNumSortIDs() );
	pMatSys->GetSortInfo( sortInfos.Base() );

	for( int iFace=0; iFace < faces.Count(); iFace++ )
	{
		m_StoredFaces[iFace].m_LightmapPageID = sortInfos[faces[iFace].m_LightmapSortID].lightmapPageID;
	}

	// Setup the gamma table.
	BuildGammaTable( 2.2f, 2.2f, 0, 1 );


	// Set lightmap texture coordinates.
	for( int iFace=0; iFace < faces.Size(); iFace++ )
	{
		CFace *pFace = &faces[iFace];
		CStoredFace *pStoredFace = &m_StoredFaces[iFace];
		texinfo_t *pTexInfo = &file.texinfo[pFace->m_pDFace->texinfo];

		int lightmapPageSize[2];
		pMatSys->GetLightmapPageSize( pFace->m_pStoredFace->m_LightmapPageID, &lightmapPageSize[0], &lightmapPageSize[1] );

		pStoredFace->m_BumpSTexCoordOffset = (float)pStoredFace->m_LightmapSize[0] / lightmapPageSize[0];

		// Set its texture coordinates.
		for( int iVert=0; iVert < pFace->m_nVerts; iVert++ )
		{
			CVert *pVert = &verts[ pFace->m_iVertStart + iVert ];
			Vector &vPos = pVert->m_vPos;

			for( int iCoord=0; iCoord < 2; iCoord++ )
			{
				float *lmVec = pFace->m_LightmapVecs[iCoord];
				float flVal = lmVec[0]*vPos[0] + lmVec[1]*vPos[1] + lmVec[2]*vPos[2] + lmVec[3] - pFace->m_LightmapTextureMinsInLuxels[iCoord];

				flVal += pFace->m_pStoredFace->m_OffsetIntoLightmapPage[iCoord];
				flVal += 0.5f; // bilinear...
				flVal /= lightmapPageSize[iCoord];
				Assert( _finite(flVal) );
				pVert->m_vLightCoords[iCoord] = flVal;

				pVert->m_vTexCoords[iCoord] = 
					DotProduct( vPos, *((Vector*)pTexInfo->textureVecsTexelsPerWorldUnits[iCoord]) ) + 
					pTexInfo->textureVecsTexelsPerWorldUnits[iCoord][3];
				
				if( pStoredFace->m_pMaterial )
				{
					if( iCoord == 0 )
						pVert->m_vTexCoords[iCoord] /= pStoredFace->m_pMaterial->m_pMaterial->GetMappingWidth();
					else
						pVert->m_vTexCoords[iCoord] /= pStoredFace->m_pMaterial->m_pMaterial->GetMappingHeight();
				}
			}
		}
	}


	// Create displacements.
	CUtlVector<CDispInfoFaces> dispInfos;
	CreateDisplacements( file, faces, dispInfos );

	BuildLMGroups( file, faces, verts, dispInfos );
	BuildDrawCommands();

	ReloadLightmaps();
	return true;
}


void CBSPLighting::Term()
{
	if( m_pBSPLightingThread )
	{
		m_pBSPLightingThread->Release();
		m_pBSPLightingThread = 0;
	}

	m_nTotalTris = 0;
	
	if( m_hVRadDLL )
	{
		if( m_pVRadDLL )
		{
			// Save the .r0 and .bsp files.
			m_pVRadDLL->Serialize();

			m_pVRadDLL->Release();
			m_pVRadDLL = 0;
		}

		Sys_UnloadModule( m_hVRadDLL );
		m_hVRadDLL = 0;
	}

	m_StoredFaces.Purge();
}


bool CBSPLighting::Serialize()
{
	if( m_pBSPLightingThread )
	{
		// Only serialize if we're not currently in the middle of lighting.
		if( m_pBSPLightingThread->GetCurrentState() == IBSPLightingThread::STATE_FINISHED )
			return m_pVRadDLL->Serialize();
	}

	return false;
}


void CBSPLighting::StartLighting( char const *pVMFFileWithEnts )
{
	if( m_pBSPLightingThread )
	{
		m_pBSPLightingThread->StartLighting( pVMFFileWithEnts );
		m_bLightingInProgress = true;
	}
}


float CBSPLighting::GetPercentComplete()
{
	if( m_bLightingInProgress && m_pBSPLightingThread )
		return m_pBSPLightingThread->GetPercentComplete();
	else
		return -1;
}


void CBSPLighting::Interrupt()
{
	if( m_pBSPLightingThread )
		m_pBSPLightingThread->Interrupt();
}


bool CBSPLighting::CheckForNewLightmaps()
{
	if( !m_pBSPLightingThread )
		return false;

	// Has it finished lighting?
	int curState = m_pBSPLightingThread->GetCurrentState();
	if( m_bLightingInProgress )
	{
		if( curState == IBSPLightingThread::STATE_FINISHED )
		{
			m_bLightingInProgress = false;
			ReloadLightmaps();
			return true;
		}
		else if( curState == IBSPLightingThread::STATE_IDLE )
		{
			m_bLightingInProgress = false;
		}
	}

	return false;
}


#define DRAWLIGHTMAPPAGE
#if defined( DRAWLIGHTMAPPAGE )
	void DrawLightmapPage( IMaterialSystem *materialSystemInterface, int lightmapPageID )
	{
		IMaterial *g_materialDebugLightmap = materialSystemInterface->FindMaterial( "debug/debuglightmap", TEXTURE_GROUP_OTHER );

		// assumes that we are already in ortho mode.
		int lightmapPageWidth, lightmapPageHeight;

		CMatRenderContextPtr pRenderContext( materialSystemInterface );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, g_materialDebugLightmap );

		materialSystemInterface->GetLightmapPageSize( lightmapPageID, &lightmapPageWidth, &lightmapPageHeight );
		pRenderContext->BindLightmapPage( lightmapPageID );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		// texcoord 1 is lightmaptexcoord for fixed function.
		static int yOffset = 30;

		meshBuilder.TexCoord2f( 1, 0.0f, 0.0f );
		meshBuilder.Position3f( 0.0f, yOffset, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.TexCoord2f( 1, 1.0f, 0.0f );
		meshBuilder.Position3f( lightmapPageWidth, yOffset, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.TexCoord2f( 1, 1.0f, 1.0f );
		meshBuilder.Position3f( lightmapPageWidth, yOffset+lightmapPageHeight, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.TexCoord2f( 1, 0.0f, 1.0f );
		meshBuilder.Position3f( 0.0f, yOffset+lightmapPageHeight, 0.0f );
		meshBuilder.AdvanceVertex();


		meshBuilder.End();
		pMesh->Draw();
	}
#endif


void CBSPLighting::Draw()
{
	if( m_FaceMaterials.Count() == 0 )
		return;

	IMaterialSystem *pMatSys = MaterialSystemInterface();
	if( !pMatSys )
		return;

	CMatRenderContextPtr pRenderContext( pMatSys );
	
	CheckForNewLightmaps();

	pRenderContext->Flush();

#if defined( DRAWLIGHTMAPPAGE )
	static bool bDrawIt = false;
	if( bDrawIt )
	{
		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		pRenderContext->Ortho( 0, 0, 300, 300, -99999, 99999 );
		
		static int iPageToDraw = 0;
		DrawLightmapPage( MaterialSystemInterface(), iPageToDraw );

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
		
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();
	}
#endif

	// Draw everything from each material.
	FOR_EACH_LL( m_FaceMaterials, iMat )
	{
		CFaceMaterial *pMat = m_FaceMaterials[iMat];
		
		pRenderContext->Bind( pMat->m_pMaterial );

		FOR_EACH_LL( pMat->m_MaterialBufs, iBuf )
		{
			CMaterialBuf *pBuf = pMat->m_MaterialBufs[iBuf];

			for( int iCmd=0; iCmd < pBuf->m_DrawCommands.Count(); iCmd++ )
			{
				CDrawCommand *pCmd = pBuf->m_DrawCommands[iCmd];

				pRenderContext->BindLightmapPage( pCmd->m_LightmapPageID );
				pBuf->m_pMesh->Draw( pCmd->m_PrimLists.Base(), pCmd->m_PrimLists.Count() );
			}
		}
	}

	pRenderContext->Flush();
}


void CBSPLighting::AssignFaceMaterialCounts(
	CBSPInfo &file,
	CUtlVector<CFace> &faces )
{
	FOR_EACH_LL( m_FaceMaterials, i )
	{
		CFaceMaterial *pMat = m_FaceMaterials[i];
		
		// Start off an initial CMaterialBuf to dump the faces in.
		CMaterialBuf *pBuf = new CMaterialBuf;
		pMat->m_MaterialBufs.AddToTail( pBuf );

		FOR_EACH_LL( pMat->m_Faces, iFace )
		{
			CStoredFace *pStoredFace = pMat->m_Faces[iFace];
			CFace *pFace = pStoredFace->m_pFace;

			pStoredFace->m_iFirstIndex = pBuf->m_nIndices;

			if( pFace->m_iDispInfo == -1 )
			{
				pStoredFace->m_nIndices = (pFace->m_nVerts - 2) * 3;
				
				pBuf->m_nIndices += (pFace->m_nVerts - 2) * 3;
				pBuf->m_nVerts += pFace->m_nVerts;
			}
			else
			{
				ddispinfo_t *pDisp = &file.g_dispinfo[pFace->m_iDispInfo];
				
				int nTris  = Square( 1 << pDisp->power ) * 2;
				int nVerts = Square( (1 << pDisp->power) + 1 );

				pStoredFace->m_nIndices = nTris * 3;
				
				pBuf->m_nIndices += nTris * 3;
				pBuf->m_nVerts += nVerts;
			}

			pBuf->m_Faces.AddToTail( pStoredFace );

			// Don't make the buffers too big..
			if( pBuf->m_nIndices > (16*1024) || pBuf->m_nVerts > (16*1024) )
			{
				pBuf = new CMaterialBuf;
				pMat->m_MaterialBufs.AddToTail( pBuf );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Determines the appropriate vertex format for LMGroup meshes
//-----------------------------------------------------------------------------
VertexFormat_t CBSPLighting::ComputeLMGroupVertexFormat( IMaterial * pMaterial )
{
	VertexFormat_t vertexFormat = pMaterial->GetVertexFormat();

	// FIXME: set VERTEX_FORMAT_COMPRESSED if there are no artifacts and if it saves enough memory (use 'mem_dumpvballocs')
	vertexFormat &= ~VERTEX_FORMAT_COMPRESSED;
	// FIXME: check for and strip unused vertex elements (bone weights+indices, TANGENT_S/T?) - requires reliable material vertex formats first

	return vertexFormat;
}

void CBSPLighting::BuildLMGroups(
	CBSPInfo &file,
	CUtlVector<CFace> &faces,
	CUtlVector<CVert> &verts,
	CUtlVector<CDispInfoFaces> &dispInfos
	)
{
	// Count everything in each CFaceMaterial.
	AssignFaceMaterialCounts( file, faces );


	IMaterialSystem *pMatSys = MaterialSystemInterface();
	if( !pMatSys )
		return;

	CMatRenderContextPtr pRenderContext( pMatSys );

	// Now create the static buffers.
	FOR_EACH_LL( m_FaceMaterials, iMat )
	{
		CFaceMaterial *pMat = m_FaceMaterials[iMat];

		FOR_EACH_LL( pMat->m_MaterialBufs, iBuf )
		{
			CMaterialBuf *pBuf = pMat->m_MaterialBufs[iBuf];

			VertexFormat_t vertexFormat = ComputeLMGroupVertexFormat( pMat->m_pMaterial );
			pBuf->m_pMesh = pRenderContext->CreateStaticMesh( vertexFormat, "terd", pMat->m_pMaterial );
			if( !pBuf->m_pMesh )
				continue;

			bool bNeedsBumpmap = pMat->m_pMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS );

			CMeshBuilder mb;
			mb.Begin( pBuf->m_pMesh, MATERIAL_TRIANGLES, pBuf->m_nVerts, pBuf->m_nIndices );

				// Write all the faces in.
				int iCurBaseVert = 0;

				FOR_EACH_LL( pBuf->m_Faces, iFace )
				{
					CStoredFace *pStoredFace = pBuf->m_Faces[iFace];
					CFace *pFace = pStoredFace->m_pFace;

					if( pFace->m_iDispInfo == -1 )
					{
						// It's a regular face.
						CVert *pVerts = &verts[pFace->m_iVertStart];

						for( int iVert=0; iVert < pFace->m_nVerts; iVert++ )
						{
							mb.Position3fv( (float*)&pVerts[iVert].m_vPos );
							
							mb.TexCoord2fv( 0, pVerts[iVert].m_vTexCoords.Base() );
							mb.TexCoord2fv( 1, pVerts[iVert].m_vLightCoords.Base() );
							if( bNeedsBumpmap )
								mb.TexCoord2f ( 2, pStoredFace->m_BumpSTexCoordOffset, 0 );
							
							mb.Color3f( 1,1,1 );
							mb.AdvanceVertex();		  
						}

						// Write the indices.
						for( int iTri=0; iTri < pFace->m_nVerts-2; iTri++ )
						{
							mb.Index( iCurBaseVert );			mb.AdvanceIndex();
							mb.Index( iCurBaseVert+iTri+1 );	mb.AdvanceIndex();
							mb.Index( iCurBaseVert+iTri+2 );	mb.AdvanceIndex();
						}
						
						iCurBaseVert += pFace->m_nVerts;
					}
					else
					{
						// It's a displacement.
						CDispInfoFaces *pDisp = &dispInfos[pFace->m_iDispInfo];
			
						// Generate the index list.
						unsigned short indices[ (1<<MAX_MAP_DISP_POWER) * (1<<MAX_MAP_DISP_POWER) * 6 ];
						
						int nRequired = DispCommon_GetNumTriIndices( pDisp->m_Power );
						Assert( nRequired <= sizeof(indices)/sizeof(indices[0]) );

						DispCommon_GenerateTriIndices( pDisp->m_Power, indices );

						for( int iIndex=0; iIndex < nRequired; iIndex++ )
						{
							mb.Index( indices[iIndex] + iCurBaseVert );
							mb.AdvanceIndex();
						}

						// Generate the vert list.
						for( int iVert=0; iVert < pDisp->m_Verts.Count(); iVert++ )
						{
							mb.Position3fv( (float*)&pDisp->m_Verts[iVert].m_vPos );
							mb.TexCoord2fv( 0, (float*)&pDisp->m_Verts[iVert].m_vTexCoords );
							mb.TexCoord2fv( 1, (float*)&pDisp->m_Verts[iVert].m_vLightCoords );
							
							if( bNeedsBumpmap )
								mb.TexCoord2f ( 2, pStoredFace->m_BumpSTexCoordOffset, 0 );
							
							mb.AdvanceVertex();
						}

						iCurBaseVert += pDisp->m_Verts.Count();;
					}
				}
		
			mb.End();
		}
	}
}


bool FindDrawCommand( CUtlVector<CBSPLighting::CDrawCommand*> &drawCommands, int lmPageID, int &index )
{
	for( int i=0; i < drawCommands.Count(); i++ )
	{
		if( drawCommands[i]->m_LightmapPageID == lmPageID )
		{
			index = i;
			return true;
		}
	}
	return false;
}


void CBSPLighting::BuildDrawCommands()
{
	FOR_EACH_LL( m_FaceMaterials, iMat )
	{
		CFaceMaterial *pMat = m_FaceMaterials[iMat];

		FOR_EACH_LL( pMat->m_MaterialBufs, iBuf )
		{
			CMaterialBuf *pBuf = pMat->m_MaterialBufs[iBuf];

			// Group by lightmap page IDs.
			FOR_EACH_LL( pBuf->m_Faces, iFace )
			{
				CStoredFace *pFace = pBuf->m_Faces[iFace];
				
				int index;
				if( !FindDrawCommand( pBuf->m_DrawCommands, pFace->m_LightmapPageID, index ) )
				{
					index = pBuf->m_DrawCommands.AddToTail( new CDrawCommand );
					pBuf->m_DrawCommands[index]->m_LightmapPageID = pFace->m_LightmapPageID;
				}

				CPrimList primList;
				primList.m_FirstIndex = pFace->m_iFirstIndex;
				primList.m_NumIndices = pFace->m_nIndices;
				pBuf->m_DrawCommands[index]->m_PrimLists.AddToTail( primList );
			}
		}
	}
}


void CBSPLighting::ReloadLightmaps()
{
	if( !m_pVRadDLL )
		return;

	IMaterialSystem *pMatSys = MaterialSystemInterface();
	if( !pMatSys )
		return;

	CBSPInfo bspInfo;
	m_pVRadDLL->GetBSPInfo( &bspInfo );

	if( !bspInfo.lightdatasize )
		return;

	Vector4D blocklights[4][MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER];

	for( int iFace=0; iFace < m_StoredFaces.Count(); iFace++ )
	{
		CStoredFace *pFace = &m_StoredFaces[iFace];

		// Avoid updating lightmaps in faces that weren't touched.
		if( bspInfo.m_pFacesTouched && !bspInfo.m_pFacesTouched[pFace->m_iMapFace] )
			continue;

		dface_t *pIn = &bspInfo.dfaces[ pFace->m_iMapFace ];
		int nLuxels = pFace->m_LightmapSize[0] * pFace->m_LightmapSize[1];

		bool bNeedsBumpmap = pFace->m_pMaterial->m_pMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS );

		texinfo_t *pTexInfo = &bspInfo.texinfo[ bspInfo.dfaces[pFace->m_iMapFace].texinfo ];
		bool bHasBumpmap = SurfHasBumpedLightmaps( pTexInfo->flags );
			
		int nLightmaps = 1;
		if( bNeedsBumpmap && bHasBumpmap )
			nLightmaps = 4;

		ColorRGBExp32 *pLightmap = (ColorRGBExp32 *)&bspInfo.dlightdata[pIn->lightofs];
		int iLightmap;
		for( iLightmap=0; iLightmap < nLightmaps; iLightmap++ )
		{
			for( int iLuxel=0; iLuxel < nLuxels; iLuxel++ )
			{
				blocklights[iLightmap][iLuxel][0] = TexLightToLinear( pLightmap->r, pLightmap->exponent );
				blocklights[iLightmap][iLuxel][1] = TexLightToLinear( pLightmap->g, pLightmap->exponent );
				blocklights[iLightmap][iLuxel][2] = TexLightToLinear( pLightmap->b, pLightmap->exponent );
				blocklights[iLightmap][iLuxel][3] = 1;
				++pLightmap;
			}
		}

		// If it needs bumpmaps but doesn't have them in the file, then just copy 
		// the lightmap data into the other lightmaps like the engine does.
		if( bNeedsBumpmap && !bHasBumpmap )
		{
			for( iLightmap=1; iLightmap < 4; iLightmap++ )
			{
				memcpy( blocklights[iLightmap], blocklights[0], nLuxels * sizeof( blocklights[0][0] ) );
			}
		}

		if( bNeedsBumpmap )
		{
			pMatSys->UpdateLightmap(
				pFace->m_LightmapPageID,
				pFace->m_LightmapSize,
				pFace->m_OffsetIntoLightmapPage,
				(float*)blocklights[0], (float*)blocklights[1], (float*)blocklights[2], (float*)blocklights[3] );
		}
		else
		{
			pMatSys->UpdateLightmap(
				pFace->m_LightmapPageID,
				pFace->m_LightmapSize,
				pFace->m_OffsetIntoLightmapPage,
				(float*)blocklights[0], NULL, NULL, NULL );
		}
	}
}


bool CBSPLighting::LoadVRADDLL( char const *pFilename )
{
	// Load VRAD's DLL.
	m_hVRadDLL = Sys_LoadModule( "vrad_dll.dll" );
	if( !m_hVRadDLL )
		return false;

	CreateInterfaceFn fn = Sys_GetFactory( m_hVRadDLL );
	if( !fn )
		return false;

	int retCode = 0;
	m_pVRadDLL = (IVRadDLL*)fn( VRAD_INTERFACE_VERSION, &retCode );
	if( !m_pVRadDLL )
		return false;

	// Tell VRAD to load the BSP file.	
	if( !m_pVRadDLL->Init( pFilename ) )
		return false;

	return true;
}


void CBSPLighting::CreateDisplacements( CBSPInfo &file, CUtlVector<CFace> &faces, CUtlVector<CDispInfoFaces> &dispInfos )
{
/*
	IMaterialSystem *pMatSys = MaterialSystemInterface();

	dispInfos.SetSize( file.g_numdispinfo );
	for( int iFace=0; iFace < faces.Size(); iFace++ )
	{
		CFace *pFace = &faces[iFace];
		CStoredFace *pStoredFace = &m_StoredFaces[iFace];
		dface_t *pInFace = pFace->m_pDFace;

		if( pInFace->dispinfo == -1 )
			continue;

		ddispinfo_t *pInDisp = &file.g_dispinfo[pInFace->dispinfo];
		CDispInfoFaces *pOutDisp = &dispInfos[pInFace->dispinfo];

		pOutDisp->m_Power = pInDisp->power;
		int nVertsPerSide = (1 << pInDisp->power) + 1;

		pOutDisp->m_Verts.SetSize( pInDisp->m_LODs[0].m_nVerts );

		int lightmapPageSize[2];
		pMatSys->GetLightmapPageSize( pFace->m_pStoredFace->m_LightmapPageID, &lightmapPageSize[0], &lightmapPageSize[1] );
		
		for( int iVert=0; iVert < pInDisp->m_LODs[0].m_nVerts; iVert++ )
		{
			ddisp_lod_vert_t *pInVert = &file.ddispverts[ pInDisp->m_LODs[0].m_iVertStart + iVert ];
			CVert *pOutVert = &pOutDisp->m_Verts[iVert];

			pOutVert->m_vPos = pInVert->m_vPos;
			for( int iCoord=0; iCoord < 2; iCoord++ )
			{
				float flVal = pInVert->m_LightCoords[iCoord];

				flVal += pFace->m_pStoredFace->m_OffsetIntoLightmapPage[iCoord];
				flVal += 0.5f;
				flVal /= lightmapPageSize[iCoord];
				Assert( _finite(flVal) );
				pOutVert->m_vLightCoords[iCoord] = flVal;

				pOutVert->m_vTexCoords[iCoord] = pInVert->m_TexCoords[iCoord];
				
				if( iCoord == 0 )
					pOutVert->m_vTexCoords[iCoord] /= pStoredFace->m_pMaterial->m_pMaterial->GetMappingWidth();
				else
					pOutVert->m_vTexCoords[iCoord] /= pStoredFace->m_pMaterial->m_pMaterial->GetMappingHeight();
			}
		}
	}
*/
}


void CBSPLighting::InitMaterialLUT( CBSPInfo &file )
{
	m_StringTableIDToMaterial.SetSize( file.nTexDataStringTable );
	for( int i=0; i < m_StringTableIDToMaterial.Count(); i++ )
		m_StringTableIDToMaterial[i] = 0;
}


CBSPLighting::CFaceMaterial* CBSPLighting::FindOrAddMaterial( CBSPInfo &file, int stringTableID )
{
	if( stringTableID >= m_StringTableIDToMaterial.Count() )
	{
		Assert( false );
		return 0;
	}

	if( m_StringTableIDToMaterial[stringTableID] )
	{
		return m_StringTableIDToMaterial[stringTableID];
	}
	else
	{
		IMaterial *pMaterial = 0;
		char *pMaterialName = &file.texDataStringData[ file.texDataStringTable[ stringTableID ] ];
		if( pMaterialName )
			pMaterial = MaterialSystemInterface()->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER );

		// Don't add CFaceMaterials without a material.
		if( !pMaterial )
			return 0;

		// This is lovely. We have to call this stuff to get it to precalculate the data it needs.
		pMaterial->GetMappingHeight();
		pMaterial->RecomputeStateSnapshots();

		CFaceMaterial *pMat = new CFaceMaterial;
		if( pMaterial->IsTranslucent() )
			m_FaceMaterials.AddToTail( pMat );
		else
			m_FaceMaterials.AddToHead( pMat );

		pMat->m_pMaterial = pMaterial;

		m_StringTableIDToMaterial[stringTableID] = pMat;
		return pMat;
	}
}


IBSPLighting* CreateBSPLighting()
{
	return new CBSPLighting;
}
