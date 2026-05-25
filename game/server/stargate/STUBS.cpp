//============= Copyright © StargateTC Source Team =============================
//
// One-file stubs for the remaining preserved entities. Each just registers its
// classname against CBaseEntity so the engine accepts maps that reference them
// (otherwise vbsp/spawn would log "Spawnfunction for <classname> not found").
//
// Replace these one by one as gameplay is ported — copy a stub out into its
// own file and flesh it out following the dhd_console.cpp / sg_event.cpp
// pattern.
//
//==============================================================================
#include "cbase.h"

#define STUB_ENTITY(classname) \
    class C##classname : public CBaseAnimating \
    { \
    public: \
        DECLARE_CLASS( C##classname, CBaseAnimating ); \
        void Spawn() override \
        { \
            BaseClass::Spawn(); \
            SetSolid( SOLID_VPHYSICS ); \
            SetMoveType( MOVETYPE_NONE ); \
            Msg( "[StargateTC] inert stub spawned: " #classname "\n" ); \
        } \
    }; \
    LINK_ENTITY_TO_CLASS( classname, C##classname )

STUB_ENTITY( stargate_dialer );
STUB_ENTITY( goauld_sarcophagus );
STUB_ENTITY( zat_pickup );
STUB_ENTITY( staff_pickup );
STUB_ENTITY( naqahdah_pickup );

// Weapons — eventually derive from CBaseHL2MPCombatWeapon. For now an inert
// pickup so maps spawn-clean.
STUB_ENTITY( zat_gun );
STUB_ENTITY( staff_gun );
