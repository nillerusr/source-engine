//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Functions related to quests
//
//=============================================================================

#ifndef ECON_QUESTS
#define ECON_QUESTS
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Given a quest item, return if the quest is considered "unidentified"
//-----------------------------------------------------------------------------
bool IsQuestItemUnidentified( const CEconItem* pQuestItem );
bool IsQuestItemReadyToTurnIn( const IEconItemInterface* pQuestItem );
bool IsQuestItemFullyCompleted( const IEconItemInterface* pQuestItem );
uint32 GetEarnedStandardPoints( const IEconItemInterface* pQuestItem );
uint32 GetEarnedBonusPoints( const IEconItemInterface* pQuestItem );

#endif // ECON_QUESTS