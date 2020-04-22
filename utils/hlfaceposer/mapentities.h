//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPENTITIES_H
#define MAPENTITIES_H
#ifdef _WIN32
#pragma once
#endif

class Vector;

class IMapEntities
{
public:
	virtual void	CheckUpdateMap( char const *mapname ) = 0;
	virtual bool	LookupOrigin( char const *name, Vector& origin, QAngle& angles ) = 0;

	virtual int		Count() = 0;
	virtual char const *GetName( int number ) = 0;

};

extern IMapEntities *mapentities;

#endif // MAPENTITIES_H
