//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERINFOPANEL_H
#define SERVERINFOPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "VarListPropertyPage.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CServerInfoPanel : public CVarListPropertyPage
{
public:
	CServerInfoPanel(vgui::Panel *parent, const char *name);
	~CServerInfoPanel();

	// gets the current hostname
	const char *GetHostname();

	// Called when page is loaded.  Data should be reloaded from document into controls.
	virtual void OnResetData();

protected:
	// vgui overrides
	virtual void OnThink();

	// special editing of map cycle list
	virtual void OnEditVariable(KeyValues *rule);

	// handles responses from the server
	virtual void OnServerDataResponse(const char *value, const char *response);

private:
	void UpdateMapCycleValue();

	float m_flUpdateTime;
	int m_iPlayerCount, m_iMaxPlayers;
	float m_flLastUptimeReceiveTime;
	long m_iLastUptimeReceived;
	long m_iLastUptimeDisplayed;

	bool m_bMapListRetrieved;

	// used to store some strings for mapcycle
	CUtlVector<CUtlSymbol> m_AvailableMaps;
	CUtlVector<CUtlSymbol> m_MapCycle;
	void ParseIntoMapList(const char *maplist, CUtlVector<CUtlSymbol> &mapArray);

	typedef CVarListPropertyPage BaseClass;
};

#endif // SERVERINFOPANEL_H