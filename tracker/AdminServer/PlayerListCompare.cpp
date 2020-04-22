//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include <KeyValues.h>
#include <vgui_controls/ListPanel.h>	


int __cdecl PlayerNameCompare(const KeyValues *elem1, const KeyValues *elem2 )
{

	if ( !elem1 || !elem2 )  // No meaningful comparison
	{
		return 0;  
	}

/*	const char *name1 = elem1->GetString("name");
	const char *name2 = elem2->GetString("name");

	return stricmp(name1,name2);
	*/
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Ping comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayerPingCompare(const KeyValues *elem1, const KeyValues *elem2 )
{
	vgui::ListPanelItem *p1, *p2;
	p1 = *(vgui::ListPanelItem **)elem1;
	p2 = *(vgui::ListPanelItem **)elem2;

	if ( !p1 || !p2 )  // No meaningful comparison
	{
		return 0;  
	}

	int ping1 = p1->kv->GetInt("ping");
	int ping2 = p2->kv->GetInt("ping");

	if ( ping1 < ping2 )
		return -1;
	else if ( ping1 > ping2 )
		return 1;

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Ping comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayerAuthCompare(const void *elem1, const void *elem2 )
{
	vgui::ListPanelItem *p1, *p2;
	p1 = *(vgui::ListPanelItem **)elem1;
	p2 = *(vgui::ListPanelItem **)elem2;

	if ( !p1 || !p2 )  // No meaningful comparison
	{
		return 0;  
	}

	const char *authid1 = p1->kv->GetString("authid");
	const char *authid2 = p2->kv->GetString("authid");

	return stricmp(authid1,authid2);
}




//-----------------------------------------------------------------------------
// Purpose: Loss comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayerLossCompare(const void *elem1, const void *elem2 )
{
	vgui::ListPanelItem *p1, *p2;
	p1 = *(vgui::ListPanelItem **)elem1;
	p2 = *(vgui::ListPanelItem **)elem2;

	if ( !p1 || !p2 )  // No meaningful comparison
	{
		return 0;  
	}

	int loss1 = p1->kv->GetInt("loss");
	int loss2 = p2->kv->GetInt("loss");


	if ( loss1 < loss2 )
		return -1;
	else if ( loss1 > loss2 )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Frags comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayerFragsCompare(const void *elem1, const void *elem2 )
{
	vgui::ListPanelItem *p1, *p2;
	p1 = *(vgui::ListPanelItem **)elem1;
	p2 = *(vgui::ListPanelItem **)elem2;

	if ( !p1 || !p2 )  // No meaningful comparison
	{
		return 0;  
	}

	int frags1 = p1->kv->GetInt("frags");
	int frags2 = p2->kv->GetInt("frags");


	if ( frags1 < frags2 )
		return -1;
	else if ( frags1 > frags2 )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Player connection time comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayerTimeCompare( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int h1 = 0, h2 = 0, m1 = 0, m2 = 0, s1 = 0, s2 = 0;
	float t1=0,t2=0;

	const char *time1 = item1.kv->GetString("time");
	const char *time2 = item2.kv->GetString("time");

	int numFields1 = sscanf(time1,"%i:%i:%i",&h1,&m1,&s1);
	int numFields2 = sscanf(time2,"%i:%i:%i",&h2,&m2,&s2);

	switch ( numFields1 )
	{
	case 2:
		s1 = m1;
		m1 = h1;
		h1 = 0;
		break;
	case 1:
		s1 = h1;
		m1 = 0;
		h1 = 0;
		break;
	case 0:
		s1 = 0;
		m1 = 0;
		h1 = 0;
	}

	switch ( numFields2 )
	{
	case 2:
		s2 = m2;
		m2 = h2;
		h2 = 0;
		break;
	case 1:
		s2 = h2;
		m2 = 0;
		h2 = 0;
		break;
	case 0:
		s2 = 0;
		m2 = 0;
		h2 = 0;
	}

	t1=(float)(h1*3600+m1*60+s1);
	t2=(float)(h2*3600+m2*60+s2);
	
	if ( t1 < t2 )
		return -1;
	else if ( t1 > t2 )
		return 1;

	return 0;
}