//============= Copyright (c) StargateTC Source Team ==========================
//
// Source-side port of the original GoldSrc multi_gate_1..multi_gate_49 family.
//
// Ghidra shows every multi_gate_N as a 0x248-byte CMultigateN object with the
// same ManagerUse/ManagerThink loop as CMultiManager plus a small baked target
// table. The first gate contains chevronsnd, dial1change1, dhdglyph1_on, and
// ch1relayon at zero delay; later gates follow the same naming convention.
//
//=============================================================================
#include "cbase.h"
#include "eventqueue.h"
#include "utlvector.h"

namespace
{
    constexpr int kMaxGateTargets = 16;

    struct GateTarget
    {
        string_t targetName;
        float delay;
    };
}

class CStargateMultiGate : public CBaseEntity
{
public:
    DECLARE_CLASS( CStargateMultiGate, CBaseEntity );
    DECLARE_DATADESC();

    explicit CStargateMultiGate( int gateIndex = 0 )
        : m_iGateIndex( gateIndex ),
          m_iCurrentTarget( 0 ),
          m_flStartTime( 0.0f )
    {
    }

    bool KeyValue( const char *szKeyName, const char *szValue ) override;
    void Spawn() override;
    void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) override;
    void ManagerThink();

protected:
    void AddTarget( const char *pszTarget, float flDelay );
    void AddDefaultTargets();

private:
    CUtlVector<GateTarget> m_Targets;
    int m_iGateIndex;
    int m_iCurrentTarget;
    float m_flStartTime;
    EHANDLE m_hActivator;
};

BEGIN_DATADESC( CStargateMultiGate )
    DEFINE_FIELD( m_iGateIndex, FIELD_INTEGER ),
    DEFINE_FIELD( m_iCurrentTarget, FIELD_INTEGER ),
    DEFINE_FIELD( m_flStartTime, FIELD_TIME ),
    DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
    DEFINE_THINKFUNC( ManagerThink ),
END_DATADESC()

bool CStargateMultiGate::KeyValue( const char *szKeyName, const char *szValue )
{
    if ( BaseClass::KeyValue( szKeyName, szValue ) )
        return true;

    if ( m_Targets.Count() < kMaxGateTargets && szValue && *szValue )
    {
        char *pEnd = NULL;
        const float flDelay = static_cast<float>( strtod( szValue, &pEnd ) );
        if ( pEnd != szValue )
        {
            AddTarget( szKeyName, flDelay );
            return true;
        }
    }

    return BaseClass::KeyValue( szKeyName, szValue );
}

void CStargateMultiGate::Spawn()
{
    BaseClass::Spawn();
    SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );
    SetUse( &CStargateMultiGate::Use );

    if ( m_Targets.Count() == 0 )
        AddDefaultTargets();
}

void CStargateMultiGate::Use( CBaseEntity *pActivator, CBaseEntity *, USE_TYPE, float )
{
    if ( m_Targets.Count() == 0 )
        return;

    m_hActivator = pActivator;
    m_iCurrentTarget = 0;
    m_flStartTime = gpGlobals->curtime;
    SetThink( &CStargateMultiGate::ManagerThink );
    SetNextThink( gpGlobals->curtime );
}

void CStargateMultiGate::ManagerThink()
{
    const float flElapsed = gpGlobals->curtime - m_flStartTime;

    while ( m_iCurrentTarget < m_Targets.Count() && m_Targets[m_iCurrentTarget].delay <= flElapsed )
    {
        const char *pszTarget = STRING( m_Targets[m_iCurrentTarget].targetName );
        variant_t emptyVariant;
        g_EventQueue.AddEvent( pszTarget, "Use", emptyVariant, 0.0f, m_hActivator.Get(), this );
        ++m_iCurrentTarget;
    }

    if ( m_iCurrentTarget < m_Targets.Count() )
    {
        SetNextThink( m_flStartTime + m_Targets[m_iCurrentTarget].delay );
        return;
    }

    SetThink( NULL );
}

void CStargateMultiGate::AddTarget( const char *pszTarget, float flDelay )
{
    if ( !pszTarget || !*pszTarget || m_Targets.Count() >= kMaxGateTargets )
        return;

    GateTarget &target = m_Targets[m_Targets.AddToTail()];
    target.targetName = AllocPooledString( pszTarget );
    target.delay = flDelay;
}

void CStargateMultiGate::AddDefaultTargets()
{
    if ( m_iGateIndex <= 0 )
        return;

    char targetName[64];

    AddTarget( "chevronsnd", 0.0f );

    Q_snprintf( targetName, sizeof( targetName ), "dial%dchange1", m_iGateIndex );
    AddTarget( targetName, 0.0f );

    Q_snprintf( targetName, sizeof( targetName ), "dhdglyph%d_on", m_iGateIndex );
    AddTarget( targetName, 0.0f );

    Q_snprintf( targetName, sizeof( targetName ), "ch%drelayon", m_iGateIndex );
    AddTarget( targetName, 0.0f );
}

#define LINK_MULTIGATE_ENTITY( index ) \
    class CStargateMultiGate##index : public CStargateMultiGate \
    { \
    public: \
        DECLARE_CLASS( CStargateMultiGate##index, CStargateMultiGate ); \
        CStargateMultiGate##index() : BaseClass( index ) {} \
    }; \
    LINK_ENTITY_TO_CLASS( multi_gate_##index, CStargateMultiGate##index )

LINK_MULTIGATE_ENTITY( 1 );
LINK_MULTIGATE_ENTITY( 2 );
LINK_MULTIGATE_ENTITY( 3 );
LINK_MULTIGATE_ENTITY( 4 );
LINK_MULTIGATE_ENTITY( 5 );
LINK_MULTIGATE_ENTITY( 6 );
LINK_MULTIGATE_ENTITY( 7 );
LINK_MULTIGATE_ENTITY( 8 );
LINK_MULTIGATE_ENTITY( 9 );
LINK_MULTIGATE_ENTITY( 10 );
LINK_MULTIGATE_ENTITY( 11 );
LINK_MULTIGATE_ENTITY( 12 );
LINK_MULTIGATE_ENTITY( 13 );
LINK_MULTIGATE_ENTITY( 14 );
LINK_MULTIGATE_ENTITY( 15 );
LINK_MULTIGATE_ENTITY( 16 );
LINK_MULTIGATE_ENTITY( 17 );
LINK_MULTIGATE_ENTITY( 18 );
LINK_MULTIGATE_ENTITY( 19 );
LINK_MULTIGATE_ENTITY( 20 );
LINK_MULTIGATE_ENTITY( 21 );
LINK_MULTIGATE_ENTITY( 22 );
LINK_MULTIGATE_ENTITY( 23 );
LINK_MULTIGATE_ENTITY( 24 );
LINK_MULTIGATE_ENTITY( 25 );
LINK_MULTIGATE_ENTITY( 26 );
LINK_MULTIGATE_ENTITY( 27 );
LINK_MULTIGATE_ENTITY( 28 );
LINK_MULTIGATE_ENTITY( 29 );
LINK_MULTIGATE_ENTITY( 30 );
LINK_MULTIGATE_ENTITY( 31 );
LINK_MULTIGATE_ENTITY( 32 );
LINK_MULTIGATE_ENTITY( 33 );
LINK_MULTIGATE_ENTITY( 34 );
LINK_MULTIGATE_ENTITY( 35 );
LINK_MULTIGATE_ENTITY( 36 );
LINK_MULTIGATE_ENTITY( 37 );
LINK_MULTIGATE_ENTITY( 38 );
LINK_MULTIGATE_ENTITY( 39 );
LINK_MULTIGATE_ENTITY( 40 );
LINK_MULTIGATE_ENTITY( 41 );
LINK_MULTIGATE_ENTITY( 42 );
LINK_MULTIGATE_ENTITY( 43 );
LINK_MULTIGATE_ENTITY( 44 );
LINK_MULTIGATE_ENTITY( 45 );
LINK_MULTIGATE_ENTITY( 46 );
LINK_MULTIGATE_ENTITY( 47 );
LINK_MULTIGATE_ENTITY( 48 );
LINK_MULTIGATE_ENTITY( 49 );
