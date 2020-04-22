//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef TMESSAGE_H
#define TMESSAGE_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DEMO_MESSAGE "__DEMOMESSAGE__"
#define NETWORK_MESSAGE1 "__NETMESSAGE__1"
#define NETWORK_MESSAGE2 "__NETMESSAGE__2"
#define NETWORK_MESSAGE3 "__NETMESSAGE__3"
#define NETWORK_MESSAGE4 "__NETMESSAGE__4"
#define NETWORK_MESSAGE5 "__NETMESSAGE__5"
#define NETWORK_MESSAGE6 "__NETMESSAGE__6"

#define MAX_NETMESSAGE	6

#include "client_textmessage.h"

extern client_textmessage_t	*gMessageTable;
extern int					gMessageTableCount;

extern client_textmessage_t	gNetworkTextMessage[MAX_NETMESSAGE];
extern char					gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
extern const char			*gNetworkMessageNames[MAX_NETMESSAGE];

// text message system
void					TextMessageInit( void );
client_textmessage_t *TextMessageGet( const char *pName );
void					TextMessageShutdown( void );

void TextMessage_DemoMessage( const char *pszMessage, float fFadeInTime, float fFadeOutTime, float fHoldTime );
void TextMessage_DemoMessageFull( const char *pszMessage, client_textmessage_t const *message );

#ifdef __cplusplus
}
#endif // __cplusplus

#endif		//TMESSAGE_H
