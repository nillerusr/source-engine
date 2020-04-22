//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tfc_player.h"
#include "tfc_building.h"


//=========================================================================
// Destroys a single Engineer building
void DestroyBuilding(CTFCPlayer *eng, char *bld)
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, bld );
	while ( pEnt )
	{
		CTFBaseBuilding *pBuilding = dynamic_cast<CTFBaseBuilding*>( pEnt );

		if (pBuilding && pBuilding->real_owner == eng)
		{
			// If it's fallen out of the world, give the engineer
			// some metal back
			int pos = UTIL_PointContents(pEnt->GetAbsOrigin());
#ifdef TFCTODO // CONTENTS_SKY doesn't exist in the new engine
			if (pos == CONTENT_SOLID || pos == CONTENT_SKY) 
#else
			if (pos == CONTENTS_SOLID)
#endif
			{
				eng->GiveAmmo( 100, TFC_AMMO_CELLS );
				eng->TeamFortress_CheckClassStats();
			}

			pEnt->TakeDamage( CTakeDamageInfo( pEnt, pEnt, 500, 0 ) );
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, bld );
	}
}


//=========================================================================
// Destroys a teleporter (determined by type)
void DestroyTeleporter(CTFCPlayer *eng, int type)
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "building_teleporter" );
	while ( pEnt )
	{
		CTFTeleporter *pTeleporter = dynamic_cast<CTFTeleporter*>( pEnt );

		if (pTeleporter && pTeleporter->real_owner == eng && pTeleporter->m_iType == type )
		{
			// If it's fallen out of the world, give the engineer
			// some metal back
			int pos = UTIL_PointContents(pEnt->GetAbsOrigin());
#ifdef TFCTODO // CONTENTS_SKY doesn't exist in the new engine
			if (pos == CONTENT_SOLID || pos == CONTENT_SKY) 
#else
			if (pos == CONTENTS_SOLID)
#endif
			{
				eng->GiveAmmo( 100, TFC_AMMO_CELLS );
				eng->TeamFortress_CheckClassStats();
			}

			pEnt->TakeDamage( CTakeDamageInfo( pEnt, pEnt, 500, 0 ) );
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "building_teleporter" );
	}
}
