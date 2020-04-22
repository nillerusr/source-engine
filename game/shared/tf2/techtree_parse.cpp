//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared code that parse the Technology Tree on the client & server.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "utlvector.h"
#include "tf_shareddefs.h"
#include "techtree.h"
#include <KeyValues.h>
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Parse a class result chunk inside a technology keyvalue
//-----------------------------------------------------------------------------
void ParseClassResult( CBaseTechnology *pTechnology, KeyValues *pkvResult, int iClass )
{
	if ( !pkvResult )
		return;

	// All is a special case, used by general technologies
	if ( iClass == TFCLASS_CLASS_COUNT )
	{
		for (int i = TFCLASS_RECON; i < TFCLASS_CLASS_COUNT; i++)
		{
			pTechnology->SetClassResultSound( i, pkvResult->GetString( "sound", NULL ) );
			pTechnology->SetClassResultDescription( i, pkvResult->GetString( "description", NULL ) );
			pTechnology->SetClassResultAssociateWeapons( i, pkvResult->GetInt( "associateweapons", 0 ) ? true : false );
		}
	}
	else
	{
		pTechnology->SetClassResultSound( iClass, pkvResult->GetString( "sound", NULL ) );
		pTechnology->SetClassResultDescription( iClass, pkvResult->GetString( "description", NULL ) );
		pTechnology->SetClassResultAssociateWeapons( iClass, pkvResult->GetInt( "associateweapons", 0 ) ? true : false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse a technology from a keyvalues chunk in the data file
//-----------------------------------------------------------------------------
void ParseTechnology( CBaseTechnology *pTechnology, KeyValues *pkvTech )
{
	// Get the general data
	pTechnology->SetName( pkvTech->GetName() );
	pTechnology->SetPrintName( pkvTech->GetString( "printname", "" ) );
	// Use the print name if no button name specified
	pTechnology->SetButtonName( pkvTech->GetString( "buttonname", pTechnology->GetPrintName() ) );
	pTechnology->SetDescription( pkvTech->GetString( "description", "" ) );
	pTechnology->SetTextureName( pkvTech->GetString( "texture", "" ) );
	pTechnology->SetLevel( pkvTech->GetInt( "level", 0 ) );
	pTechnology->SetGoalTechnology( pkvTech->GetInt( "goal", 0 ) > 0 );
	pTechnology->SetHidden( pkvTech->GetInt( "hidden", 0 ) != 0 );

	// Retrieve weapon associations
	KeyValues *pkvWeaponAssociations = pkvTech->FindKey( "associated_weapons" );
	if ( pkvWeaponAssociations )
	{
		KeyValues *pkvWA = pkvWeaponAssociations->GetFirstSubKey();
		while ( pkvWA )
		{
			const char *weaponname = pkvWA->GetString();
			if ( weaponname && weaponname[ 0 ] )
			{
				pTechnology->AddAssociatedWeapon( weaponname );
			}

			pkvWA = pkvWA->GetNextKey();
		}
	}

	// Get the cost
	pTechnology->SetCost( pkvTech->GetFloat( "resourcecost", 0.0 ) );

	// Get the class results
	KeyValues *pkvClassResults = pkvTech->FindKey( "class_results" );
	if ( pkvClassResults )
	{
		// Try and get each class result
		for ( int iClass=0; iClass < TFCLASS_CLASS_COUNT; iClass++ )
		{
			ParseClassResult( pTechnology, pkvClassResults->FindKey( GetTFClassInfo( iClass )->m_pClassName ), iClass );
		}
		
		ParseClassResult( pTechnology, pkvClassResults->FindKey( "all" ), TFCLASS_CLASS_COUNT );
	}

	// Get any technologies contained within this one
	KeyValues *pkvTechnologies = pkvTech->FindKey( "technologies" );
	if ( pkvTechnologies )
	{
		KeyValues *pkvTechnology = pkvTechnologies->GetFirstSubKey();
		int iContainedTechs = 0;
		while ( pkvTechnology )
		{
			if ( iContainedTechs >= MAX_CONTAINED_TECHNOLOGIES )
				break;

			pTechnology->AddContainedTechnology( pkvTechnology->GetString() ); 

			iContainedTechs++;
			pkvTechnology = pkvTechnology->GetNextKey();
		}
	}

	// Get any dependencies for this tech
	KeyValues *pkvDependencies = pkvTech->FindKey( "dependencies" );
	if ( pkvDependencies )
	{
		KeyValues *pkvTechnology = pkvDependencies->GetFirstSubKey();
		int iDependentTechs = 0;
		while ( pkvTechnology )
		{
			if ( iDependentTechs >= MAX_DEPENDANT_TECHNOLOGIES )
				break;

			pTechnology->AddDependentTechnology( pkvTechnology->GetString() ); 

			iDependentTechs++;
			pkvTechnology = pkvTechnology->GetNextKey();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse the technology tree file and dump the data into the utlvector list
//-----------------------------------------------------------------------------
bool ParseTechnologyFile( CUtlVector< CBaseTechnology * > &pTechnologyList, IFileSystem* filesystem, int nTeamNumber, char *sFileName )
{
	// Open the technology tree datafile
	KeyValues *pkvTechTreeFile = new KeyValues( "TechTreeDataFile" );
	if ( pkvTechTreeFile->LoadFromFile( filesystem, sFileName, "GAME" ) == false )
		return false;
	
	// Parse the list of techs
	KeyValues *pkvTech = pkvTechTreeFile->GetFirstSubKey();
	while ( pkvTech )
	{
		// Reached the maximum number of techs allowed?
		if ( pTechnologyList.Size() >= MAX_TECHNOLOGIES )
		{
			pkvTechTreeFile->deleteThis();
			return false;
		}

		// Create the technology
		CBaseTechnology *pTechnology = NULL;
		int nTeamTech = pkvTech->GetInt( "team", 0 );
		if ((nTeamTech == 0) || (nTeamTech == nTeamNumber))
		{
			pTechnology = new CBaseTechnology();
			ParseTechnology( pTechnology, pkvTech );

			// Find out if it's already in the list
			for ( int i = 0; i < pTechnologyList.Size(); i++ )
			{
				if ( !strcmp(pTechnologyList[i]->GetName(), pTechnology->GetName() ) )
				{
					// Found it in the tree already, so delete and continue
					delete pTechnology;
					pTechnology = NULL;
					break;
				}
			}
		}

		// If we haven't deleted it, add it to the list
		if ( pTechnology )
		{
			pTechnologyList.AddToTail( pTechnology );
		}

		pkvTech = pkvTech->GetNextKey();
	}

	pkvTechTreeFile->deleteThis();
	return true;
}

