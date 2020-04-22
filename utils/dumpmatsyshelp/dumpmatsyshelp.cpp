//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/dbg.h"
#include <windows.h>
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "../materialsystem/ishadersystem.h"
#include "utlvector.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"

CreateInterfaceFn g_MatSysFactory = NULL;
CreateInterfaceFn g_ShaderAPIFactory = NULL;

class CShaderDLLInfo
{
public:
	char m_Filename[MAX_PATH];
	IShaderDLLInternal *m_pInternal;
};
CUtlVector<CShaderDLLInfo> g_ShaderDLLs;

bool LoadShaderDLL( const char *pFilename )
{
	// Load the new shader
	CSysModule *hInstance = g_pFullFileSystem->LoadModule( pFilename );
	if ( !hInstance )
		return false;

	// Get at the shader DLL interface
	CreateInterfaceFn factory = Sys_GetFactory( hInstance );
	if (!factory)
	{
		g_pFullFileSystem->UnloadModule( hInstance );
		return false;
	}

	IShaderDLLInternal *pShaderDLL = (IShaderDLLInternal*)factory( SHADER_DLL_INTERFACE_VERSION, NULL );
	if ( !pShaderDLL )
	{
		g_pFullFileSystem->UnloadModule( hInstance );
		return false;
	}

	CShaderDLLInfo *pOut = &g_ShaderDLLs[ g_ShaderDLLs.AddToTail() ];
	pOut->m_pInternal = pShaderDLL;
	Q_strncpy( pOut->m_Filename, pFilename, sizeof( pOut->m_Filename ) );

	return true;
}


void PrintHeader( void )
{
	printf( "<HTML>\n" );
	printf( "<HEAD>\n" );
	printf( "<TITLE>Valve Source Shader Reference</TITLE>\n" );
	printf( "</HEAD>\n" );
	printf( "<CENTER>\n" );
	printf( "<H1>Valve Source Shader Reference</H1>\n" );
	printf( "</CENTER>\n" );
}

void PrintShaderContents( int dllID )
{
	IShaderDLLInternal *pShaderDLL = g_ShaderDLLs[dllID].m_pInternal;
	int nShaders = pShaderDLL->ShaderCount();
	int i;
	printf( "<H2>%s</H2><BR>\n", g_ShaderDLLs[dllID].m_Filename );
	printf( "<dl>\n" ); // define list
	for( i = 0; i < nShaders; i++ )
	{
		IShader *pShader = pShaderDLL->GetShader( i );
		printf( "<A HREF=\"#%s_%s\">\n", g_ShaderDLLs[dllID].m_Filename, pShader->GetName() );
		printf( "<dt>%s</A>\n", pShader->GetName() );
//		int nParams = pShader->GetNumParams();
	}
	printf( "</dl>\n" ); // end define list
}

void PrintShaderHelp( int dllID )
{
	IShaderDLLInternal *pShaderDLL = g_ShaderDLLs[dllID].m_pInternal;
	int nShaders = pShaderDLL->ShaderCount();
	int i;
	printf( "<H2>%s</H2><BR>\n", g_ShaderDLLs[dllID].m_Filename );
	printf( "<dl>\n" ); // define list
	for( i = 0; i < nShaders; i++ )
	{
		IShader *pShader = pShaderDLL->GetShader( i );
		printf( "<A NAME=\"%s_%s\"></A>\n", g_ShaderDLLs[dllID].m_Filename, pShader->GetName() );
		printf( "<dt>%s<dl>\n", pShader->GetName() );
		int nParams = pShader->GetNumParams();
		int j;
		for( j = 0; j < nParams; j++ )
		{
			printf( "<dt>%s\n<dd>%s\n", 
				pShader->GetParamName( j ),
				pShader->GetParamHelp( j )
				);
		}
		printf( "</dl><br>\n" ); // end define list
	}
	printf( "</dl>\n" ); // end define list
}

void PrintFooter( void )
{
	printf( "</HTML>\n" );
}

int main( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );
	FileSystem_Init( "" );
	PrintHeader();
	LoadShaderDLL( "stdshader_dx6.dll" );
	LoadShaderDLL( "stdshader_dx7.dll" );
	LoadShaderDLL( "stdshader_dx8.dll" );
	LoadShaderDLL( "stdshader_dx9.dll" );
	int i;
	for( i = 0; i < g_ShaderDLLs.Count(); i++ )
	{
		PrintShaderContents( i );
	}
	for( i = 0; i < g_ShaderDLLs.Count(); i++ )
	{
		PrintShaderHelp( i );
	}
	PrintFooter();
	FileSystem_Term();
	return 0;
}