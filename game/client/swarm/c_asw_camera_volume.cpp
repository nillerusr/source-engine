#include "cbase.h"
#include "c_asw_camera_volume.h"
#include "mapentities_shared.h"
#include "datacache/imdlcache.h"
#include "gamestringpool.h"

CUtlVector<C_ASW_Camera_Volume*>	g_ASWCameraVolumes;

C_ASW_Camera_Volume::C_ASW_Camera_Volume()
{
	g_ASWCameraVolumes.AddToTail(this);
	m_fCameraPitch = 90;
}

C_ASW_Camera_Volume::~C_ASW_Camera_Volume()
{
	g_ASWCameraVolumes.FindAndRemove(this);
}

C_ASW_Camera_Volume *C_ASW_Camera_Volume::CreateNew( bool bForce )
{
	return new C_ASW_Camera_Volume();
}

void C_ASW_Camera_Volume::Spawn()
{
	SetSolid( SOLID_BBOX );
	SetModel( STRING( GetModelName() ) );    // set size and link into world

	BaseClass::Spawn();

	m_takedamage = DAMAGE_NO;	
}

bool C_ASW_Camera_Volume::Initialize()
{
	if ( InitializeAsClientEntity( NULL, false ) == false )
	{
		return false;
	}
	
	Spawn();

	const model_t *mod = GetModel();
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetCollisionBounds( mins, maxs );
	}

	SetBlocksLOS( false ); // this should be a small object
	SetNextClientThink( CLIENT_THINK_NEVER );

	return true;
}

float C_ASW_Camera_Volume::IsPointInCameraVolume(const Vector &src)
{
	int c=g_ASWCameraVolumes.Count();
	for (int i=0;i<c;i++)
	{
		if (g_ASWCameraVolumes[i]->CollisionProp()->IsPointInBounds(src))
			return g_ASWCameraVolumes[i]->m_fCameraPitch;
	}
	return -1;
}

void C_ASW_Camera_Volume::RecreateAll()
{
	DestroyAll();	
	ParseAllEntities( engine->GetMapEntitiesString() );
}

void C_ASW_Camera_Volume::DestroyAll()
{
	while (g_ASWCameraVolumes.Count() > 0 )
	{
		C_ASW_Camera_Volume *p = g_ASWCameraVolumes[0];
		p->Release();
	}
}

const char *C_ASW_Camera_Volume::ParseEntity( const char *pEntData )
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];
	
	MDLCACHE_CRITICAL_SECTION();

	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}

	if ( !Q_strcmp( className, "asw_camera_control" ) )
	{
		// always force clientside entities placed in maps
		C_ASW_Camera_Volume *pEntity = C_ASW_Camera_Volume::CreateNew( true ); 

		if ( pEntity )
		{	// Set up keyvalues.
			pEntity->ParseMapData(&entData);
			
			if ( !pEntity->Initialize() )
				pEntity->Release();
		
			return entData.CurrentBufferPosition();
		}
	}
	
	// Just skip past all the keys.
	char keyName[MAPKEY_MAXLENGTH];
	char value[MAPKEY_MAXLENGTH];
	if ( entData.GetFirstKey(keyName, value) )
	{
		do 
		{
		} 
		while ( entData.GetNextKey(keyName, value) );
	}

	//
	// Return the current parser position in the data block
	//
	return entData.CurrentBufferPosition();
}

bool C_ASW_Camera_Volume::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "model" ) )
	{
		SetModelName( AllocPooledString( szValue ) );
		return true;
	}
	if ( FStrEq( szKeyName, "angtype" ) )
	{		
		int iAngleType = atoi(szValue);
		if (iAngleType == 0)	// 0 is top down
		{
			m_fCameraPitch = 90.0f;
		}
		else	// 1 is 40 degree
		{
			m_fCameraPitch = 40.0f;
		}
		return true;
	}
	return BaseClass::KeyValue(szKeyName, szValue);
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void C_ASW_Camera_Volume::ParseAllEntities(const char *pMapData)
{
	int nEntities = 0;

	char szTokenBuffer[MAPKEY_MAXLENGTH];

	//
	//  Loop through all entities in the map data, creating each.
	//
	for ( ; true; pMapData = MapEntity_SkipToNextEntity(pMapData, szTokenBuffer) )
	{
		//
		// Parse the opening brace.
		//
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );

		//
		// Check to see if we've finished or not.
		//
		if (!pMapData)
			break;

		if (token[0] != '{')
		{
			Error( "C_ASW_Camera_Volume::ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		//
		// Parse the entity and add it to the spawn list.
		//

		pMapData = ParseEntity( pMapData );

		nEntities++;
	}
}

bool C_ASW_Camera_Volume::ShouldDraw()
{
	return false;
}