#ifndef _DEFINED_ASW_USABLEOBJECTSENUMERATOR_H
#define _DEFINED_ASW_USABLEOBJECTSENUMERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "ehandle.h"
#include "ispatialpartition.h"

#ifndef CLIENT_DLL
	#define C_BaseEntity CBaseEntity
	#define C_ASW_Player CASW_Player
	#define C_ASW_Marine CASW_Marine
#endif

class C_BaseEntity;
class C_ASW_Player;

#define ASW_PARTITION_ALL_SERVER_EDICTS	(			\
	PARTITION_ENGINE_NON_STATIC_EDICTS |	\
	PARTITION_ENGINE_STATIC_PROPS |			\
	PARTITION_ENGINE_TRIGGER_EDICTS |		\
	PARTITION_ENGINE_SOLID_EDICTS			\
	)

// Enumator class for finding nearby usable items
class CASW_UsableObjectsEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CASW_UsableObjectsEnumerator );
public:
	//Forced constructor
	CASW_UsableObjectsEnumerator( float radius, C_ASW_Player *pPlayer );

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int	GetObjectCount();

	C_BaseEntity *GetObject( int index );

public:
	//Data members
	float	m_flRadiusSquared;

	CUtlVector< CHandle< C_BaseEntity > > m_Objects;
	C_ASW_Player *m_pLocal;
};

#endif // _DEFINED_ASW_USABLEOBJECTSENUMERATOR_H
