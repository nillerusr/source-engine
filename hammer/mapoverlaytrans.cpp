//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================

#include <stdafx.h>
#include "MapEntity.h"
#include "MapOverlayTrans.h"
#include "DispShore.h"
#include "TextureSystem.h"
#include "ChunkFile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapOverlayTransition::CMapOverlayTransition()
{
	m_ShoreData.m_pTexture = NULL;
	m_ShoreData.m_vecLengthTexcoord.Init();
	m_ShoreData.m_vecWidthTexcoord.Init();
	m_ShoreData.m_flWidths[0] = 0.0f;
	m_ShoreData.m_flWidths[1] = 0.0f;
	m_bIsWater = true;
	m_aFaceCache1.Purge();
	m_aFaceCache2.Purge();
	m_bDebugDraw = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapOverlayTransition::~CMapOverlayTransition()
{
}

//-----------------------------------------------------------------------------
// Purpose:
// NOTE: static
//-----------------------------------------------------------------------------
CMapClass *CMapOverlayTransition::Create( CHelperInfo *pInfo, CMapEntity *pParent )
{
	CMapOverlayTransition *pOverlayTrans = new CMapOverlayTransition;
	return pOverlayTrans;
}

//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapOverlayTransition::PostloadWorld( CMapWorld *pWorld )
{
	OnApply();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::CalcBounds( BOOL bFullUpdate )
{
	CMapClass::CalcBounds( bFullUpdate );

	Shoreline_t *pShoreline = GetShoreManager()->GetShoreline( ( int )GetParent() );
	if ( pShoreline )
	{
		Vector vecMins( 99999.0f, 99999.0f, 99999.0f );
		Vector vecMaxs( -99999.0f, -99999.0f, -99999.0f );

		m_Render2DBox.ResetBounds();
		m_CullBox.ResetBounds();

		int nSegmentCount = pShoreline->m_aSegments.Count();
		for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			for ( int iAxis = 0; iAxis < 3; ++iAxis )
			{
				if ( pShoreline->m_aSegments[iSegment].m_vecPoints[0][iAxis] < vecMins[iAxis] )
				{
					vecMins[iAxis] = pShoreline->m_aSegments[iSegment].m_vecPoints[0][iAxis];
				}

				if ( pShoreline->m_aSegments[iSegment].m_vecPoints[1][iAxis] < vecMins[iAxis] )
				{
					vecMins[iAxis] = pShoreline->m_aSegments[iSegment].m_vecPoints[1][iAxis];
				}

				if ( pShoreline->m_aSegments[iSegment].m_vecPoints[0][iAxis] > vecMaxs[iAxis] )
				{
					vecMaxs[iAxis] = pShoreline->m_aSegments[iSegment].m_vecPoints[0][iAxis];
				}

				if ( pShoreline->m_aSegments[iSegment].m_vecPoints[1][iAxis] > vecMaxs[iAxis] )
				{
					vecMaxs[iAxis] = pShoreline->m_aSegments[iSegment].m_vecPoints[1][iAxis];
				}
			}
		}

		m_Render2DBox.UpdateBounds( vecMins, vecMaxs );
		m_CullBox = m_Render2DBox;
		m_BoundingBox = m_CullBox;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapClass *CMapOverlayTransition::Copy( bool bUpdateDependencies )
{
	CMapOverlayTransition *pCopy = new CMapOverlayTransition;
	if ( pCopy )
	{
		pCopy->CopyFrom( this, bUpdateDependencies );
	}

	return pCopy;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapClass *CMapOverlayTransition::CopyFrom( CMapClass *pObject, bool bUpdateDependencies )
{
	// Verify the object is of the correct type and cast.
	Assert( pObject->IsMapClass( MAPCLASS_TYPE( CMapOverlayTransition ) ) );
	CMapOverlayTransition *pFrom = ( CMapOverlayTransition* )pObject;
	if ( pFrom )
	{
		m_ShoreData.m_pTexture = pFrom->m_ShoreData.m_pTexture;
		m_ShoreData.m_vecLengthTexcoord = pFrom->m_ShoreData.m_vecLengthTexcoord;
		m_ShoreData.m_vecWidthTexcoord = pFrom->m_ShoreData.m_vecWidthTexcoord;
		m_ShoreData.m_flWidths[0] = pFrom->m_ShoreData.m_flWidths[0];
		m_ShoreData.m_flWidths[1] = pFrom->m_ShoreData.m_flWidths[1];
	}

	return this;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnParentKeyChanged( const char* szKey, const char* szValue )
{
	// Material data.
	if ( !stricmp( szKey, "material" ) )
	{
		IEditorTexture *pTex = g_Textures.FindActiveTexture( szValue );
		if ( pTex )
		{
			m_ShoreData.m_pTexture = pTex;
		}
	}

	// Texture data.
	if ( !stricmp( szKey, "LengthTexcoordStart" ) )	
	{ 
		m_ShoreData.m_vecLengthTexcoord[0] = atof( szValue );
	}
	if ( !stricmp( szKey, "LengthTexcoordEnd" ) )	
	{ 
		m_ShoreData.m_vecLengthTexcoord[1] = atof( szValue );
	}
	if ( !stricmp( szKey, "WidthTexcoordStart" ) )	
	{ 
		m_ShoreData.m_vecWidthTexcoord[0] = atof( szValue );
	}
	if ( !stricmp( szKey, "WidthTexcoordEnd" ) )	
	{ 
		m_ShoreData.m_vecWidthTexcoord[1] = atof( szValue );
	}

	// Width data.
	if ( !stricmp( szKey, "Width1" ) )	
	{ 
		m_ShoreData.m_flWidths[0] = atof( szValue );
	}
	if ( !stricmp( szKey, "Width2" ) )	
	{ 
		m_ShoreData.m_flWidths[1] = atof( szValue );
	}

	// Debug data.
	if ( !stricmp( szKey, "DebugDraw" ) )
	{
		m_bDebugDraw = true;
		if ( atoi( szValue ) == 0 )
		{
			m_bDebugDraw = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnNotifyDependent( CMapClass *pObject, Notify_Dependent_t eNotifyType )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnAddToWorld( CMapWorld *pWorld )
{
	OnApply();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnRemoveFromWorld( CMapWorld *pWorld, bool bNotifyChildren )
{
	GetShoreManager()->RemoveShoreline( ( int )GetParent() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnUndoRedo( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::DoTransform( const VMatrix& matrix )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnPaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::OnClone( CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlayTransition::Render3D( CRender3D *pRender )
{
	GetShoreManager()->Draw( pRender );
	if ( m_bDebugDraw )
	{
		GetShoreManager()->DebugDraw( pRender );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Generate a face list from the parent entities' side list child.
//-----------------------------------------------------------------------------
bool CMapOverlayTransition::BuildFaceCaches( void )
{
	CMapEntity *pEntity = dynamic_cast<CMapEntity*>( GetParent() );
	if ( pEntity )
	{
		const CMapObjectList *pChildren = pEntity->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			CMapSideList *pSideList = dynamic_cast<CMapSideList*>( pChildren->Element(pos) );
			if ( pSideList )
			{
				// Check name.
				if ( !stricmp( "sides" ,pSideList->GetKeyName() ) )
				{
					int nFaceCount = pSideList->GetFaceCount();
					for ( int iFace = 0; iFace < nFaceCount; ++iFace )
					{
						m_aFaceCache1.AddToTail( pSideList->GetFace( iFace ) );
					}
				}
				else if ( !stricmp( "sides2", pSideList->GetKeyName() ) )
				{
					int nFaceCount = pSideList->GetFaceCount();
					for ( int iFace = 0; iFace < nFaceCount; ++iFace )
					{
						if ( m_bIsWater )
						{
							// Verify that the face is a water face.
							if ( pSideList->GetFace( iFace )->GetTexture()->IsWater() )
							{
								m_aFaceCache2.AddToTail( pSideList->GetFace( iFace ) );
							}
						}
						else
						{
							m_aFaceCache2.AddToTail( pSideList->GetFace( iFace ) );
						}
					}
				}
			}
		}
	}

	return ( ( m_aFaceCache1.Count() > 0 ) && ( m_aFaceCache2.Count() > 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Create the transition overlays.
//-----------------------------------------------------------------------------
bool CMapOverlayTransition::OnApply( void )
{
	if ( m_bIsWater )
	{
		// Create the shoreline.
		if ( GetShoreManager()->Init() )
		{
			if ( BuildFaceCaches() )
			{
				m_nShorelineId = ( int )GetParent();

				GetShoreManager()->AddShoreline( m_nShorelineId );
				Shoreline_t *pShoreline = GetShoreManager()->GetShoreline( m_nShorelineId );
				pShoreline->m_ShoreData = m_ShoreData;

				GetShoreManager()->BuildShoreline( m_nShorelineId, m_aFaceCache1, m_aFaceCache2 );

				// Clean up the face list.
				m_aFaceCache1.Purge();
				m_aFaceCache2.Purge();
			}

			GetShoreManager()->Shutdown();
		}

		// Post updated.
		PostUpdate( Notify_Changed );

		return true;
	}
	else
	{
		// This part is not implemented yet!
		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapOverlayTransition::LoadVMF( CChunkFile *pFile )
{
	// This doesn't need to be implemented until we can "edit" the overlay data.  For
	// now just regenerate the data.

	return ChunkFile_Ok;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapOverlayTransition::SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo )
{
	ChunkFileResult_t eResult = pFile->BeginChunk("overlaytransition");

	m_nShorelineId = ( int )GetParent();
	Shoreline_t *pShoreline = GetShoreManager()->GetShoreline( m_nShorelineId );
	if ( pShoreline )
	{
		int nOverlayCount = pShoreline->m_aOverlays.Count();
		for ( int iOverlay = 0; iOverlay < nOverlayCount; ++iOverlay )
		{
			CMapOverlay *pOverlay = &pShoreline->m_aOverlays[iOverlay];
			if ( pOverlay )
			{
				pOverlay->SaveDataToVMF( pFile, pSaveInfo );
			}
		}
	} 

	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->EndChunk();
	}

	return eResult;
}
