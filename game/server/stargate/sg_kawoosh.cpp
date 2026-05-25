//============= Copyright (c) StargateTC Source Team ==========================
//
// stargate_kawoosh - unstable vortex damage volume. The original GoldSrc DLL
// exposes this as a custom gate entity; this Source pass makes it operational
// through normal inputs so stargate_event can trigger it from Hammer I/O.
//
//=============================================================================
#include "cbase.h"
#include "stargate/sg_constants.h"

class CStargateKawoosh : public CBaseEntity
{
public:
    DECLARE_CLASS( CStargateKawoosh, CBaseEntity );
    DECLARE_DATADESC();

    void Spawn() override;
    void InputFire( inputdata_t &inputdata );
    void CollapseThink();

private:
    float m_flRadius;
};

LINK_ENTITY_TO_CLASS( stargate_kawoosh, CStargateKawoosh );

BEGIN_DATADESC( CStargateKawoosh )
    DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Fire", InputFire ),
    DEFINE_THINKFUNC( CollapseThink ),
END_DATADESC()

void CStargateKawoosh::Spawn()
{
    BaseClass::Spawn();
    SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );

    if ( m_flRadius <= 0.0f )
        m_flRadius = StargateTC::KAWOOSH_RADIUS;
}

void CStargateKawoosh::InputFire( inputdata_t & )
{
    CTakeDamageInfo info( this, this, StargateTC::KAWOOSH_DAMAGE, DMG_ENERGYBEAM | DMG_DISSOLVE );
    RadiusDamage( info, GetAbsOrigin(), m_flRadius, CLASS_NONE, NULL );
    SetThink( &CStargateKawoosh::CollapseThink );
    SetNextThink( gpGlobals->curtime + StargateTC::KAWOOSH_DURATION );
}

void CStargateKawoosh::CollapseThink()
{
    SetThink( NULL );
}
