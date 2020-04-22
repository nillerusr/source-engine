//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	APPLOADER.CPP
//
//	Stub executeable
//=====================================================================================//
#include "xbox_loader.h"

struct installData_t
{
	char			**m_ppSrcFiles;
	char			**m_ppDstFiles;
	DWORD			*m_pDstFileSizes;
	int				m_numFiles;
	DWORD			m_totalSize;
	xCompressHeader	**m_ppxcHeaders;
};

DWORD			g_installStartTime;
DWORD			g_installElapsedTime;
installData_t	g_installData;
CopyStats_t		g_copyStats;
int				g_activeDevice;
__int64			g_loaderStartTime;


//-----------------------------------------------------------------------------
// GetLocalizedLoadingString
//-----------------------------------------------------------------------------
const wchar_t *GetLocalizedLoadingString()
{
	switch( XGetLanguage() )
	{
	case XC_LANGUAGE_FRENCH:
		return L"CHARGEMENT...";
	case XC_LANGUAGE_ITALIAN:
		return L"CARICAMENTO...";
	case XC_LANGUAGE_GERMAN:
		return L"LÄDT...";
	case XC_LANGUAGE_SPANISH:
		return L"CARGANDO...";
	}
	return L"LOADING...";
}

//-----------------------------------------------------------------------------
// GetNextLangauge
// Start at -1
//-----------------------------------------------------------------------------
int GetNextLanguage( int languageID )
{
	if ( languageID < 0 )
		return XC_LANGUAGE_ENGLISH;

	// cycle to end
	switch ( languageID )
	{
	case XC_LANGUAGE_ENGLISH:
		return XC_LANGUAGE_FRENCH;
	case XC_LANGUAGE_FRENCH:
		return XC_LANGUAGE_ITALIAN;
	case XC_LANGUAGE_ITALIAN:
		return XC_LANGUAGE_GERMAN;
	case XC_LANGUAGE_GERMAN:
		return XC_LANGUAGE_SPANISH;
	case XC_LANGUAGE_SPANISH:
		return -1;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// GetLanguageString
//-----------------------------------------------------------------------------
const char *GetLanguageString( int languageID )
{
	switch( languageID )
	{
	case XC_LANGUAGE_FRENCH:
		return "french";
	case XC_LANGUAGE_ITALIAN:
		return "italian";
	case XC_LANGUAGE_GERMAN:
		return "german";
	case XC_LANGUAGE_SPANISH:
		return "spanish";
	}
	return "english";
}

//-----------------------------------------------------------------------------
// FixupNamespaceFilename
//-----------------------------------------------------------------------------
bool FixupNamespaceFilename( const char *pFilename, char *pOutFilename, int languageID )
{
	char newFilename[MAX_PATH];

	bool bFixup = false;
	int dstLen = 0;
	int srcLen = strlen( pFilename );
	for ( int i=0; i<srcLen+1; i++ )
	{
		// replace every occurrence of % with language
		if ( pFilename[i] == '%' )
		{
			int len = strlen( GetLanguageString( languageID ) );
			memcpy( newFilename + dstLen, GetLanguageString( languageID ), len );
			dstLen += len;
			bFixup = true;
		}
		else
		{
			newFilename[dstLen] = pFilename[i];
			dstLen++;
		}
	}

	strcpy( pOutFilename, newFilename );
	return bFixup;
}

//-----------------------------------------------------------------------------
// DeleteOtherLocalizedFiles
//-----------------------------------------------------------------------------
void DeleteOtherLocalizedFiles( const char *pFilename, int languageIDToKeep )
{
	char	newFilename[MAX_PATH];
	char	mrkFilename[MAX_PATH];
	bool	bFixup;

	int languageID = -1;
	while ( 1 )
	{
		languageID = GetNextLanguage( languageID );
		if ( languageID == -1 )
		{
			// cycled through
			break;
		}
		
		if ( languageID == languageIDToKeep )
		{
			// skip
			continue;
		}
		
		bFixup = FixupNamespaceFilename( pFilename, newFilename, languageID );
		if ( !bFixup )
		{
			// nothing to do
			continue;
		}

		SetFileAttributes( newFilename, FILE_ATTRIBUTE_NORMAL );
		DeleteFile( newFilename );

		// delete marker
		strcpy( mrkFilename, newFilename );
		strcat( mrkFilename, ".mrk" );
		SetFileAttributes( mrkFilename, FILE_ATTRIBUTE_NORMAL );
		DeleteFile( mrkFilename );
	}
}

//-----------------------------------------------------------------------------
// LerpColor
//-----------------------------------------------------------------------------
unsigned int LerpColor( unsigned int c0, unsigned int c1, float t )
{
	int 			i;
	float			a;
	float			b;
	unsigned char*	c;
	unsigned int	newcolor;

	if ( t <= 0.0f )
		return c0;
	else if ( t >= 1.0f )
		return c1;

	// lerp each component
	c = (unsigned char*)&newcolor;
	for ( i=0; i<4; i++ )
	{
		a    = (float)(c0 & 0xFF);
		b    = (float)(c1 & 0xFF);
		*c++ = (unsigned char)(a + t*(b-a));

		// next color component
		c0 >>= 8;
		c1 >>= 8;
	}		

	return newcolor;
}

//-----------------------------------------------------------------------------
// ConvertToWideString
//-----------------------------------------------------------------------------
void ConvertToWideString( wchar_t *pDst, const char *pSrc )
{
	int len = strlen( pSrc )+1;
	for (int i=0; i<len; i++)
	{
		pDst[i] = pSrc[i];
	}
}

//-----------------------------------------------------------------------------
// CopyString
//-----------------------------------------------------------------------------
char *CopyString( const char *pString )
{
	char *pNewString = (char *)malloc( strlen( pString ) + 1 );
	strcpy( pNewString, pString );

	return pNewString;
}

//-----------------------------------------------------------------------------
// FixFilename
//-----------------------------------------------------------------------------
void FixFilename( char *pPath )
{
	int len = strlen( pPath );
	for (int i=0; i<len; ++i)
	{
		if ( pPath[i] == '/')
			pPath[i] = '\\';
	}
}

//-----------------------------------------------------------------------------
// StripQuotes
//-----------------------------------------------------------------------------
void StripQuotes( char *pToken )
{
	int len = strlen( pToken );
	if ( pToken[0] == '"' && pToken[len-1] == '"' )
	{
		memcpy( pToken, pToken+1, len-2 );
		pToken[len-2] = '\0';
	}
}

//-----------------------------------------------------------------------------
// DoesFileExist
//-----------------------------------------------------------------------------
bool DoesFileExist( const char *pFilename, DWORD *pSize )
{
	HANDLE hFile = CreateFile( pFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		if ( pSize )
		{
			*pSize = GetFileSize( hFile, NULL );
			if ( *pSize == (DWORD)-1 )
			{
				*pSize = 0;
			}
		}
	
		// exists
		CloseHandle( hFile );
		return true;
	}

	// not present
	return false;
}

//-----------------------------------------------------------------------------
//	NormalizePath
//
//-----------------------------------------------------------------------------
void NormalizePath( char* path, bool forceToLower )
{
	int	i;
	int	srclen;

	srclen = strlen( path );
	for ( i=0; i<srclen; i++ )
	{
		if ( path[i] == '/' )
			path[i] = '\\';
		else if ( forceToLower && ( path[i] >= 'A' && path[i] <= 'Z' ) )
			path[i] = path[i] - 'A' + 'a';
	}
}

//-----------------------------------------------------------------------------
// DeleteAllFiles
//
//-----------------------------------------------------------------------------
void DeleteAllFiles( const char* pDirectory, int level, bool bRecurse )
{
	HANDLE			hFind;
	WIN32_FIND_DATA	findData;
	char			basepath[MAX_PATH];
	char			searchpath[MAX_PATH];
	char			filename[MAX_PATH];

	TL_AddSeperatorToPath( (char*)pDirectory, basepath );
	strcpy( searchpath, basepath );
	strcat( searchpath, "*.*" );

	hFind = FindFirstFile( searchpath, &findData );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			if ( !stricmp( findData.cFileName, "." ) || !stricmp( findData.cFileName, ".." ) )
			{
				continue;
			}

			strcpy( filename, basepath );
			strcat( filename, findData.cFileName );
			NormalizePath( filename, false );

			if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if ( bRecurse )
				{
					DeleteAllFiles( filename, level+1, true );
					RemoveDirectory( filename );
					continue;
				}
			}

			SetFileAttributes( filename, FILE_ATTRIBUTE_NORMAL );
			DeleteFile( filename );
		}
		while ( FindNextFile( hFind, &findData ) );
		FindClose( hFind );
	}
}

//-----------------------------------------------------------------------------
// GetXCompressedHeader
//-----------------------------------------------------------------------------
bool GetXCompressedHeader( const char *pFilename, xCompressHeader *pHeader )
{
	HANDLE hFile = CreateFile( pFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD dwBytesRead = 0;
		ReadFile( hFile, pHeader, sizeof( xCompressHeader ), &dwBytesRead, NULL );
		CloseHandle( hFile );
		if ( pHeader->nMagic == xCompressHeader::MAGIC && pHeader->nVersion == xCompressHeader::VERSION )
		{
			// valid
			return true;
		}
	}

	// invalid
	return false;
}

//-----------------------------------------------------------------------------
// IsTargetFileValid
//
// Optional non-zero expected source file size must matcg
//-----------------------------------------------------------------------------
bool IsTargetFileValid( const char *pFilename, DWORD dwSrcFileSize )
{
	char	mrkFilename[MAX_PATH];
	DWORD	dwTargetFileSize = 0;
	DWORD	dwSize = 0;
	DWORD	dwAttributes;

	// all valid target files are non-zero
	if ( !DoesFileExist( pFilename, &dwTargetFileSize ) || !dwTargetFileSize )
	{
		return false;
	}

	dwAttributes = GetFileAttributes( pFilename );
	if ( dwAttributes != (DWORD)-1 && ( dwAttributes & FILE_ATTRIBUTE_READONLY ) )
	{
		// target files marked read only don't get overwritten
		return true;
	}

	// all valid target files must have marker file
	// presence ensures file was successfully copied
	strcpy( mrkFilename, pFilename );
	strcat( mrkFilename, ".mrk" );
	if ( !DoesFileExist( mrkFilename, NULL ) )
	{
		// cannot rely on contents, regardless of size match
		DeleteFile( pFilename );
		return false;
	}

	if ( dwSrcFileSize && dwSrcFileSize != dwTargetFileSize )
	{
		DeleteFile( pFilename );
		return false;
	}

	// assume valid
	return true;
}

//-----------------------------------------------------------------------------
// Constructor for CXBoxLoader class
//-----------------------------------------------------------------------------
CXBoxLoader::CXBoxLoader() : CXBApplication()
{
	// need a persistent time base, use the RTC
	// all other tick counters reset across relaunch
	FILETIME fileTime;
	GetSystemTimeAsFileTime( &fileTime );
	g_loaderStartTime = ((ULARGE_INTEGER*)&fileTime)->QuadPart;

	m_contextCode            = 0;
	m_pLastMovieFrame        = NULL;
	m_pVB                    = NULL;
	m_bAllowAttractAbort     = false;
	m_numFiles               = 0;
	m_bLaunch                = false;
	m_dwLoading              = 0;
	m_bDrawLegal             = false;
	m_LegalTime              = 0;
	m_installThread          = NULL;
	m_State                  = 0;
	m_bDrawLoading           = false;
	m_bDrawProgress          = false;
	m_bInstallComplete       = false;
	m_FrameCounter           = 0;
	m_MovieCount             = 0;
	m_bMovieErrorIsFatal     = false;
	m_bDrawDebug             = false;
	m_LoadingBarStartTime    = 0;
	m_LoadingBarEndTime      = 0;
	m_LegalStartTime         = 0;
	m_bCaptureLastMovieFrame = 0;
	m_bDrawSlideShow         = false;
	m_SlideShowStartTime     = 0;
	m_pLogData               = NULL;
	m_pDefaultTrueTypeFont   = NULL;
}

//-----------------------------------------------------------------------------
// FatalMediaError
//-----------------------------------------------------------------------------
void CXBoxLoader::FatalMediaError()
{
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0L );

	LPDIRECT3DSURFACE8 pBackBuffer;
    m_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );

	LPCWSTR	pLine1;
	LPCWSTR	pLine2;
	switch( XGetLanguage() )
	{
	case XC_LANGUAGE_FRENCH:
		pLine1 = L"Le disque utilisé présente une anomalie."; 
		pLine2 = L"Il est peut-être sale ou endommagé.";
		break;
	case XC_LANGUAGE_ITALIAN:
		pLine1 = L"Il disco in uso ha qualche problema.";
		pLine2 = L"Potrebbe essere sporco o danneggiato.";
		break;
	case XC_LANGUAGE_GERMAN:
		pLine1 = L"Bei der benutzten CD ist ein Problem aufgetreten.";
		pLine2 = L"Möglicherweise ist sie verschmutzt oder beschädigt.";
		break;
	case XC_LANGUAGE_SPANISH:
		pLine1 = L"Hay un problema con el disco que está usando.";
		pLine2 = L"Puede estar sucio o dañado.";
		break;
	default:
		pLine1 = L"There is a problem with the disc you are using.";
		pLine2 = L"It may be dirty or damaged.";
		break;
	}

	if ( m_pDefaultTrueTypeFont )
	{
		m_pDefaultTrueTypeFont->SetTextAlignment( XFONT_CENTER|XFONT_TOP );
		m_pDefaultTrueTypeFont->TextOut( pBackBuffer, pLine1, (unsigned)-1, 320, 240-15 );
		m_pDefaultTrueTypeFont->TextOut( pBackBuffer, pLine2, (unsigned)-1, 320, 240+15 );
	}

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

	pBackBuffer->Release();

	// forever
	while (1);
}

//-----------------------------------------------------------------------------
// LoadTexture
//-----------------------------------------------------------------------------
D3DTexture *CXBoxLoader::LoadTexture( int resourceID )
{
    // Get access to the texture
    return ( m_xprResource.GetTexture( resourceID ) );
}

//-----------------------------------------------------------------------------
// LoadFont
//-----------------------------------------------------------------------------
HRESULT CXBoxLoader::LoadFont( CXBFont *pFont, int resourceID )
{
	return pFont->Create( m_xprResource.GetTexture( resourceID ), 
                   m_xprResource.GetData( resourceID + sizeof(D3DTexture) ) );
}

//-----------------------------------------------------------------------------
// DrawRect
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawRect( int x, int y, int w, int h, DWORD color )
{
    // Set states
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice::SetRenderState( D3DRS_ZENABLE, FALSE ); 
    D3DDevice::SetRenderState( D3DRS_FOGENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
	D3DDevice::SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID ); 
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE, FALSE ); 
    D3DDevice::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );

    FLOAT fX1 = x;
    FLOAT fY1 = y;
    FLOAT fX2 = x + w - 1;
    FLOAT fY2 = y + h - 1;

    D3DDevice::Begin( D3DPT_QUADLIST );
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, color );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY1, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX2, fY2, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX1, fY2, 1.0f, 1.0f );
    D3DDevice::End();
}


//-----------------------------------------------------------------------------
// DrawTexture
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawTexture( D3DTexture *pD3DTexture, int x, int y, int w, int h, int color )
{
	struct VERTEX { D3DXVECTOR4 p; D3DCOLOR c; FLOAT tu, tv; };
	if ( !m_pVB )
	{
		// Create a vertex buffer for rendering the help screen
		D3DDevice::CreateVertexBuffer( 4*sizeof(VERTEX), D3DUSAGE_WRITEONLY, 0L, D3DPOOL_DEFAULT, &m_pVB );
	}

	VERTEX* v;
    m_pVB->Lock( 0, 0, (BYTE**)&v, 0L );

    // Calculate vertex positions
    FLOAT fLeft     = x + 0.5f;
    FLOAT fTop      = y + 0.5f;
    FLOAT fRight    = x+w - 0.5f;
    FLOAT fBottom   = y+h - 0.5f;

	// position
    v[0].p  = D3DXVECTOR4( fLeft, fTop, 0, 0 ); 
    v[1].p  = D3DXVECTOR4( fRight, fTop, 0, 0 );  
    v[2].p  = D3DXVECTOR4( fRight, fBottom, 0, 0 );  
    v[3].p  = D3DXVECTOR4( fLeft, fBottom, 0, 0 ); 

	// color
	v[0].c  = color; 
	v[1].c  = color; 
	v[2].c  = color; 
	v[3].c  = color;

	D3DSURFACE_DESC	desc;
	pD3DTexture->GetLevelDesc( 0, &desc );

	// linear texcoords
	v[0].tu = 0; 
	v[0].tv = 0;
	v[1].tu = desc.Width; 
	v[1].tv = 0;
	v[2].tu = desc.Width;
	v[2].tv = desc.Height;
	v[3].tu = 0;
	v[3].tv = desc.Height;

    m_pVB->Unlock();

    // Set state to render the image
    D3DDevice::SetTexture( 0, pD3DTexture );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
	D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSW,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetRenderState( D3DRS_ZENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_ALWAYS );
	D3DDevice::SetRenderState( D3DRS_FOGENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1 );

    // Render the image
    D3DDevice::SetStreamSource( 0, m_pVB, sizeof(VERTEX) );
    D3DDevice::DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
}

//-----------------------------------------------------------------------------
// StartVideo
//
// May take a few ms.
//-----------------------------------------------------------------------------
HRESULT CXBoxLoader::StartVideo( const CHAR* strFilename, bool bFromMemory, bool bFatalOnError )
{   
	HRESULT hr;
	if ( bFromMemory )
	{
		// play from memory, so as no to interfere with disc access
		hr = m_player.OpenMovieFromMemory( strFilename, D3DFMT_LIN_A8R8G8B8, m_pd3dDevice, TRUE );
	}
	else
	{
		// play from disc
		hr = m_player.OpenFile( strFilename, D3DFMT_LIN_A8R8G8B8, m_pd3dDevice, TRUE );
	}

	// can fail anytime
	m_bMovieErrorIsFatal = bFatalOnError;

	m_MovieCount++;

	if ( FAILED( hr ) )
	{
		OUTPUT_DEBUG_STRING( "Video playback failed!\n" );

		if ( bFatalOnError )
		{
			FatalMediaError();
		}
	}

    return hr;
}

//-----------------------------------------------------------------------------
// StopVideo
//
// May take a few ms.
//-----------------------------------------------------------------------------
void CXBoxLoader::StopVideo()
{
	m_player.TerminatePlayback();
}

//-----------------------------------------------------------------------------
// LoadInstallScript
//
// Parse filenames to be copied
//-----------------------------------------------------------------------------
bool CXBoxLoader::LoadInstallScript()
{
	void			*pFileData = NULL;
	DWORD			fileSize = 0;
	DWORD			dwSrcSize;
	HRESULT			hr;
	char			srcFile[MAX_PATH];
	char			dstFile[MAX_PATH];
	char			localizedFile[MAX_PATH];
	HANDLE			hFind;
	char			sourceFilename[MAX_PATH];
	char			sourcePath[MAX_PATH];
	char			targetFilename[MAX_PATH];
	char			filename[MAX_PATH];
	WIN32_FIND_DATA	findData;
	bool			bCompressed;
	xCompressHeader	xcHeader;
	char			*pVersion;
	bool			bTargetIsLocalized;
	int				languageID;
	
	memset( &g_installData, 0, sizeof( installData_t ) );

	hr = XBUtil_LoadFile( "D:\\LoaderMedia\\install.txt", &pFileData, &fileSize );
	if ( hr != S_OK || !fileSize )
	{
		return false;
	}

	languageID = XGetLanguage();

	// full re-install
	bool bForce = true;

	// scan
	TL_SetScriptData( (char*)pFileData, fileSize );
	while ( 1 )
	{
		char *pToken = TL_GetToken( true );
		if ( !pToken || !pToken[0] )
			break;
		StripQuotes( pToken );
		strcpy( srcFile, pToken);

		pToken = TL_GetToken( true );
		if ( !pToken || !pToken[0] )
			break;
		StripQuotes( pToken );
		strcpy( dstFile, pToken);

		// replace with language token
		FixupNamespaceFilename( srcFile, srcFile, languageID );
		bTargetIsLocalized = FixupNamespaceFilename( dstFile, localizedFile, languageID );

		if ( bTargetIsLocalized )
		{
			// localized files are allowed to change without requiring a full re-install
			bool bDeleteMapCache = false;
			if ( !IsTargetFileValid( localizedFile, 0 ) )
			{
				// must delete map cache to ensure localized files have enough room
				bDeleteMapCache = true;
			}

			// only allowing one localized file of this type, delete all others
			DeleteOtherLocalizedFiles( dstFile, languageID );
			strcpy( dstFile, localizedFile );

			if ( bDeleteMapCache )
			{
				char mapPath[MAX_PATH];
				strcpy( mapPath, localizedFile );
				TL_StripFilename( mapPath );
				TL_AddSeperatorToPath( mapPath, mapPath );
				strcat( mapPath, "maps\\" );
				DeleteAllFiles( mapPath, 0, false );
			}
		}

		pVersion = strstr( dstFile, "version_" );
		if ( pVersion )
		{
			if ( m_numFiles )
			{
				// version statement out of sequence
				return false;
			}
		
			m_Version = atoi( pVersion + strlen( "version_" ) );

			if ( IsTargetFileValid( dstFile, 0 ) )
			{
				// version file exists, files should be same
				bForce = false;
			}

			if ( bForce )
			{
				// delete all files at the specified directory
				strcpy( targetFilename, dstFile );
				TL_StripFilename( targetFilename );
				DeleteAllFiles( targetFilename, 0, true );
			}
		}

		// source file could be wildcard, get path only
		strcpy( sourcePath, srcFile );
		TL_StripFilename( sourcePath );

		hFind = FindFirstFile( srcFile, &findData );
		if ( hFind != INVALID_HANDLE_VALUE )
		{
			do
			{
				if ( !stricmp( findData.cFileName, "." ) || !stricmp( findData.cFileName, ".." ) )
				{
					continue;
				}
				if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				{
					continue;
				}

				TL_AddSeperatorToPath( sourcePath, sourceFilename );
				strcat( sourceFilename, findData.cFileName );
				NormalizePath( sourceFilename, false );

				// target filename may be path or absolute file
				strcpy( targetFilename, dstFile );

				TL_StripPath( dstFile, filename );
				if ( !filename[0] )
				{
					// target filename is path only
					TL_AddSeperatorToPath( dstFile, targetFilename );
					strcat( targetFilename, findData.cFileName );
					NormalizePath( targetFilename, false );
				}

				if ( !DoesFileExist( sourceFilename, &dwSrcSize ) )
				{
					// can't validate source
					return false;
				}
			
				if ( strstr( sourceFilename, ".xz_" ) && strstr( targetFilename, ".xzp" ) )
				{
					bCompressed = true;
					if ( GetXCompressedHeader( sourceFilename, &xcHeader ) )
					{
						g_installData.m_totalSize += xcHeader.nUncompressedFileSize;

						if ( !bForce && IsTargetFileValid( targetFilename, xcHeader.nUncompressedFileSize ) )
						{
							// target already exists, no need to recopy
							g_copyStats.m_bytesCopied += xcHeader.nUncompressedFileSize;
							continue;
						}
					}
					else
					{
						// invalid
						return false;
					}
				}
				else
				{
					g_installData.m_totalSize += dwSrcSize;

					bCompressed = false;
					if ( !bForce && IsTargetFileValid( targetFilename, dwSrcSize ) )
					{
						// target already exists, no need to recopy
						g_copyStats.m_bytesCopied += dwSrcSize;
						continue;
					}
				}

				if ( m_numFiles < MAX_FILES )
				{
					m_fileSrc[m_numFiles]  = CopyString( sourceFilename );
					m_fileDest[m_numFiles] = CopyString( targetFilename );

					if ( bCompressed )
					{
						xCompressHeader *pxcHeader = new xCompressHeader;
						memcpy( pxcHeader, &xcHeader, sizeof( xCompressHeader ) );
						m_fileCompressionHeaders[m_numFiles] = pxcHeader;
						m_fileDestSizes[m_numFiles] = pxcHeader->nUncompressedFileSize;
					}
					else
					{
						m_fileCompressionHeaders[m_numFiles] = NULL;
						m_fileDestSizes[m_numFiles] = dwSrcSize;
					}

					m_numFiles++;
				}
			}
			while ( FindNextFile( hFind, &findData ) );
			FindClose( hFind );
		}
		else
		{
			// source file not found, invalid
			return false;
		}
	}

	// finsihed with install script
	free( pFileData );

	g_installData.m_ppSrcFiles     = m_fileSrc;
	g_installData.m_ppDstFiles     = m_fileDest;
	g_installData.m_ppxcHeaders    = m_fileCompressionHeaders;
	g_installData.m_pDstFileSizes  = m_fileDestSizes;
	g_installData.m_numFiles       = m_numFiles;

	return true;
}

//-----------------------------------------------------------------------------
// Copies all install files to the hard drive
//-----------------------------------------------------------------------------
DWORD WINAPI InstallThreadFunc( LPVOID lpParam ) 
{
	char	mrkFilename[MAX_PATH];
	bool	bSuccess;
	HANDLE	hFile;

	g_installStartTime = GetTickCount();

	// started loading
	*(DWORD*)lpParam = 1;

	for ( int i = 0; i < g_installData.m_numFiles; ++i )
	{
		DWORD bytesCopied = g_copyStats.m_bytesCopied;

		// install has already validated, if it's in the list, copy it
		bSuccess = CopyFileOverlapped( g_installData.m_ppSrcFiles[i], g_installData.m_ppDstFiles[i], g_installData.m_ppxcHeaders[i], &g_copyStats );

		strcpy( mrkFilename, g_installData.m_ppDstFiles[i] );
		strcat( mrkFilename, ".mrk" );

		SetFileAttributes( mrkFilename, FILE_ATTRIBUTE_NORMAL );
		if ( bSuccess )
		{
			// add marker
			hFile = CreateFile( mrkFilename, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile != INVALID_HANDLE_VALUE )
			{
				CloseHandle( hFile );
			}
		}
		else
		{
			// remove marker
			DeleteFile( mrkFilename );
			DeleteFile( g_installData.m_ppDstFiles[i] );

			// errors can't stop install
			// snap progress to expected completion
			g_copyStats.m_bytesCopied = bytesCopied + g_installData.m_pDstFileSizes[i];
		}
	}

	g_installElapsedTime = GetTickCount() - g_installStartTime;

	// finished loading
	*(DWORD*)lpParam = 0;

	return 0;
}

//-----------------------------------------------------------------------------
// Verify disk space
//-----------------------------------------------------------------------------
bool CXBoxLoader::VerifyInstall( void )
{
	memset( &g_copyStats, 0, sizeof( CopyStats_t ) );

	LoadLogFile();

	if ( !LoadInstallScript() )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Look for possible forensic log file
//-----------------------------------------------------------------------------
void CXBoxLoader::LoadLogFile( void )
{
#if defined( XBOX_FORENSIC_LOG )
	HRESULT		hr;
	char		*pFileData = NULL;
	DWORD		fileSize = 0;

	hr = XBUtil_LoadFile( "Z:\\hl2fatal.log", (void**)&pFileData, &fileSize );
	if ( hr != S_OK || !fileSize )
	{
		return;
	}

	// copy and null terminate
	m_pLogData = (char *)malloc( fileSize+1 );

	int j = 0;
	for (int i=0; i<(int)fileSize; i++)
	{
		if ( pFileData[i] == 0x0D )
			continue;
		m_pLogData[j++] = pFileData[i];
	}
	m_pLogData[j] = '\0';

	free( pFileData );
#endif
}

//-----------------------------------------------------------------------------
// Starts installation to disk
//-----------------------------------------------------------------------------
bool CXBoxLoader::StartInstall( void )
{
	// Start the install thread
	m_installThread = CreateThread( NULL, 0, &InstallThreadFunc, &m_dwLoading, 0, 0 );
	if ( !m_installThread )
	{
		// failed
		return false;
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Shows the legal text
//-----------------------------------------------------------------------------
void CXBoxLoader::StartLegalScreen( int legal )
{
	m_bDrawLegal     = true;
	m_LegalTime      = GetTickCount();
	m_LegalStartTime = 0;

	switch ( legal )
	{
	case LEGAL_MAIN:
		m_pLegalTexture = m_pMainLegalTexture;
		break;
	case LEGAL_SOURCE:
		m_pLegalTexture = m_pSourceLegalTexture;
		break;
	}
}

//-----------------------------------------------------------------------------
// DrawLegals
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawLegals()
{
	unsigned int	color;
	float			t;

	if ( !m_bDrawLegal )
		return;

	if ( !m_LegalStartTime )
	{
		m_LegalStartTime = GetTickCount();
	}

	// fade legals
	t = (float)(GetTickCount() - m_LegalStartTime)/LEGAL_DISPLAY_TIME;
	if ( t < 0.1f ) 
	{
		// fade up
		color = LerpColor( 0xFF000000, 0xFFFFFFFF, t*10.0f );
	}
	else if ( t < 0.9f )
	{
		// hold
		color = 0xFFFFFFFF;
	}
	else
	{
		// fade out
		color = LerpColor( 0xFFFFFFFF, 0xFF000000, (t-0.9f)*10.0f );
	}

	DrawTexture( m_pLegalTexture, 0, 0, 640, 480, color ); 
}

//-----------------------------------------------------------------------------
// DrawSlideshow
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawSlideshow()
{
	float			t;

	if ( !m_bDrawSlideShow )
		return;

	if ( !m_SlideShowStartTime )
	{
		m_SlideShowStartTime = GetTickCount();
		m_SlideShowCount = -1;
		m_bFinalSlide = false;
	}

	if ( !m_bInstallComplete && ( GetTickCount() - m_SlideShowStartTime > SLIDESHOW_SLIDETIME ) )
	{
		// next slide
		m_SlideShowCount++;
		m_SlideShowStartTime = GetTickCount();
	}

	t = ( GetTickCount() - m_SlideShowStartTime )/(float)SLIDESHOW_FLIPTIME;
	if ( t >= 1.0f )
		t = 1.0f;

	if ( m_bInstallComplete && !m_bFinalSlide && t >= 1.0f )
	{
		// wait for current slide to complete
		// final slide must transition back to transition screen
		m_SlideShowStartTime = GetTickCount();
		m_bFinalSlide = true;
		t = 0;
	}

	if ( !m_bFinalSlide )
	{
		// fade next slide in
		unsigned int fadeInColor  = LerpColor( 0x00FFFFFF, 0xFFFFFFFF, t );
		if ( fadeInColor != 0xFFFFFFFF && m_SlideShowCount != -1 )
			DrawTexture( m_pSlideShowTextures[m_SlideShowCount % MAX_SLIDESHOW_TEXTURES], 0, 0, 640, 480, 0xFFFFFFFF ); 
		DrawTexture( m_pSlideShowTextures[(m_SlideShowCount + 1) % MAX_SLIDESHOW_TEXTURES], 0, 0, 640, 480, fadeInColor );
	}
	else
	{
		// fade last slide out
		unsigned int fadeInColor  = LerpColor( 0xFFFFFFFF, 0x00FFFFFF, t );
		DrawTexture( m_pSlideShowTextures[(m_SlideShowCount + 1) % MAX_SLIDESHOW_TEXTURES], 0, 0, 640, 480, fadeInColor );
	}

	if ( m_bInstallComplete && m_bFinalSlide && t >= 1.0f )
	{
		// end of slideshow
		m_bDrawSlideShow = false;
	}
}

//-----------------------------------------------------------------------------
// DrawDebug
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawDebug()
{
#ifndef _RETAIL
	if ( !m_bDrawDebug )
		return;

	DrawRect( 0, 0, 640, 480, 0xC0000000 );

	m_Font.Begin();
	m_Font.SetScaleFactors( 0.8f, 0.8f );

	int xPos = SCREEN_WIDTH/2;
	int yPos = SCREEN_HEIGHT/4;
	float rate;

	wchar_t	textBuffer[256];
	swprintf( textBuffer, L"Version: %d", m_Version );
	m_Font.DrawText( 40, 40, 0xffffffff, textBuffer, 0 );

	wchar_t srcFilename[MAX_PATH];
	wchar_t dstFilename[MAX_PATH];
	ConvertToWideString( srcFilename, g_copyStats.m_srcFilename );
	ConvertToWideString( dstFilename, g_copyStats.m_dstFilename );
	swprintf( textBuffer, L"From: %s (%.2f MB)", srcFilename, (float)g_copyStats.m_readSize/(1024.0f*1024.0f) );
	m_Font.DrawText( xPos, yPos + 20, 0xffffffff, textBuffer, XBFONT_CENTER_X );
	swprintf( textBuffer, L"To: %s (%.2f MB)", dstFilename, (float)g_copyStats.m_writeSize/(1024.0f*1024.0f)  );
	m_Font.DrawText( xPos, yPos + 40, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	if ( g_copyStats.m_bufferReadTime && m_dwLoading )
		rate = ( g_copyStats.m_bufferReadSize/(1024.0f*1024.0f) ) / ( g_copyStats.m_bufferReadTime * 0.001f );
	else
		rate = 0;
	swprintf( textBuffer, L"Buffer Read: %.2f MB (%.2f MB/s) (%d)", g_copyStats.m_bufferReadSize/(1024.0f*1024.0f), rate, g_copyStats.m_numReadBuffers );
	m_Font.DrawText( xPos, yPos + 80, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	rate = g_copyStats.m_inflateTime && m_dwLoading ? (float)g_copyStats.m_inflateSize/(g_copyStats.m_inflateTime * 0.001f) : 0;
	swprintf( textBuffer, L"Inflate: %.2f MB (%.2f MB/s)", g_copyStats.m_inflateSize/(1024.0f*1024.0f), rate/(1024.0f*1024.0f) );
	m_Font.DrawText( xPos, yPos + 100, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	if ( g_copyStats.m_bufferWriteTime && m_dwLoading )
		rate = ( g_copyStats.m_bufferWriteSize/(1024.0f*1024.0f) ) / ( g_copyStats.m_bufferWriteTime * 0.001f );
	else
		rate = 0;
	swprintf( textBuffer, L"Buffer Write: %.2f MB (%.2f MB/s) (%d)", g_copyStats.m_bufferWriteSize/(1024.0f*1024.0f), rate, g_copyStats.m_numWriteBuffers );
	m_Font.DrawText( xPos, yPos + 120, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	rate = g_copyStats.m_totalReadTime && m_dwLoading ? (float)g_copyStats.m_totalReadSize/(g_copyStats.m_totalReadTime * 0.001f) : 0;
	swprintf( textBuffer, L"Total Read: %d MB (%.2f MB/s)", g_copyStats.m_totalReadSize/(1024*1024), rate/(1024.0f*1024.0f) );
	m_Font.DrawText( xPos, yPos + 160, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	rate = g_copyStats.m_totalWriteTime && m_dwLoading ? (float)g_copyStats.m_totalWriteSize/(g_copyStats.m_totalWriteTime * 0.001f) : 0;
	swprintf( textBuffer, L"Total Write: %d MB (%.2f MB/s)", g_copyStats.m_totalWriteSize/(1024*1024), rate/(1024.0f*1024.0f) );
	m_Font.DrawText( xPos, yPos + 180, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	float elapsed = (float)(GetTickCount() - g_installStartTime) * 0.001f;
	if ( m_dwLoading )
	{
		if ( elapsed )
			rate = g_copyStats.m_totalWriteSize/elapsed;
		else
			rate = 0;
	}
	else
	{
		if ( g_installElapsedTime )
			rate = g_copyStats.m_totalWriteSize/(g_installElapsedTime * 0.001f);
		else
			rate = 0;
	}
	swprintf( textBuffer, L"Progress: %d/%d MB Elapsed: %d secs (%.2f MB/s)", g_copyStats.m_bytesCopied/(1024*1024), g_installData.m_totalSize/(1024*1024), (int)elapsed, rate/(1024.0f*1024.0f) );
	m_Font.DrawText( xPos, yPos + 220, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	swprintf( textBuffer, L"Errors: %d", g_copyStats.m_copyErrors );
	m_Font.DrawText( xPos, yPos + 240, 0xffffffff, textBuffer, XBFONT_CENTER_X );

	m_Font.End();
#endif
}

void CXBoxLoader::DrawLog()
{
#if defined( XBOX_FORENSIC_LOG )
	wchar_t	textBuffer[1024];
	int		numChars;

	if ( !m_pLogData )
		return;

	DrawRect( 0, 0, 640, 480, 0xC0000000 );

	m_Font.Begin();
	m_Font.SetScaleFactors( 0.8f, 0.8f );

	char *pStart = m_pLogData;
	char *pEnd = pStart;
	int yPos = 40;
	for (int i=0; i<20; i++)
	{
		pEnd = strstr( pStart, "\n" );
		if ( !pEnd )
			numChars = strlen( pStart );
		else
			numChars = pEnd-pStart;
	
		if ( numChars )
		{
			for (int j=0; j<numChars; j++)
			{
				textBuffer[j] = pStart[j];
			}
			textBuffer[j] = 0;
			m_Font.DrawText( 40, yPos, 0xffffffff, textBuffer, 0 );
		}

		if ( !pEnd )
			break;

		// next line
		pStart = pEnd+1;
		yPos += 10;
	}

	m_Font.End();
#endif
}

//-----------------------------------------------------------------------------
// DrawLoadingMarquee
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawLoadingMarquee()
{
	if ( !m_bDrawLoading )
		return;

	int y = 0.80f*480;
	DrawTexture( m_pLoadingIconTexture, (640-64)/2, y-64, 64, 64, 0xFFFFFFFF ); 

	// draw loading text
	m_Font.Begin();
	m_Font.SetScaleFactors( 0.8f, 0.8f );
	m_Font.DrawText( 320, y, PROGRESS_TEXT_COLOR, GetLocalizedLoadingString(), XBFONT_CENTER_X );
	m_Font.End();
}

//-----------------------------------------------------------------------------
// DrawProgressBar
//-----------------------------------------------------------------------------
void CXBoxLoader::DrawProgressBar()
{
	if ( !m_bDrawProgress )
		return;
	
	if ( !m_LoadingBarStartTime )
	{
		m_LoadingBarStartTime = GetTickCount();
	}

	// slide the loading bar up
	float tUp = (float)(GetTickCount() - m_LoadingBarStartTime)/LOADINGBAR_UPTIME;
	if ( tUp > 1.0f)
		tUp = 1.0f;
	float y = 480.0f + tUp*((float)PROGRESS_Y - 480.0f);

	float t0 = 0;
	float t1 = 0;
	int numSegments = 0;
	if ( tUp == 1.0f )
	{
		// loading bar is up
		// don't snap, animate progress to current level of completion
		t0 = (float)g_copyStats.m_bytesCopied/(float)g_installData.m_totalSize;
		if ( t0 > 1.0f )
			t0 = 1.0f;
		t1 = (float)(GetTickCount() - m_LoadingBarStartTime - LOADINGBAR_WAITTIME)/LOADINGBAR_SLIDETIME;
		if ( t1 < 0.0f )
			t1 = 0.0f;
		else if ( t1 > 1.0f)
			t1 = 1.0f;
		numSegments = t0 * t1 * (float)SEGMENT_COUNT;
	}

#if 0
	float tDown = 0;
	if ( t0 == 1.0f && t1 == 1.0f && !m_dwLoading )
	{
		// loading anim and copying of data are finished
		// slide the loading bar down
		if ( !m_LoadingBarEndTime )
		{
			m_LoadingBarEndTime = GetTickCount();
		}
		tDown = (float)(GetTickCount() - m_LoadingBarEndTime - LOADINGBAR_WAITTIME)/LOADINGBAR_UPTIME;
		if ( tDown < 0.0f )
			tDown = 0.0f;
		else if ( tDown > 1.0f)
			tDown = 1.0f;
		y = PROGRESS_Y + tDown*(480.0f - (float)PROGRESS_Y);
	}

	if ( tDown == 1.0f )
	{
		// loading bar is offscreen
		m_bInstallComplete = true;
	}
#else
	if ( t0 == 1.0f && t1 == 1.0f && !m_dwLoading )
	{
		m_bInstallComplete = true;
	}
#endif

	int x = (640-FOOTER_W)/2;
	DrawTexture( m_pFooterTexture, x, y, FOOTER_W, 480 - PROGRESS_Y, PROGRESS_FOOTER_COLOR ); 
	x += FOOTER_W - 35;

	// draw left justified loading text
	m_Font.Begin();
	m_Font.SetScaleFactors( 0.8f, 0.8f );
	int textWidth = m_Font.GetTextWidth( GetLocalizedLoadingString() );
	x -= SEGMENT_W + textWidth;
	m_Font.DrawText( x, y+20, PROGRESS_TEXT_COLOR, GetLocalizedLoadingString(), XBFONT_LEFT );
	m_Font.End();

	// draw progess bar
	x -= SEGMENT_W + PROGRESS_W;
	DrawRect( x, y+25, PROGRESS_W, PROGRESS_H, PROGRESS_INSET_COLOR );
	for ( int i =0; i<numSegments; i++ )
	{
		DrawRect( x, y+25+2, SEGMENT_W, PROGRESS_H-4, PROGRESS_SEGMENT_COLOR );
		x += SEGMENT_W+SEGMENT_GAP;
	}

}

//-----------------------------------------------------------------------------
// Name: PlayVideoFrame()
// Desc: Plays one frame of video if a movie is currently open and if there is
// a frame available. This function is safe to call at any time.
//-----------------------------------------------------------------------------
BOOL CXBoxLoader::PlayVideoFrame()
{
    if ( !m_player.IsPlaying() )
        return FALSE;

    const FLOAT fMovieWidth = FLOAT( m_player.GetWidth() );
    const FLOAT fMovieHeight = FLOAT( m_player.GetHeight() );

    // Move to the next frame.
    LPDIRECT3DTEXTURE8 pTexture = 0;
	pTexture = m_player.AdvanceFrameForTexturing( m_pd3dDevice );

    // See if the movie is over now.
    if ( !m_player.IsPlaying() )
    {
		if ( m_bCaptureLastMovieFrame )
		{
			m_bCaptureLastMovieFrame = false;

			// Copy Texture
			if ( m_pLastMovieFrame )
			{
				m_pLastMovieFrame->Release();
				m_pLastMovieFrame = NULL;
			}

			if ( pTexture )
			{
				// copy the last frame
				D3DSURFACE_DESC	d3dSurfaceDesc;
				D3DLOCKED_RECT	srcRect;
				D3DLOCKED_RECT	dstRect;
				pTexture->GetLevelDesc( 0, &d3dSurfaceDesc );
				m_pd3dDevice->CreateTexture( d3dSurfaceDesc.Width, d3dSurfaceDesc.Height, 0, 0, d3dSurfaceDesc.Format, 0, &m_pLastMovieFrame );
				pTexture->LockRect( 0, &srcRect, NULL, D3DLOCK_READONLY | D3DLOCK_NOOVERWRITE );
				m_pLastMovieFrame->LockRect( 0, &dstRect, NULL, D3DLOCK_READONLY | D3DLOCK_NOOVERWRITE );
				memcpy( dstRect.pBits, srcRect.pBits, srcRect.Pitch*d3dSurfaceDesc.Height );
			}
		}

        // Clean up the movie, then return.
        m_player.Destroy();
        return FALSE;
    }

    // If no texture is ready, return TRUE to indicate that a movie is playing,
    // but don't render anything yet.
    if ( !pTexture )
        return TRUE;

    const FLOAT fSizeY   = 480.0f;
    const FLOAT fOriginX = 320.0f - ( fSizeY * .5f * fMovieWidth / fMovieHeight );
    const FLOAT fOriginY = 240.0f - fSizeY * .5f;

    // Draw the texture.
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    // Draw the texture as a quad.
    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

    // Wrapping isn't allowed on linear textures.
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );

    FLOAT fLeft   = fOriginX + 0.5f;
    FLOAT fRight  = fOriginX + ( fSizeY * fMovieWidth) / fMovieHeight - 0.5f;
    FLOAT fTop    = fOriginY + 0.5f;
    FLOAT fBottom = fOriginY + fSizeY - 0.5f;

    // On linear textures the texture coordinate range is from 0,0 to width,height, instead
    // of 0,0 to 1,1.
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->Begin( D3DPT_QUADLIST );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, fMovieHeight );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fLeft,  fBottom, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, 0 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fLeft,  fTop,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, fMovieWidth, 0 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fRight, fTop,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, fMovieWidth, fMovieHeight );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fRight, fBottom, 0.0f, 1.0f );
    m_pd3dDevice->End();
 
	return TRUE;
}

//-----------------------------------------------------------------------------
// LaunchHL2
//-----------------------------------------------------------------------------
void CXBoxLoader::LaunchHL2( unsigned int contextCode )
{
	LAUNCH_DATA			launchData;
	RelaunchHeader_t	*pRelaunch;
	const char			*pHL2Name;

	memset( &launchData, 0, sizeof( LAUNCH_DATA ) );

	// build the relaunch structure that HL2 uses
	pRelaunch = GetRelaunchHeader( launchData.Data );

	pRelaunch->magicNumber = RELAUNCH_MAGIC_NUMBER;
	pRelaunch->nBytesRelaunchData = 0;

	if ( ( contextCode & CONTEXTCODE_MAGICMASK ) == CONTEXTCODE_HL2MAGIC )
	{
		// ok to re-establish persistent data
		pRelaunch->bRetail     = (contextCode & CONTEXTCODE_RETAIL_MODE) > 0;
		pRelaunch->bInDebugger = (contextCode & CONTEXTCODE_INDEBUGGER) > 0;
	}
	else
	{
		// ensure we launch under retail conditions
		contextCode            = CONTEXTCODE_NO_XBDM;
		g_activeDevice         = -1;
		pRelaunch->bRetail     = true;
		pRelaunch->bInDebugger = false;
	}

	pRelaunch->contextCode  = contextCode;
	pRelaunch->activeDevice = g_activeDevice;
	pRelaunch->startTime    = g_loaderStartTime;

	// launch the xbe that is expected
	if ( contextCode & CONTEXTCODE_DEBUG_XBE )
	{
		// debug xbe
		pHL2Name = "D:\\hl2d_xbox.xbe";
	}
	else if ( contextCode & CONTEXTCODE_RELEASE_XBE )
	{
		// release xbe
		pHL2Name = "D:\\hl2r_xbox.xbe";
	}
	else
	{
		// default launch to retail xbe
		pHL2Name = "D:\\hl2_xbox.xbe";
	}

	XLaunchNewImage( pHL2Name, &launchData );

	// failed
	FatalMediaError();
}

//-----------------------------------------------------------------------------
// Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxLoader::Initialize()
{
	DWORD			launchType;
	LAUNCH_DATA		launchData;

	// no active device until set
	g_activeDevice = -1;

	// get launch info and command line params needed for early setting of systems
	LPSTR	pCmdLine = "";
	DWORD	retVal   = XGetLaunchInfo( &launchType, &launchData );
	if ( retVal == ERROR_SUCCESS )
	{
		if ( launchType == LDT_FROM_DASHBOARD )
		{
			// launched from dashboard
			LD_FROM_DASHBOARD *pLaunchFromDashboard = (LD_FROM_DASHBOARD *)(&launchData);
			m_contextCode = pLaunchFromDashboard->dwContext;
		}
		else if ( launchType == LDT_TITLE )
		{
			// launched directly from HL2 to do something
			LAUNCH_DATA* pLaunchData = (LAUNCH_DATA *)(&launchData);
			pCmdLine = (char *)pLaunchData->Data;

			RelaunchHeader_t *pHeader = GetRelaunchHeader( pLaunchData->Data );
			if ( pHeader->magicNumber == RELAUNCH_MAGIC_NUMBER )
			{
				m_contextCode  = pHeader->contextCode;
				g_activeDevice = pHeader->activeDevice;
			}
		}
		else if ( launchType == LDT_FROM_DEBUGGER_CMDLINE )
		{
			// launched from the debugger
			LAUNCH_DATA* pLaunchData = (LAUNCH_DATA *)(&launchData);
			pCmdLine = (char *)pLaunchData->Data;
			strlwr( pCmdLine );

			// assume retail mode
			m_contextCode |= CONTEXTCODE_HL2MAGIC;
			m_contextCode |= CONTEXTCODE_RETAIL_MODE;

			if ( strstr( pCmdLine, "-indebugger" ) )
			{
				m_contextCode |= CONTEXTCODE_INDEBUGGER;
			}
#ifndef _RETAIL
			if ( DmIsDebuggerPresent() )
			{
				m_contextCode |= CONTEXTCODE_INDEBUGGER;
			}
#endif
			if ( strstr( pCmdLine, "-debug" ) )
			{
				// launch to debug xbe
				m_contextCode |= CONTEXTCODE_DEBUG_XBE;
			}
			else if ( strstr( pCmdLine, "-release" ) )
			{
				// launch to release xbe
				m_contextCode |= CONTEXTCODE_RELEASE_XBE;
			}
			else
			{
				// default launch to retail xbe
				m_contextCode |= CONTEXTCODE_RETAIL_XBE|CONTEXTCODE_NO_XBDM;
				if ( strstr( pCmdLine, "-dev" ) )
				{
					// force dev link
					m_contextCode &= ~CONTEXTCODE_NO_XBDM;
				}
			}

			if ( strstr( pCmdLine, "-attract" ) )
			{
				// running the attract sequence
				m_contextCode |= CONTEXTCODE_ATTRACT;
			}
		}
	}

	if ( ( m_contextCode & CONTEXTCODE_MAGICMASK ) != CONTEXTCODE_HL2MAGIC )
	{
		// unknown, run the install normally
		// 0 is a special indicator, due to lack of valid magic
		m_contextCode = 0;
	}
	else
	{
		if ( m_contextCode & CONTEXTCODE_DASHBOARD )
		{
			// coming from dashboard, back to HL2 - immediately!
			LaunchHL2( m_contextCode );
			return S_OK;
		}
	}

   if ( FAILED( XFONT_OpenDefaultFont( &m_pDefaultTrueTypeFont ) ) )
   {
        return XBAPPERR_MEDIANOTFOUND;
   }

	// load install resources for context
	// Load resource file
	if ( FAILED( m_xprResource.Create( "D:\\LoaderMedia\\loader.xpr" ) ) )
	{
		return XBAPPERR_MEDIANOTFOUND;
	}

	if ( FAILED( LoadFont( &m_Font, loader_Font_OFFSET ) ) )
	{
		return XBAPPERR_MEDIANOTFOUND;
	}

	if ( !( m_contextCode & CONTEXTCODE_ATTRACT ) )
	{
		m_pFooterTexture = LoadTexture( loader_Footer_OFFSET );
		if ( !m_pFooterTexture )
		{
			return XBAPPERR_MEDIANOTFOUND;
		}

		switch( XGetLanguage() )
		{
		case XC_LANGUAGE_FRENCH:
			m_pMainLegalTexture = LoadTexture( loader_MainLegal_french_OFFSET );
			break;
		case XC_LANGUAGE_ITALIAN:
			m_pMainLegalTexture = LoadTexture( loader_MainLegal_italian_OFFSET );
			break;
		case XC_LANGUAGE_GERMAN:
			m_pMainLegalTexture = LoadTexture( loader_MainLegal_german_OFFSET );
			break;
		case XC_LANGUAGE_SPANISH:
			m_pMainLegalTexture = LoadTexture( loader_MainLegal_spanish_OFFSET );
			break;
		default:
			m_pMainLegalTexture = LoadTexture( loader_MainLegal_english_OFFSET );
			break;
		}
		if ( !m_pMainLegalTexture )
		{
			return XBAPPERR_MEDIANOTFOUND;
		}

		m_pSourceLegalTexture = LoadTexture( loader_SourceLegal_OFFSET );
		if ( !m_pSourceLegalTexture )
		{
			return XBAPPERR_MEDIANOTFOUND;
		}

		switch( XGetLanguage() )
		{
		case XC_LANGUAGE_FRENCH:
			m_pSlideShowTextures[0] = LoadTexture( loader_SlideShow1_french_OFFSET );
			break;
		case XC_LANGUAGE_ITALIAN:
			m_pSlideShowTextures[0] = LoadTexture( loader_SlideShow1_italian_OFFSET );
			break;
		case XC_LANGUAGE_GERMAN:
			m_pSlideShowTextures[0] = LoadTexture( loader_SlideShow1_german_OFFSET );
			break;
		case XC_LANGUAGE_SPANISH:
			m_pSlideShowTextures[0] = LoadTexture( loader_SlideShow1_spanish_OFFSET );
			break;
		default:
			m_pSlideShowTextures[0] = LoadTexture( loader_SlideShow1_english_OFFSET );
			break;
		}
		m_pSlideShowTextures[1] = LoadTexture( loader_SlideShow2_OFFSET );
		m_pSlideShowTextures[2] = LoadTexture( loader_SlideShow3_OFFSET );
		m_pSlideShowTextures[3] = LoadTexture( loader_SlideShow4_OFFSET );
		m_pSlideShowTextures[4] = LoadTexture( loader_SlideShow5_OFFSET );
		m_pSlideShowTextures[5] = LoadTexture( loader_SlideShow6_OFFSET );
		m_pSlideShowTextures[6] = LoadTexture( loader_SlideShow7_OFFSET );
		m_pSlideShowTextures[7] = LoadTexture( loader_SlideShow8_OFFSET );
		m_pSlideShowTextures[8] = LoadTexture( loader_SlideShow9_OFFSET );
		for ( int i=0; i<MAX_SLIDESHOW_TEXTURES; i++ )
		{
			if ( !m_pSlideShowTextures )
				return XBAPPERR_MEDIANOTFOUND;
		}

		if ( !VerifyInstall() )
		{
			OUTPUT_DEBUG_STRING( "Install failed!\n" );
			return -1;
		}
	}

	m_pLoadingIconTexture = LoadTexture( loader_LoadingIcon_OFFSET );
	if ( !m_pLoadingIconTexture )
	{
		return XBAPPERR_MEDIANOTFOUND;
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Performs per-frame video checks
//-----------------------------------------------------------------------------
void CXBoxLoader::TickVideo()
{
	if ( m_bMovieErrorIsFatal && m_player.IsFailed() )
	{
		FatalMediaError();
	}
}

//-----------------------------------------------------------------------------
// Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxLoader::FrameMove()
{
	bool bFailed = false;

	TickVideo();

	if ( m_State >= 10 && g_copyStats.m_copyErrors )
	{
		FatalMediaError();
	}

	if ( m_State >= 10 && ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START ) )
	{
		m_bDrawDebug ^= 1;
	}

	if ( m_bAllowAttractAbort && ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] || ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START ) ) )
	{
		StopVideo();

		// allow only once, until reset
		m_bAllowAttractAbort = false;
	}

	switch ( m_State )
	{
		case 0:
			if ( m_contextCode & CONTEXTCODE_ATTRACT )
			{
				// play attract mode
				m_State = 1;
			}
			else
			{
				// normal installation
				m_State = 9;
			}
			break;

		case 1:
			// Play the attract video
			if ( FAILED( StartVideo( "D:\\LoaderMedia\\Demo_Attract.xmv", false, false ) ) )
			{
				// jump to finish
				m_State = 4;
			}
			else
			{
				m_State = 2;
			}
			break;
			
		case 2:
			if ( m_player.IsPlaying() || m_player.IsFailed() )
			{
				// attract is playing, wait for finish
				m_State = 3;
				m_bAllowAttractAbort = true;
			}
			break;

		case 3:
			if ( !m_player.IsPlaying() || m_player.IsFailed() )
			{
				// attract is over or aborted
				m_State = 4;
			}
			break;

		case 4:
			// place loading
			m_bDrawLoading = true;
			m_FrameCounter = 0;
			m_State = 5;
			break;

		case 5:
			// wait for two frames to pass to ensure loading is rendered
			if ( m_FrameCounter > 2 )
			{
				m_bLaunch = true;
				m_State = 6;
			}
			break;

		case 6:
			// idle
			m_State = 6;
			break;
		
		case 9:
			// Play the opening Valve video
			StartVideo( "D:\\LoaderMedia\\Valve_Leader.xmv", true, true );

			// Start the async installation process
			if ( !StartInstall() )
			{
				OUTPUT_DEBUG_STRING( "Install failed!\n" );
				bFailed = true;
				break;
			}
			m_State = 10;
			break;

		case 10:
			if ( m_player.IsPlaying() )
			{
				// intro is playing, wait for finish
				m_State = 15;
			}
			break;

		case 15:
			if ( !m_player.IsPlaying() )
			{
				// intro is over
				m_State = 20;
			}
			break;

		case 20:
			// start legals
			StartLegalScreen( LEGAL_SOURCE );
			m_State = 25;
			break;

		case 25:
			if ( m_bDrawLegal && GetTickCount() - m_LegalTime > LEGAL_DISPLAY_TIME )
			{
				// advance to next legal
				StartLegalScreen( LEGAL_MAIN );
				m_State = 30;
			}
			break;

		case 30:
			if ( m_bDrawLegal && GetTickCount() - m_LegalTime > LEGAL_DISPLAY_TIME )
			{
				// end of all legals
				if ( IsTargetFileValid( "Z:\\LoaderMedia\\Title_Load.xmv", 0 ) )
				{
					m_bDrawLegal = false;
					m_State = 40;
				}
			}
			break;

		case 40:
			// play the gordon/alyx hl2 game movie
			m_bCaptureLastMovieFrame = true;
			StartVideo( "Z:\\LoaderMedia\\Title_Load.xmv", true, true );
			m_State = 50;
			break;

		case 50:
			if ( m_player.IsPlaying() )
			{
				// title movie is playing, wait for finish
				m_State = 60;
			}
			break;

		case 60:
			if ( m_player.GetElapsedTime() >= 8000 && !m_bDrawProgress )
			{
				// wait for known audio click, then put up progress bar
				// start the loading bar animation
				m_bDrawProgress = true;
			}
			
			if ( m_bInstallComplete && m_bDrawProgress )
			{
				// install has completed
				if ( m_player.IsPlaying() )
				{
					// force the movie to end 
					m_player.TerminatePlayback();
				}
				m_State = 70;
			}
			else if ( !m_bInstallComplete && !m_player.IsPlaying() )
			{
				// intro movie has finished, but install is still running
				// start up slideshow cycler
				m_bDrawSlideShow = true;
				m_State = 70;
			}
			break;

		case 70:
			// wait for movie or slideshow to stop
			if ( !m_player.IsPlaying() && !m_bDrawSlideShow )
			{
				m_bLaunch = true;
				m_State = 80;
			}
			break;

		case 80:
			// idle
			break;
	}


	if ( bFailed )
	{
		FatalMediaError();
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxLoader::Render()
{
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0L );

    // Play a frame from a video.
    BOOL bPlayedFrame = PlayVideoFrame();

	// hold the last frame of the title movie
	if ( m_State >= 60 && !bPlayedFrame )
	{
		DrawTexture( m_pLastMovieFrame, 0, 0, 640, 480, 0xFFFFFFFF );
	}

	DrawSlideshow();
	DrawLoadingMarquee();
	DrawProgressBar();
	DrawLegals();
	DrawDebug();
	DrawLog();

	if ( m_bLaunch )
	{
		// The installation has finished
		// Persist the image before launching hl2
		m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
		m_pd3dDevice->PersistDisplay();

		// Make sure the installation thread has completely exited
		if ( m_installThread )
		{
			WaitForSingleObject( m_installThread, INFINITE );
			CloseHandle( m_installThread );
		}

		LaunchHL2( m_contextCode );
		return S_OK;
	}

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

	m_FrameCounter++;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxLoader xbApp;
    if ( FAILED( xbApp.Create() ) )
	{
		xbApp.FatalMediaError();
	}
	xbApp.Run();
}

