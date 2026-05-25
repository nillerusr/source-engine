//============= Copyright © StargateTC Source Team =============================
//
// Shared constants for the Stargate gameplay layer. Mirror these in any client-
// side prediction code. Numbers come from the original GoldSrc mod's
// `dlls/sg_*.cpp` files and from gameplay testing notes in
// `stargatetc-portable-main/HPB_bot_ReadMe.txt`.
//
//==============================================================================
#ifndef SG_CONSTANTS_H
#define SG_CONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

namespace StargateTC
{
    // Weapons
    constexpr float ZAT_DAMAGE_FIRST_SHOT   = 35.0f;  // stuns
    constexpr float ZAT_DAMAGE_SECOND_SHOT  = 999.0f; // kills
    constexpr float ZAT_REFIRE_TIME         = 0.9f;
    constexpr float ZAT_RANGE               = 2048.0f;

    constexpr float STAFF_DAMAGE_DIRECT     = 60.0f;
    constexpr float STAFF_DAMAGE_RADIUS     = 30.0f;
    constexpr float STAFF_RADIUS            = 96.0f;
    constexpr float STAFF_REFIRE_TIME       = 1.2f;

    // Dial / event
    constexpr float DIAL_SYMBOL_TIME        = 1.6f;   // per chevron lock
    constexpr int   DIAL_SYMBOL_COUNT       = 7;
    constexpr float KAWOOSH_DURATION        = 1.8f;
    constexpr float KAWOOSH_DAMAGE          = 9999.0f;// instakill anything in volume
    constexpr float KAWOOSH_RADIUS          = 256.0f;
    constexpr float GATE_TRAVEL_COOLDOWN    = 1.0f;

    // Ring transporter
    constexpr float RING_TRAVEL_TIME        = 1.5f;
    constexpr float RING_COOLDOWN           = 4.0f;

    // Sarcophagus
    constexpr float SARC_REGEN_PER_SEC      = 10.0f;
    constexpr float SARC_MAX_HEAL           = 100.0f;

    // Teams (carried through info_player_torri / info_player_goauld)
    enum Team : int
    {
        TEAM_NONE     = 0,
        TEAM_TORRI    = 2,    // Tau'ri / SG-1
        TEAM_GOAULD   = 3,
        TEAM_JAFFA    = 4,
        TEAM_UNAS     = 5,
    };
}

#endif // SG_CONSTANTS_H
