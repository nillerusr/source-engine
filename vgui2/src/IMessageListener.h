//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IMESSAGELISTENER_H
#define IMESSAGELISTENER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

class KeyValues;

namespace vgui
{

enum MessageSendType_t
{
	MESSAGE_SENT = 0,
	MESSAGE_POSTED,
	MESSAGE_RECEIVED
};

class VPanel;


class IMessageListener
{
public:
	virtual void Message( VPanel* pSender, VPanel* pReceiver, 
		KeyValues* pKeyValues, MessageSendType_t type ) = 0;
};

IMessageListener* MessageListener();

}

#endif // IMESSAGELISTENER_H
