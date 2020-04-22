//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "stdafx.h"
#include "filesystem_tools.h"
#include "KeyValues.h"
#include "physdll.h"
#include "materialsystem/imesh.h"
#include "utlvector.h"

char g_szAppName[] = "VPhysics perf test";
bool g_bCaptureOnFocus = false;

IPhysics			*physics = NULL;
IPhysicsCollision	*physcollision = NULL;
IPhysicsSurfaceProps *physprops = NULL;
IMaterial *g_materialFlatshaded = NULL;
IMaterial *g_pWireframeMaterial = NULL;
int gKeys[256];

const objectparams_t g_PhysDefaultObjectParams =
{
	NULL,
	1.0f, //mass
	1.0f, // inertia
	0.0f, // damping
	0.0f, // rotdamping
	0.05f, // rotIntertiaLimit
	"DEFAULT",
	NULL,// game data
	0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f, // drag coefficient
	true,// enable collisions?
};


void AddSurfacepropFile( const char *pFileName, IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	// Load file into memory
	FileHandle_t file = pFileSystem->Open( pFileName, "rb" );

	if ( file )
	{
		int len = pFileSystem->Size( file );

		// read the file
		char *buffer = (char *)stackalloc( len+1 );
		pFileSystem->Read( buffer, len, file );
		pFileSystem->Close( file );
		buffer[len] = 0;
		pProps->ParseSurfaceData( pFileName, buffer );
		// buffer is on the stack, no need to free
	}
}

void PhysParseSurfaceData( IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSurfacepropFile( sub->GetString(), pProps, pFileSystem );
				continue;
			}

			Warning( "surfaceprops::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SURFACEPROP_MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SURFACEPROP_MANIFEST_FILE );
	}

	manifest->deleteThis();
}

struct physics_test_object_t
{
	IPhysicsObject *pPhysics;
	ICollisionQuery *pModel;
};

struct physicstest_t
{
	IPhysicsEnvironment *physenv;
	CUtlVector<physics_test_object_t> list;

	void Clear()
	{
		physenv->SetQuickDelete( true );
		for ( int i = 0; i < list.Count(); i++ )
		{
			physcollision->DestroyQueryModel( list[i].pModel );
			physenv->DestroyObject( list[i].pPhysics );
		}
		list.Purge();
		physics->DestroyEnvironment( physenv );
	}
	void InitEnvironment()
	{
		physenv = physics->CreateEnvironment();
		//g_EntityCollisionHash = physics->CreateObjectPairHash();
		physenv->EnableDeleteQueue( true );

		//physenv->SetCollisionSolver( &g_Collisions );
		//physenv->SetCollisionEventHandler( &g_Collisions );
		//physenv->SetConstraintEventHandler( g_pConstraintEvents );
		//physenv->SetObjectEventHandler( &g_Objects );
		
		physenv->SetSimulationTimestep( DEFAULT_TICK_INTERVAL ); // 15 ms per tick
		// HL Game gravity, not real-world gravity
		physenv->SetGravity( Vector( 0, 0, -600.0f ) );
		physenv->SetAirDensity( 0.5f );
	}

	int AddObject( IPhysicsObject *pObject )
	{
		int index = list.AddToTail();
		list[index].pPhysics = pObject;
		list[index].pModel = physcollision->CreateQueryModel( (CPhysCollide *)pObject->GetCollide() );
		return index;
	}

	void CreateGround( float size )
	{
		{
			CPhysCollide *pCollide = physcollision->BBoxToCollide( Vector(-size,-size,-24), Vector(size,size,0) );
			objectparams_t params = g_PhysDefaultObjectParams;
			IPhysicsObject *pGround = physenv->CreatePolyObjectStatic( pCollide, physprops->GetSurfaceIndex( "default" ), vec3_origin, vec3_angle, &params );
			AddObject( pGround );
		}

		for ( int i = 0; i < 20; i++ )
		{
			CPhysCollide *pCollide = physcollision->BBoxToCollide( Vector(-24,-24,-24), Vector(24,24,24) );
			objectparams_t params = g_PhysDefaultObjectParams;
			params.mass = 150.0f;
			IPhysicsObject *pGround = physenv->CreatePolyObject( pCollide, physprops->GetSurfaceIndex( "default" ), Vector(64*(i%4),64 * (i%5),1024), vec3_angle, &params );
			AddObject( pGround );
			pGround->Wake();
		}
	}

	void Explode( const Vector &origin, float force )
	{
		for ( int i = 0; i < list.Count(); i++ )
		{
			if ( !list[i].pPhysics->IsMoveable() )
				continue;

			Vector pos, dir;
			list[i].pPhysics->GetPosition( &pos, NULL );
			dir = pos - origin;
			dir.z += 10;
			VectorNormalize( dir );
			list[i].pPhysics->ApplyForceCenter( dir * force );
		}
	}
	void RandomColor( float *color, int key )
	{
		static bool first = true;
		static colorVec colors[256];

		if ( first )
		{
			int r, g, b;
			first = false;
			for ( int i = 0; i < 256; i++ )
			{
				do 
				{
					r = rand()&255;
					g = rand()&255;
					b = rand()&255;
				} while ( (r+g+b)<256 );
				colors[i].r = r;
				colors[i].g = g;
				colors[i].b = b;
				colors[i].a = 255;
			}
		}

		int index = key & 255;
		color[0] = colors[index].r * (1.f / 255.f);
		color[1] = colors[index].g * (1.f / 255.f);
		color[2] = colors[index].b * (1.f / 255.f);
		color[3] = colors[index].a * (1.f / 255.f);
	}

	void DrawObject( ICollisionQuery *pModel, IMaterial *pMaterial, IPhysicsObject *pObject )
	{
		matrix3x4_t matrix;
		pObject->GetPositionMatrix( &matrix );
		CMatRenderContextPtr pRenderContext(g_MaterialSystemApp.m_pMaterialSystem);
		pRenderContext->Bind( pMaterial );

		int vertIndex = 0;
		for ( int i = 0; i < pModel->ConvexCount(); i++ )
		{
			float color[4];
			RandomColor( color, i + (int)pObject );
			IMesh* pMatMesh = pRenderContext->GetDynamicMesh( );
			CMeshBuilder meshBuilder;
			int triCount = pModel->TriangleCount( i );
			meshBuilder.Begin( pMatMesh, MATERIAL_TRIANGLES, triCount );

			for ( int j = 0; j < triCount; j++ )
			{
				Vector objectSpaceVerts[3];
				pModel->GetTriangleVerts( i, j, objectSpaceVerts );

				for ( int k = 0; k < 3; k++ )
				{
					Vector v;
					
					VectorTransform (objectSpaceVerts[k], matrix, v);
					meshBuilder.Position3fv( v.Base() );
					meshBuilder.Color4fv( color );
					meshBuilder.AdvanceVertex();
				}
			}
			meshBuilder.End( false, true );
		}
	}

	void Draw()
	{
		for ( int i = 0; i < list.Count(); i++ )
		{
			DrawObject( list[i].pModel, g_materialFlatshaded, list[i].pPhysics );
		}
	}
	void Simulate( float frametime )
	{
		physenv->Simulate( frametime );
	}
};

physicstest_t staticTest;

void AppInit( void )
{
	memset( gKeys, 0, sizeof(gKeys) );
	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	if (!(physics = (IPhysics *)physicsFactory( VPHYSICS_INTERFACE_VERSION, NULL )) ||
		!(physcollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL )) ||
		!(physprops = (IPhysicsSurfaceProps *)physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL )) )
	{
		return;
	}

	PhysParseSurfaceData( physprops, g_pFullFileSystem );
	g_materialFlatshaded = g_MaterialSystemApp.m_pMaterialSystem->FindMaterial("debug/debugdrawflatpolygons", TEXTURE_GROUP_OTHER, true);
	g_pWireframeMaterial = g_MaterialSystemApp.m_pMaterialSystem->FindMaterial("shadertest/wireframevertexcolor", TEXTURE_GROUP_OTHER);
	staticTest.InitEnvironment();
	staticTest.CreateGround( 1024 );
}

void FPSControls( float frametime, float mouseDeltaX, float mouseDeltaY, Vector& cameraPosition, QAngle& cameraAngles, float speed )
{
	cameraAngles[1] -= mouseDeltaX;
	cameraAngles[0] -= mouseDeltaY;

	if ( cameraAngles[0] < -85 )
		cameraAngles[0] = -85;
	if ( cameraAngles[0] > 85 )
		cameraAngles[0] = 85;

	Vector forward, right, up;
	AngleVectors( cameraAngles, &forward, &right, &up );

	if ( gKeys[ 'W' ] )
		VectorMA( cameraPosition, frametime * speed, forward, cameraPosition );
	if ( gKeys[ 'S' ] )
		VectorMA( cameraPosition, -frametime * speed, forward, cameraPosition );
	if ( gKeys[ 'A' ] )
		VectorMA( cameraPosition, -frametime * speed, right, cameraPosition );
	if ( gKeys[ 'D' ] )
		VectorMA( cameraPosition, frametime * speed, right, cameraPosition );
}


void SetupCamera( Vector& cameraPosition, QAngle& cameraAngles )
{
	CMatRenderContextPtr pRenderContext(g_MaterialSystemApp.m_pMaterialSystem);
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );
	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( 90,  0, 0, 1 );

    pRenderContext->Rotate( -cameraAngles[2],  1, 0, 0);	// roll
	pRenderContext->Rotate( -cameraAngles[0],  0, 1, 0);	// pitch
    pRenderContext->Rotate( -cameraAngles[1],  0, 0, 1);	// yaw

    pRenderContext->Translate( -cameraPosition[0],  -cameraPosition[1],  -cameraPosition[2] );
}

static Vector cameraPosition = Vector(0,0,128);
static QAngle cameraAngles = vec3_angle;

void AppRender( float frametime, float mouseDeltaX, float mouseDeltaY )
{
	FPSControls( frametime, mouseDeltaX, mouseDeltaY, cameraPosition, cameraAngles, 300 );
	SetupCamera( cameraPosition, cameraAngles );

	staticTest.Simulate( frametime );
	staticTest.Draw();
}

void AppExit( void )
{
	staticTest.Clear();

	//physics->DestroyObjectPairHash( g_EntityCollisionHash );
	//g_EntityCollisionHash = NULL;
	physics->DestroyAllCollisionSets();
}

void AppKey( int key, int down )
{
	gKeys[ key & 255 ] = down;
}


void AppChar( int key )
{
	if ( key == ' ' )
	{
		staticTest.Explode( cameraPosition, 150 * 100 );
	}
}

