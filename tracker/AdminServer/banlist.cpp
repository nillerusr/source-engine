//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#include "banlist.h"
#include "Iresponse.h"

#include "PlayerMsgHandler.h"
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

CBanList::CBanList(IResponse *target,serveritem_t &server, const char *rconPassword) {
	
	memcpy(&m_Server, &server,sizeof(serveritem_t));
	m_pResponseTarget=target;

	m_bIsRefreshing=false;
	m_bNewBanList=false;
	m_bRconFailed=false;
	m_bGotIPs = false;

	v_strncpy(m_szRconPassword,rconPassword,100);
	
	m_pRcon = new CRcon(this  , server,rconPassword);
	m_BanList=NULL;
}

CBanList::~CBanList() {
	delete m_pRcon;
}


//-----------------------------------------------------------------------------
// Purpose: sends a status query packet to a single server
//-----------------------------------------------------------------------------
void CBanList::SendQuery()
{
	// now use "rcon status" to pull the list of bans
	m_pRcon->SendRcon("listid");
	m_bGotIPs=false;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBanList::RunFrame()
{
	if(m_pRcon) 
	{
		m_pRcon->RunFrame();
	}

}


void CBanList::ServerResponded()
{

	if(m_bGotIPs == false) 
	{
		m_BanList.RemoveAll();

		const char *rconResp = m_pRcon->RconResponse();
		const char *cur = strstr(rconResp,"UserID filter list:")
								+ strlen("UserID filter list:");

		if( (unsigned int)cur == strlen("UserID filter list:")) 
			// if strstr returned NULL and we added a strlen to it...
		{
			cur=NULL;
		}
	// listid format:
	//	UserID filter list:
	// 1 23434 : 20.000 min

	// and on empty it produces:
	//	IP filter list: empty

		while(cur!=NULL) 
		{

			TokenLine banLine;
			cur++; // dodge the newline
			banLine.SetLine(cur); // need to add one here, to remove the "\n"
			if(banLine.CountToken() >= 4 ) 
			{
				Bans_t ban;

				memset(&ban,0x0,sizeof(Bans_t));
	
				v_strncpy(ban.name,banLine.GetToken(1),20);
				sscanf(banLine.GetToken(3),"%f",&ban.time);
				ban.type= ID;
				m_BanList.AddToTail(ban);
	
			}
			cur=strchr(cur,'\n');
		
		}
		
		m_bGotIPs=true;

		// now find out the list of banned ips
		m_pRcon->SendRcon("listip");
	} 
	else 
	{

	// listip format:
	//	IP filter list:
	//192.168.  1. 66 : 20.000 min

			const char *rconResp = m_pRcon->RconResponse();
		const char *cur = strstr(rconResp,"IP filter list:")
								+ strlen("IP filter list:");

		if( (unsigned int)cur == strlen("IP filter list:")) 
			// if strstr returned NULL and we added a strlen to it...
		{
			cur=NULL;
		}


		while(cur!=NULL) 
		{

			char tmpStr[512]; 
			Bans_t ban;
	
			cur++; // dodge past the newline
			v_strncpy(tmpStr,cur,512);

			memset(&ban,0x0,sizeof(Bans_t));
	
			if( strchr(tmpStr,':') != NULL )
			{
				char *time; 
				time = strchr(tmpStr,':')+1;
				tmpStr[strchr(tmpStr,':')-tmpStr]=0;
				
				v_strncpy(ban.name,tmpStr,20);
				unsigned int i=0;
				while(i<strlen(ban.name)) // strip all the white space out...
				{
					if( ban.name[i]==' ') 
					{
						strcpy(&ban.name[i],&ban.name[i+1]);
					} 
					else 
					{
						i++;
					}
				}

				sscanf(time,"%f",&ban.time);
				ban.type= IP;
				m_BanList.AddToTail(ban);
	
			}
			cur=strchr(cur,'\n');
		
		}
		
		m_bNewBanList=true;
		m_bIsRefreshing=false;




		// notify the UI of the new server info
		m_pResponseTarget->ServerResponded();
	}

}

void CBanList::ServerFailedToRespond()
{
	m_bNewBanList=true;
	m_bIsRefreshing=false;

	if(m_bRconFailed==false)
	{
		m_bRconFailed=true;
	//	CDialogKickPlayer *box = new CDialogKickPlayer();
		//box->addActionSignalTarget(this);
	//	box->Activate("","Bad Rcon Password","badrcon");
	}

//	m_pResponseTarget->ServerFailedToRespond();
}

void CBanList::Refresh() 
{
	SendQuery();
}

bool CBanList::IsRefreshing() 
{
	return m_bIsRefreshing;
}

serveritem_t &CBanList::GetServer() 
{
	return m_Server;
}


bool CBanList::NewBanList() 
{
	return m_bNewBanList;
}

CUtlVector<Bans_t> *CBanList::GetBanList() 
{
	m_bNewBanList=false;
	return &m_BanList;
}

void CBanList::SetPassword(const char *newPass) 
{
	m_pRcon->SetPassword(newPass);
	m_bRconFailed=false;
}