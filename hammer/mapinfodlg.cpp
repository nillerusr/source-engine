//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "IEditorTexture.h"
#include "MapEntity.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "MapInfoDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static BOOL CountObject(CMapClass *pobj);


BEGIN_MESSAGE_MAP(CMapInfoDlg, CDialog)
	//{{AFX_MSG_MAP(CMapInfoDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Callback for enumerating map objects while gathering statistics for
//			the map information dialog. Routes each object to the apppropriate
//			handler.
// Input  : *pobj - Object to count.
// Output : Returns TRUE to continue enumerating.
//-----------------------------------------------------------------------------
static BOOL CountObject(CMapClass *pobj, unsigned int dwParam)
{
	CMapInfoDlg *pdlg = (CMapInfoDlg *)dwParam;

	if (pdlg != NULL)
	{
		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
		{
			pdlg->CountSolid((CMapSolid *)pobj);
		}
		else if (pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
		{
			pdlg->CountEntity((CMapEntity *)pobj);
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pWorld - 
//			pParent
//-----------------------------------------------------------------------------
CMapInfoDlg::CMapInfoDlg(CMapWorld *pWorld, CWnd* pParent /*=NULL*/)
	: CDialog(CMapInfoDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMapInfoDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	this->pWorld = pWorld;
}


//-----------------------------------------------------------------------------
// Purpose: Gathers statistics about an entity.
// Input  : *pEntity - Entity to count.
//-----------------------------------------------------------------------------
void CMapInfoDlg::CountEntity(CMapEntity *pEntity)
{
	if (pEntity->IsPlaceholder())
	{
		m_uPointEntityCount++;
	}
	else
	{
		m_uSolidEntityCount++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gathers statistics about a face.
// Input  : *pFace - Face to count.
//-----------------------------------------------------------------------------
void CMapInfoDlg::CountFace(CMapFace *pFace)
{
	if (pFace->GetTexture() != NULL)
	{
		CountTexture(pFace->GetTexture());
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gathers statistics about a brush.
// Input  : *pSolid - Brush to count.
//-----------------------------------------------------------------------------
void CMapInfoDlg::CountSolid(CMapSolid *pSolid)
{
	m_uSolidCount++;

	UINT uFaceCount = pSolid->GetFaceCount();
	m_uFaceCount += uFaceCount;

	for (UINT uFace = 0; uFace < uFaceCount; uFace++)
	{
		CMapFace *pFace = pSolid->GetFace(uFace);
		if (pFace != NULL)
		{
			CountFace(pFace);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gathers statistics about this texture. Increments the count of
//			unique textures in the map and total texture memory used. Adds this
//			texture's WAD file to the list of used WADs.
// Input  : *pTex - Texture to count.
//-----------------------------------------------------------------------------
void CMapInfoDlg::CountTexture(IEditorTexture *pTex)
{
	//
	// If this texture is in our list, don't do anything - it has been tallied.
	//
	for (UINT uTexture = 0; uTexture < m_uUniqueTextures; uTexture++)
	{
		if (m_pTextures[uTexture] == pTex)
		{
			return;
		}
	}

	//
	// Add the texture to our list of used textures.
	//
	m_pTextures[m_uUniqueTextures] = pTex;
	m_uUniqueTextures++;

	//
	// Calculate memory used by this texture.
	//
	short nWidth = pTex->GetWidth();
	short nHeight = pTex->GetHeight();

	m_uTextureMemory += nWidth * nHeight * 3;

	//
	// Add the filename to the list box if it isn't already there.
	//
	const char *pszFileName = pTex->GetFileName();
	if (pszFileName[0] != '\0')
	{
		if (m_WadsUsed.FindStringExact(0, pszFileName) == LB_ERR)
		{
			m_WadsUsed.AddString(pszFileName);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets up child windows.
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CMapInfoDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMapInfoDlg)
	DDX_Control(pDX, IDC_FACES, m_Faces);
	DDX_Control(pDX, IDC_SOLIDS, m_Solids);
	DDX_Control(pDX, IDC_SOLIDENTITIES, m_SolidEntities);
	DDX_Control(pDX, IDC_POINTENTITIES, m_PointEntities);
	DDX_Control(pDX, IDC_UNIQUETEXTURES, m_UniqueTextures);
	DDX_Control(pDX, IDC_TEXTUREMEMORY, m_TextureMemory);
	DDX_Control(pDX, IDC_WADSUSED, m_WadsUsed);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: Tallies up statistics about this map and puts it in the dialog
//			controls for display.
//-----------------------------------------------------------------------------
BOOL CMapInfoDlg::OnInitDialog(void)
{
	CDialog::OnInitDialog();
	
	m_uSolidCount = 0;
	m_uPointEntityCount = 0;
	m_uSolidEntityCount = 0;
	m_uFaceCount = 0;
	m_uUniqueTextures = 0;
	m_uTextureMemory = 0;

	// count objects!
	pWorld->EnumChildren(ENUMMAPCHILDRENPROC(CountObject), (DWORD)this);

	char szBuf[128];
	ultoa(m_uSolidCount, szBuf, 10);
	m_Solids.SetWindowText(szBuf);

	ultoa(m_uSolidEntityCount, szBuf, 10);
	m_SolidEntities.SetWindowText(szBuf);

	ultoa(m_uPointEntityCount, szBuf, 10);
	m_PointEntities.SetWindowText(szBuf);
	
	ultoa(m_uFaceCount, szBuf, 10);
	m_Faces.SetWindowText(szBuf);

	ultoa(m_uUniqueTextures, szBuf, 10);
	m_UniqueTextures.SetWindowText(szBuf);

	ultoa(m_uTextureMemory, szBuf, 10);
	sprintf(szBuf, "%u bytes (%.2f MB)", m_uTextureMemory, (float)m_uTextureMemory / 1024000.0);
	m_TextureMemory.SetWindowText(szBuf);
	
	return TRUE;
}

