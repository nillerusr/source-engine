//============= Copyright © StargateTC Source Team =============================
//
// stargate_event — the master entity that drives a Stargate dial. Other gate
// pieces (the rotating ring brushwork, chevron light props, kawoosh emitter)
// hook to this via parentname / OnDial outputs.
//
// Inputs:
//   Dial        — kick off the 7-chevron sequence
//   ForceClose  — interrupt the dial (or shut down an open wormhole)
//   Lock        — disable for this round
//
// Outputs:
//   OnDialStart
//   OnChevronLock     (passes chevron index 1..7)
//   OnKawoosh
//   OnWormholeOpen
//   OnWormholeClose
//
// First-pass behavior: spawns, registers, accepts inputs, fires outputs on a
// fixed schedule driven by StargateTC::DIAL_SYMBOL_TIME. No gameplay
// consequences yet — those land when the player-teleport + kawoosh damage
// logic ports.
//
//==============================================================================
#include "cbase.h"
#include "stargate/sg_constants.h"

class CStargateEvent : public CBaseEntity
{
public:
    DECLARE_CLASS( CStargateEvent, CBaseEntity );
    DECLARE_DATADESC();

    void Spawn() override;
    void Precache() override;

    // I/O
    void InputDial( inputdata_t &inputdata );
    void InputForceClose( inputdata_t &inputdata );
    void InputLock( inputdata_t &inputdata );

    void DialThink();
    void WormholeOpenThink();

private:
    int          m_iChevronsLocked      { 0 };
    bool         m_bLocked              { false };

    COutputEvent m_OnDialStart;
    COutputInt   m_OnChevronLock;
    COutputEvent m_OnKawoosh;
    COutputEvent m_OnWormholeOpen;
    COutputEvent m_OnWormholeClose;
};

LINK_ENTITY_TO_CLASS( stargate_event, CStargateEvent );

BEGIN_DATADESC( CStargateEvent )
    DEFINE_INPUTFUNC( FIELD_VOID, "Dial",       InputDial ),
    DEFINE_INPUTFUNC( FIELD_VOID, "ForceClose", InputForceClose ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Lock",       InputLock ),
    DEFINE_OUTPUT(    m_OnDialStart,     "OnDialStart" ),
    DEFINE_OUTPUT(    m_OnChevronLock,   "OnChevronLock" ),
    DEFINE_OUTPUT(    m_OnKawoosh,       "OnKawoosh" ),
    DEFINE_OUTPUT(    m_OnWormholeOpen,  "OnWormholeOpen" ),
    DEFINE_OUTPUT(    m_OnWormholeClose, "OnWormholeClose" ),
    DEFINE_THINKFUNC( DialThink ),
    DEFINE_THINKFUNC( WormholeOpenThink ),
END_DATADESC()


void CStargateEvent::Spawn()
{
    BaseClass::Spawn();
    Precache();
    SetSolid( SOLID_NONE );
}

void CStargateEvent::Precache()
{
    // TODO: precache gate ring rotation sound, chevron lock click, kawoosh whoosh.
}

void CStargateEvent::InputDial( inputdata_t & )
{
    if ( m_bLocked )
        return;
    m_iChevronsLocked = 0;
    m_OnDialStart.FireOutput( this, this );
    SetThink( &CStargateEvent::DialThink );
    SetNextThink( gpGlobals->curtime + StargateTC::DIAL_SYMBOL_TIME );
}

void CStargateEvent::InputForceClose( inputdata_t & )
{
    SetThink( NULL );
    m_iChevronsLocked = 0;
    m_OnWormholeClose.FireOutput( this, this );
}

void CStargateEvent::InputLock( inputdata_t & )
{
    m_bLocked = true;
    SetThink( NULL );
}

void CStargateEvent::DialThink()
{
    m_iChevronsLocked++;
    m_OnChevronLock.Set( m_iChevronsLocked, this, this );

    if ( m_iChevronsLocked >= StargateTC::DIAL_SYMBOL_COUNT )
    {
        m_OnKawoosh.FireOutput( this, this );
        SetThink( &CStargateEvent::WormholeOpenThink );
        SetNextThink( gpGlobals->curtime + StargateTC::KAWOOSH_DURATION );
        return;
    }
    SetNextThink( gpGlobals->curtime + StargateTC::DIAL_SYMBOL_TIME );
}

void CStargateEvent::WormholeOpenThink()
{
    m_OnWormholeOpen.FireOutput( this, this );
    SetThink( NULL );
}
