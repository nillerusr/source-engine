//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerInfoPanel.h"
#include "MapCycleEditDialog.h"

#include <vgui/ISystem.h>

#include <ctype.h>
#include <stdio.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerInfoPanel::CServerInfoPanel(vgui::Panel *parent, const char *name) : CVarListPropertyPage(parent, name)
{
	LoadControlSettings("Admin/GamePanelInfo.res", "PLATFORM");
	LoadVarList("Admin/MainServerConfig.vdf");
	m_iLastUptimeDisplayed = 0;
	m_flUpdateTime = 0.0f;
	m_bMapListRetrieved = false;
	RemoteServer().AddServerMessageHandler(this, "UpdatePlayers");
	RemoteServer().AddServerMessageHandler(this, "UpdateMap");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerInfoPanel::~CServerInfoPanel()
{
	RemoteServer().RemoveServerDataResponseTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: gets the current hostname
//-----------------------------------------------------------------------------
const char *CServerInfoPanel::GetHostname()
{
	return GetVarString("hostname");
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes page if necessary
//-----------------------------------------------------------------------------
void CServerInfoPanel::OnThink()
{
	if (m_flUpdateTime < vgui::system()->GetFrameTime())
	{
		OnResetData();
	}

	// check uptime count
	int time = m_iLastUptimeReceived + (int)(vgui::system()->GetFrameTime() - m_flLastUptimeReceiveTime);
	if (time != m_iLastUptimeDisplayed)
	{
		m_iLastUptimeDisplayed = time;
		char timeText[64];
		_snprintf(timeText, sizeof(timeText), "%0.1i:%0.2i:%0.2i:%0.2i", (time / 3600) / 24, (time / 3600), (time / 60) % 60, time % 60);
		SetControlString("UpTimeText", timeText);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Data is be reloaded from document into controls.
//-----------------------------------------------------------------------------
void CServerInfoPanel::OnResetData()
{
	// only ask for maps list once
	if (!m_bMapListRetrieved)
	{
		RemoteServer().RequestValue(this, "maplist");
		m_bMapListRetrieved = true;
	}

	// refresh our vars
	RefreshVarList();

	// ask for the constant data
	RemoteServer().RequestValue(this, "playercount");
	RemoteServer().RequestValue(this, "maxplayers");
	RemoteServer().RequestValue(this, "gamedescription");
	RemoteServer().RequestValue(this, "uptime");
	RemoteServer().RequestValue(this, "ipaddress");

	// update once every minute
	m_flUpdateTime = (float)system()->GetFrameTime() + (60 * 1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: handles responses from the server
//-----------------------------------------------------------------------------
void CServerInfoPanel::OnServerDataResponse(const char *value, const char *response)
{
	if (!stricmp(value, "playercount"))
	{
		m_iPlayerCount = atoi(response);
	}
	else if (!stricmp(value, "maxplayers"))
	{
		m_iMaxPlayers = atoi(response);

		// update control
		char buf[128];
		buf[0] = 0;
		if (m_iMaxPlayers > 0)
		{
			sprintf(buf, "%d / %d", m_iPlayerCount, m_iMaxPlayers);
		}
		SetControlString("PlayersText", buf);
	}
	else if (!stricmp(value, "gamedescription"))
	{
		SetControlString("GameText", response);
	}
	else if (!stricmp(value, "hostname"))
	{
		PostActionSignal(new KeyValues("UpdateTitle"));
	}
	else if (!stricmp(value, "UpdateMap") || !stricmp(value, "UpdatePlayers"))
	{
		// server has indicated a change, force an update
		m_flUpdateTime = 0.0f;
	}
	else if (!stricmp(value, "maplist"))
	{
		SetCustomStringList("map", response);
		// save off maplist for use in mapcycle editing
		ParseIntoMapList(response, m_AvailableMaps);
		// don't chain through, not in varlist
		return;
	}
	else if (!stricmp(value, "uptime"))
	{
		// record uptime for extrapolation
		m_iLastUptimeReceived = atoi(response);
		m_flLastUptimeReceiveTime = (float)system()->GetFrameTime();
	}
	else if (!stricmp(value, "ipaddress"))
	{
		SetControlString("ServerIPText", response);
	}
	else if (!stricmp(value, "mapcycle"))
	{
		ParseIntoMapList(response, m_MapCycle);
		UpdateMapCycleValue();
		// don't chain through, we set the value ourself
		return;
	}

	// always chain through, in case this variable is in the list as well
	BaseClass::OnServerDataResponse(value, response);

	// post update
	if (!stricmp(value, "map"))
	{
		// map has changed, update map cycle view
		UpdateMapCycleValue();
	}
}

//-----------------------------------------------------------------------------
// Purpose: special editing of map cycle list
//-----------------------------------------------------------------------------
void CServerInfoPanel::OnEditVariable(KeyValues *rule)
{
	if (!stricmp(rule->GetName(), "mapcycle"))
	{
		CMapCycleEditDialog *dlg = new CMapCycleEditDialog(this, "MapCycleEditDialog");
		dlg->Activate(this, m_AvailableMaps, m_MapCycle);
	}
	else
	{
		BaseClass::OnEditVariable(rule);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the value shown in the mapcycle field
//-----------------------------------------------------------------------------
void CServerInfoPanel::UpdateMapCycleValue()
{
	// look at current map
	CUtlSymbol currentMap = GetVarString("map");
	if (!currentMap.IsValid())
		return;

	// find it in the map cycle list
	int listPoint = -1;
	for (int i = 0; i < m_MapCycle.Count(); i++)
	{
		if (!stricmp(m_MapCycle[i].String(), currentMap.String()))
		{
			listPoint = i;
		}
	}

	// take out the next 2 maps and make them the value
	char nextMaps[512];
	nextMaps[0] = 0;
	bool needComma = false;
	for (int i = 0; i < 2; i++)
	{
		int point = listPoint + i + 1;
		if (point >= m_MapCycle.Count())
		{
			point -= m_MapCycle.Count();
		}

		if (m_MapCycle.IsValidIndex(point))
		{
			if (needComma)
			{
				strcat(nextMaps, ", ");
			}
			strcat(nextMaps, m_MapCycle[point].String());
			needComma = true;
		}
	}

	// add some elipses to show there is more maps
	if (needComma)
	{
		strcat(nextMaps, ", ");
	}
	strcat(nextMaps, "...");

	// show in varlist
	SetVarString("mapcycle", nextMaps);
}

//-----------------------------------------------------------------------------
// Purpose: Seperates out a newline-seperated map list into an array
//-----------------------------------------------------------------------------
void CServerInfoPanel::ParseIntoMapList(const char *maplist, CUtlVector<CUtlSymbol> &mapArray)
{
	mapArray.RemoveAll();

	const char *parse = maplist;
	while (*parse)
	{
		// newline-seperated map list
		//!! this should be done with a more standard tokenizer
		if (isspace(*parse))
		{
			parse++;
			continue;
		}

		// pull out the map name
		const char *end = strstr(parse, "\n");
		const char *end2 = strstr(parse, "\r");
		if (end && end2 && end2 < end)
		{
			end = end2;
		}
		if (!end)
			break;

		char customString[64];
		int nameSize = end - parse;
		if (nameSize >= sizeof(customString))
		{
			nameSize = sizeof(customString) - 1;
		}

		// copy in the name
		strncpy(customString, parse, nameSize);
		customString[nameSize] = 0;
		parse = end;

		// add to the list string that aren't comments
		if (nameSize > 0 && !(customString[0] == '/' && customString[1] == '/'))
		{
			int i = mapArray.AddToTail();
			mapArray[i] = customString;
		}
	}
}

