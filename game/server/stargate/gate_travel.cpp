//============= Copyright (c) StargateTC Source Team ==========================
//
// zone_gatetravel and trigger_adv_teleport recovered from the GoldSrc DLL.
// Ghidra's GTravelTouch path checks IsPlayer(), clears velocity, and starts a
// short travel lockout. The Source port keeps that observable behavior and adds
// explicit destination targeting for converted maps.
//
//=============================================================================
#include "cbase.h"
#include "triggers.h"
#include "stargate/sg_constants.h"

class CStargateTravelZone : public CBaseTrigger
{
public:
    DECLARE_CLASS( CStargateTravelZone, CBaseTrigger );
    DECLARE_DATADESC();

    void Spawn() override;
    void TravelTouch( CBaseEntity *pOther );

private:
    void TeleportPlayer( CBaseEntity *pPlayer );
    float m_flNextTouchTime { 0.0f };
};

LINK_ENTITY_TO_CLASS( zone_gatetravel, CStargateTravelZone );
LINK_ENTITY_TO_CLASS( trigger_adv_teleport, CStargateTravelZone );

BEGIN_DATADESC( CStargateTravelZone )
    DEFINE_FIELD( m_flNextTouchTime, FIELD_TIME ),
    DEFINE_ENTITYFUNC( TravelTouch ),
END_DATADESC()

void CStargateTravelZone::Spawn()
{
    BaseClass::Spawn();
    InitTrigger();
    SetTouch( &CStargateTravelZone::TravelTouch );
}

void CStargateTravelZone::TravelTouch( CBaseEntity *pOther )
{
    if ( !pOther || !pOther->IsPlayer() || gpGlobals->curtime < m_flNextTouchTime )
        return;

    pOther->SetAbsVelocity( vec3_origin );
    pOther->SetBaseVelocity( vec3_origin );
    TeleportPlayer( pOther );

    m_flNextTouchTime = gpGlobals->curtime + StargateTC::GATE_TRAVEL_COOLDOWN;
}

void CStargateTravelZone::TeleportPlayer( CBaseEntity *pPlayer )
{
    CBaseEntity *pDestination = gEntList.FindEntityByName( NULL, m_target );
    if ( !pDestination )
        return;

    Vector destination = pDestination->GetAbsOrigin();
    QAngle angles = pDestination->GetAbsAngles();
    Vector velocity = vec3_origin;
    pPlayer->Teleport( &destination, &angles, &velocity );
}
