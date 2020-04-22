//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "mapslist.h"
#include "Iresponse.h"

#include "Socket.h"
#include "proto_oob.h"
#include "DialogGameInfo.h"
#include "inetapi.h"
#include "TokenLine.h"
#include "dialogkickplayer.h"
#include <string.h>

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

CMapsList::CMapsList(IResponse *target,serveritem_t &server, const char *rconPassword,const char *mod) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;
	
	if(strcmp(mod,"valve") )
	{
		v_strncpy(m_sMod,mod,64);
	}
	else
	{
		m_sMod[0]=0; // its the "valve" default mod, list all maps
	}

	m_bIsRefreshing=false;
	m_bNewMapsList=false;
	m_bRconFailed=false;

	v_strncpy(m_szRconPassword,rconPassword,100);
	
	m_pRcon = new CRcon(this  , server,rconPassword);
}

CMapsList::~CMapsList() {
	delete m_pRcon;
}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CMapsList::SendQuery()
{
	m_bIsRefreshing=true;
	m_pRcon->SendRcon("maps *");	
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapsList::RunFrame()
{
	if(m_pRcon) 
	{
		m_pRcon->RunFrame();
	}
}


void CMapsList::ServerResponded()
{
	char store[2048];
	strcpy(store, m_pRcon->RconResponse());
	char *cur=store;
	char *next=NULL;
	int i=0,k=0;
	bool checkDir=false;

// maps format:
//	Dir:  valve
//-------------
//(fs) rapidcore.bsp
//(fs) frenzy.bsp
//(fs) crossfire.bsp
//
//Dir:  valve
//-------------
//(pak) boot_camp.bsp
//(pak) bounce.bsp

	m_MapsList.RemoveAll();

	char check[21];
	_snprintf(check,21,"%s/",m_sMod);
	if(strstr(store,check))
	{ // if the name of the mod dir is in the return list its old style format
		checkDir=true;
	}


	while(cur!=NULL) 
	{	
		cur++;
		next=strchr(cur,'\n');
		if(next!=NULL) 
		{
			*next='\0';
		}

		if( strncmp(cur,"Dir:",4) && strncmp(cur,"-------------",13)  )
		{	
			TokenLine mapsLine;
			mapsLine.SetLine(cur);
		
			if(mapsLine.CountToken() >= 2 ) 
			{
				char tmpMap[100];
				v_strncpy(tmpMap,mapsLine.GetToken(1),100); // type
				char *end = strstr(tmpMap,".bsp"); // cull the .bsp
				if(end != NULL)  
				{
					*end='\0';
				}
				if(checkDir==true && strstr(tmpMap,m_sMod)==NULL ) 
					// if the map name doesn't have the mod dir in it 
					// and its not running the "valve" mod continue
				{
					cur=next;
					continue;
				}

				if ( strchr(tmpMap,'/') )  // remove the directory part of the map name
				{ 
					strcpy(tmpMap,strrchr(tmpMap,'/')+1);	
				}

				for(k=0;k<m_MapsList.Count();k++)  // now check it doesn't already exist inside the map list...
				{
					if(!strncmp(tmpMap,m_MapsList[k].name,strlen(tmpMap)) )
					{
						break;
					}
				}
				if(k==m_MapsList.Count()	// the map wasn't found
					&& !(tmpMap[0]=='c' && (tmpMap[1]>='1' && tmpMap[1]<='5'))	// a heuristic to remove single player maps
						&& !(tmpMap[0]=='t' && tmpMap[1]=='0') )				// from the list
				{
					Maps_t map;
					// this map wasn't found already
					v_strncpy(map.name,tmpMap,strlen(tmpMap)+1);
					if( !strcmp("(fs)",mapsLine.GetToken(0)) )
					{ // name
						map.type=FS;
					}
					else
					{
						map.type=PAK;
					}
					m_MapsList.AddToTail(map);
					i++;
				} // if k == Count

			} // CountToken
		} // if not a Dir: or "----------"
		cur=next;
	}

	m_bNewMapsList=true;
	m_bIsRefreshing=false;

	// notify the UI of the new server info
	m_pResponseTarget->ServerResponded();

}

void CMapsList::ServerFailedToRespond()
{
	// rcon failed 
	//m_pResponseTarget->ServerFailedToRespond();
}

void CMapsList::Refresh() 
{
	SendQuery();
}

bool CMapsList::IsRefreshing() 
{

	return m_bIsRefreshing;
}

serveritem_t &CMapsList::GetServer() 
{
	return m_Server;
}


bool CMapsList::NewMapsList() 
{
	return m_bNewMapsList;
}

CUtlVector<Maps_t> *CMapsList::GetMapsList() 
{
	m_bNewMapsList=false;
	return &m_MapsList;
}

void CMapsList::SetPassword(const char *newPass) 
{
	m_pRcon->SetPassword(newPass);
	m_bRconFailed=false;
}