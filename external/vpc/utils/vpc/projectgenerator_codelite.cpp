//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "projectgenerator_codelite.h"

#ifdef WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#define getcwd _getcwd
#endif

static const char *k_pchSource = "Source Files";
static const char *k_pchHeaders = "Header Files";
static const char *k_pchResources = "Resources";
static const char *k_pchVPCFiles = "VPC Files";

void CProjectGenerator_CodeLite::GenerateCodeLiteProject( CBaseProjectDataCollector *pCollector, const char *pOutFilename, const char *pMakefileFilename ) 
{
	char szProjectFile[MAX_PATH];
	sprintf( szProjectFile, "%s.project", pOutFilename );

	g_pVPC->VPCStatus( true, "Saving CodeLite project for: '%s' File: '%s'", pCollector->GetProjectName().String(), szProjectFile );

	m_fp = fopen( szProjectFile, "wt" );

	m_nIndent = 0;
	m_pCollector = pCollector;
	m_pMakefileFilename = pMakefileFilename;

	Write( "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" );
	Write( "<CodeLite_Project Name=\"%s\" InternalType=\"\">\n", pCollector->GetProjectName().String() );
	{
		++m_nIndent;
		Write( "<Description/>\n" );
		Write( "<Dependencies/>\n" );
		Write( "<Settings Type=\"Dynamic Library\">\n" );
		{
			++m_nIndent;
			Write( "<GlobalSettings>\n" );
			++m_nIndent;
			Write( "<Compiler Options=\"\" C_Options=\"\">\n" );
			++m_nIndent;
			Write( "<IncludePath Value=\"\"/>\n" );
			--m_nIndent;
			Write( "</Compiler>\n" );
			Write( "<Linker Options=\"\">\n" );
			++m_nIndent;
			Write( "<LibraryPath Value=\"\"/>\n" );
			--m_nIndent;
			Write( "</Linker>\n" );
			Write( "<ResourceCompiler Options=\"\"/>\n" );
			--m_nIndent;
			Write( "</GlobalSettings>\n" );
			Write( "<Configuration Name=\"Debug\" CompilerType=\"gnu g++\" DebuggerType=\"GNU gdb debugger\" Type=\"Dynamic Library\" BuildCmpWithGlobalSettings=\"append\" BuildLnkWithGlobalSettings=\"append\" BuildResWithGlobalSettings=\"append\">\n" );
			{
				++m_nIndent;
				Write( "<CustomBuild Enabled=\"yes\">\n" );
				{
					++m_nIndent;
					Write( "<RebuildCommand>make CFG=debug -f %s clean all</RebuildCommand>\n", pMakefileFilename );
					Write( "<CleanCommand>make CFG=debug -f %s clean</CleanCommand>\n", pMakefileFilename );
					Write( "<BuildCommand>make CFG=debug -f %s -j `getconf _NPROCESSORS_ONLN`</BuildCommand>\n", pMakefileFilename );
					Write( "<WorkingDirectory>$(ProjectPath)</WorkingDirectory>\n" );
					--m_nIndent;
				}
				Write( "</CustomBuild>\n" );
				--m_nIndent;
			}
			Write( "</Configuration>\n" );

			Write( "<Configuration Name=\"Release\" CompilerType=\"gnu g++\" DebuggerType=\"GNU gdb debugger\" Type=\"Dynamic Library\" BuildCmpWithGlobalSettings=\"append\" BuildLnkWithGlobalSettings=\"append\" BuildResWithGlobalSettings=\"append\">\n" );
			{
				++m_nIndent;
				Write( "<CustomBuild Enabled=\"yes\">\n" );
				{
					++m_nIndent;
					Write( "<RebuildCommand>make -f %s clean all</RebuildCommand>\n", pMakefileFilename );
					Write( "<CleanCommand>make -f %s clean</CleanCommand>\n", pMakefileFilename );
					Write( "<BuildCommand>make -f %s -j `getconf _NPROCESSORS_ONLN`</BuildCommand>\n", pMakefileFilename );
					Write( "<WorkingDirectory>$(ProjectPath)</WorkingDirectory>\n" );
					--m_nIndent;
				}
				Write( "</CustomBuild>\n" );
				--m_nIndent;
			}
			Write( "</Configuration>\n" );			
			--m_nIndent;
		}
		Write( "</Settings>\n" );

		{
			++m_nIndent;
			WriteFilesFolder( k_pchSource, "*.c;*.C;*.cc;*.cpp;*.cp;*.cxx;*.c++;*.prg;*.pas;*.dpr;*.asm;*.s;*.bas;*.java;*.cs;*.sc;*.e;*.cob;*.html;*.rc;*.tcl;*.py;*.pl;*.m;*.mm" );
			WriteFilesFolder( k_pchHeaders, "*.h;*.H;*.hh;*.hpp;*.hxx;*.inc;*.sh;*.cpy;*.if" );
			WriteFilesFolder( k_pchResources, "*.plist;*.strings;*.xib" );
			WriteFilesFolder( k_pchVPCFiles, "*.vpc" );
			--m_nIndent;
		}
		--m_nIndent;
	}
	Write( "</CodeLite_Project>\n" );
	fclose( m_fp );
}

void CProjectGenerator_CodeLite::WriteFilesFolder( const char *pFolderName, const char *pExtensions ) {
	CUtlVector<char*> extensions;
	V_SplitString( pExtensions, ";", extensions );

	Write( "<VirtualDirectory Name=\"%s\">\n", pFolderName );
	{
		++m_nIndent;
		for ( int i=m_pCollector->m_Files.First(); i != m_pCollector->m_Files.InvalidIndex(); i=m_pCollector->m_Files.Next( i ) ) {
			const char *pFilename = m_pCollector->m_Files[i]->GetName();

			// Make sure this file's extension is one of the extensions they're asking for.
			bool bValidExt = false;
			const char *pFileExtension = V_GetFileExtension( pFilename );
			if ( pFileExtension ) {
				for ( int iExt=0; iExt < extensions.Count(); iExt++ ) {
					const char *pTestExt = extensions[iExt];

					if ( pTestExt[0] == '*' && pTestExt[1] == '.' && V_stricmp( pTestExt+2, pFileExtension ) == 0 ) {
						bValidExt = true;
						break;
					}
				}
			}

			if ( bValidExt ) {
				char sFixedSlashes[MAX_PATH];
				V_strncpy( sFixedSlashes, pFilename, sizeof( sFixedSlashes ) );
				Write( "<File Name=\"%s\"/>\n", sFixedSlashes );
			}
		}
		--m_nIndent;
	}
	Write( "</VirtualDirectory>\n");
}

void CProjectGenerator_CodeLite::Write( const char *pMsg, ... ) {
	char sOut[8192];

	va_list marker;
	va_start( marker, pMsg );
	V_vsnprintf( sOut, sizeof( sOut ), pMsg, marker );
	va_end( marker );

	for ( int i=0; i < m_nIndent; i++ )
		fprintf( m_fp, "  " );

	fprintf( m_fp, "%s", sOut );
}
