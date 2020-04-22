//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_CVARS_H
#define DOD_CVARS_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_INTERMISSION_TIME 120

extern ConVar mp_limitteams;
extern ConVar mp_autokick;

extern ConVar mp_clan_restartround;
extern ConVar mp_clan_readyrestart;
extern ConVar mp_clan_ready_signal;
extern ConVar mp_warmup_time;
extern ConVar mp_combinemglimits; 
extern ConVar mp_restartwarmup;
extern ConVar mp_cancelwarmup;
extern ConVar mp_winlimit;

extern ConVar	mp_limitAlliesRifleman;	
extern ConVar	mp_limitAlliesAssault;		
extern ConVar	mp_limitAlliesSupport;		
extern ConVar	mp_limitAlliesSniper;		
extern ConVar	mp_limitAlliesMachinegun;
extern ConVar	mp_limitAlliesRocket;	

extern ConVar	mp_limitAxisRifleman;	
extern ConVar	mp_limitAxisAssault;		
extern ConVar	mp_limitAxisSupport;		
extern ConVar	mp_limitAxisSniper;			
extern ConVar	mp_limitAxisMachinegun;		
extern ConVar	mp_limitAxisRocket;			

#endif //DOD_CVARS_H