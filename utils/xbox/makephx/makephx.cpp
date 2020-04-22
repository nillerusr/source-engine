//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include "physdll.h"
#include "vphysics/constraints.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "simplify.h"
#include "keyvalues.h"
#include "studio.h"

IPhysicsCollision *physcollision = NULL;
IPhysicsSurfaceProps *physprops = NULL;

int	g_TotalOut = 0;
int	g_TotalCompress = 0;
bool g_bRecursive = false;
bool g_bQuiet = false;

KeyValues *g_pModelConfig = NULL;

void InitFilesystem( const char *pPath )
{
	CmdLib_InitFileSystem( pPath );
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 
}

static bool LoadSurfaceProps( const char *pMaterialFilename )
{
	if ( !physprops )
		return false;

	FileHandle_t fp = g_pFileSystem->Open( pMaterialFilename, "rb", TOOLS_READ_PATH_ID );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return false;

	int len = g_pFileSystem->Size( fp );
	char *pText = new char[len+1];
	g_pFileSystem->Read( pText, len, fp );
	g_pFileSystem->Close( fp );

	pText[len]=0;

	physprops->ParseSurfaceData( pMaterialFilename, pText );

	delete[] pText;

	return true;
}

void LoadSurfacePropsAll()
{
	// already loaded
	if ( physprops->SurfacePropCount() )
		return;

	const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( g_pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				LoadSurfaceProps( sub->GetString() );
				continue;
			}
		}
	}

	manifest->deleteThis();
}
void InitVPhysics()
{
	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	physcollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
	physprops = (IPhysicsSurfaceProps *)physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL );
	LoadSurfacePropsAll();
}

struct phyfile_t
{
	phyheader_t		header;
	vcollide_t		collide;
	int				fileSize;
};

void LoadPHYFile(phyfile_t *pOut, const char *name)
{
	memset( pOut, 0, sizeof(*pOut) );
	FileHandle_t file = g_pFullFileSystem->Open( name, "rb" );
	if ( !file )
		return;

	g_pFullFileSystem->Read( &pOut->header, sizeof(pOut->header), file );
	if ( pOut->header.size != sizeof(pOut->header) || pOut->header.solidCount <= 0 )
		return;

	pOut->fileSize = g_pFullFileSystem->Size( file );

	char *buf = (char *)_alloca( pOut->fileSize );
	g_pFullFileSystem->Read( buf, pOut->fileSize, file );
	g_pFullFileSystem->Close( file );

	physcollision->VCollideLoad( &pOut->collide, pOut->header.solidCount, (const char *)buf, pOut->fileSize );
}


void OverrideDefaultsForModel( const char *keyname, simplifyparams_t &params )
{
	KeyValues *pKeys = g_pModelConfig;
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), keyname ) )
		{
			for ( KeyValues *pData = pKeys->GetFirstSubKey(); pData; pData = pData->GetNextKey() )
			{
				if ( !Q_stricmp( pData->GetName(), "tolerance" ) )
				{
					params.tolerance = pData->GetFloat();
					if (!g_bQuiet)
					{
						Msg("%s: tolerance set to %.2f\n", keyname, params.tolerance );
					}
				}
				else if ( !Q_stricmp( pData->GetName(), "addAABB" ) )
				{
					params.addAABBToSimplifiedHull = pData->GetInt() ? true : false;
					if (!g_bQuiet)
					{
						Msg("%s: AABB %s\n", keyname, params.addAABBToSimplifiedHull ? "on" : "off" );
					}
				}
				else if ( !Q_stricmp( pData->GetName(), "singleconvex" ) )
				{
					params.forceSingleConvex = pData->GetInt() ? true : false;
					if (!g_bQuiet)
					{
						Msg("%s: Forced to single convex\n", keyname );
					}
				}
				else if ( !Q_stricmp( pData->GetName(), "mergeconvex" ) )
				{
					params.mergeConvexTolerance = pData->GetFloat();
					params.mergeConvexElements = params.mergeConvexTolerance > 0 ? true : false;
					if (!g_bQuiet)
					{
						Msg("%s: Merge convex %.2f\n", keyname, params.mergeConvexTolerance );
					}
				}
			}
			return;
		}
		pKeys = pKeys->GetNextKey();
	}
}

bool HasMultipleBones( const char *pFilename )
{
	char outName[1024];
	studiohdr_t hdr;
	Q_strncpy( outName, pFilename, sizeof(outName) );
	Q_SetExtension( outName, ".mdl", sizeof(outName) );
	FileHandle_t fp = g_pFileSystem->Open( outName, "rb", TOOLS_READ_PATH_ID );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return false;

	g_pFileSystem->Read( &hdr, sizeof(hdr), fp );
	g_pFileSystem->Close( fp );
	if ( hdr.numbones > 1 )
		return true;
	return false;
}

void WritePHXFile( const char *pName, const phyfile_t &file )
{
	if ( file.header.size != sizeof(file.header) || file.collide.solidCount <= 0 )
		return;

	CUtlBuffer out;

	char outName[1024];
	Q_snprintf( outName, sizeof(outName), "%s", pName );
	Q_SetExtension( outName, ".phx", sizeof(outName) );

	simplifyparams_t params;
	params.Defaults();
	params.tolerance = (file.collide.solidCount > 1) ? 4.0f : 2.0f;
	// single solids constraint to AABB for placement help
	params.addAABBToSimplifiedHull = (file.collide.solidCount == 1) ? true : false;
	params.mergeConvexElements = true;
	params.mergeConvexTolerance = 0.025f;
	Q_FixSlashes(outName);
	Q_strlower(outName);
	char *pSearch = Q_strstr( outName,"models\\" );
	if ( pSearch )
	{
		char keyname[1024];
		pSearch += strlen("models\\");
		Q_StripExtension( pSearch, keyname, sizeof(keyname) );
		OverrideDefaultsForModel( keyname, params );
	}
	out.Put( &file.header, sizeof(file.header) );
	int outSize = 0;
	bool bStoreSolidNames = file.collide.solidCount > 1 ? true : false;
	bStoreSolidNames = bStoreSolidNames || HasMultipleBones(outName);

	vcollide_t *pNewCollide = ConvertVCollideToPHX( &file.collide, params, &outSize, false, bStoreSolidNames);
	g_TotalOut += file.fileSize;
	for ( int i = 0; i < pNewCollide->solidCount; i++ )
	{
		int collideSize = physcollision->CollideSize( pNewCollide->solids[i] );
		out.PutInt( collideSize );
		char *pMem = new char[collideSize];
		physcollision->CollideWrite( pMem, pNewCollide->solids[i] );
		out.Put( pMem, collideSize );
		delete[] pMem;
	}

	if (!g_bQuiet)
	{
		Msg("%s Compressed %d (%d text) to %d (%d text)\n", outName, file.fileSize, file.collide.descSize, out.TellPut(), pNewCollide->descSize );
	}
	out.Put( pNewCollide->pKeyValues, pNewCollide->descSize );
	g_TotalCompress += out.TellPut();

#if 0
	//Msg("OLD:\n-----------------------------------\n%s\n", file.collide.pKeyValues );
	CPackedPhysicsDescription *pPacked = physcollision->CreatePackedDesc( pNewCollide->pKeyValues, pNewCollide->descSize );
	Msg("NEW:\n-----------------------------------\n" );
	for ( int i = 0; i < pPacked->m_solidCount; i++ )
	{
		solid_t solid;
		pPacked->GetSolid( &solid, i );
		Msg("index %d\n", solid.index );
		Msg("name %s\n", solid.name );
		Msg("mass %.2f\n", solid.params.mass );
		Msg("surfaceprop %s\n", solid.surfaceprop);
		Msg("damping %.2f\n", solid.params.damping );
		Msg("rotdamping %.2f\n", solid.params.rotdamping );
		Msg("drag %.2f\n", solid.params.dragCoefficient );
		Msg("inertia %.2f\n", solid.params.inertia );
		Msg("volume %.2f\n", solid.params.volume );
	}
#endif
	DestroyPHX( pNewCollide );
	if ( !g_pFullFileSystem->WriteFile( outName, NULL, out ) )
		Warning("Can't write file: %s\n", outName );
}

void UnloadPHYFile( phyfile_t *pFile )
{
	physcollision->VCollideUnload( &pFile->collide );
	pFile->header.size = 0;
}

void MakeFilename( char *pDest, int destSize, const char *pPathname, const char *pFilenameExt )
{
	Q_strncpy(pDest, pPathname, destSize);
	Q_AppendSlash(pDest, destSize);
	Q_strncat(pDest, pFilenameExt, destSize);
}

void MakeDirname( char *pDest, int destSize, const char *pPathname, const char *pSubdir )
{
	MakeFilename(pDest, destSize , pPathname, pSubdir);
}

int main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		Msg("Usage:\nmakephx [options] <FILESPEC>\ne.g. makephx [-r] *.phy\n");
		return 0;
	}

	CommandLine()->CreateCmdLine( argc, argv );
	g_bRecursive = CommandLine()->FindParm("-r") > 0 ? true : false;
	g_bQuiet = CommandLine()->FindParm("-quiet") > 0 ? true : false;
	InitFilesystem( "*.*" );
	InitVPhysics();
	// disable automatic packing, we want to do this ourselves.
	physcollision->SetPackOnLoad( false );
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	InstallSpewFunction();

	g_pModelConfig = new KeyValues("config");
	g_pModelConfig->LoadFromFile( g_pFullFileSystem, "phx.cfg", "GAME" );
	g_TotalOut = 0;
	g_TotalCompress = 0;
	FileFindHandle_t handle;
	char fullpath[1024], currentFile[1024], dirName[1024], nameext[256];
	strcpy( fullpath, argv[argc-1] );
	strcpy( fullpath, ExpandPath( fullpath ) );
	strcpy( fullpath, ExpandArg( fullpath ) );
	Q_strncpy(dirName, fullpath, sizeof(dirName));
	Q_StripFilename(dirName);
	Q_strncpy(nameext, fullpath + strlen(dirName)+1, sizeof(nameext));
	CUtlVector< const char * > directoryList;
	directoryList.AddToTail( strdup(dirName) );
	int current = 0;
	int count = 0;
	do 
	{
		if ( g_bRecursive )
		{
			MakeFilename( currentFile, sizeof(currentFile), directoryList[current], "*.*" );
			const char *pFilename = g_pFullFileSystem->FindFirst( currentFile, &handle );
			while ( pFilename )
			{
				if ( pFilename[0] != '.' && g_pFullFileSystem->FindIsDirectory( handle ) )
				{
					MakeDirname( currentFile, sizeof(currentFile), directoryList[current], pFilename );
					directoryList.AddToTail(strdup(currentFile));
				}
				pFilename = g_pFullFileSystem->FindNext( handle );
			}
			g_pFullFileSystem->FindClose( handle );
		}

		MakeFilename(currentFile, sizeof(currentFile), directoryList[current], nameext);
		const char *pFilename = g_pFullFileSystem->FindFirst( currentFile, &handle );
		while ( pFilename )
		{
			phyfile_t phy;
			MakeFilename(currentFile, sizeof(currentFile), directoryList[current], pFilename);
			LoadPHYFile( &phy, currentFile );
			if ( phy.collide.isPacked || phy.collide.solidCount < 1 )
			{
				Msg("%s is not a valid PHY file\n", currentFile );
			}
			else
			{
				WritePHXFile( currentFile, phy );
				count++;
			}
			UnloadPHYFile( &phy );
			pFilename = g_pFullFileSystem->FindNext( handle );
		}
		g_pFullFileSystem->FindClose( handle );
		current++;
	} while( current < directoryList.Count() );

	if ( count )
	{
		if (!g_bQuiet)
		{
			Msg("\n------\nTotal %s, %s\nSaved %s\n", Q_pretifymem( g_TotalOut ), Q_pretifymem( g_TotalCompress ), Q_pretifymem( g_TotalOut - g_TotalCompress ) );
			Msg("%.2f%% savings\n", ((float)(g_TotalOut-g_TotalCompress) / (float)g_TotalOut) * 100.0f );
		}
	}
	else
	{
		Msg("No files found in %s!\n", directoryList[current] );
	}

	return 0;
}
