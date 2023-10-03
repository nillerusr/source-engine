#ifndef _INCLUDED_ENV_AMBIENT_LIGHT_H
#define _INCLUDED_ENV_AMBIENT_LIGHT_H

#include "spatialentity.h"

//------------------------------------------------------------------------------
// Purpose : Ambient light controller entity
//------------------------------------------------------------------------------
class CEnvAmbientLight : public CSpatialEntity
{
	DECLARE_CLASS( CEnvAmbientLight, CSpatialEntity );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );

	void InputSetColor(inputdata_t &inputdata);
	void SetColor( const Vector &vecColor );

private:
	color32	m_Color;

	CNetworkVector( m_vecColor );
};

#endif // _INCLUDED_ENV_AMBIENT_LIGHT_H