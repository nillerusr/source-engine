//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "MapWorld.h"
#include "MessageWnd.h"
#include "IEditorTexture.h"
#include "GlobalFunctions.h"
#include "TextureSystem.h"
#include "TextureConverter.h"
#include "filesystem.h"
#include "Hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CProgressDlg *	CTextureConverter::m_pProgDlg;
int				CTextureConverter::m_nSolidCount;
int				CTextureConverter::m_nFaceCount;
int				CTextureConverter::m_nDecalCount;
int				CTextureConverter::m_nCurrentSolid;
int				CTextureConverter::m_nCurrentDecal;
int				CTextureConverter::m_nSuccesses;
int				CTextureConverter::m_nErrors;
int				CTextureConverter::m_nSkipped;
int				CTextureConverter::m_nWarnings;


//-----------------------------------------------------------------------------
// Purpose: Reset counters.
// Input  : 
// Output : Counters all reset to 0.
//-----------------------------------------------------------------------------
void CTextureConverter::Initialize( void )
{
	m_nSolidCount	= 0;
	m_nCurrentSolid	= 0;
	m_nFaceCount	= 0;
	m_nDecalCount	= 0;
	m_nCurrentDecal	= 0;

	m_nSuccesses	= 0;
	m_nErrors		= 0;
	m_nSkipped		= 0;
	m_nWarnings		= 0;
}


//-----------------------------------------------------------------------------
// Purpose: Recurse through the contents of the map, passing solid objects on
//          to be converted.
// Input  : pWorld - pointer to the map to have textures converted.
// Output : All solid faces and decals in the world have WAD3 textures
//          converted to VMT.
//-----------------------------------------------------------------------------
void CTextureConverter::ConvertWorldTextures( CMapWorld * pWorld )
{
	Initialize();

	// Bring the message window to the front, to display conversion info
	g_pwndMessage->Activate();

	Msg( mwStatus, "Converting textures from WAD to VMT format..." );

	// Set up a progress meter dialogue
	m_pProgDlg = new CProgressDlg;
	m_pProgDlg->Create();
	m_pProgDlg->SetStep( 1 );
	m_pProgDlg->SetWindowText( "Preparing to convert textures..." );

	// Run the converter
	ConvertSolids( pWorld );
	ConvertDecals( pWorld );
	DisplayStatistics();

	// Destroy the progress meter
	if ( m_pProgDlg )
	{
		m_pProgDlg->DestroyWindow();
		delete m_pProgDlg;
		m_pProgDlg = NULL;
	}

	AfxMessageBox( "Conversion complete.  Check the Hammer \"Messages\" window for complete details." );
}


//-----------------------------------------------------------------------------
// Purpose: Recurse through the contents of the map, passing solid objects on
//          to be converted.
// Input  : pWorld - pointer to the map to have textures converted.
// Output : All solid faces in the world have WAD3 textures converted to VMT.
//-----------------------------------------------------------------------------
void CTextureConverter::ConvertSolids( CMapWorld * pWorld )
{
	// Count total map solids so we know how many we have to do (for progress meter).
	pWorld->EnumChildren( ENUMMAPCHILDRENPROC( CountMapSolids ), 0, MAPCLASS_TYPE( CMapSolid ) );

	m_pProgDlg->SetRange( 0, m_nSolidCount );
	m_pProgDlg->SetStep( 2 );
	m_pProgDlg->SetWindowText( "Converting solids..." );

	// Cycle through the solids again and convert as necessary.
	pWorld->EnumChildren( ENUMMAPCHILDRENPROC( CheckSolidTextures ), 0, MAPCLASS_TYPE( CMapSolid ) );
}


//-----------------------------------------------------------------------------
// Purpose: Enumeration function, increment the solids counter.
// Input  : 
// Output : Always return true to continue enumerating.
//-----------------------------------------------------------------------------
bool CTextureConverter::CountMapSolids( CMapSolid *, DWORD )
{
	m_nSolidCount++;

	return true;	// return true to continue enumerating
}


//-----------------------------------------------------------------------------
// Purpose: Enumeration function, check all the faces of a solid for texture conversion
// Input  : pSolid - map solid to be checked.
// Output : Always return true to continue enumerating.
//-----------------------------------------------------------------------------
bool CTextureConverter::CheckSolidTextures( CMapSolid * pSolid, DWORD )
{
	int nFaceCount;

	m_nCurrentSolid++;

	if ( m_nCurrentSolid % 100 == 0 )
		m_pProgDlg->SetPos( m_nCurrentSolid );

	// check each face of the solid
	nFaceCount = pSolid->GetFaceCount();
	while( nFaceCount-- )
	{
		CheckFaceTexture( pSolid->GetFace( nFaceCount ) );
	}

	return true;	// return true to continue enumerating
}


//-----------------------------------------------------------------------------
// Purpose: Check the texture of a face to determine if conversion is necessary.
// Input  : pFace - a map face.
// Output : 
//-----------------------------------------------------------------------------
void CTextureConverter::CheckFaceTexture( CMapFace * pFace )
{
	m_nFaceCount++;

	// Criteria for needing conversion is a) being a dummy texture AND b) having no slashes
	// in the texture name.

	if ( !pFace->GetTexture()->IsDummy() )
	{
		m_nSkipped++;
		return;
	}

	if ( strchr( pFace->GetTexture()->GetName(), '/') != NULL )
	{
		m_nSkipped++;
		return;
	}

	ConvertFaceTexture( pFace );
}


bool TextureEndsIn( const char *pTextureName, const char *pEnd )
{
	const char *pLast = strrchr( pTextureName, '\\' );
	if ( strrchr( pTextureName, '/' ) > pLast )
		pLast = strrchr( pTextureName, '/' );

	if ( pLast )
		return stricmp( pLast+1, pEnd ) == 0;
	else
		return stricmp( pTextureName, pEnd ) == 0;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if any materials match the old texture of a face and replace
//          appropriately.
// Input  : pFace - a map face known to need conversion.
// Output : 
//-----------------------------------------------------------------------------
void CTextureConverter::ConvertFaceTexture( CMapFace * pFace )
{
	EditorTextureList_t tlMatches;
	IEditorTexture *pNewTexture;

	const char *pTextureName = pFace->GetTexture()->GetName();
	
	// Check for SKY and SKIP brushes.
	char *replacements[][2] = 
	{
		{ "sky",		"tools/toolsskybox" },
		{ "skip",		"tools/toolsskip" },
		{ "aaatrigger", "tools/toolstrigger" },
		{ "hint",		"tools/toolshint" },
		{ "clip",		"tools/toolsclip" },
		{ "null",		"tools/toolsnodraw" }
	};
	for ( int i=0; i < sizeof( replacements ) / sizeof( replacements[0] ); i++ )
	{
		if ( TextureEndsIn( pTextureName, replacements[i][0] ) )
		{
			pNewTexture = g_Textures.FindActiveTexture( replacements[i][1] );
			if ( pNewTexture )
			{
				ReplaceFaceTexture( pFace, pNewTexture );
				return;
			}
		}
	}

	GetNewTextureMatches( pTextureName, tlMatches );

	switch( tlMatches.Count() )
	{
	case 0:
		MsgConvertFace( pFace, "ERROR: No matching material.  Cannot convert." );
		m_nErrors++;

		break;
	case 1:
		pNewTexture = tlMatches.Element(0);
		ReplaceFaceTexture( pFace, pNewTexture );

		break;
	default:
		// Multiple matches.  For now, just use the first.
		pNewTexture = tlMatches.Element(0);
		ReplaceFaceTexture( pFace, pNewTexture );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Recurse through the contents of the map, passing decal objects on
//          to be converted.
// Input  : pWorld - pointer to the map to have textures converted.
// Output : All decals in the world have WAD3 textures converted to VMT.
//-----------------------------------------------------------------------------
void CTextureConverter::ConvertDecals( CMapWorld * pWorld )
{
	// Count total map decals so we know how many we have to do (for progress meter).
	pWorld->EnumChildren( ENUMMAPCHILDRENPROC( CountMapDecals ), 0, MAPCLASS_TYPE( CMapEntity ) );

	m_pProgDlg->SetRange( 0, m_nDecalCount );
	m_pProgDlg->SetStep( 3 );
	m_pProgDlg->SetWindowText( "Converting decals..." );

	// Cycle through the solids again and convert as necessary.
	pWorld->EnumChildren( ENUMMAPCHILDRENPROC( CheckDecalTextures ), 0, MAPCLASS_TYPE( CMapEntity ) );
}


//-----------------------------------------------------------------------------
// Purpose: Enumeration function, increment the decals counter if entity is a decal.
// Input  : 
// Output : Always return true to continue enumerating.
//-----------------------------------------------------------------------------
bool CTextureConverter::CountMapDecals( CMapEntity * pEnt, DWORD )
{
	if ( !strcmp( pEnt->GetClassName(), "infodecal" ) )
		m_nDecalCount++;

	return true;	// return true to continue enumerating
}


//-----------------------------------------------------------------------------
// Purpose: Enumeration function, check a decal's texture to determine if
//          conversion is necessary.
// Input  : pEnt - map decal to be checked.
// Output : Always return true to continue enumerating.
//-----------------------------------------------------------------------------
bool CTextureConverter::CheckDecalTextures( CMapEntity * pEnt, DWORD )
{
	if ( strcmp( pEnt->GetClassName(), "infodecal" ) )
		return true;	// not a decal, return true to continue enumerating

	m_nCurrentDecal++;

	m_pProgDlg->SetPos( m_nCurrentDecal );

	if ( strchr( pEnt->GetKeyValue( "texture" ), '/') != NULL )
	{
		m_nSkipped++;
	}
	else
	{
		ConvertDecalTexture( pEnt );
	}

	return true;	// return true to continue enumerating
}


//-----------------------------------------------------------------------------
// Purpose: Determine if any materials match the old texture of a decal and replace
//          appropriately.
// Input  : pEnt - a map decal known to need conversion.
// Output : 
//-----------------------------------------------------------------------------
void CTextureConverter::ConvertDecalTexture( CMapEntity * pEnt )
{
	EditorTextureList_t tlMatches;
	IEditorTexture *pNewTexture;

	GetNewTextureMatches( pEnt->GetKeyValue( "texture" ), tlMatches );

	switch( tlMatches.Count() )
	{
	case 0:
		MsgConvertDecal( pEnt, "ERROR: No matching material.  Cannot convert." );
		m_nErrors++;

		break;
	case 1:
		pNewTexture = tlMatches.Element(0);
		ReplaceDecalTexture( pEnt, pNewTexture );

		break;
	default:
		// Multiple matches.  For now, just use the first.
		pNewTexture = tlMatches.Element(0);

		MsgConvertDecal( pEnt, "WARNING: Multiple matches found.  Using first match (%s).", pNewTexture->GetName() );
		m_nWarnings++;

		ReplaceDecalTexture( pEnt, pNewTexture );

		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Look for material matches for an old texture and add them to a list.
// Input  : pszOldName - old texture name.
//          pMatchList - empty texture list.
// Output : pMatchList - texture list is filled in with matching material textures.
//-----------------------------------------------------------------------------
void CTextureConverter::GetNewTextureMatches( const char * pszOldName, EditorTextureList_t &tlMatchList )
{
	IEditorTexture *	pTexture;
	int			nIndex;

	nIndex		= 0;
	pTexture	= g_Textures.EnumActiveTextures( &nIndex, tfVMT );

	// loop through all VMT textures
	while ( pTexture != NULL )
	{
		if ( TextureNameMatchesMaterialName( pszOldName, pTexture->GetName() ) )  // check for a match
		{
			tlMatchList.AddToTail( pTexture );
		}

		pTexture = g_Textures.EnumActiveTextures( &nIndex, tfVMT );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Compare an old texture name to a new material name.
// Input  : pszTextureName - Old texture name.
//          pszMaterialName - New material name.
// Output : Return true if the old texture name is the same (case insensitive)
//          as the last token (delimiter '/') of the new material name, otherwise
//          return false.
//-----------------------------------------------------------------------------
bool CTextureConverter::TextureNameMatchesMaterialName( const char * pszTextureName, const char * pszMaterialName )
{
	const char * pszPartialMaterialName;		// sublocation of the material name

	pszPartialMaterialName = strrchr( pszMaterialName, '/' );  // Find the last '/'

	if ( pszPartialMaterialName != NULL)
	{
		pszPartialMaterialName++;	// Point to the character after the '/'
	}
	else
	{
		pszPartialMaterialName = pszMaterialName;  // No slashes found, just point to the name
	}

	// No '/' found in the VMT name, or the name ended in a '/'.  This shouldn't happen.
	if ( ( pszPartialMaterialName == NULL ) || strlen( pszPartialMaterialName ) == 0 )
		return false;

	if ( stricmp( pszTextureName, pszPartialMaterialName ) == 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Change the texture on a face, re-scaling if possible.
// Input  : pFace - a map face
//          pNewTexture - texture to place on the map face.
// Output : pFace has a new texture pointer set.
//-----------------------------------------------------------------------------
void CTextureConverter::ReplaceFaceTexture( CMapFace * pFace, IEditorTexture * pNewTexture )
{
	if ( !pNewTexture->Load() )	// make sure new texture is loaded
	{
		MsgConvertFace( pFace, "WARNING: Couldn't load new material.  Texture converted but not re-scaled." );
		m_nWarnings++;
	}

	RescaleFaceTexture( pFace, pNewTexture );

	pFace->SetTexture( pNewTexture );

	m_nSuccesses++;
}


//-----------------------------------------------------------------------------
// Purpose: Change the texture on a decal
// Input  : pEntity - a map decal
//          pNewTexture - texture to place on the map face.
// Output : pEnt has a new texture set
//-----------------------------------------------------------------------------
void CTextureConverter::ReplaceDecalTexture( CMapEntity * pEnt, IEditorTexture * pNewTexture )
{
	pEnt->SetKeyValue( "texture", pNewTexture->GetName() );
	m_nSuccesses++;
}


//-----------------------------------------------------------------------------
// Purpose: Find a WAD3 texture based on a name search.
// Input  : pszName - name of the texture to search for.
// Output : return a texture if found, otherwise NULL.
//-----------------------------------------------------------------------------
IEditorTexture * CTextureConverter::FindWAD3Texture( const char * pszName )
{
	IEditorTexture *	pTexture;
	int			nIndex;

	nIndex		= 0;
	pTexture	= g_Textures.EnumActiveTextures( &nIndex, tfWAD3 );

	// loop through all the WAD3 textures
	while ( pTexture != NULL )
	{
		if ( !strcmp( pTexture->GetName(), pszName ) )		//check for exact match
			return pTexture;

		pTexture = g_Textures.EnumActiveTextures( &nIndex, tfWAD3 );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Change the scale and shift values of a texture, based on old texture
//          image dimensions compared to new texture image dimensions.
// Input  : pFace - map face that the texture to be scaled is on.
//          pOldTexture - the old texture.
//          pNewTexture - the new texture.
// Output : pFace->scale and pface->texture are modified if either the height or
//          width (or both) of the texture has changed.
//-----------------------------------------------------------------------------
void CTextureConverter::RescaleFaceTexture( CMapFace * pFace, IEditorTexture * pNewTexture )
{
	int nNewWidth;
	int nNewHeight;

	int nOldWidth = -1;
	int nOldHeight = -1;
	
	// First look for the .resizeinfo in the mod dir (hl2\dod), then the game dir (hl2\hl2).
	char resizeInfoFilename[512];
	Q_snprintf( resizeInfoFilename, sizeof( resizeInfoFilename ), "materials\\%s.resizeinfo", pNewTexture->GetName() );
	FileHandle_t fp = g_pFileSystem->Open( resizeInfoFilename, "rt" );
	if ( !fp )
	{
		return;
	}

	char line[512];
	int nScanned = 0;
	if ( g_pFullFileSystem->ReadLine( line, sizeof( line ), fp ) )
	{
		nScanned = sscanf( line, "%d %d", &nOldWidth, &nOldHeight );
	}
	g_pFileSystem->Close( fp );
	if ( nScanned != 2 || nOldWidth < 0 || nOldHeight < 0 || nOldWidth > 5000 || nOldHeight > 5000 )
		return;
	
	nNewWidth	= pNewTexture->GetWidth();
	nNewHeight	= pNewTexture->GetHeight();

	// Divide by 0 checks
	if ( ( nOldWidth == 0 ) || ( nOldHeight == 0 ) )
	{
		MsgConvertFace( pFace,
						"WARNING: Invalid old texture dimensions (%dx%d).  Texture converted but not re-scaled.",
						nOldWidth,
						nOldHeight
						);
		m_nWarnings++;
		return;
	}

	// Divide by 0 checks
	if ( ( nNewWidth == 0 ) || ( nNewHeight == 0 ) )
	{
		MsgConvertFace( pFace,
						"WARNING: Invalid new material dimensions (%dx%d).  Texture converted but not re-scaled.",
						nNewWidth,
						nNewHeight
						);
		m_nWarnings++;
		return;
	}



	if ( nOldWidth != nNewWidth )
	{
		// Adjust the width scale by an old to new ratio
		pFace->texture.scale[ 0 ] = pFace->texture.scale[ 0 ] * nOldWidth / nNewWidth;

		// Adjust the height shift by a new to old ratio
		pFace->texture.UAxis[ 3 ] = pFace->texture.UAxis[ 3 ] * nNewWidth / nOldWidth;
	}

	if ( nOldHeight != nNewHeight )
	{
		// Adjust the height scale by an old to new ratio
		pFace->texture.scale[ 1 ] = pFace->texture.scale[ 1 ] * nOldHeight / nNewHeight;

		// Adjust the height shift by a new to old ratio
		pFace->texture.VAxis[ 3 ] = pFace->texture.VAxis[ 3 ] * nNewHeight / nOldHeight;
	}

	pFace->CalcTextureCoords();		// recompute internals base on the new scaling and shifting.
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to WC's message window about the specified face.
// Input  : pFace - The map face the message relates to
//			format - The message format string, *printf style
//			... - The remaining arguments of the *printf style message
// Output : A status message is sent to the WC message window.
//-----------------------------------------------------------------------------
void CTextureConverter::MsgConvertFace( CMapFace * pFace, const char * format, ... )
{
	va_list	ptr;
	char	message[ 1024 ];
	Vector	vecFaceCenter;

	pFace->GetCenter( vecFaceCenter );

	va_start( ptr, format );
	_vsnprintf( message, 1024, format, ptr );
	va_end( ptr );

	Msg(	mwStatus,
			"[face] %s at (%d,%d,%d):  %s",
			pFace->GetTexture()->GetName(),
			(int)vecFaceCenter[ 0 ],
			(int)vecFaceCenter[ 1 ],
			(int)vecFaceCenter[ 2 ],
			message
	);
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to WC's message window about the specified decal.
// Input  : pEnt - The map decal the message relates to
//			format - The message format string, *printf style
//			... - The remaining arguments of the *printf style message
// Output : A status message is sent to the WC message window.
//-----------------------------------------------------------------------------
void CTextureConverter::MsgConvertDecal( CMapEntity * pEnt, const char * format, ... )
{
	va_list	ptr;
	char	message[ 1024 ];
	Vector	vecOrigin;

	pEnt->GetOrigin( vecOrigin );

	va_start( ptr, format );
	_vsnprintf( message, 1024, format, ptr );
	va_end( ptr );

	Msg(	mwStatus,
			"[decal] %s at (%d,%d,%d):  %s",
			pEnt->GetKeyValue("texture"),
			(int) vecOrigin.x,
			(int) vecOrigin.y,
			(int) vecOrigin.z,
			message
	);
}


//-----------------------------------------------------------------------------
// Purpose: Display information about the full conversion process.
// Input  : 
// Output : Values of the counters are logged.
//-----------------------------------------------------------------------------
void CTextureConverter::DisplayStatistics( void )
{
	Msg( mwStatus, "==================" );
	Msg( mwStatus, "Conversion summary:" );
	Msg( mwStatus, "==================" );
	Msg( mwStatus, "Total solids:                 %10d", m_nSolidCount );
	Msg( mwStatus, "Total faces:                  %10d", m_nFaceCount );
	Msg( mwStatus, "Total decals:                 %10d", m_nDecalCount );
	Msg( mwStatus, "Total conversions:            %10d", m_nFaceCount + m_nDecalCount );
	Msg( mwStatus, "Successful conversions:       %10d", m_nSuccesses );
	Msg( mwStatus, "Skipped conversions           %10d", m_nSkipped );
	Msg( mwStatus, "Conversion errors:            %10d", m_nErrors );
	Msg( mwStatus, "Conversion warnings:          %10d", m_nWarnings );
}
