//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: queries server for the command list, and then use QueryCommand() to see
//			if the server supports this command. 
//
// $NoKeywords: $
//=============================================================================

#include <ctype.h> // isspace() define
#include <string.h>

#include "CMDList.h"
#include "Iresponse.h"

#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"
#include "inetapi.h"
#include "TokenLine.h"
#include "dialogkickplayer.h"

extern void v_strncpy(char *dest, const char *src, int bufsize);

typedef enum
{
	NONE = 0,
	INFO_REQUESTED,
	INFO_RECEIVED
} RCONSTATUS;


typedef enum 
{
	FS,
	PAK
} MAP_TYPES;

CCMDList::CCMDList(IResponse *target,serveritem_t &server, const char *rconPassword) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));

	m_bGotCommands = false;
	m_pResponseTarget=target;

	v_strncpy(m_szRconPassword,rconPassword,100);
	
	m_pRcon = new CRcon(this  , server,rconPassword);
	m_pRcon->SendRcon("cmdlist");
	m_CMDList.RemoveAll();
}

CCMDList::~CCMDList() {
	delete m_pRcon;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCMDList::RunFrame()
{
	if(m_pRcon) 
	{
		m_pRcon->RunFrame();
	}
}


void CCMDList::ServerResponded()
{
	char store[2048];
	strcpy(store, m_pRcon->RconResponse());
	char *cur=store;
	char *next=NULL;
	char *cmd=NULL;
	bool cmd_end=false;

// response format:
//Command List
//--------------
//_unloadmodule
// ...
//	writeip
//--------------
//125 Total Commands
//CmdList ? for syntax

	while(cur!=NULL) 
	{	
		if(next!=NULL)
		{	
			cur++;
		}
		next=strchr(cur,'\n');
		if(next!=NULL) 
		{
			*next='\0';
		}
		if( strncmp(cur,"Command List",12) && strncmp(cur,"-------------",13)  
			&& strncmp(cur,"Total Commands",14) && strncmp(cur,"CmdList ? for syntax",20)  )
		{
			char *removeWhiteSpace=cur;
			while(!isspace(*removeWhiteSpace) && removeWhiteSpace<next)
			{
				removeWhiteSpace++;
			}
			*removeWhiteSpace='\0';

			cmd = new char[strlen(cur)];
			if(cmd)
			{
				strcpy(cmd,cur);
				m_CMDList.AddToTail(cmd);
			}
		} 
		else if ( ! strncmp(cur,"CmdList ? for syntax",20))
		{
			cmd_end=true;
		}
		cur=next;
	}
	
	if( cmd_end )
	{
		m_bGotCommands = true;
		m_pResponseTarget->ServerResponded();

	}

}

void CCMDList::ServerFailedToRespond()
{
	//m_pResponseTarget->ServerFailedToRespond();
}


serveritem_t &CCMDList::GetServer() 
{
	return m_Server;
}


bool CCMDList::QueryCommand(char *cmd) 
{
	if(!m_bGotCommands)
		return false;


	for(int i=0;i<m_CMDList.Count();i++)
	{
		char *cmd_in = m_CMDList[i];
		if(!stricmp(cmd,m_CMDList[i]))
			break;
	}
	if(i!=m_CMDList.Count())
	{
		return true;
	}
	else
		return false;

}

bool CCMDList::GotCommands()
{
	return m_bGotCommands;
}

void CCMDList::SetPassword(const char *newPass) 
{
	m_pRcon->SetPassword(newPass);
	m_bGotCommands = false;
	m_CMDList.RemoveAll();

	m_pRcon->SendRcon("cmdlist");
}