//============= Copyright © StargateTC Source Team =============================
//
// ring_transporter — Goa'uld ring teleport pad. A player stepping onto the pad
// gets sent to the paired pad after a short delay. Pairs are matched via the
// `target` keyvalue (each ring names the other).
//
//==============================================================================
#include "cbase.h"
#include "stargate/sg_constants.h"

class CRingTransporter : public CBaseEntity
{
public:
    DECLARE_CLASS( CRingTransporter, CBaseEntity );
    DECLARE_DATADESC();

    void Spawn() override;
    void TouchTransport( CBaseEntity *pOther );
    void DoTransport();
    void Cooldown() { m_flNextActive = 0; SetNextThink( TICK_NEVER_THINK ); }

private:
    EHANDLE  m_hPending;
    float    m_flNextActive { 0.0f };
};

LINK_ENTITY_TO_CLASS( ring_transporter, CRingTransporter );

BEGIN_DATADESC( CRingTransporter )
    DEFINE_FIELD( m_hPending,     FIELD_EHANDLE ),
    DEFINE_FIELD( m_flNextActive, FIELD_TIME ),
    DEFINE_ENTITYFUNC( TouchTransport ),
    DEFINE_THINKFUNC( DoTransport ),
END_DATADESC()


void CRingTransporter::Spawn()
{
    BaseClass::Spawn();
    SetSolid( SOLID_BBOX );
    SetCollisionGroup( COLLISION_GROUP_NONE );
    UTIL_SetSize( this, Vector(-48,-48,0), Vector(48,48,16) );
    SetTouch( &CRingTransporter::TouchTransport );
}

void CRingTransporter::TouchTransport( CBaseEntity *pOther )
{
    if ( !pOther || !pOther->IsPlayer() ) return;
    if ( gpGlobals->curtime < m_flNextActive ) return;

    m_hPending = pOther;
    m_flNextActive = gpGlobals->curtime + StargateTC::RING_COOLDOWN;
    SetThink( &CRingTransporter::DoTransport );
    SetNextThink( gpGlobals->curtime + StargateTC::RING_TRAVEL_TIME );
    // TODO: spawn ring VFX + sound here.
}

void CRingTransporter::DoTransport()
{
    CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target );
    CBaseEntity *pPlayer = m_hPending;
    if ( pTarget && pPlayer )
    {
        pPlayer->Teleport( &pTarget->GetAbsOrigin(), NULL, NULL );
    }
    m_hPending = NULL;
    SetThink( &CRingTransporter::Cooldown );
    SetNextThink( gpGlobals->curtime + StargateTC::RING_COOLDOWN );
}
