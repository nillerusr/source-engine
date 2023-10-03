#include "cbase.h"
#include "c_asw_snow_volume.h"
#include "c_asw_snow_emitter.h"
#include "mapentities_shared.h"
#include "datacache/imdlcache.h"
#include "gamestringpool.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"

CUtlVector<C_ASW_Snow_Volume*>	g_ASWSnowVolumes;
CSmartPtr<CASWSnowEmitter> g_hSnowEmitter = NULL;
CSmartPtr<CASWSnowEmitter> g_hSnowCloudEmitter = NULL;


C_ASW_Snow_Volume::C_ASW_Snow_Volume()
{
	m_iSnowType = 1;	
	m_fCameraPitch = 90;
}

C_ASW_Snow_Volume::~C_ASW_Snow_Volume()
{
	g_ASWSnowVolumes.FindAndRemove(this);
	if (g_ASWSnowVolumes.Count() <= 0)
	{
		g_hSnowEmitter = NULL;
		g_hSnowCloudEmitter = NULL;
	}
}

C_ASW_Snow_Volume *C_ASW_Snow_Volume::CreateNew( bool bForce )
{
	return new C_ASW_Snow_Volume();
}

void C_ASW_Snow_Volume::Spawn()
{
	SetSolid( SOLID_BBOX );
	SetModel( STRING( GetModelName() ) );    // set size and link into world

	if (g_ASWSnowVolumes.Count() <= 0)
	{
		// we're the first snow volume, so create our snow emitter
		if (!g_hSnowEmitter.IsValid())
		{
			Msg("Created snow emitter\n");
			g_hSnowEmitter = CASWSnowEmitter::Create( "asw_emitter" );
			if ( g_hSnowEmitter.IsValid() )
			{
				if (m_iSnowType == 0)
					g_hSnowEmitter->UseTemplate("snow2");
				else
				{
					g_hSnowEmitter->UseTemplate("snow3");
					g_hSnowEmitter->m_bWrapParticlesToSpawnBounds = true;
				}
				g_hSnowEmitter->SetActive(true);			
			}
			else
			{
				Msg("Error spawning snow emitter\n");
			}
			// only get clouds for heavy snow
			if (m_iSnowType == 1)
			{
				g_hSnowCloudEmitter = CASWSnowEmitter::Create( "asw_emitter" );
				if ( g_hSnowCloudEmitter.IsValid() )
				{			
					g_hSnowCloudEmitter->UseTemplate("snowclouds");
					g_hSnowCloudEmitter->SetActive(true);
					g_hSnowEmitter->m_bWrapParticlesToSpawnBounds = true;
				}
				else
				{
					Msg("Error spawning snow cloud emitter\n");
				}
			}
		}
	}
	g_ASWSnowVolumes.AddToTail(this);

	BaseClass::Spawn();

	m_takedamage = DAMAGE_NO;	
}

bool C_ASW_Snow_Volume::Initialize()
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

bool C_ASW_Snow_Volume::IsPointInSnowVolume(const Vector &src)
{
	int c=g_ASWSnowVolumes.Count();
	for (int i=0;i<c;i++)
	{
		if (g_ASWSnowVolumes[i]->CollisionProp()->IsPointInBounds(src))
			return true;
	}
	return false;
}

void C_ASW_Snow_Volume::RecreateAll()
{
	DestroyAll();	
	ParseAllEntities( engine->GetMapEntitiesString() );
}

void C_ASW_Snow_Volume::DestroyAll()
{
	while (g_ASWSnowVolumes.Count() > 0 )
	{
		C_ASW_Snow_Volume *p = g_ASWSnowVolumes[0];
		p->Release();
	}
}

const char *C_ASW_Snow_Volume::ParseEntity( const char *pEntData )
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];
	
	MDLCACHE_CRITICAL_SECTION();

	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}

	if ( !Q_strcmp( className, "asw_snow_volume" ) )
	{
		// always force clientside entitis placed in maps
		C_ASW_Snow_Volume *pEntity = C_ASW_Snow_Volume::CreateNew( true ); 

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

bool C_ASW_Snow_Volume::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "model" ) )
	{
		SetModelName( AllocPooledString( szValue ) );
		return true;
	}
	if ( FStrEq( szKeyName, "snowtype" ) )
	{		
		m_iSnowType = atoi(szValue);		
		return true;
	}
	return BaseClass::KeyValue(szKeyName, szValue);
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void C_ASW_Snow_Volume::ParseAllEntities(const char *pMapData)
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
			Error( "C_ASW_Snow_Volume::ParseAllEntities: found %s when expecting {", token);
			continue;
		}

		//
		// Parse the entity and add it to the spawn list.
		//

		pMapData = ParseEntity( pMapData );

		nEntities++;
	}
}

bool C_ASW_Snow_Volume::ShouldDraw()
{
	return false;
}

void C_ASW_Snow_Volume::UpdateSnow(C_ASW_Player *pPlayer)
{
	if (!pPlayer)
		return;

	if (g_hSnowEmitter.IsValid())
	{
		C_ASW_Marine *pMarine = pPlayer->GetSpectatingMarine();
		if (!pMarine)
			pMarine = pPlayer->GetMarine();

		Vector vecSnowPos;
		if (pMarine)
			vecSnowPos = pMarine->GetAbsOrigin();
		else
			vecSnowPos = pPlayer->m_vecLastMarineOrigin;

		vecSnowPos += Vector(0,0,52);

		if (g_hSnowEmitter->m_hLastMarine.Get() != pMarine)
		{
			g_hSnowEmitter->SetSortOrigin(vecSnowPos);
			g_hSnowEmitter->GetBinding().DetectChanges();
			//g_hSnowEmitter->DoPresimulate(vecSnowPos, QAngle(0,0,0));
			//if (g_hSnowCloudEmitter.IsValid())
				//g_hSnowCloudEmitter->DoPresimulate(vecSnowPos, QAngle(0,0,0));
		}
		g_hSnowEmitter->m_hLastMarine = pMarine;
		g_hSnowEmitter->Think(gpGlobals->frametime, vecSnowPos, QAngle(0,0,0));
		//if (g_hSnowCloudEmitter.IsValid())
			//g_hSnowCloudEmitter->Think(gpGlobals->frametime, vecSnowPos, QAngle(0,0,0));
	}
}
