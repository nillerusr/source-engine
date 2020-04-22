//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cmdlib.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "physdll.h"
#include "phyfile.h"
#include "phxfile.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "vphysics_interface.h"
#include "vcollide_parse.h"
#include "vphysics/constraints.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "simplify.h"
#include "mathlib/compressed_vector.h"
#include "keyvalues.h"

IPhysicsCollision *physcollision = NULL;
IPhysicsSurfaceProps *physprops = NULL;

int	g_TotalOut = 0;
int	g_TotalCompress = 0;

void InitFilesystem( const char *pPath )
{
	CmdLib_InitFileSystem( pPath );
	//FileSystem_Init( ".", 0, FS_INIT_COMPATIBILITY_MODE );
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 
}

static bool LoadSurfaceProps( const char *pMaterialFilename )
{
	if ( !physprops )
		return false;

	// already loaded
	if ( physprops->SurfacePropCount() )
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



void WritePHXFile( const char *pName, const phyfile_t &file )
{
	if ( file.header.size != sizeof(file.header) || file.collide.solidCount <= 0 )
		return;

	CUtlBuffer out;

	char outName[1024];
	Q_snprintf( outName, sizeof(outName), "%s", pName );
	Q_SetExtension( outName, ".phx", sizeof(outName) );

	out.Put( &file.header, sizeof(file.header) );
	int outSize = 0;
	float tolerance = (file.collide.solidCount > 1) ? 3.0f : 7.0f;

	vcollide_t *pNewCollide = ConvertVCollideToPHX( &file.collide, tolerance, &outSize, false );
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

	Msg("%s Compressed %d (%d text) to %d (%d text)\n", outName, file.fileSize, file.collide.descSize, out.TellPut(), pNewCollide->descSize );
	out.Put( pNewCollide->pKeyValues, pNewCollide->descSize );
	g_TotalCompress += out.TellPut();

	Msg("OLD:\n-----------------------------------\n%s\n", file.collide.pKeyValues );
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

	if ( !g_pFullFileSystem->WriteFile( outName, NULL, out ) )
		Error("Error writing %s\n", outName );
}

void UnloadPHYFile( phyfile_t *pFile )
{
	physcollision->VCollideUnload( &pFile->collide );
	pFile->header.size = 0;
}

int main( int argc, char *argv[] )
{
	CommandLine()->CreateCmdLine( argc, argv );
	InstallSpewFunction();
	InitFilesystem( argv[argc-1] );
	InitVPhysics();
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	g_TotalOut = 0;
	g_TotalCompress = 0;
	FileFindHandle_t handle;
	char fullpath[1024], filebase[1024];
	strcpy( fullpath, argv[argc-1] );
	strcpy( fullpath, ExpandPath( fullpath ) );
	strcpy( fullpath, ExpandArg( fullpath ) );
	Q_FileBase(fullpath, filebase, sizeof(filebase));

	const char *pFilename = g_pFullFileSystem->FindFirst( argv[argc-1], &handle );
	while ( pFilename )
	{
#if 0
		if ( g_pFullFileSystem->FindIsDirectory( handle ) )
		{
		}
#endif
		g_pFullFileSystem->RelativePathToFullPath( pFilename, NULL, filebase, sizeof( filebase ) );

		phyfile_t phy;
		strcpy( filebase, ExpandPath( filebase ) );
		strcpy( filebase, ExpandArg( filebase ) );
		LoadPHYFile( &phy, filebase );
		WritePHXFile( filebase, phy );
		UnloadPHYFile( &phy );
		pFilename = g_pFullFileSystem->FindNext( handle );
	}
	g_pFullFileSystem->FindClose( handle );
	Msg("Total %s, %s\nSaved %s\n", Q_pretifymem( g_TotalOut ), Q_pretifymem( g_TotalCompress ), Q_pretifymem( g_TotalOut - g_TotalCompress ) );
	Msg("%.2f%% savings\n", ((float)(g_TotalOut-g_TotalCompress) / (float)g_TotalOut) * 100.0f );

	return 0;
}
