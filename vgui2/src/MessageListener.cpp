//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "IMessageListener.h"
#include "VPanel.h"
#include "vgui_internal.h"

#include <KeyValues.h>
#include "vgui/IClientPanel.h"
#include "vgui/IVGui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace vgui
{

//-----------------------------------------------------------------------------
// Implementation of the message listener
//-----------------------------------------------------------------------------
class CMessageListener : public IMessageListener
{
public:
	virtual void Message( VPanel* pSender, VPanel* pReceiver, KeyValues* pKeyValues, MessageSendType_t type );
};

void CMessageListener::Message( VPanel* pSender, VPanel* pReceiver, KeyValues* pKeyValues, MessageSendType_t type )
{
	char const *pSenderName = "NULL";
	if (pSender)
		pSenderName = pSender->Client()->GetName();

	char const *pSenderClass = "NULL";
	if (pSender)
		pSenderClass = pSender->Client()->GetClassName();

	char const *pReceiverName = "unknown name";
	if (pReceiver)
		pReceiverName = pReceiver->Client()->GetName();

	char const *pReceiverClass = "unknown class";
	if (pReceiver)
		pReceiverClass = pReceiver->Client()->GetClassName();

	// FIXME: Make a bunch of filters here
	
	// filter out key focus messages
	if (!strcmp (pKeyValues->GetName(), "KeyFocusTicked"))
	{
		return;
	}
	// filter out mousefocus messages		
	else if (!strcmp (pKeyValues->GetName(), "MouseFocusTicked"))	
	{
		return;
	}
	// filter out cursor movement messages
	else if (!strcmp (pKeyValues->GetName(), "CursorMoved"))
	{
		return;
	}
	// filter out cursor entered messages
	else if (!strcmp (pKeyValues->GetName(), "CursorEntered"))
	{
		return;
	}
	// filter out cursor exited messages
	else if (!strcmp (pKeyValues->GetName(), "CursorExited"))
	{
		return;
	}
	// filter out MouseCaptureLost messages
	else if (!strcmp (pKeyValues->GetName(), "MouseCaptureLost"))
	{
		return;
	}
	// filter out MousePressed messages
	else if (!strcmp (pKeyValues->GetName(), "MousePressed"))
	{
		return;
	}
	// filter out MouseReleased messages
	else if (!strcmp (pKeyValues->GetName(), "MouseReleased"))
	{
		return;
	}
	// filter out Tick messages
	else if (!strcmp (pKeyValues->GetName(), "Tick"))
	{
		return;
	}

	Msg( "%s : (%s (%s) - > %s (%s)) )\n", 
	pKeyValues->GetName(), pSenderClass, pSenderName, pReceiverClass, pReceiverName );
		
}


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CMessageListener s_MessageListener;
IMessageListener *MessageListener()
{
	return &s_MessageListener;
}

}