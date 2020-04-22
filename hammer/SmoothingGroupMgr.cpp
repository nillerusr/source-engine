//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdafx.h>
#include "smoothinggroupmgr.h"
#include "mapface.h"
#include "ChunkFile.h"

class CSmoothingGroupMgr : public ISmoothingGroupMgr
{
public:

	CSmoothingGroupMgr();
	~CSmoothingGroupMgr();

	SmoothingGroupHandle_t		CreateGroup( void );
	void						DestroyGroup( SmoothingGroupHandle_t hGroup ); 

	bool						IsGroup( SmoothingGroupHandle_t hGroup );

	void						AddFaceToGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace );
	void						RemoveFaceFromGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace );

	void						SetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup, float flAngle );
	float						GetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup );

	int							GetFaceCountInGroup( SmoothingGroupHandle_t hGroup );
	CMapFace				   *GetFaceFromGroup( SmoothingGroupHandle_t hGroup, int iFace );

	ChunkFileResult_t			SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo );
	ChunkFileResult_t			LoadVMF( CChunkFile *pFile );

private:

#if 0
	static ChunkFileResult_t LoadSmoothingGroupMgrCallback( const char *szKey, const char *szValue, CSmoothingGroupMgr *pSmoothingGroupMgr );
	static ChunkFileResult_t LoadSmoothingGroupMgrKeyCallback( const char *szKey, const char *szValue, CSmoothingGroupMgr *pSmoothingGroupMgr );

	static ChunkFileResult_t LoadSmoothingGroupCallback( CChunkFile *pFile, SmoothingGroup_t *pGroup );
	static ChunkFileResult_t LoadSmoothingGroupKeyCallback( const char *szKey, const char *szValue, SmoothingGroup_t *pGroup );
#endif

private:

	struct SmoothingGroup_t
	{
		int						m_nID;
		CUtlVector<CMapFace*>	m_aFaces;
		float					m_flSmoothingAngle;
	};

	CUtlVector<SmoothingGroup_t>	m_aSmoothingGroups;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ISmoothingGroupMgr *SmoothingGroupMgr( void )
{
	static CSmoothingGroupMgr s_SmoothingGroupMgr;
	return &s_SmoothingGroupMgr;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSmoothingGroupMgr::CSmoothingGroupMgr()
{
	m_aSmoothingGroups.SetSize( MAX_SMOOTHING_GROUP_COUNT );
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CSmoothingGroupMgr::~CSmoothingGroupMgr()
{
	m_aSmoothingGroups.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Add a face to the smoothing group.
//-----------------------------------------------------------------------------
void CSmoothingGroupMgr::AddFaceToGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		// Check to see if we already have this face in this group.
		if ( pGroup->m_aFaces.Find( pFace ) == -1 )
		{
			pFace->AddSmoothingGroupHandle( hGroup );
			pGroup->m_aFaces.AddToTail( pFace );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSmoothingGroupMgr::RemoveFaceFromGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		int iFace = pGroup->m_aFaces.Find( pFace );
		if ( iFace != -1 )
		{
			pFace->RemoveSmoothingGroupHandle( hGroup );
			pGroup->m_aFaces.Remove( iFace );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSmoothingGroupMgr::SetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup, float flAngle )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		pGroup->m_flSmoothingAngle = flAngle;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CSmoothingGroupMgr::GetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		return pGroup->m_flSmoothingAngle;
	}

	return -1.0f;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSmoothingGroupMgr::GetFaceCountInGroup( SmoothingGroupHandle_t hGroup )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		return pGroup->m_aFaces.Count();
	}

	return -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapFace *CSmoothingGroupMgr::GetFaceFromGroup( SmoothingGroupHandle_t hGroup, int iFace )
{
	// Validate data.
	Assert( hGroup != INVALID_SMOOTHING_GROUP );
	Assert( hGroup >= 0 );
	Assert( hGroup < MAX_SMOOTHING_GROUP_COUNT );

	int iGroup = static_cast<int>( hGroup );
	SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
	if ( pGroup )
	{
		return pGroup->m_aFaces[iFace];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Save the smoothing group data.
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo )
{
	int nGroupCount = 0;
	for ( int iGroup = 0; iGroup < MAX_SMOOTHING_GROUP_COUNT; ++iGroup )
	{
		if ( m_aSmoothingGroups[iGroup].m_aFaces.Count() != 0 )
		{
			nGroupCount++;
		}
	}

	if ( nGroupCount == 0 )
		return ChunkFile_Ok;

	ChunkFileResult_t eResult = pFile->BeginChunk( "smoothing_groups" );

	for ( iGroup = 0; iGroup < MAX_SMOOTHING_GROUP_COUNT; ++iGroup )
	{
		SmoothingGroup_t *pGroup = &m_aSmoothingGroups[iGroup];
		int nFaceCount = pGroup->m_aFaces.Count();
		if ( nFaceCount == 0 )
			continue;

		char szBuf[MAX_KEYVALUE_LEN];
		char szTemp[80];

		// Save the smoothing group.
		if ( eResult == ChunkFile_Ok )
		{
			eResult = pFile->BeginChunk( "group" );
			if ( eResult == ChunkFile_Ok )
			{
				eResult = pFile->WriteKeyValueInt( "id", iGroup );

				if ( eResult == ChunkFile_Ok )
				{
					eResult = pFile->WriteKeyValueFloat( "angle", pGroup->m_flSmoothingAngle );
				}

				if ( eResult == ChunkFile_Ok )
				{
					eResult = pFile->WriteKeyValueInt( "number_faces", nFaceCount );
				}
				
				if ( eResult == ChunkFile_Ok )
				{
					int nColCount = 20;
					int nRowCount = ( nFaceCount / nColCount ) + 1;
					
					for ( int iRow = 0; iRow < nRowCount; ++iRow )
					{
						bool bFirst = true;
						szBuf[0] = '\0';
						
						for ( int iCol = 0; iCol < nColCount; ++iCol )
						{
							int iFace = ( iRow * 20 ) + iCol;
							if ( iFace >= nFaceCount )
								continue;
							
							if (!bFirst)
							{
								strcat(szBuf, " ");
							}

							CMapFace *pFace = pGroup->m_aFaces[iFace];
							if ( pFace )
							{
								bFirst = false;
								sprintf( szTemp, "%d", pFace->GetFaceID() );
								strcat( szBuf, szTemp );
							}
						}
						
						char szKey[10];
						sprintf( szKey, "row%d", iRow );
						eResult = pFile->WriteKeyValue( szKey, szBuf );
					}
				}
			}

			if ( eResult == ChunkFile_Ok )
			{
				eResult = pFile->EndChunk();
			}
		}
	}

	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->EndChunk();
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Purpose: Load smoothing group data.
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::LoadVMF( CChunkFile *pFile )
{
//	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadSmoothingGroupMgrCallback, this );
//	return eResult;

	return ChunkFile_Ok;
}

#if 0

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::LoadSmoothingGroupMgrCallback( const char *szKey, const char *szValue, 
																	 CSmoothingGroupMgr *pSmoothingGroupMgr )
{
    // Get a pointer to the next available smoothing group slot.
	SmoothingGroup_t *pGroup = new SmoothingGroup_t;
	if ( !pGroup )
		return;

	// Set up handlers for the subchunks that we are interested in.
	CChunkHandlerMap Handlers;
	Handlers.AddHandler( "group", ( ChunkHandler_t )LoadsSmoothingGroupCallback, SmoothingGroup_t *pGroup );

	pFile->PushHandlers( &Handlers );
	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadSmoothingGroupMgrCallback, this );
	pFile->PopHandlers();

	if ( eResult == ChunkFile_Ok )
	{
		pGroup->m_nID

		SmoothingGroup_t *pLoadGroup = &pSmoothingGroupMgr->m_aSmoothingGroups[pGroup->m_nID];
		if ( pLoadGroup )
		{
			pLoadGroup->m_nID = pGroup->m_nID;
			pLoadGroup->m_flSmoothingAngle = pGroup->m_flSmoothingAngle;
			pLoadGroup->m_aFaces.CopyArray( pGroup->m_aFaces.Base(), pGroup->m_aFaces.Count() );
		}
	}

	return eResult;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::LoadSmoothingGroupMgrKeyCallback( const char *szKey, const char *szValue, 
																	    CSmoothingGroupMgr *pSmoothingGroupMgr )
{
	return;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::LoadSmoothingGroupCallback( CChunkFile *pFile, SmoothingGroup_t *pGroup )
{
	return( pFile->ReadChunk( ( KeyHandler_t )LoadDispNormalsKeyCallback, pGroup ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CSmoothingGroupMgr::LoadSmoothingGroupKeyCallback( const char *szKey, const char *szValue, SmoothingGroup_t *pGroup )
{
	int nId;
	if ( !strnicmp( szKey, "id", 2 ) )
	{
		CChunkFile::ReadKeyValueInt( szValue, pGroup->m_nID );
	}

	if ( !strnicmp( szKey, "angle", 5 ) )
	{
		CChunkFile::ReadKeyValueFloat( szValue, pGroup->m_flSmoothingAngle );
	}

	if ( !strnicmp( szKey, "number_faces", 12 ) )
	{
		int nFaceCount;
		CChunkFile::ReadKeyValueInt( szValue, nFaceCount );
		pGroup->m_aFaces.SetSize( nFaceCount );
	}

	if ( !strnicmp(szKey, "row", 3 ) )
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

		char szBuf[MAX_KEYVALUE_LEN];
		strcpy( szBuf, szValue );

		int iRow = atoi( &szKey[3] );

		char *pszNext = strtok( szBuf, " " );
		int nIndex = nRow * 20;

		int nFaceID;
		while ( pszNext != NULL )
		{
			nFaceID = ( float )atof( pszNext );
			CMapFace *pFace = 
			


CMapFace *CMapWorld::FaceID_FaceForID(int nFaceID)


			pszNext = strtok( NULL, " " );
			nIndex++;
		}
	}

	return ChunkFile_Ok ;
}
#endif

