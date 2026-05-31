//============= Copyright © StargateTC Source Team =============================
//
// dhd_console — the Dial-Home-Device. A player +USEs it to fire the matching
// stargate_event's Dial input. Includes a per-DHD lockout while the gate is
// active to prevent re-dial.
//
// Keyvalues:
//   target  — name of the stargate_event to dial
//
//==============================================================================
#include "cbase.h"
#include "stargate/sg_constants.h"

class CDHDConsole : public CBaseAnimating
{
public:
    DECLARE_CLASS( CDHDConsole, CBaseAnimating );
    DECLARE_DATADESC();

    void Spawn() override;
    void Precache() override;
    void Use( CBaseEntity *pActivator, CBaseEntity *pCaller,
              USE_TYPE useType, float value ) override;

    int  ObjectCaps() override { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }

private:
    bool m_bBusy { false };

    void Unbusy()  { m_bBusy = false; SetNextThink( TICK_NEVER_THINK ); }
};

LINK_ENTITY_TO_CLASS( dhd_console, CDHDConsole );

BEGIN_DATADESC( CDHDConsole )
    DEFINE_THINKFUNC( Unbusy ),
END_DATADESC()

void CDHDConsole::Spawn()
{
    BaseClass::Spawn();
    Precache();
    SetModel( "models/stargate/dhd_console.mdl" );
    SetSolid( SOLID_VPHYSICS );
    SetMoveType( MOVETYPE_NONE );
}

void CDHDConsole::Precache()
{
    PrecacheModel( "models/stargate/dhd_console.mdl" );
    PrecacheScriptSound( "Stargate.DHDPress" );
}

void CDHDConsole::Use( CBaseEntity *pActivator, CBaseEntity *,
                       USE_TYPE, float )
{
    if ( m_bBusy || !pActivator || !pActivator->IsPlayer() )
        return;
    EmitSound( "Stargate.DHDPress" );

    CBaseEntity *pGate = gEntList.FindEntityByName( NULL, m_target );
    if ( pGate )
    {
        variant_t empty;
        pGate->AcceptInput( "Dial", pActivator, this, empty, 0 );
    }
    m_bBusy = true;
    SetThink( &CDHDConsole::Unbusy );
    SetNextThink( gpGlobals->curtime +
                  StargateTC::DIAL_SYMBOL_COUNT * StargateTC::DIAL_SYMBOL_TIME +
                  StargateTC::KAWOOSH_DURATION );
}
