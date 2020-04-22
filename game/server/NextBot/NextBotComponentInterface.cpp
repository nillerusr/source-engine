// NextBotComponentInterface.cpp
// Implentation of system methods for NextBot component interface
// Author: Michael Booth, May 2006
//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"

#include "NextBotInterface.h"
#include "NextBotComponentInterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

INextBotComponent::INextBotComponent( INextBot *bot )
{
	m_curInterval = TICK_INTERVAL;
	m_lastUpdateTime = 0;
	m_bot = bot;
	
	// register this component with the bot
	bot->RegisterComponent( this );
}


