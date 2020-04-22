//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Texture management functions. Exposes a list of available textures,
//			texture groups, and Most Recently Used textures.
//
//			There is one texture context per game configuration in GameCfg.ini.
//			
//=============================================================================//

#include "stdafx.h"
#include <process.h>
#include <io.h>
#include <sys\stat.h>
#include <fcntl.h>
#include "DummyTexture.h"		// Specific IEditorTexture implementation
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "Material.h"			// Specific IEditorTexture implementation
#include "Options.h"
#include "TextureSystem.h"
#include "WADTexture.h"			// Specific IEditorTexture implementation
#include "WADTypes.h"
#include "hammer.h"
#include "filesystem.h"
#include "materialsystem/itexture.h"
#include "tier1/utldict.h"
#include "FaceEditSheet.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)


#define _GraphicCacheAllocate(n)	malloc(n)
#define IsSortChr(ch) ((ch == '-') || (ch == '+'))


//-----------------------------------------------------------------------------
// Stuff for loading WAD3 files.
//-----------------------------------------------------------------------------
typedef struct
{
	int			filepos;
	int			disksize;
	int			size;					// uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16];				// must be null terminated
} WAD3lumpinfo_t;


	
//-----------------------------------------------------------------------------
// List of global graphics
//-----------------------------------------------------------------------------
CTextureSystem g_Textures;



//-----------------------------------------------------------------------------
// CMaterialFileChangeWatcher implementation.
//-----------------------------------------------------------------------------
void CMaterialFileChangeWatcher::Init( CTextureSystem *pSystem, int context )
{
	m_pTextureSystem = pSystem;
	m_Context = context;

	m_Watcher.Init( this );
	
	char searchPaths[1024 * 16];
	if ( g_pFullFileSystem->GetSearchPath( "GAME", false, searchPaths, sizeof( searchPaths ) ) > 0 )
	{
		CUtlVector<char*> searchPathList;
		V_SplitString( searchPaths, ";", searchPathList );

		for ( int i=0; i < searchPathList.Count(); i++ )
		{
			m_Watcher.AddDirectory( searchPathList[i], "materials", true );
		}
		
		searchPathList.PurgeAndDeleteElements();
	}
	else
	{
		Warning( "Error in GetSearchPath. Dynamic material list updating will not be available." );
	}
}

void CMaterialFileChangeWatcher::OnFileChange( const char *pRelativeFilename, const char *pFullFilename )
{
	//Msg( "OnNewFile: %s\n", pRelativeFilename );

	CTextureSystem::EFileType eFileType;
	if ( CTextureSystem::GetFileTypeFromFilename( pRelativeFilename, &eFileType ) )
		m_pTextureSystem->OnFileChange( pRelativeFilename, m_Context, eFileType );
}

void CMaterialFileChangeWatcher::Update()
{
	m_Watcher.Update();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Creates the "All" group and sets it as the active group.
//-----------------------------------------------------------------------------
CTextureSystem::CTextureSystem(void)
{
	m_pLastTex = NULL;
	m_nLastIndex = 0;
	m_pActiveContext = NULL;
	m_pActiveGroup = NULL;
	m_pCubemapTexture = NULL;
	m_pNoDrawTexture = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees the list of groups and dummy textures.
//-----------------------------------------------------------------------------
CTextureSystem::~CTextureSystem(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureSystem::FreeAllTextures()
{
	if ( m_pCubemapTexture )
	{
	 	m_pCubemapTexture->DecrementReferenceCount();
		m_pCubemapTexture = NULL;
	}

	int nContextCount = m_TextureContexts.Count();
	for (int nContext = 0; nContext < nContextCount; nContext++)
	{
		TextureContext_t *pContext = &m_TextureContexts.Element(nContext);

		//
		// Delete all the texture groups for this context.
		//
		int nGroupCount = pContext->Groups.Count();
		for (int nGroup = 0; nGroup < nGroupCount; nGroup++)
		{
			delete pContext->Groups.Element(nGroup);
		}

		//
		// Delete dummy textures.
		//
		int nDummyCount = pContext->Dummies.Count();
		for (int nDummy = 0; nDummy < nDummyCount; nDummy++)
		{
			IEditorTexture *pTex = pContext->Dummies.Element(nDummy);
			delete pTex;
		}
	}

	//
	// Delete all the textures from the master list.
	//
	for (int i = 0; i < m_Textures.Count(); i++)
	{
		IEditorTexture *pTex = m_Textures[i];
		delete pTex;
	}
	m_Textures.RemoveAll();

	m_pLastTex = NULL;
	m_nLastIndex = -1;


	// Delete the keywords.
	m_Keywords.PurgeAndDeleteElements();
	m_ChangeWatchers.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a texture to the master list of textures.
// Input  : pTexture - Pointer to texture to add.
// Output : Returns the index of the texture in the master texture list.
//-----------------------------------------------------------------------------
int CTextureSystem::AddTexture(IEditorTexture *pTexture)
{
	return m_Textures.AddToTail(pTexture);
}


//-----------------------------------------------------------------------------
// Purpose: Begins iterating the list of texture/material keywords.
//-----------------------------------------------------------------------------
int CTextureSystem::GetNumKeywords(void)
{
	return(m_Keywords.Count());
}


//-----------------------------------------------------------------------------
// Purpose: Continues iterating the list of texture/material keywords.
//-----------------------------------------------------------------------------
const char *CTextureSystem::GetKeyword(int pos)
{
	return(m_Keywords.Element(pos));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *piIndex - 
//			bUseMRU - 
// Output : 
//-----------------------------------------------------------------------------
IEditorTexture *CTextureSystem::EnumActiveTextures(int *piIndex, TEXTUREFORMAT eDesiredFormat) const
{
	Assert(piIndex != NULL);
	
	if (piIndex != NULL)
	{
		if (m_pActiveGroup != NULL)
		{
			IEditorTexture *pTex = NULL;

			do
			{
				pTex = m_pActiveGroup->GetTexture(*piIndex);
				if (pTex != NULL)
				{
					(*piIndex)++;

					if ((eDesiredFormat == tfNone) || (pTex->GetTextureFormat() == eDesiredFormat))
					{
						return(pTex);
					}
				}
			} while (pTex != NULL);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the texture system.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTextureSystem::Initialize(HWND hwnd)
{
	bool bWAD = CWADTexture::Initialize();
	bool bMaterial = CMaterial::Initialize(hwnd);

	return(bWAD && bMaterial);
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down the texture system.
//-----------------------------------------------------------------------------
void CTextureSystem::ShutDown(void)
{
	CWADTexture::ShutDown();
	CMaterial::ShutDown();
	FreeAllTextures();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszName - 
//			piIndex - 
//			bDummy - 
// Output : 
//-----------------------------------------------------------------------------
IEditorTexture *CTextureSystem::FindActiveTexture(LPCSTR pszInputName, int *piIndex, BOOL bDummy)
{

	// The .vmf file format gets confused if there are backslashes in material names,
	// so make sure they're all using forward slashes here.
	char szName[MAX_PATH];
	Q_StrSubst( pszInputName, "\\", "/", szName, sizeof( szName ) );
	const char *pszName = szName;
	IEditorTexture *pTex = NULL;
	//
	// Check the cache first.
	//
	if (m_pLastTex && !stricmp(pszName, m_pLastTex->GetName()))
	{
		if (piIndex)
		{
			*piIndex = m_nLastIndex;
		}

		return m_pLastTex;
	}

	int iIndex = 0;

	// We're finding by name, so we don't care what the format is as long as the name matches.
	if ( m_pActiveGroup )
	{
		pTex = m_pActiveGroup->FindTextureByName( pszName, &iIndex, tfNone );
		if ( pTex )
		{
			if ( piIndex )
				*piIndex = iIndex;
			
			m_pLastTex = pTex;
			m_nLastIndex = iIndex;
			
			return pTex;
		}
	}

	//
	// Let's try again, this time with \textures\ decoration
	// TODO: remove this?
	//
	{
		iIndex = 0;
		char szBuf[512];

		sprintf(szBuf, "textures\\%s", pszName);

		for (int i = strlen(szBuf) -1; i >= 0; i--)
		{
			if (szBuf[i] == '/')
				szBuf[i] = '\\';
		}

		strlwr(szBuf);

		if ( m_pActiveGroup )
		{
			pTex = m_pActiveGroup->FindTextureByName( szBuf, &iIndex, tfNone );
			if ( pTex )
			{
				if ( piIndex )
					*piIndex = iIndex;
				
				m_pLastTex = pTex;
				m_nLastIndex = iIndex;
				
				return pTex;
			}
		}
	}
	//
	// Caller doesn't want dummies.
	//
	if (!bDummy)
	{
		return(NULL);
	}

	Assert(!piIndex);

	//
	// Check the list of dummies for a texture with the same name and texture format.
	//
	if (m_pActiveContext)
	{
		int nDummyCount = m_pActiveContext->Dummies.Count();
		for (int nDummy = 0; nDummy < nDummyCount; nDummy++)
		{
			IEditorTexture *pTexDummy = m_pActiveContext->Dummies.Element(nDummy);
			if (!strcmpi(pszName, pTexDummy->GetName()))
			{
				m_pLastTex = pTexDummy;
				m_nLastIndex = -1;
				return(pTexDummy);
			}
		}

		//
		// Not found; add a dummy as a placeholder for the missing texture.
		//
		pTex = AddDummy(pszName, g_pGameConfig->GetTextureFormat());
	}

	if (pTex != NULL)
	{
		m_pLastTex = pTex;
		m_nLastIndex = -1;
	}

	return(pTex);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTex - 
//-----------------------------------------------------------------------------
void CTextureSystem::AddMRU(IEditorTexture *pTex)
{
	if (!m_pActiveContext)
		return;

	int nIndex = m_pActiveContext->MRU.Find(pTex);
	if (nIndex != -1)
	{
		m_pActiveContext->MRU.Remove(nIndex);
	}
	else if (m_pActiveContext->MRU.Count() == 8)
	{
		m_pActiveContext->MRU.Remove(7);
	}

	m_pActiveContext->MRU.AddToHead(pTex);
}


//-----------------------------------------------------------------------------
// Purpose: Change palette on all textures.
// Input  : 
// dvs: need to handle a palette change for Quake support
//-----------------------------------------------------------------------------
void CTextureSystem::InformPaletteChanged()
{
//	int nGraphics = GetCount();
//
//	for (int i = 0; i < nGraphics; i++)
//	{
//		IEditorTexture *pTex = &GetAt(i);
//	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the texture context that corresponds to the given game config.
//-----------------------------------------------------------------------------
TextureContext_t *CTextureSystem::FindTextureContextForConfig(CGameConfig *pConfig)
{
	for (int i = 0; i < m_TextureContexts.Count(); i++)
	{
		if (m_TextureContexts.Element(i).pConfig == pConfig)
		{
			return &m_TextureContexts.Element(i);
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureSystem::SetActiveConfig(CGameConfig *pConfig)
{
	TextureContext_t *pContext = FindTextureContextForConfig(pConfig);
	if (pContext)
	{
		m_pActiveContext = pContext;
		m_pActiveGroup = m_pActiveContext->pAllGroup;
	}
	else
	{
		m_pActiveContext = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *pcszName - 
//-----------------------------------------------------------------------------
void CTextureSystem::SetActiveGroup(const char *pcszName)
{
	if (!m_pActiveContext)
		return;

	char szBuf[MAX_PATH];
	sprintf(szBuf, "textures\\%s", pcszName);

	int iCount = m_pActiveContext->Groups.Count();
	for (int i = 0; i < iCount; i++)
	{
		CTextureGroup *pGroup = m_pActiveContext->Groups.Element(i);
		if (!strcmpi(pGroup->GetName(), pcszName))
		{
			m_pActiveGroup = pGroup;
			return;
		}

		if (strstr(pGroup->GetName(), pcszName))
		{
			m_pActiveGroup = pGroup;
			return;
		}

	}

	TRACE0("No Group Found!");
}


void HammerFileSystem_ReportSearchPath( const char *szPathID )
{
	char szSearchPath[ 4096 ];
	g_pFullFileSystem->GetSearchPath( szPathID, true, szSearchPath, sizeof( szSearchPath ) );

	Msg( mwStatus, "------------------------------------------------------------------" );

	char *pszOnePath = strtok( szSearchPath, ";" );
	while ( pszOnePath )
	{
		Msg( mwStatus, "Search Path (%s): %s", szPathID, pszOnePath );
		pszOnePath = strtok( NULL, ";" );
	}
}


//-----------------------------------------------------------------------------
// FIXME: Make this work correctly, using the version in filesystem_tools.cpp
// (it doesn't work currently owing to filesystem setup issues)
//-----------------------------------------------------------------------------
void HammerFileSystem_SetGame( const char *pExeDir, const char *pModDir )
{
	static bool s_bOnce = false;
	Assert( !s_bOnce );
	s_bOnce = true;

	char buf[MAX_PATH];

	Q_snprintf( buf, MAX_PATH, "%s\\hl2", pExeDir );
	g_pFullFileSystem->AddSearchPath( buf, "GAME", PATH_ADD_TO_HEAD );

	if ( pModDir && *pModDir != '\0' )
	{
		g_pFullFileSystem->AddSearchPath( pModDir, "GAME", PATH_ADD_TO_HEAD );
	}

	HammerFileSystem_ReportSearchPath( "GAME" );
}


//-----------------------------------------------------------------------------
// Purpose: Loads textures from all texture files.
//-----------------------------------------------------------------------------
void CTextureSystem::LoadAllGraphicsFiles(void)
{
	FreeAllTextures();

	// For each game config...
	// dvs: Disabled for single-config running.
	//for (int nConfig = 0; nConfig < Options.configs.GetGameConfigCount(); nConfig++)
	{
		//CGameConfig *pConfig = Options.configs.GetGameConfig(nConfig);
		CGameConfig *pConfig = g_pGameConfig;

		// Create a new texture context with the WADs and materials for that config.
		TextureContext_t *pContext = AddTextureContext();

		// Bind it to this config.
		pContext->pConfig = pConfig;

		// Create a group to hold all the textures for this context.
		pContext->pAllGroup = new CTextureGroup("All Textures");
		pContext->Groups.AddToTail(pContext->pAllGroup);

		HammerFileSystem_SetGame(pConfig->m_szGameExeDir, pConfig->m_szModDir);

		// Set the new context as the active context.
		m_pActiveContext = pContext;

		// Load the textures for all WAD files set in this config.
		// Only do this for configs that use WAD textures.
		if (pConfig->GetTextureFormat() == tfWAD3)
		{
			LoadWADFiles(pConfig);
		}

		// Load the materials for this config.
		// Do this unconditionally so that we get necessary editor materials.
		LoadMaterials(pConfig);

		m_pActiveContext->pAllGroup->Sort();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads all WAD files for the given game config.
//-----------------------------------------------------------------------------
void CTextureSystem::LoadWADFiles(CGameConfig *pConfig)
{
	// dvs: FIXME: WADs are not currently per-config
	for (int i = 0; i < Options.textures.nTextureFiles; i++)
	{
		LoadGraphicsFile(Options.textures.TextureFiles[i]);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads all the materials for the given game config.
//-----------------------------------------------------------------------------
void CTextureSystem::LoadMaterials(CGameConfig *pConfig)
{
	CTextureGroup *pGroup = new CTextureGroup("Materials");
	pGroup->SetTextureFormat(tfVMT);
	m_pActiveContext->Groups.AddToTail(pGroup);

	// Add all the materials to the group.
	CMaterial::EnumerateMaterials( this, "materials", (int)pGroup, INCLUDE_WORLD_MATERIALS );
	
	// Watch the materials directory recursively...
	CMaterialFileChangeWatcher *pWatcher = new CMaterialFileChangeWatcher;
	pWatcher->Init( this, (int)pGroup );
	m_ChangeWatchers.AddToTail( pWatcher );

	Assert( m_pCubemapTexture == NULL );

	m_pCubemapTexture = MaterialSystemInterface()->FindTexture( "editor/cubemap", NULL, true );

	if ( m_pCubemapTexture )
	{
		m_pCubemapTexture->IncrementReferenceCount();
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->BindLocalCubemap( m_pCubemapTexture );
	}
	
	// Get the nodraw texture.
	m_pNoDrawTexture = NULL;
	for ( int i=0; i < m_Textures.Count(); i++ )
	{
		if ( V_stricmp( m_Textures[i]->GetName(), "tools/toolsnodraw" ) == 0 || V_stricmp( m_Textures[i]->GetName(), "tools/toolsnodraw" ) == 0 )
		{
			m_pNoDrawTexture = m_Textures[i];
			break;
		}
	}
	if ( !m_pNoDrawTexture )				
		m_pNoDrawTexture = CMaterial::CreateMaterial( "tools/toolsnodraw", true );
}

void CTextureSystem::RebindDefaultCubeMap()
{
	// rebind with the default cubemap
	
	if (  m_pCubemapTexture )
	{
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->BindLocalCubemap( m_pCubemapTexture );
	}
}


void CTextureSystem::UpdateFileChangeWatchers()
{
	for ( int i=0; i < m_ChangeWatchers.Count(); i++ )
		m_ChangeWatchers[i]->Update();
}


void CTextureSystem::OnFileChange( const char *pFilename, int context, CTextureSystem::EFileType eFileType )
{
	// It requires the forward slashes later...
	char fixedSlashes[MAX_PATH];
	V_StrSubst( pFilename, "\\", "/", fixedSlashes, sizeof( fixedSlashes ) );	

	// Get rid of the extension.
	if ( V_strlen( fixedSlashes ) < 5 )
	{
		Assert( false );
		return;
	}
	fixedSlashes[ V_strlen( fixedSlashes ) - 4 ] = 0;


	// Handle it based on what type of file we've got.
	if ( eFileType == k_eFileTypeVMT )
	{
		IEditorTexture *pTex = FindActiveTexture( fixedSlashes, NULL, FALSE );
		if ( pTex )
		{
			pTex->Reload( true );
		}
		else
		{
			EnumMaterial( fixedSlashes, context );
			IEditorTexture *pTexFixed = FindActiveTexture( fixedSlashes, NULL, FALSE );
			if ( pTexFixed )
			{
				GetMainWnd()->m_TextureBar.NotifyNewMaterial( pTexFixed );
				GetMainWnd()->GetFaceEditSheet()->NotifyNewMaterial( pTexFixed );
			}
		}
	}
	else if ( eFileType == k_eFileTypeVTF )
	{
		// Whether a VTF was added, removed, or modified, we do the same thing.. refresh it and any materials that reference it.
		ITexture *pTexture = materials->FindTexture( fixedSlashes, TEXTURE_GROUP_UNACCOUNTED, false );
		if ( pTexture )
		{
			pTexture->Download( NULL );
			ReloadMaterialsUsingTexture( pTexture );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Load any materials that reference this texture. Used so we can refresh a 
// material's preview image if a relevant .vtf changes.
//-----------------------------------------------------------------------------
void CTextureSystem::ReloadMaterialsUsingTexture( ITexture *pTestTexture )
{
	for ( int i=0; i < m_Textures.Count(); i++ )
	{
		IEditorTexture *pEditorTex = m_Textures[i];
		IMaterial *pMat = pEditorTex->GetMaterial( false );
		if ( !pMat )
			continue;
		
		IMaterialVar **pParams = pMat->GetShaderParams();
		int nParams = pMat->ShaderParamCount();
		for ( int iParam=0; iParam < nParams; iParam++ )
		{
			if ( pParams[iParam]->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
				continue;
			
			ITexture *pTex = pParams[iParam]->GetTextureValue();
			if ( !pTex )
				continue;
			
			if ( pTex == pTestTexture )
			{
				pEditorTex->Reload( true );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Figure out the file type from its extension. Returns false if we don't have an enum for that extension.
//-----------------------------------------------------------------------------
bool CTextureSystem::GetFileTypeFromFilename( const char *pFilename, CTextureSystem::EFileType *pFileType )
{
	char strRight[16];
	V_StrRight( pFilename, 4, strRight, sizeof( strRight ) );
	if ( V_stricmp( strRight, ".vmt" ) == 0 )
	{
		*pFileType = CTextureSystem::k_eFileTypeVMT;
		return true;
	}
	else if ( V_stricmp( strRight, ".vtf" ) == 0 )
	{
		*pFileType = CTextureSystem::k_eFileTypeVTF;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Loads textures from all texture files.
//-----------------------------------------------------------------------------
void CTextureSystem::ReloadTextures( const char *pFilterName )
{
	MaterialSystemInterface()->ReloadMaterials( pFilterName );

	for ( int i = 0; i < m_Textures.Count(); i++ )
	{
		if ( !Q_stristr( pFilterName, m_Textures[i]->GetName() ) )
			continue;

		m_Textures[i]->Reload( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a placeholder texture for a texture that exists in the map, but
//			was not found on disk.
// Input  : pszName - Name of missing texture.
// Output : Returns a pointer to the new dummy texture.
//-----------------------------------------------------------------------------
IEditorTexture *CTextureSystem::AddDummy(LPCTSTR pszName, TEXTUREFORMAT eFormat)
{
	if (!m_pActiveContext)
		return NULL;

	IEditorTexture *pTex = new CDummyTexture(pszName, eFormat);
	m_pActiveContext->Dummies.AddToTail(pTex);

	return(pTex);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : elem1 - 
//			elem2 - 
// Output : static int __cdecl
//-----------------------------------------------------------------------------
static int __cdecl SortTexturesProc(IEditorTexture * const *elem1, IEditorTexture * const *elem2)
{
	IEditorTexture *pElem1 = *((IEditorTexture **)elem1);
	IEditorTexture *pElem2 = *((IEditorTexture **)elem2);

	Assert((pElem1 != NULL) && (pElem2 != NULL));
	if ((pElem1 == NULL) || (pElem2 == NULL))
	{
		return(0);
	}

	const char *pszName1 = pElem1->GetName();
	const char *pszName2 = pElem2->GetName();

	char ch1 = pszName1[0];
	char ch2 = pszName2[0];

	if (IsSortChr(ch1) && !IsSortChr(ch2))
	{
		int iFamilyLen = strlen(pszName1+2);
		int iFamily = strnicmp(pszName1+2, pszName2, iFamilyLen);
		if (!iFamily)
		{
			return(-1);	// same family - put elem1 before elem2
		}
		return(iFamily);	// sort normally
	}
	else if (!IsSortChr(ch1) && IsSortChr(ch2))
	{
		int iFamilyLen = strlen(pszName2+2);
		int iFamily = strnicmp(pszName1, pszName2+2, iFamilyLen);
		if (!iFamily)
		{
			return(1);	// same family - put elem2 before elem1
		}
		return(iFamily);	// sort normally
	}
	else if (IsSortChr(ch1) && IsSortChr(ch2))
	{
		// do family name sorting
		int iFamily = strcmpi(pszName1+2, pszName2+2);

		if (!iFamily)
		{
			// same family - sort by number
			return pszName1[1] - pszName2[1];
		}

		// different family
		return(iFamily);
	}

	return(strcmpi(pszName1, pszName2));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sizeSrc - 
//			sizeDest - 
//			*src - 
//			*dest - 
//-----------------------------------------------------------------------------
void ScaleBitmap(CSize sizeSrc, CSize sizeDest, char *src, char *dest)
{
    int i;
    int e_y = (sizeSrc.cy << 1) - sizeDest.cy;
    int sizeDest2_y = (sizeDest.cy << 1);
	int sizeSrc2_y = sizeSrc.cy << 1;
	int srcline = 0, destline = 0;
	char *srclinep, *destlinep;
	int e_x = (sizeSrc.cx << 1) - sizeDest.cx;
	int sizeDest2_x = (sizeDest.cx << 1);
	int sizeSrc2_x = sizeSrc.cx << 1;

    for( i = 0; i < sizeDest.cy; i++ )
    {
		// scale by X
		{
			srclinep = src + (srcline * sizeSrc.cx);
			destlinep = dest + (destline * sizeDest.cx);

			for( int j = 0; j < sizeDest.cx; j++ )
			{
				*destlinep = *srclinep;

				while( e_x >= 0 )
				{
					++srclinep;
					e_x -= sizeDest2_x;
				}

				++destlinep;
				e_x += sizeSrc2_x;
			}
		}

        while( e_y >= 0 )
        {
            ++srcline;
            e_y -= sizeDest2_y;
        }

        ++destline;
        e_y += sizeSrc2_y;
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			*piIndex - 
// Output : GRAPHICSFILESTRUCT *
//-----------------------------------------------------------------------------
bool CTextureSystem::FindGraphicsFile(GRAPHICSFILESTRUCT *pFileInfo, DWORD id, int *piIndex)
{
	for (int i = 0; i < m_GraphicsFiles.Count(); i++)
	{
		if (m_GraphicsFiles[i].id == id)
		{
			if (piIndex)
			{
				piIndex[0] = i;
			}

			if (pFileInfo != NULL)
			{
				*pFileInfo = m_GraphicsFiles[i];
			}

			return(true);
		}
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			fd - 
//			pGroup - 
//-----------------------------------------------------------------------------
void CTextureSystem::LoadGraphicsFileWAD3(GRAPHICSFILESTRUCT *pFile, int fd, CTextureGroup *pGroup)
{
	// read wad header
	wadinfo_t hdr;
	_lseek(fd, 0, SEEK_SET);
	_read(fd, (char*)&hdr, sizeof hdr);

	_lseek(fd, hdr.infotableofs, SEEK_SET);

	// allocate directory memory.
	WAD3lumpinfo_t *dir = new WAD3lumpinfo_t[hdr.numlumps];
		
	// read entries.
	_read(fd, dir, sizeof(WAD3lumpinfo_t) * hdr.numlumps);

	// load graphics!
	for (int i = 0; i < hdr.numlumps; i++)
	{
		if (dir[i].type == TYP_MIPTEX)
		{
			_lseek(fd, dir[i].filepos, SEEK_SET);

			CWADTexture *pNew = new CWADTexture;
			if (pNew != NULL)
			{
				if (pNew->Init(fd, pFile->id, FALSE, dir[i].name))
				{
					pNew->SetTextureFormat(pFile->format);

					//
					// Add the texture to master list of textures.
					//
					AddTexture(pNew);

					//
					// Add the texture's index to the given group and to the "All" group.
					//
					pGroup->AddTexture(pNew);
					if (pGroup != m_pActiveContext->pAllGroup)
					{
						m_pActiveContext->pAllGroup->AddTexture(pNew);
					}
				}
				else
				{
					delete pNew;
				}
			}
		}
	}

	// free memory
	delete[] dir;
}


//-----------------------------------------------------------------------------
// Purpose: Loads all textures in a given graphics file and returns an ID for
//			the file.
// Input  : filename - Full path of graphics file to load.
// Output : Returns the file ID.
//-----------------------------------------------------------------------------
DWORD CTextureSystem::LoadGraphicsFile(const char *pFilename)
{
	static DWORD __GraphFileID = 1;	// must start at 1.

	//
	// Make sure it's not already there.
	//
	int i = m_GraphicsFiles.Count() - 1;
	while (i > -1)
	{
		if (!strcmp(m_GraphicsFiles[i].filename, pFilename))
		{
			return(m_GraphicsFiles[i].id);
		}

		i--;
	}

	//
	// Is this a WAD file?
	//
	DWORD dwAttrib = GetFileAttributes(pFilename);
	if (dwAttrib == 0xFFFFFFFF)
	{
		return(0);
	}

	GRAPHICSFILESTRUCT gf;

	if (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		// open the file, and add it to the GraphicFileList array
		gf.fd = _open(pFilename, _O_BINARY | _O_RDONLY);
		if (gf.fd == -1)
		{
			// todo: if errno is "out of handles", close some other
			// graphics files.

			// StatusMsg(IDS_ERROPENGRAPHFILE, errno);
			return 0;	// could not open
		}

		char buf[4];
		_read(gf.fd, buf, 4);

		//
		// Make sure the file is in a format that we can read.
		//
		if (!memcmp(buf, "WAD3", 4))
		{
			gf.format = tfWAD3;
		}
		else
		{
			char str[MAX_PATH*2];
			Q_snprintf( str, sizeof(str), "The file \"%s\" is not a valid WAD3 file and will not be used.", pFilename);
			AfxMessageBox(str, MB_ICONEXCLAMATION | MB_OK);
			_close(gf.fd);
			return(0);
		}
	}

	// got it -- setup the rest of the gf structure
	gf.id = __GraphFileID++;
	Q_strncpy( gf.filename, pFilename, sizeof(gf.filename) );
	gf.bLoaded = FALSE;

	//
	// Add file to list of texture files.
	//
	m_GraphicsFiles.AddToTail(gf);

	//
	// Create a new texture group for the file.
	//
	CTextureGroup *pGroup = new CTextureGroup(pFilename);
	pGroup->SetTextureFormat(gf.format);
	m_pActiveContext->Groups.AddToTail(pGroup);
	
	//
	// Load the textures from the file and place them in the texture group.
	//
	LoadGraphicsFileWAD3(&gf, gf.fd, pGroup);
	gf.bLoaded = TRUE;

	//
	// Sort this group's list
	//
	pGroup->Sort();

	return(gf.id);
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether or not there is at least one available texture
//			group for a given texture format.
// Input  : format - Texture format to look for.
// Output : Returns TRUE if textures of a given format are available, FALSE if not.
//-----------------------------------------------------------------------------
bool CTextureSystem::HasTexturesForConfig(CGameConfig *pConfig)
{
	if (!pConfig)
		return false;

	TextureContext_t *pContext = FindTextureContextForConfig(pConfig);
	if (!pContext)
		return false;

	int nCount = pContext->Groups.Count();
	for (int i = 0; i < nCount; i++)
	{
		CTextureGroup *pGroup = pContext->Groups.Element(i);
		if (pGroup->GetTextureFormat() == pConfig->GetTextureFormat())
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Used to add all the world materials into the material list
//-----------------------------------------------------------------------------
bool CTextureSystem::EnumMaterial( const char *pMaterialName, int nContext )
{
	CTextureGroup *pGroup = (CTextureGroup *)nContext;
	CMaterial *pMaterial = CMaterial::CreateMaterial(pMaterialName, false);
	if (pMaterial != NULL)
	{
		// Add it to the master list of textures.
		AddTexture(pMaterial);

		// Add the texture's index to the given group and to the "All" group.
		pGroup->AddTexture(pMaterial);
		if (pGroup != m_pActiveContext->pAllGroup)
		{
			m_pActiveContext->pAllGroup->AddTexture(pMaterial);
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Registers the keywords as existing in a particular material
//-----------------------------------------------------------------------------
void CTextureSystem::RegisterTextureKeywords( IEditorTexture *pTexture )
{
	//
	// Add any new keywords from this material to the list of keywords.
	//
	char szKeywords[MAX_PATH];
	pTexture->GetKeywords(szKeywords);
	if (szKeywords[0] != '\0')
	{
		char *pch = strtok(szKeywords, " ,;");
		while (pch != NULL)
		{
			// dvs: hide in a Find function
			bool bFound = false;
			
			for( int pos=0; pos < m_Keywords.Count(); pos++ )
			{
				const char *pszTest = m_Keywords.Element(pos);
				if (!stricmp(pszTest, pch))
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				char *pszKeyword = new char[strlen(pch) + 1];
				strcpy(pszKeyword, pch);
				m_Keywords.AddToTail(pszKeyword);
			}

			pch = strtok(NULL, " ,;");
		}
	}
}


//-----------------------------------------------------------------------------
// Used to lazily load in all the textures
//-----------------------------------------------------------------------------
void CTextureSystem::LazyLoadTextures()
{
	if ( m_pActiveContext && m_pActiveContext->pAllGroup && !IsRunningInEngine() )
	{
		m_pActiveContext->pAllGroup->LazyLoadTextures();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : TextureContext_t
//-----------------------------------------------------------------------------
TextureContext_t *CTextureSystem::AddTextureContext()
{
	// Allocate a new texture context.
	int nIndex = m_TextureContexts.AddToTail();

	// Add the group to this config's list of texture groups.
	TextureContext_t *pContext = &m_TextureContexts.Element(nIndex);
	return pContext;
}


//-----------------------------------------------------------------------------
// Opens the source file associated with a material
//-----------------------------------------------------------------------------
void CTextureSystem::OpenSource( const char *pMaterialName )
{
	if ( !pMaterialName )
		return;

	char pRelativePath[MAX_PATH];
	Q_snprintf( pRelativePath, MAX_PATH, "materials/%s.vmt", pMaterialName );

	char pFullPath[MAX_PATH];
	if ( g_pFullFileSystem->GetLocalPath( pRelativePath, pFullPath, MAX_PATH ) )
	{
		ShellExecute( NULL, "open", pFullPath, NULL, NULL, SW_SHOWNORMAL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pszName - Name of group, ie "Materials" or "u:\hl\tfc\tfc.wad".
//-----------------------------------------------------------------------------
CTextureGroup::CTextureGroup(const char *pszName)
{
	strcpy(m_szName, pszName);
	m_eTextureFormat = tfNone;
	m_nTextureToLoad = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a texture to this group.
// Input  : pTexture - Texture to add.
//-----------------------------------------------------------------------------
void CTextureGroup::AddTexture(IEditorTexture *pTexture)
{
	int index = m_Textures.AddToTail(pTexture);
	m_TextureNameMap.Insert( pTexture->GetName(), index );
}


//-----------------------------------------------------------------------------
// Purpose: Sorts the group.
//-----------------------------------------------------------------------------
void CTextureGroup::Sort(void)
{
	m_Textures.Sort(SortTexturesProc);

	// Redo the name map.
	m_TextureNameMap.RemoveAll();
	for ( int i=0; i < m_Textures.Count(); i++ )
	{
		IEditorTexture *pTex = m_Textures[i];
		m_TextureNameMap.Insert( pTex->GetName(), i );
	}

	// Changing the order means we don't know where we should be loading from
	m_nTextureToLoad = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a texture by index.
// Input  : nIndex - Index of the texture in this group.
//-----------------------------------------------------------------------------
IEditorTexture *CTextureGroup::GetTexture(int nIndex)
{
	if ((nIndex >= m_Textures.Count()) || (nIndex < 0))
	{
		return(NULL);
	}

	return(m_Textures[nIndex]);
}


//-----------------------------------------------------------------------------
// finds a texture by name
//-----------------------------------------------------------------------------
IEditorTexture *CTextureGroup::GetTexture( char const* pName )
{
	for (int i = 0; i < m_Textures.Count(); i++)
	{
		if (!strcmp(pName, m_Textures[i]->GetName()))
			return m_Textures[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Quickly find a texture by name.
//-----------------------------------------------------------------------------
IEditorTexture* CTextureGroup::FindTextureByName( const char *pName, int *piIndex, TEXTUREFORMAT eDesiredFormat )
{
	int iMapEntry = m_TextureNameMap.Find( pName );
	if ( iMapEntry == m_TextureNameMap.InvalidIndex() )
	{
		return NULL;
	}
	else
	{
		IEditorTexture *pTex = m_Textures[ m_TextureNameMap[iMapEntry] ];
		if ((eDesiredFormat == tfNone) || (pTex->GetTextureFormat() == eDesiredFormat))
			return pTex;
		else
			return NULL;
	}		
}


//-----------------------------------------------------------------------------
// Used to lazily load in all the textures
//-----------------------------------------------------------------------------
void CTextureGroup::LazyLoadTextures()
{
	// Load at most once per call
	while (m_nTextureToLoad < m_Textures.Count())
	{
		if (!m_Textures[m_nTextureToLoad]->IsLoaded())
		{
			m_Textures[m_nTextureToLoad]->Load();
			++m_nTextureToLoad;
			return;
		}

		// This one was already loaded; skip it
		++m_nTextureToLoad;
	}
}

