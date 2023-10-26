#ifndef _INCLUDED_ASW_SCANNER_OBJECTS_ENUMERATOR_H
#define _INCLUDED_ASW_SCANNER_OBJECTS_ENUMERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "ehandle.h"
#include "ispatialpartition.h"

#ifdef CLIENT_DLL
	#define CBaseEntity C_BaseEntity
#endif

class CBaseEntity;

#define ASW_PARTITION_ALL_SERVER_EDICTS	(			\
	PARTITION_ENGINE_NON_STATIC_EDICTS |	\
	PARTITION_ENGINE_STATIC_PROPS |			\
	PARTITION_ENGINE_TRIGGER_EDICTS |		\
	PARTITION_ENGINE_SOLID_EDICTS			\
	)

// Enumator class for finding entities that show up on the scanner
class CASW_Scanner_Objects_Enumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CASW_Scanner_Objects_Enumerator );
public:
	//Forced constructor
	CASW_Scanner_Objects_Enumerator( float radius, Vector &vecCenter );

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int	GetObjectCount();

	CBaseEntity *GetObject( int index );

public:
	//Data members
	float	m_flRadius;
	float	m_flRadiusSquared;

	CUtlVector< CHandle< CBaseEntity > > m_Objects;
	Vector m_vecScannerCenter;
};

#endif // _INCLUDED_ASW_SCANNER_OBJECTS_ENUMERATOR_H
