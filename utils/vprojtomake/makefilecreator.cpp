//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
// 
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h" 
#include "makefilecreator.h" 
#include "cmdlib.h"

//-----------------------------------------------------------------------------
// Purpose:  constructor
//-----------------------------------------------------------------------------
CMakefileCreator::CMakefileCreator() {
	m_FileToBaseDirMapping.SetLessFunc( DefLessFunc( int ) );
}

//-----------------------------------------------------------------------------
// Purpose:  destructor
//-----------------------------------------------------------------------------
CMakefileCreator::~CMakefileCreator()
{
}

void CMakefileCreator::CreateMakefiles( CVCProjConvert & proj )
{
	m_ProjName = proj.GetName();
	m_BaseDir = proj.GetBaseDir();	
	for ( int i = 0; i < proj.GetNumConfigurations(); i++ )
	{
		m_FileToBaseDirMapping.RemoveAll();
		m_BuildDirectories.RemoveAll();
		m_BaseDirs.RemoveAll();

		CreateMakefileName( proj.GetName().String(), proj.GetConfiguration(i) );
		CreateBaseDirs( proj.GetConfiguration(i) );
		FileHandle_t f = g_pFileSystem->Open( m_MakefileName.String(), "w+" );
		if ( !f )
		{
			Warning( "failed to open %s for writing.\n", m_MakefileName.String() );
			continue;
		}
		OutputDirs(f);
		OutputIncludes( proj.GetConfiguration(i), f );
		OutputObjLists( proj.GetConfiguration(i), f );
		OutputMainBuilder(f);
		OutputBuildTarget(f);
		g_pFileSystem->Close(f);
	}
}


void CMakefileCreator::CreateBaseDirs( CVCProjConvert::CConfiguration & config )
{
//	m_BaseDirs.Insert( "" );
	for ( int i = 0; i < config.GetNumFileNames(); i++ )
	{
		if ( config.GetFileType(i) == CVCProjConvert::CConfiguration::FILE_SOURCE ) 
		{
			char basedir[ MAX_PATH ];
			char fulldir[ MAX_PATH ];
			Q_snprintf( fulldir, sizeof(fulldir), "%s/%s", m_BaseDir.String(), config.GetFileName(i) );
			if ( Q_ExtractFilePath( fulldir, basedir, sizeof(basedir) ) )	
			{
				Q_FixSlashes( basedir );
				Q_StripTrailingSlash( basedir );
				int index =  m_BaseDirs.Find( basedir );
				if ( index == m_BaseDirs.InvalidIndex() )
				{
					index = m_BaseDirs.Insert( basedir );
				}
				m_FileToBaseDirMapping.Insert(i, index );
			}
			else
			{
				m_FileToBaseDirMapping.Insert(i, 0 );
			}
		}
	}
}

void CMakefileCreator::CleanupFileName( char *name )
{
	for ( int i = Q_strlen( name ) - 1; i >= 0; --i )
	{
		if ( name[i] == ' ' ||  name[i] == '|' || name[i] == '\\' || name[i] == '/' || ( name[i] == '.' && i>=1 && name[i-1] == '.' ))
		{
			Q_memmove( &name[i], &name[i+1], Q_strlen( name ) - i - 1 );
			name[ Q_strlen( name ) - 1 ] = 0;
		}
	}
}

void CMakefileCreator::CreateMakefileName( const char *projectName, CVCProjConvert::CConfiguration & config )
{
	char makefileName[ MAX_PATH ];
	Q_snprintf( makefileName, sizeof(makefileName), "Makefile.%s_%s", projectName, config.GetName().String() );
	CleanupFileName( makefileName );
	m_MakefileName = makefileName;
}

void CMakefileCreator::CreateDirectoryFriendlyName( const char *dirName, char *friendlyDirName, int friendlyDirNameSize )
{
	Q_strncpy( friendlyDirName, dirName, friendlyDirNameSize );

	int i;
	for ( i = Q_strlen( friendlyDirName ) - 1; i >= 0; --i )
	{
		if ( friendlyDirName[i] == '/' || friendlyDirName[i] == '\\' )
		{
			friendlyDirName[i] = '_';
		}
		if ( isalpha( friendlyDirName[i] ) )
		{
			friendlyDirName[i] = toupper(friendlyDirName[i]);
		}
		if ( friendlyDirName[i] == '.' )
		{
			Q_memmove( &friendlyDirName[i], &friendlyDirName[i+1], Q_strlen( friendlyDirName ) - i - 1 );
			friendlyDirName[ Q_strlen( friendlyDirName ) - 1 ] = 0;
		}
	}

	// strip any leading/trailing underscores
	while ( friendlyDirName[0] == '_' && Q_strlen(friendlyDirName)>0 )
	{
		Q_memmove( &friendlyDirName[0], &friendlyDirName[1], Q_strlen( friendlyDirName )- 1 );
		friendlyDirName[ Q_strlen( friendlyDirName ) - 1 ] = 0;
	}
	while ( Q_strlen(friendlyDirName)>0 && friendlyDirName[Q_strlen(friendlyDirName)-1] == '_'  )
	{
		friendlyDirName[ Q_strlen( friendlyDirName ) - 1 ] = 0;
	}

	CleanupFileName( friendlyDirName );
}

void CMakefileCreator::CreateObjDirectoryFriendlyName ( char *name )
{
#ifdef _WIN32
	char *updir = "..\\";
#else
	char *updir = "../";
#endif

	char *sep = Q_strstr( name, updir );	
	while ( sep )
	{
		Q_strcpy( sep, sep + strlen(updir) );
		sep = Q_strstr( sep, updir );	
	}
}

void CMakefileCreator::FileWrite( FileHandle_t f, const char *fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	char stringBuf[ 4096 ];
	Q_vsnprintf( stringBuf, sizeof(stringBuf), fmt, args );
	va_end(args);
	g_pFileSystem->Write( stringBuf, Q_strlen(stringBuf), f );
}

void CMakefileCreator::OutputIncludes( CVCProjConvert::CConfiguration & config, FileHandle_t f )
{
	FileWrite( f, "INCLUDES=" );
	for ( int i = 0; i < config.GetNumIncludes(); i++ )
	{
		FileWrite( f, "-I%s ", config.GetInclude(i) );
	}

	for ( int i = 0; i < config.GetNumDefines(); i++ )
	{
		FileWrite( f, "-D%s ", config.GetDefine(i) );
	}
	FileWrite( f, "\n\n" );
}

void CMakefileCreator::OutputDirs( FileHandle_t f )
{
	for ( int i = m_BaseDirs.First(); i != m_BaseDirs.InvalidIndex(); i = m_BaseDirs.Next(i) )
	{
		const char *dirName = m_BaseDirs.GetElementName(i);
		if ( !dirName || !Q_strlen(dirName) )
		{
			dirName = m_BaseDir.String();
		}

		char friendlyDirName[ MAX_PATH ];
		CreateDirectoryFriendlyName( dirName, friendlyDirName, sizeof(friendlyDirName) );
		int dirLen = Q_strlen(friendlyDirName);
		Q_strncat( friendlyDirName, "_SRC_DIR", sizeof(friendlyDirName), COPY_ALL_CHARACTERS );
		struct OutputDirMapping_t dirs;
		dirs.m_SrcDir = friendlyDirName;
		dirs.m_iBaseDirIndex = i;
		friendlyDirName[ dirLen ] = 0;
		Q_strncat( friendlyDirName, "_OBJ_DIR", sizeof(friendlyDirName), COPY_ALL_CHARACTERS );
		dirs.m_ObjDir = friendlyDirName;		
		friendlyDirName[ dirLen ] = 0;
		Q_strncat( friendlyDirName, "_OBJS", sizeof(friendlyDirName), COPY_ALL_CHARACTERS );
		dirs.m_ObjName = friendlyDirName;		
		
		char objDirName[ MAX_PATH ];
		Q_snprintf(  objDirName, sizeof(objDirName) , "obj%c$(NAME)_$(ARCH)%c", CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR );
		Q_strncat( objDirName, dirName, sizeof(objDirName), COPY_ALL_CHARACTERS );
		CreateObjDirectoryFriendlyName( objDirName );
		dirs.m_ObjOutputDir = objDirName;

		m_BuildDirectories.AddToTail( dirs );

		FileWrite( f, "%s=%s\n", dirs.m_SrcDir.String(), dirName );
		FileWrite( f, "%s=%s\n", dirs.m_ObjDir.String(), objDirName );
	}
	FileWrite( f, "\n\n" );
}

void CMakefileCreator::OutputMainBuilder( FileHandle_t f )
{
	int i;
	FileWrite( f, "\n\nall: dirs $(NAME)_$(ARCH).$(SHLIBEXT)\n\n" );
	FileWrite( f, "dirs:\n"  );
	for ( i = 0; i < m_BuildDirectories.Count(); i++ )
	{
		FileWrite( f, "\t-mkdir -p $(%s)\n", m_BuildDirectories[i].m_ObjDir.String()  );
	}
	FileWrite( f, "\n\n"  );


	FileWrite( f, "\n\n$(NAME)_$(ARCH).$(SHLIBEXT): " );
	for ( i = 0; i < m_BuildDirectories.Count(); i++ )
	{
		FileWrite( f, "$(%s) ", m_BuildDirectories[i].m_ObjName.String()  );
	}
	FileWrite( f, "\n\t$(CLINK) $(SHLIBLDFLAGS) $(DEBUG) -o $(BUILD_DIR)/$@ "  );
	for ( i = 0; i < m_BuildDirectories.Count(); i++ )
	{
		FileWrite( f, "$(%s) ", m_BuildDirectories[i].m_ObjName.String()  );
	}
	FileWrite( f, "$(LDFLAGS) $(CPP_LIB)\n\n"  );
}

void CMakefileCreator::OutputObjLists( CVCProjConvert::CConfiguration & config, FileHandle_t f )
{
	for ( int buildDirIndex = 0; buildDirIndex < m_BuildDirectories.Count(); buildDirIndex++ )
	{
		struct OutputDirMapping_t & dirs =  m_BuildDirectories[buildDirIndex];
		FileWrite( f, "%s= \\\n", dirs.m_ObjName.String() );

		for ( int j = m_FileToBaseDirMapping.FirstInorder(); j != m_FileToBaseDirMapping.InvalidIndex(); j = m_FileToBaseDirMapping.NextInorder(j) )
		{
			if ( dirs.m_iBaseDirIndex == m_FileToBaseDirMapping[j] )
			{
				char baseName[ MAX_PATH ];
				const char *fileName = config.GetFileName(m_FileToBaseDirMapping.Key(j));
				Q_FileBase( fileName, baseName, sizeof(baseName) );
				Q_SetExtension( baseName, ".o",  sizeof(baseName) );

				FileWrite( f, "\t$(%s)/%s \\\n", dirs.m_ObjDir.String(), baseName  );
			}
		}
		FileWrite( f, "\n\n" );
	}

}

void CMakefileCreator::OutputBuildTarget( FileHandle_t f )
{
	for( int i = 0; i < m_BuildDirectories.Count(); i++ )
	{
		struct OutputDirMapping_t & dirs = m_BuildDirectories[i];
		FileWrite( f, "$(%s)/%%.o: $(%s)/%%.cpp\n", dirs.m_ObjDir.String(), dirs.m_SrcDir.String()  );
		FileWrite( f, "\t$(DO_CC)\n\n");
	}
	
}

