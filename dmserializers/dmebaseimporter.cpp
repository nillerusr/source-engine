//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "dmserializers.h"
#include "dmebaseimporter.h"

CDmeBaseImporter::CDmeBaseImporter( char const *formatName, char const *nextFormatName ) :
	m_pFormatName( formatName ),
	m_pNextSerializer( nextFormatName )
{
}

bool CDmeBaseImporter::IsLatestVersion() const
{
	return g_pDataModel->FindLegacyUpdater( m_pNextSerializer ) == NULL;
}

// Updates ppRoot to first non-legacy generic dmx format, returns false if the conversion fails
bool CDmeBaseImporter::Update( CDmElement **ppRoot )
{
	if ( !DoFixup( *ppRoot ) )
		return false;

	if ( !m_pNextSerializer )
		return true;

	// Chain
	IDmLegacyUpdater *pUpdater = g_pDataModel->FindLegacyUpdater( m_pNextSerializer );
	if ( !pUpdater )
		return true;

	return pUpdater->Update( ppRoot );
}



CSFMBaseImporter::CSFMBaseImporter( char const *formatName, char const *nextFormatName ) :
	BaseClass( formatName, nextFormatName )
{
}
