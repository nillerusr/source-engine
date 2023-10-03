#ifndef C_ASW_MARKER_H
#define C_ASW_MARKER_H
#pragma once


#include "c_baseentity.h"	


DECLARE_AUTO_LIST( IObjectiveMarkerList );


// This class holds information about an objective marker

class C_ASW_Marker : public C_BaseEntity, public IObjectiveMarkerList
{
public:
	DECLARE_CLASS( C_ASW_Marker, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	IMPLEMENT_AUTO_LIST_GET();

	int GetMapWidth( void ) { return m_nMapWidth; }
	int GetMapHeight( void ) { return m_nMapHeight; }

	const char * GetObjectiveName( void ) const { return m_ObjectiveName; }

	bool IsComplete( void ) const { return m_bComplete; }
	bool IsEnabled( void ) const { return m_bEnabled; }

private:

	CNetworkString( m_ObjectiveName, 256 );
	CNetworkVar( int, m_nMapWidth );
	CNetworkVar( int, m_nMapHeight );
	CNetworkVar( bool, m_bComplete );
	CNetworkVar( bool, m_bEnabled );

};

#endif /* C_ASW_MARKER_H */
