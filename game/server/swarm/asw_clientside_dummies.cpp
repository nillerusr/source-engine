#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Dummy classes for entities that are created clientside.  Stops warnings about unknown entities when loading the map

class CASW_Snow_Volume_Dummy : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CASW_Snow_Volume_Dummy, CServerOnlyPointEntity );
public:
	virtual void Spawn()
	{
		BaseClass::Spawn();
		UTIL_Remove(this);
	}
};

class CASW_Scanner_Noise_Dummy : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CASW_Scanner_Noise_Dummy, CServerOnlyPointEntity );
public:
	virtual void Spawn()
	{
		BaseClass::Spawn();
		UTIL_Remove(this);
	}
};

LINK_ENTITY_TO_CLASS( asw_snow_volume, CASW_Snow_Volume_Dummy );
LINK_ENTITY_TO_CLASS( asw_scanner_noise, CASW_Scanner_Noise_Dummy );