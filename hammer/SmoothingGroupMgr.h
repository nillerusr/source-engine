//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SMOOTHINGGROUPMGR_H
#define SMOOTHINGGROUPMGR_H
#pragma once

class CMapFace;
class CChunkFile;
class CSaveInfo;

enum ChunkFileResult_t;

#define MAX_SMOOTHING_GROUP_COUNT	32
#define INVALID_SMOOTHING_GROUP		0xff

typedef unsigned char SmoothingGroupHandle_t;

//=============================================================================
//
// Smoothing Group Manager
//
class ISmoothingGroupMgr
{
public:

	virtual void					AddFaceToGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace ) = 0;
	virtual void					RemoveFaceFromGroup( SmoothingGroupHandle_t hGroup, CMapFace *pFace ) = 0;

	virtual void					SetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup, float flAngle ) = 0;
	virtual float					GetGroupSmoothingAngle( SmoothingGroupHandle_t hGroup ) = 0;

	virtual int						GetFaceCountInGroup( SmoothingGroupHandle_t hGroup ) = 0;
	virtual CMapFace			   *GetFaceFromGroup( SmoothingGroupHandle_t hGroup, int iFace ) = 0;

	virtual ChunkFileResult_t		SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo ) = 0;
	virtual ChunkFileResult_t		LoadVMF( CChunkFile *pFile ) = 0;
};

ISmoothingGroupMgr *SmoothingGroupMgr( void );

#endif // SMOOTHINGGROUPMGR_H
