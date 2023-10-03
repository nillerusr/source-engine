#include "cbase.h"
#include "c_asw_scanner_noise.h"
#include "mapentities_shared.h"
#include "datacache/imdlcache.h"
#include "gamestringpool.h"
#include "tier1/fmtstr.h"
#include "c_user_message_register.h"

C_ASW_Scanner_Noise* g_pScannerNoise = NULL;


C_ASW_Scanner_Noise::C_ASW_Scanner_Noise()
{
	if (g_pScannerNoise != NULL)
	{
		delete g_pScannerNoise;
		g_pScannerNoise = NULL;
	}

	g_pScannerNoise = this;
}

C_ASW_Scanner_Noise::~C_ASW_Scanner_Noise()
{
	m_NoiseSpots.PurgeAndDeleteElements();
	m_NoiseEnts.PurgeAndDeleteElements();
	if (g_pScannerNoise == this)
		g_pScannerNoise = NULL;
}


void C_ASW_Scanner_Noise::RecreateAll()
{
	DestroyAll();	
	ParseAllEntities( engine->GetMapEntitiesString() );
}

void C_ASW_Scanner_Noise::DestroyAll()
{
	if (g_pScannerNoise)
	{
		g_pScannerNoise->m_NoiseSpots.PurgeAndDeleteElements();
	}
}

const char *C_ASW_Scanner_Noise::ParseEntity( const char *pEntData )
{
	CEntityMapData entData( (char*)pEntData );
	char className[MAPKEY_MAXLENGTH];
	
	MDLCACHE_CRITICAL_SECTION();

	if (!entData.ExtractValue("classname", className))
	{
		Error( "classname missing from entity!\n" );
	}

	if ( !Q_strcmp( className, "asw_scanner_noise" ) )
	{
		if (!g_pScannerNoise)
		{
			g_pScannerNoise = new C_ASW_Scanner_Noise();
		}

		// Set up keyvalues.
		g_pScannerNoise->ParseMapData(&entData);			
	
		return entData.CurrentBufferPosition();
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

bool C_ASW_Scanner_Noise::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "origin" ) )
	{
		Vector vecPos;
		UTIL_StringToVector( vecPos.Base(), szValue );
		m_vecCurrentPos = vecPos;
		return true;
	}
	else if ( FStrEq( szKeyName, "core" ) )
	{		
		m_fCurrentCore = atof(szValue);		
		return true;
	}
	else if ( FStrEq( szKeyName, "maxdistance" ) )
	{		
		m_fCurrentRadius = atof(szValue);		
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Only called on BSP load. Parses and spawns all the entities in the BSP.
// Input  : pMapData - Pointer to the entity data block to parse.
//-----------------------------------------------------------------------------
void C_ASW_Scanner_Noise::ParseAllEntities(const char *pMapData)
{
	int nEntities = 0;

	char szTokenBuffer[MAPKEY_MAXLENGTH];
	//  Loop through all entities in the map data, creating each.
	for ( ; true; pMapData = MapEntity_SkipToNextEntity(pMapData, szTokenBuffer) )
	{
		// Parse the opening brace.
		char token[MAPKEY_MAXLENGTH];
		pMapData = MapEntity_ParseToken( pMapData, token );
		// Check to see if we've finished or not.
		if (!pMapData)
			break;

		if (token[0] != '{')
		{
			Error( "C_ASW_Scanner_Noise::ParseAllEntities: found %s when expecting {", token);
			continue;
		}
		// Parse the entity and add it to the spawn list.
		pMapData = ParseEntity( pMapData );

		nEntities++;
	}
}

void C_ASW_Scanner_Noise::AddScannerNoise(const Vector& vecPos, float fCoreRadius, float fMaximumDistance)
{
	CASW_NoiseSpot* pSpot = new CASW_NoiseSpot();
	if (!pSpot)
		return;

	if (fMaximumDistance <= fCoreRadius)
	{
		fMaximumDistance = fCoreRadius + 1.0f;
	}

	pSpot->m_fCoreRadius = fCoreRadius;
	pSpot->m_fMaximumDistance = fMaximumDistance;
	pSpot->m_vecPosition = vecPos;

	m_NoiseSpots.AddToTail(pSpot);
}

void C_ASW_Scanner_Noise::AddScannerEntNoise(C_BaseEntity *pEnt, float fCoreRadius, float fMaximumDistance)
{
	if (!pEnt)
		return;

	CASW_EntNoiseSpot* pSpot = new CASW_EntNoiseSpot();
	if (!pSpot)
		return;

	if (fMaximumDistance <= fCoreRadius)
	{
		fMaximumDistance = fCoreRadius + 1.0f;
	}

	pSpot->m_fCoreRadius = fCoreRadius;
	pSpot->m_fMaximumDistance = fMaximumDistance;
	pSpot->m_hEnt = pEnt;

	m_NoiseEnts.AddToTail(pSpot);
}

// returns 0 to 1 for scanner noise
float C_ASW_Scanner_Noise::GetScannerNoiseAt(const Vector& vecPos)
{
	int m = m_NoiseSpots.Count();
	int best = -1;
	float dist = 0;
	float beststrength = 0;
	float strength = 0;
	for (int i=0;i<m;i++)
	{		
		dist = m_NoiseSpots[i]->m_vecPosition.DistTo(vecPos);		
		if (dist < m_NoiseSpots[i]->m_fCoreRadius)
		{
			strength = 1;
		}
		else if (dist > m_NoiseSpots[i]->m_fMaximumDistance)
		{
			strength = 0;
		}
		else
		{
			strength = 1.0f - ( (dist - m_NoiseSpots[i]->m_fCoreRadius) /
							    (m_NoiseSpots[i]->m_fMaximumDistance - m_NoiseSpots[i]->m_fCoreRadius) );
		}
		if (best == -1 || strength > beststrength)
		{
			best = i;
			beststrength = strength;
		}
	}
	m = m_NoiseEnts.Count();
	for (int i=0;i<m;i++)
	{
		if (m_NoiseEnts[i]->m_hEnt.Get() == NULL)
			continue;

		dist = m_NoiseEnts[i]->m_hEnt->GetAbsOrigin().DistTo(vecPos);		
		if (dist < m_NoiseEnts[i]->m_fCoreRadius)
		{
			strength = 1;
		}
		else if (dist > m_NoiseEnts[i]->m_fMaximumDistance)
		{
			strength = 0;
		}
		else
		{
			strength = 1.0f - ( (dist - m_NoiseEnts[i]->m_fCoreRadius) /
							    (m_NoiseEnts[i]->m_fMaximumDistance - m_NoiseEnts[i]->m_fCoreRadius) );
		}
		if (best == -1 || strength > beststrength)
		{
			best = i;
			beststrength = strength;
		}
	}
	return beststrength;
}

void C_ASW_Scanner_Noise::ParseMapData( CEntityMapData *mapData )
{
	char keyName[MAPKEY_MAXLENGTH];
	char value[MAPKEY_MAXLENGTH];

	m_fCurrentRadius = 200;
	m_fCurrentCore = 100;
	m_vecCurrentPos = vec3_origin;

	// loop through all keys in the data block and pass the info back into the object
	if ( mapData->GetFirstKey(keyName, value) )
	{
		do 
		{
			KeyValue( keyName, value );
		} 
		while ( mapData->GetNextKey(keyName, value) );
	}

	AddScannerNoise(m_vecCurrentPos, m_fCurrentCore, m_fCurrentRadius);	
}

void C_ASW_Scanner_Noise::ListScannerNoises()
{
	int m = m_NoiseSpots.Count();
	Msg("Scanner noises:%d\n", m);
	for (int i=0;i<m;i++)
	{
		Msg("%d: Noise at %s core %f maxdist %f\n", i, VecToString(m_NoiseSpots[i]->m_vecPosition),
			m_NoiseSpots[i]->m_fCoreRadius, m_NoiseSpots[i]->m_fMaximumDistance);
	}
	m = m_NoiseEnts.Count();
	Msg("Scanner noise ents:%d\n", m);
	for (int i=0;i<m;i++)
	{
		if (m_NoiseEnts[i]->m_hEnt.Get() == NULL)
			continue;
		Msg("%d: Noise at %s core %f maxdist %f\n", i, VecToString(m_NoiseEnts[i]->m_hEnt->GetAbsOrigin()),
			m_NoiseSpots[i]->m_fCoreRadius, m_NoiseSpots[i]->m_fMaximumDistance);
	}
}

void __MsgFunc_ASWScannerNoiseEnt( bf_read &msg )
{
	int iNoisyEnt = msg.ReadShort();		
	C_BaseEntity *pEnt = dynamic_cast<C_BaseEntity*>(ClientEntityList().GetEnt(iNoisyEnt));		// turn iMarine ent index into the marine
	if (!pEnt)
		return;
	
	float fCoreRadius = msg.ReadFloat();
	float fMaxDistance = msg.ReadFloat();	

	if (g_pScannerNoise)
		g_pScannerNoise->AddScannerEntNoise(pEnt, fCoreRadius, fMaxDistance);
}
USER_MESSAGE_REGISTER( ASWScannerNoiseEnt );