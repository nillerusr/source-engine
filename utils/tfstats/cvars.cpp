//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of CCVarList
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cvars.h"

//------------------------------------------------------------------------------------------------------
// Function:	CCVarList::writeHTML
// Purpose:	generates and writes the report. generate() is not used in this object
// because there is no real intermediate data. Really data is just taken from the 
// event list, massaged abit, and written out to the html file. There is no calculation
// of stats or figures so no intermedidate data creation is needed.
// Input:	html - the html file that we want to write to. 
//------------------------------------------------------------------------------------------------------
void CCVarList::writeHTML(CHTMLFile& html)
{
	CEventListIterator it;
	html.write("<table border=0 width=100%% cols=%li><tr>\n",HTML_TABLE_NUM_COLS);
	bool startOfRow=true;
	for (it=g_pMatchInfo->eventList()->begin(); it != g_pMatchInfo->eventList()->end(); ++it)
	{
		if ((*it)->getType()==CLogEvent::CVAR_ASSIGN)
		{
			char var[100];
			char val[100];
			
			if (!(*it)->getArgument(0) || !(*it)->getArgument(1))
				return;

			(*it)->getArgument(0)->getStringValue(var);
			(*it)->getArgument(1)->getStringValue(val);
			

			//mask off any passwords that the server op may not want to be displayed
			if (stricmp(var,"rcon_password")==0 || stricmp(var,"sv_password")==0 || stricmp(var,"password")==0)
			{
				html.write("<!-- %s not shown! -->\n",var);
				continue;
			}

			if (startOfRow)
			{
				html.write("</tr>\n<tr>");
				startOfRow=false;
			}
			else
				startOfRow=true;

			html.write("\t<td><font class=cvar> %s = %s </font> </td>\n",var,val);
			
		}
	}
	html.write("</tr></table>");
}

