//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTURECONVERT_H
#define TEXTURECONVERT_H
#pragma once


#include "MapWorld.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapDecal.h"
#include "IEditorTexture.h"
#include "resource.h"
#include "ProgDlg.h"

class CTextureConverter
{
public:
	static void	ConvertWorldTextures( CMapWorld * pWorld );

private:
	static void			Initialize( void );
	static void			ConvertSolids( CMapWorld * pWorld );
	static void			ConvertDecals( CMapWorld * pWorld );
	static bool			CountMapSolids( CMapSolid * pSolid, DWORD );
	static bool			CountMapDecals( CMapEntity *, DWORD );
	static bool			CheckSolidTextures( CMapSolid * pSolid, DWORD );
	static bool			CheckDecalTextures( CMapEntity * pEnt, DWORD );
	static void			CheckFaceTexture( CMapFace * pFace );
	static void			ConvertFaceTexture( CMapFace * pFace );
	static void			ConvertDecalTexture( CMapEntity * pEnt );
	static void			GetNewTextureMatches( const char * pszOldName, EditorTextureList_t &MatchList );
	static bool			TextureNameMatchesMaterialName( const char * pszTextureName, const char * pszMaterialName );
	static void			ReplaceFaceTexture( CMapFace * pFace, IEditorTexture * pNewTexture );
	static void			ReplaceDecalTexture( CMapEntity * pEnt, IEditorTexture * pNewTexture );
	static IEditorTexture *	FindWAD3Texture( const char * pszName );
	static void			RescaleFaceTexture( CMapFace * pFace, IEditorTexture * pNewTexture );
	static void			MsgConvertFace( CMapFace * pFace, PRINTF_FORMAT_STRING const char * format, ... );
	static void			MsgConvertDecal( CMapEntity * pEnt, PRINTF_FORMAT_STRING const char * format, ... );
	static void			DisplayStatistics( void );

	static CProgressDlg *	m_pProgDlg;
	static int				m_nSolidCount;
	static int				m_nFaceCount;
	static int				m_nDecalCount;
	static int				m_nCurrentSolid;
	static int				m_nCurrentDecal;
	static int				m_nSuccesses;
	static int				m_nErrors;
	static int				m_nSkipped;
	static int				m_nWarnings;
};


#endif // TEXTURECONVERT_H
