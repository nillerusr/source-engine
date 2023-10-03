#ifndef _DEFINED_ASW_MARINEANDOBJECTENUMERATOR_H
#define _DEFINED_ASW_MARINEANDOBJECTENUMERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlVector.h"
#include "ehandle.h"
#include "ISpatialPartition.h"

class C_BaseEntity;
class C_ASW_Player;

// Enumator class for finding other marines and objects close to the
//  local player's marine
class CASW_MarineAndObjectEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CASW_MarineAndObjectEnumerator );
public:
	//Forced constructor
	CASW_MarineAndObjectEnumerator( float radius );

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

#endif // _DEFINED_ASW_MARINEANDOBJECTENUMERATOR_H
