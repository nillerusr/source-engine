#ifndef _INCLUDED_ASW_TRACE_FILTER_MELEE
#define _INCLUDED_ASW_TRACE_FILTER_MELEE
#pragma once

// One CASW_Trace_Filter_Melee instance will keep track of upto this many hit entities
static const int ASW_MAX_HITS_PER_TRACE = 8;

class CASW_Trace_Filter_Melee : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CASW_Trace_Filter_Melee );
	
	CASW_Trace_Filter_Melee( const IHandleEntity *passentity, int collisionGroup, EHANDLE hAttacker, bool bDamageAnyNPC, bool bHitBehind = false, Vector *pAttackDir = NULL, float flAttackCone = 0.0f, CUtlVector<CBaseEntity*> *pPassVec = NULL )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_hAttacker(hAttacker), m_pHit(NULL),
			m_bDamageAnyNPC(bDamageAnyNPC),
			m_fBestHitDot(-1), m_hBestHit(NULL), m_bHitBehind(bHitBehind), m_nNumHits(0), m_pAttackDir( pAttackDir ), m_pPassVec( pPassVec )
	{
		m_flMinAttackDot = cos( DEG2RAD( flAttackCone ) );
		m_bConeFilter = (fabs(flAttackCone) > 0.0f) ? true : false;

		memset( m_HitTraces, 0, sizeof( m_HitTraces ) );
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	EHANDLE				m_hAttacker;
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CBaseEntity			*m_pHit;
	Vector				m_vecHitDir;

	bool				m_bDamageAnyNPC;

	CHandle<CBaseEntity> m_hBestHit;
	float m_fBestHitDot;
	trace_t *m_pBestTrace;
	bool				m_bHitBehind;

	// not using a CUtlVectorFixed for this since trace_t don't allow copy constructors
	trace_t				m_HitTraces[ ASW_MAX_HITS_PER_TRACE ];
	int					m_nNumHits;

	// Attack cone filtering
	//  If specified, attacks will only pass if the vector from an entity to hAttacker projects onto m_vecAttackDir more than m_flMinAttackDot
	const Vector		*m_pAttackDir;
	float				m_flMinAttackDot;
	bool				m_bConeFilter;

	// Additional entities to ignore
	CUtlVector<CBaseEntity*> *m_pPassVec;

};


class CASW_Trace_Filter_Skip_Marines : public CTraceFilterSimple
{
public:
	CASW_Trace_Filter_Skip_Marines( int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

class CASW_Trace_Filter_Skip_Aliens : public CTraceFilterSimple
{
public:
	CASW_Trace_Filter_Skip_Aliens( int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

#endif // _INCLUDED_ASW_TRACE_FILTER_MELEE
