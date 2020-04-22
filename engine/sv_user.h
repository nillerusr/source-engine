//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_USER_H
#define SV_USER_H
#ifdef _WIN32
#pragma once
#endif


// Send a command to the specified client (as though the client typed the command in their console).
class client_t;
void SV_FinishParseStringCommand( client_t *cl, const char *pCommand );


#endif // SV_USER_H
