//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.cpp
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include "ViewerSettings.h"
#include "studiomodel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "windows.h"


ViewerSettings g_viewerSettings;


ViewerSettings::ViewerSettings()
{
	Q_memset( this, 0, sizeof( *this ) );
}

void InitViewerSettings ( const char *subkey )
{
	ViewerSettings save = g_viewerSettings;

	memset (&g_viewerSettings, 0, sizeof (ViewerSettings));

	// Some values should survive.  This is a crappy way to do settings in general.  Sigh.
	{
		g_viewerSettings.faceposerToolsDriveMouth = save.faceposerToolsDriveMouth;
	}

	strcpy( g_viewerSettings.registrysubkey, subkey );

	g_pStudioModel->m_angles.Init( -90.0f, 0.0f, 0.0f );
	g_pStudioModel->m_origin.Init( 0.0f, 0.0f, 50.0f );

	g_viewerSettings.renderMode = RM_TEXTURED;
	g_viewerSettings.fov = 65.0f;
	g_viewerSettings.enableNormalMapping = true;
	g_viewerSettings.enableParallaxMapping = false;
	g_viewerSettings.showNormals = false;
	g_viewerSettings.showTangentFrame = false;
	g_viewerSettings.overlayWireframe = false;
	g_viewerSettings.enableSpecular = true;
	g_viewerSettings.playSounds = true;

	g_viewerSettings.bgColor[0] = 0.25f;
	g_viewerSettings.bgColor[1] = 0.25f;
	g_viewerSettings.bgColor[2] = 0.25f;

	g_viewerSettings.gColor[0] = 0.85f;
	g_viewerSettings.gColor[1] = 0.85f;
	g_viewerSettings.gColor[2] = 0.69f;

	g_viewerSettings.lColor[0] = 1.0f;
	g_viewerSettings.lColor[1] = 1.0f;
	g_viewerSettings.lColor[2] = 1.0f;

	g_viewerSettings.aColor[0] = 0.3f;
	g_viewerSettings.aColor[1] = 0.3f;
	g_viewerSettings.aColor[2] = 0.3f;

	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f;
	g_viewerSettings.lightrot[2] = 0.0f;

	g_viewerSettings.speedScale = 1.0f;

	g_viewerSettings.application_mode = 0;

	g_viewerSettings.thumbnailsize = 128;
	g_viewerSettings.thumbnailsizeanim = 128;

	g_viewerSettings.m_iEditAttachment = -1;

	g_viewerSettings.highlightHitbox = -1;
	g_viewerSettings.highlightBone = -1;

	g_viewerSettings.speechapiindex = 0;
	g_viewerSettings.cclanguageid = 0;

	// default hlmv settings
	g_viewerSettings.xpos = 20;
	g_viewerSettings.ypos = 20;
	g_viewerSettings.width = 640;
	g_viewerSettings.height = 700;

	g_viewerSettings.originAxisLength = 10.0f;
}


bool RegReadVector( HKEY hKey, const char *szSubKey, Vector& value )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( szBuff );

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)szBuff,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (sscanf( szBuff, "(%f %f %f)", &value[0], &value[1], &value[2] ) != 3)
		return false;

	return true;
}

bool RegReadQAngle( HKEY hKey, const char *szSubKey, QAngle& value )
{
	Vector tmp;
	if (RegReadVector( hKey, szSubKey, tmp ))
	{
		value.Init( tmp.x, tmp.y, tmp.z );
		return true;
	}
	return false;
}


bool RegReadColor( HKEY hKey, const char *szSubKey, float value[4] )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( szBuff );

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)szBuff,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (sscanf( szBuff, "(%f %f %f %f)", &value[0], &value[1], &value[2], &value[3] ) != 4)
		return false;

	return true;
}


bool RegWriteVector( HKEY hKey, const char *szSubKey, Vector& value )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwSize;           // Size of element data

	sprintf( szBuff, "(%f %f %f)", value[0], value[1], value[2] );
	dwSize = strlen( szBuff );

	lResult = RegSetValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(LPBYTE)szBuff,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}

bool RegWriteQAngle( HKEY hKey, const char *szSubKey, QAngle& value )
{
	Vector tmp;
	tmp.Init( value.x, value.y, value.z );
	return RegWriteVector( hKey, szSubKey, tmp );
}


bool RegWriteColor( HKEY hKey, const char *szSubKey, float value[4] )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwSize;           // Size of element data

	sprintf( szBuff, "(%f %f %f %f)", value[0], value[1], value[2], value[3] );
	dwSize = strlen( szBuff );

	lResult = RegSetValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(LPBYTE)szBuff,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}

bool RegReadBool( HKEY hKey, const char *szSubKey, bool *value )
{
	LONG lResult;           // Registry function result code
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	DWORD dwTemp;

	dwSize = sizeof( dwTemp );

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)&dwTemp,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (dwType != REG_DWORD)
		return false;

	*value = (dwTemp != 0);

	return true;
}

bool RegReadInt( HKEY hKey, const char *szSubKey, int *value )
{
	LONG lResult;           // Registry function result code
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( DWORD );

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)value,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (dwType != REG_DWORD)
		return false;

	return true;
}


bool RegWriteInt( HKEY hKey, const char *szSubKey, int value )
{
	LONG lResult;           // Registry function result code
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( DWORD );

	lResult = RegSetValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_DWORD,		// type buffer
		(LPBYTE)&value,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}




bool RegReadFloat( HKEY hKey, const char *szSubKey, float *value )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( szBuff );

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)szBuff,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	*value = atof( szBuff );

	return true;
}


bool RegWriteFloat( HKEY hKey, const char *szSubKey, float value )
{
	LONG lResult;           // Registry function result code
	char szBuff[128];       // Temp. buffer
	DWORD dwSize;           // Size of element data

	sprintf( szBuff, "%f", value );
	dwSize = strlen( szBuff );

	lResult = RegSetValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(LPBYTE)szBuff,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}



bool RegReadString( HKEY hKey, const char *szSubKey, char *string, int size )
{
	LONG lResult;           // Registry function result code
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = size;

	lResult = RegQueryValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)string,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (dwType != REG_SZ)
		return false;

	return true;
}


bool RegWriteString( HKEY hKey, const char *szSubKey, char *string )
{
	LONG lResult;           // Registry function result code
	DWORD dwSize;           // Size of element data

	dwSize = strlen( string );

	lResult = RegSetValueEx(
		hKey,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(LPBYTE)string,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}









LONG RegViewerSettingsKey( const char *filename, PHKEY phKey, LPDWORD lpdwDisposition )
{
	if (strlen( filename ) == 0)
		return ERROR_KEY_DELETED;
	
	char szFileName[1024];

	strcpy( szFileName, filename );

	// strip out bogus characters
	for (char *cp = szFileName; *cp; cp++)
	{
		if (*cp == '\\' || *cp == '/' || *cp == ':')
			*cp = '.';
	}

	char szModelKey[1024];

	sprintf( szModelKey, "Software\\Valve\\%s\\%s", g_viewerSettings.registrysubkey, szFileName );

	return RegCreateKeyEx(
		HKEY_CURRENT_USER,	// handle of open key 
		szModelKey,			// address of name of subkey to open 
		0,					// DWORD ulOptions,	  // reserved 
		NULL,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		phKey,				// Key we are creating
		lpdwDisposition);    // Type of creation
}


LONG RegViewerRootKey( PHKEY phKey, LPDWORD lpdwDisposition )
{
	char szRootKey[1024];

	sprintf( szRootKey, "Software\\Valve\\%s", g_viewerSettings.registrysubkey );

	return RegCreateKeyEx(
		HKEY_CURRENT_USER,	// handle of open key 
		szRootKey,			// address of name of subkey to open 
		0,					// DWORD ulOptions,	  // reserved 
		NULL,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		phKey,				// Key we are creating
		lpdwDisposition);    // Type of creation
}


bool LoadViewerSettingsInt( char const *keyname, int *value )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	HKEY hModelKey;

	lResult = RegViewerSettingsKey( "hlfaceposer", &hModelKey, &dwDisposition);
	
	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		return false;
	}

	*value = 0;
	RegReadInt( hModelKey, keyname, value );
	return true;
}

bool SaveViewerSettingsInt ( const char *keyname, int value )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	HKEY hModelKey;
	lResult = RegViewerSettingsKey( "hlfaceposer", &hModelKey, &dwDisposition);

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	RegWriteInt( hModelKey, keyname, value );
	return true;
}

bool LoadViewerSettings (const char *filename, StudioModel *pModel )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	HKEY hModelKey;

	if (filename == NULL || pModel == NULL)
		return false;

	lResult = RegViewerSettingsKey( filename, &hModelKey, &dwDisposition);
	
	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		return false;
	}

	RegReadQAngle( hModelKey, "Rot", pModel->m_angles );
	RegReadVector( hModelKey, "Trans", pModel->m_origin );
	RegReadColor( hModelKey, "bgColor", g_viewerSettings.bgColor );
	RegReadColor( hModelKey, "gColor", g_viewerSettings.gColor );
	RegReadColor( hModelKey, "lColor", g_viewerSettings.lColor );
	RegReadColor( hModelKey, "aColor", g_viewerSettings.aColor );
	RegReadQAngle( hModelKey, "lightrot", g_viewerSettings.lightrot );

	int iTemp;
	float flTemp;
	char szTemp[256];

	RegReadString( hModelKey, "sequence", szTemp, sizeof( szTemp ) );
	iTemp = pModel->LookupSequence( szTemp );
	pModel->SetSequence( iTemp );
	RegReadString( hModelKey, "overlaySequence0", szTemp, sizeof( szTemp ) );
	iTemp = pModel->LookupSequence( szTemp );
	RegReadFloat( hModelKey, "overlayWeight0", &flTemp );
	pModel->SetOverlaySequence( 0, iTemp, flTemp );
	RegReadString( hModelKey, "overlaySequence1", szTemp, sizeof( szTemp ) );
	iTemp = pModel->LookupSequence( szTemp );
	RegReadFloat( hModelKey, "overlayWeight1", &flTemp );
	pModel->SetOverlaySequence( 1, iTemp, flTemp );
	RegReadString( hModelKey, "overlaySequence2", szTemp, sizeof( szTemp ) );
	iTemp = pModel->LookupSequence( szTemp );
	RegReadFloat( hModelKey, "overlayWeight2", &flTemp );
	pModel->SetOverlaySequence( 2, iTemp, flTemp );
	RegReadString( hModelKey, "overlaySequence3", szTemp, sizeof( szTemp ) );
	iTemp = pModel->LookupSequence( szTemp );
	RegReadFloat( hModelKey, "overlayWeight3",&flTemp );
	pModel->SetOverlaySequence( 3, iTemp, flTemp );

	RegReadFloat( hModelKey, "speedscale", &g_viewerSettings.speedScale );
	if (g_viewerSettings.speedScale > 1.0)
		g_viewerSettings.speedScale = 1.0;

	RegReadInt( hModelKey, "viewermode", &g_viewerSettings.application_mode );
	RegReadInt( hModelKey, "thumbnailsize", &g_viewerSettings.thumbnailsize );
	RegReadInt( hModelKey, "thumbnailsizeanim", &g_viewerSettings.thumbnailsizeanim );

	if ( g_viewerSettings.thumbnailsize == 0 )
	{
		g_viewerSettings.thumbnailsize = 128;
	}
	if ( g_viewerSettings.thumbnailsizeanim == 0 )
	{
		g_viewerSettings.thumbnailsizeanim = 128;
	}

	RegReadInt( hModelKey, "speechapiindex", &g_viewerSettings.speechapiindex );
	RegReadInt( hModelKey, "cclanguageid", &g_viewerSettings.cclanguageid );

	RegReadBool( hModelKey, "showground", &g_viewerSettings.showGround );
	RegReadBool( hModelKey, "showbackground", &g_viewerSettings.showBackground );
	RegReadBool( hModelKey, "showshadow", &g_viewerSettings.showShadow );
	RegReadBool( hModelKey, "showillumpos", &g_viewerSettings.showIllumPosition );
	RegReadBool( hModelKey, "enablenormalmapping", &g_viewerSettings.enableNormalMapping );
	RegReadBool( hModelKey, "playsounds", &g_viewerSettings.playSounds );
	RegReadBool( hModelKey, "showoriginaxis", &g_viewerSettings.showOriginAxis );
	RegReadFloat( hModelKey, "originaxislength", &g_viewerSettings.originAxisLength );

	char merge_buffer[32];
	for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
	{
		Q_snprintf( merge_buffer, sizeof( merge_buffer ), "merge%d", i + 1 );
		RegReadString( hModelKey, merge_buffer, g_viewerSettings.mergeModelFile[i], sizeof( g_viewerSettings.mergeModelFile[i] ) );
	}
	
	return true;
}





bool LoadViewerRootSettings( void )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	HKEY hRootKey;
	lResult = RegViewerRootKey( &hRootKey, &dwDisposition);

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	RegReadInt( hRootKey, "renderxpos", &g_viewerSettings.xpos );
	RegReadInt( hRootKey, "renderypos", &g_viewerSettings.ypos );
	RegReadInt( hRootKey, "renderwidth", &g_viewerSettings.width );
	RegReadInt( hRootKey, "renderheight", &g_viewerSettings.height );

	RegReadBool( hRootKey, "faceposerToolsDriveMouth", &g_viewerSettings.faceposerToolsDriveMouth );

	return true;
}



bool SaveViewerSettings (const char *filename, StudioModel *pModel )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	if (filename == NULL || pModel == NULL)
		return false;

	HKEY hModelKey;
	lResult = RegViewerSettingsKey( filename, &hModelKey, &dwDisposition);

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	CStudioHdr *hdr = pModel->GetStudioHdr();
	if ( !hdr )
		return false;

	RegWriteQAngle( hModelKey, "Rot", pModel->m_angles );
	RegWriteVector( hModelKey, "Trans", pModel->m_origin );
	RegWriteColor( hModelKey, "bgColor", g_viewerSettings.bgColor );
	RegWriteColor( hModelKey, "gColor", g_viewerSettings.gColor );
	RegWriteColor( hModelKey, "lColor", g_viewerSettings.lColor );
	RegWriteColor( hModelKey, "aColor", g_viewerSettings.aColor );
	RegWriteQAngle( hModelKey, "lightrot", g_viewerSettings.lightrot );
	RegWriteString( hModelKey, "sequence", hdr->pSeqdesc( pModel->GetSequence() ).pszLabel() );
	RegWriteString( hModelKey, "overlaySequence0", hdr->pSeqdesc( pModel->GetOverlaySequence( 0 ) ).pszLabel() );
	RegWriteFloat( hModelKey, "overlayWeight0", pModel->GetOverlaySequenceWeight( 0 ) );
	RegWriteString( hModelKey, "overlaySequence1", hdr->pSeqdesc( pModel->GetOverlaySequence( 1 ) ).pszLabel() );
	RegWriteFloat( hModelKey, "overlayWeight1", pModel->GetOverlaySequenceWeight( 1 ) );
	RegWriteString( hModelKey, "overlaySequence2", hdr->pSeqdesc( pModel->GetOverlaySequence( 2 ) ).pszLabel() );
	RegWriteFloat( hModelKey, "overlayWeight2", pModel->GetOverlaySequenceWeight( 2 ) );
	RegWriteString( hModelKey, "overlaySequence3", hdr->pSeqdesc( pModel->GetOverlaySequence( 3 ) ).pszLabel() );
	RegWriteFloat( hModelKey, "overlayWeight3", pModel->GetOverlaySequenceWeight( 3 ) );
	RegWriteFloat( hModelKey, "speedscale", g_viewerSettings.speedScale );
	RegWriteInt( hModelKey, "viewermode", g_viewerSettings.application_mode );
	RegWriteInt( hModelKey, "thumbnailsize", g_viewerSettings.thumbnailsize );
	RegWriteInt( hModelKey, "thumbnailsizeanim", g_viewerSettings.thumbnailsizeanim );
	RegWriteInt( hModelKey, "speechapiindex", g_viewerSettings.speechapiindex );
	RegWriteInt( hModelKey, "cclanguageid", g_viewerSettings.cclanguageid );
	RegWriteInt( hModelKey, "showground", g_viewerSettings.showGround );
	RegWriteInt( hModelKey, "showbackground", g_viewerSettings.showBackground );
	RegWriteInt( hModelKey, "showshadow", g_viewerSettings.showShadow );
	RegWriteInt( hModelKey, "showillumpos", g_viewerSettings.showIllumPosition );
	RegWriteInt( hModelKey, "enablenormalmapping", g_viewerSettings.enableNormalMapping );
	RegWriteInt( hModelKey, "playsounds", g_viewerSettings.playSounds );
	RegWriteInt( hModelKey, "showoriginaxis", g_viewerSettings.showOriginAxis );
	RegWriteFloat( hModelKey, "originaxislength", g_viewerSettings.originAxisLength );

	char merge_buffer[32];
	for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
	{
		Q_snprintf( merge_buffer, sizeof( merge_buffer ), "merge%d", i + 1 );
		RegWriteString( hModelKey, merge_buffer, g_viewerSettings.mergeModelFile[i] );
	}

	return true;
}

bool SaveViewerRootSettings( void )
{
	LONG lResult;           // Registry function result code
	DWORD dwDisposition;    // Type of key opening event

	HKEY hRootKey;
	lResult = RegViewerRootKey( &hRootKey, &dwDisposition);

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	RegWriteInt( hRootKey, "renderxpos", g_viewerSettings.xpos );
	RegWriteInt( hRootKey, "renderypos", g_viewerSettings.ypos );
	RegWriteInt( hRootKey, "renderwidth", g_viewerSettings.width );
	RegWriteInt( hRootKey, "renderheight", g_viewerSettings.height );

	RegWriteInt( hRootKey, "faceposerToolsDriveMouth", g_viewerSettings.faceposerToolsDriveMouth ? 1 : 0 );

	return true;
}
