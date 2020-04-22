//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef RULESINFOMSGHANDLERDETAILS_H
#define RULESINFOMSGHANDLERDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "Socket.h"
#include "utlvector.h"

class CRulesInfo;

#include <VGUI_PropertyPage.h>
#include <VGUI_Frame.h>
#include <VGUI_ListPanel.h>
#include <VGUI_KeyValues.h>


//-----------------------------------------------------------------------------
// Purpose: Socket handler for pinging internet servers
//-----------------------------------------------------------------------------
class CRulesInfoMsgHandlerDetails : public CMsgHandler
{
public:
	CRulesInfoMsgHandlerDetails(CRulesInfo *baseobject, HANDLERTYPE type, void *typeinfo = NULL);

	virtual bool Process(netadr_t *from, CMsgBuffer *msg);

private:
	// the parent class that we push info back to
	CRulesInfo *m_pRulesInfo;
	CUtlVector<vgui::KeyValues *> *m_vRules;
};




#endif // RULESINFOMSGHANDLERDETAILS_H
