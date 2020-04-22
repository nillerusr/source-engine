//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

// base vgui interfaces
#include <VGUI_Controls.h>
#include <VGUI_IInput.h>
#include <VGUI_ISurface.h>
#include <VGUI_IScheme.h>
#include <VGUI_IVGui.h>
#include <VGUI_MouseCode.h>
#include "filesystem.h"


// vgui controls
#include <VGUI_Button.h>
#include <VGUI_CheckButton.h>
#include <VGUI_ComboBox.h>
#include <VGUI_FocusNavGroup.h>
#include <VGUI_Frame.h>
#include <VGUI_KeyValues.h>
#include <VGUI_ListPanel.h>
#include <VGUI_MessageBox.h>
#include <VGUI_Panel.h>
#include <VGUI_PropertySheet.h>
#include <VGUI_ToggleButton.h>
#include <VGUI_QueryBox.h>


// serverbrowser headers
#include "inetapi.h"
//#include "msgbuffer.h"
#include "proto_oob.h"
#include "ServerContextMenu.h"
#include "socket.h"
#include "util.h"
#include "vinternetdlg.h"
#include "dialogcvarchange.h"
//#include "ModList.h"
#include "DialogGameInfo.h"
#include "ConfigPanel.h"
 

// game list
#include "FavoriteGames.h"
#include "GamePanelInfo.h"

// tracker stuff
//#include "Tracker.h"
#include "TrackerProtocol.h"
//#include "OnlineStatus.h"


// interface to game engine / tracker
#include "IRunGameEngine.h"

using namespace vgui;

static VInternetDlg *s_InternetDlg = NULL;
CSysModule * g_hTrackerNetModule = NULL;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
VInternetDlg::VInternetDlg( unsigned int userid ) : Frame(NULL, "VInternetDlg")
{
	s_InternetDlg = this;
	
	m_iUserID=userid;
	m_bLoggedIn=false;

	MakePopup();

	m_pSavedData = NULL;

	// create the controls
	m_pContextMenu = new CServerContextMenu(this);
//	m_pContextMenu->SetVisible(false);

	m_pFavoriteGames = new CFavoriteGames(this);

	SetMinimumSize(570, 550);

	m_pGameList = m_pFavoriteGames;

	// property sheet
	m_pTabPanel = new PropertySheet(this, "GameTabs");

	m_pTabPanel->SetTabWidth(150);
//	m_pTabPanel->SetScrolling(true);
	m_pTabPanel->AddPage(m_pFavoriteGames, "My Servers");
	m_pTabPanel->AddActionSignalTarget(this);

	m_pStatusLabel = new Label(this, "StatusLabel", "");

	LoadControlSettings("Admin\\DialogAdminServer.res");

	m_pStatusLabel->SetText("");


	// Setup tracker objects
	// tracker doc
	//g_pTrackerDoc = new CTrackerDoc();

	// create the networking
	/*m_pServerSession = new CServerSession();

	// load networking dll
	char szDLL[_MAX_PATH];

	// now load the net interface so we can use it
	g_pFullFileSystem->GetLocalPath("Friends/TrackerNET.dll", szDLL);
	g_pFullFileSystem->GetLocalCopy(szDLL);
	g_hTrackerNetModule = Sys_LoadModule(szDLL);

	CreateInterfaceFn netFactory = Sys_GetFactory(g_hTrackerNetModule);
	m_pNet = (ITrackerNET *)netFactory(TRACKERNET_INTERFACE_VERSION, NULL);

	m_pNet->Initialize(27030, 27100);
	m_iServerAddr=m_pNet->GetNetAddress("tracker3.valvesoftware.com:1200");
	
	// uncomment this to do the "tracker" magic
	//SendInitialLogin();
	*/

	// load filters
	LoadFilters();
	// load window settings
	LoadDialogState(this, "AdminServer");

	// let us be ticked every frame
	ivgui()->AddTickSignal(this->GetVPanel());


}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
VInternetDlg::~VInternetDlg()
{
	// set a flag indicating the threads should kill themselves
//	m_pNet->Shutdown(false);
//	m_pNet->deleteThis();
//	m_pNet = NULL;

	Sys_UnloadModule(g_hTrackerNetModule);


}

//-----------------------------------------------------------------------------
// Purpose: Called once to set up
//-----------------------------------------------------------------------------
void VInternetDlg::Initialize()
{
	SetTitle("Admin", true);
	SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : serverID - 
// Output : serveritem_t
//-----------------------------------------------------------------------------
serveritem_t &VInternetDlg::GetServer(unsigned int serverID)
{
	return m_pGameList->GetServer(serverID);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VInternetDlg::Open( void )
{	
	m_pTabPanel->RequestFocus();
	// if serverbrowser file is not there we will try to transfer the favorites list.
	FileHandle_t f = g_pFullFileSystem->Open("AdminServer.vdf", "rb");
	if (f)
	{
		g_pFullFileSystem->Close( f );
	}

	
	surface()->SetMinimized(GetVPanel(), false);
	SetVisible(true);
	RequestFocus();
	m_pTabPanel->RequestFocus();
	MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose: relayouts the dialogs controls
//-----------------------------------------------------------------------------
void VInternetDlg::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	
	// game list in middle
	m_pTabPanel->SetBounds(8, y + 8, GetWide() - 16, tall - (28));
	x += 4;

	// status text along bottom
	m_pStatusLabel->SetBounds(x + 2, (tall - y) + 40, wide - 6, 20);
	m_pStatusLabel->SetContentAlignment(Label::a_northwest);

	Repaint();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VInternetDlg::OnClose()
{
	// bug here if you exit before logging in.
	SaveDialogState(this, "AdminServer");
	SaveFilters();
	Frame::OnClose();	
	
}

//-----------------------------------------------------------------------------
// Purpose: Loads filter settings from disk
//-----------------------------------------------------------------------------
void VInternetDlg::LoadFilters()
{
  	// free any old filters
  	if (m_pSavedData)
  	{
  		m_pSavedData->deleteThis();
  	}

	m_pSavedData = new KeyValues ("Filters");
	if (!m_pSavedData->LoadFromFile(g_pFullFileSystem, "Admin\\AdminServer.vdf", true, "PLATFORM"))
	{
		// file not successfully loaded, create the default key
		m_pSavedData->FindKey("List", true);
		m_pSavedData->FindKey("List/Default", true);
		m_pSavedData->SetString("DefaultFilter", "Default");
	}

	// load favorite servers
	KeyValues *favorites = m_pSavedData->FindKey("Favorites", true);
	m_bSaveRcon= m_pSavedData->FindKey("SaveRcon", true)->GetInt();

	m_pFavoriteGames->LoadFavoritesList(favorites,m_bSaveRcon);

	m_bAutoRefresh= m_pSavedData->FindKey("AutoRefresh", true)->GetInt();
	if(!m_bAutoRefresh)
	{
		m_iRefreshTime=0;
	}
	else
	{
		m_iRefreshTime= m_pSavedData->FindKey("RefreshTime", true)->GetInt();
	}

	m_bGraphs= m_pSavedData->FindKey("ShowGraphs", true)->GetInt();
	if(!m_bGraphs)
	{
		m_iGraphsRefreshTime=0;
	}
	else
	{
		m_iGraphsRefreshTime= m_pSavedData->FindKey("GraphsRefreshTime", true)->GetInt();
	}

	m_bDoLogging= m_pSavedData->FindKey("GetLogs", true)->GetInt();


	m_pTabPanel->SetActivePage(m_pFavoriteGames);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VInternetDlg::SaveFilters()
{

	// get the favorites list
	KeyValues *favorites = m_pSavedData->FindKey("Favorites", true);
	m_pFavoriteGames->SaveFavoritesList(favorites,m_bSaveRcon);
	m_pSavedData->SaveToFile(g_pFullFileSystem, "Admin\\AdminServer.vdf", "PLATFORM");

}

void VInternetDlg::SetConfig(bool autorefresh,bool savercon,int refreshtime,bool graphs,int graphsrefreshtime,bool getlogs)
{
	m_bAutoRefresh=autorefresh;
	m_bDoLogging = getlogs;
	m_bSaveRcon=savercon;
	if(m_bAutoRefresh)
	{
		m_iRefreshTime=refreshtime;
	}
	else
	{
		m_iRefreshTime=0;
	}
	
	m_bGraphs = graphs;
	if(graphs)
	{
		m_iGraphsRefreshTime=graphsrefreshtime;
	}
	else
	{
		m_iGraphsRefreshTime=0;
	}

	m_pSavedData->SetInt("AutoRefresh",autorefresh);
	m_pSavedData->SetInt("SaveRcon",savercon);
	m_pSavedData->SetInt("RefreshTime",refreshtime);
	m_pSavedData->SetInt("GraphsRefreshTime",graphsrefreshtime);
	m_pSavedData->SetInt("ShowGraphs",graphs);
	m_pSavedData->SetInt("GetLogs",getlogs);

	
}


//-----------------------------------------------------------------------------
// Purpose: Updates status test at bottom of window
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void VInternetDlg::UpdateStatusText(const char *fmt, ...)
{
	if ( !m_pStatusLabel )
		return;

	char str[ 1024 ];
	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( str, fmt, argptr );
	va_end( argptr );

	m_pStatusLabel->SetText( str );
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a static instance of this dialog
// Output : VInternetDlg
//----------------------------------------------------------------------------
VInternetDlg *VInternetDlg::GetInstance()
{
	return s_InternetDlg;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CServerContextMenu
//-----------------------------------------------------------------------------
CServerContextMenu *VInternetDlg::GetContextMenu()
{
	return m_pContextMenu;
}

//-----------------------------------------------------------------------------
// Purpose: begins the process of joining a server from a game list
//			the game info dialog it opens will also update the game list
//-----------------------------------------------------------------------------
CDialogGameInfo *VInternetDlg::JoinGame(IGameList *gameList, unsigned int serverIndex)
{
	// open the game info dialog, then mark it to attempt to connect right away
	CDialogGameInfo *gameDialog = OpenGameInfoDialog(gameList, serverIndex);

	// set the dialog name to be the server name
	gameDialog->Connect();

	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: joins a game by a specified IP, not attached to any game list
//-----------------------------------------------------------------------------
CDialogGameInfo *VInternetDlg::JoinGame(int serverIP, int serverPort, const char *titleName)
{
	// open the game info dialog, then mark it to attempt to connect right away
	CDialogGameInfo *gameDialog = OpenGameInfoDialog(serverIP, serverPort, titleName);

	// set the dialog name to be the server name
	gameDialog->Connect();

	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: opens a game info dialog from a game list
//-----------------------------------------------------------------------------
CDialogGameInfo *VInternetDlg::OpenGameInfoDialog(IGameList *gameList, unsigned int serverIndex)
{
	CDialogGameInfo *gameDialog = new CDialogGameInfo(gameList, serverIndex);
	serveritem_t &server = gameList->GetServer(serverIndex);
	gameDialog->Run(server.name);
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: opens a game info dialog by a specified IP, not attached to any game list
//-----------------------------------------------------------------------------
CDialogGameInfo *VInternetDlg::OpenGameInfoDialog(int serverIP, int serverPort, const char *titleName)
{
	CDialogGameInfo *gameDialog = new CDialogGameInfo(NULL, 0, serverIP, serverPort);
	gameDialog->Run(titleName);
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Save position and window size of a dialog from the .vdf file.
// Input  : *dialog - panel we are setting position and size 
//			*dialogName -  name of dialog in the .vdf file
//-----------------------------------------------------------------------------
void VInternetDlg::SaveDialogState(Panel *dialog, const char *dialogName)
{
	// write the size and position to the document
	int x, y, wide, tall;
	dialog->GetBounds(x, y, wide, tall);

	KeyValues *data;
	data = m_pSavedData->FindKey(dialogName, true);

	data->SetInt("x", x);
	data->SetInt("y", y);
	data->SetInt("w", wide);
	data->SetInt("t", tall);
}

//-----------------------------------------------------------------------------
// Purpose: Load position and window size of a dialog from the .vdf file.
// Input  : *dialog - panel we are setting position and size 
//			*dialogName -  name of dialog in the .vdf file
//-----------------------------------------------------------------------------
void VInternetDlg::LoadDialogState(Panel *dialog, const char *dialogName)
{						   
	// read the size and position from the document
	KeyValues *data;
	data = m_pSavedData->FindKey(dialogName, true);

	// calculate defaults, center of the screen
	int x, y, wide, tall, dwide, dtall;
	int nx, ny, nwide, ntall;
	vgui::surface()->GetScreenSize(wide, tall);
	dialog->GetSize(dwide, dtall);
	x = (int)((wide - dwide) * 0.5);
	y = (int)((tall - dtall) * 0.5);

	// set dialog
	nx = data->GetInt("x", x);
	ny = data->GetInt("y", y);
	nwide = data->GetInt("w", dwide);
	ntall = data->GetInt("t", dtall);

	// make sure it's on the screen. If it isn't, move it over so it is.
	if (nx + nwide > wide)
	{
		nx = wide - nwide;
	}
	if (ny + ntall > tall)
	{
		ny = tall - ntall;
	}
	if (nx < 0)
	{
		nx = 0;
	}
	if (ny < 0)
	{
		ny = 0;
	}

	dialog->SetBounds(nx, ny, nwide, ntall);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *dest - 
//			*src - 
//			bufsize - 
//-----------------------------------------------------------------------------
void v_strncpy(char *dest, const char *src, int bufsize)
{
	if (src == dest)
		return;

	strncpy(dest, src, bufsize - 1);
	dest[bufsize - 1] = 0;
}

void VInternetDlg::ConfigPanel()
{
		CConfigPanel *config = new CConfigPanel(m_bAutoRefresh,m_bSaveRcon,m_iRefreshTime,m_bGraphs,m_iGraphsRefreshTime,m_bDoLogging);
		config->Run();
}

void VInternetDlg::OnManageServer(int serverID) 
{
	int i;
	serveritem_t &server = m_pFavoriteGames->GetServer(serverID); 
	netadr_t addr;
	memcpy(addr.ip,server.ip,4);
	addr.port=(server.port & 0xff) << 8 | (server.port & 0xff00) >> 8;
	addr.type=NA_IP;

	const char *netString = net->AdrToString(&addr);
	char tabName[20];

	for(i=0;i<m_pTabPanel->GetNumPages();i++)
	{

		m_pTabPanel->GetTabTitle(i,tabName,20);
		if(!stricmp(netString,tabName))
		{
			break;
		}
	}
	
	if(i==m_pTabPanel->GetNumPages())
	{

		if(m_bSaveRcon) 
		{ // rcons are being saved		
			if(strlen(server.rconPassword)>0) 
			{ // this rcon password is already saved :)
				ManageServer(serverID,server.rconPassword);
				return;
			}
		}

		// otherwise ask for an rcon password
		CDialogCvarChange *box = new CDialogCvarChange();
		char id[5];
		_snprintf(id,5,"%i",serverID);
		box->AddActionSignalTarget(this);
		box->SetTitle("Enter Rcon Password",true);
		box->SetLabelText("CvarNameLabel","");
		box->SetLabelText("PasswordLabel","Password:");

		box->MakePassword();
		box->Activate(id, "","rconpassword","Enter Rcon Password for this Server");
	}
	else
	{
		m_pTabPanel->SetActivePage(m_pTabPanel->GetPage(i));
	}
}

void VInternetDlg::OnPlayerDialog(vgui::KeyValues *data)
{
	const char *type=data->GetString("type");
	const char *playerName=data->GetString("player");
	if(!stricmp(type,"rconpassword")) 
	{
		const char *value=data->GetString("value");
		serveritem_t &server = m_pFavoriteGames->GetServer(atoi(playerName)); // we encode the serverid in the name field :)
		strncpy(server.rconPassword,value,sizeof(server.rconPassword)); // save this password

		ManageServer(atoi(playerName),value);
	}
}

void VInternetDlg::ManageServer(int serverID,const char *pass)
{
	serveritem_t &server = m_pFavoriteGames->GetServer(serverID); 
	netadr_t addr;
	memcpy(addr.ip,server.ip,4);
	addr.port=(server.port & 0xff) << 8 | (server.port & 0xff00) >> 8;
	addr.type=NA_IP;

	m_pGamePanelInfo = new CGamePanelInfo(this,"Current Server",server.gameDir,m_iRefreshTime,m_iGraphsRefreshTime,m_bDoLogging);

	m_pTabPanel->AddPage(m_pGamePanelInfo,net->AdrToString(&addr) );

	m_pGamePanelInfo->ChangeGame(server,pass);
	m_pTabPanel->SetActivePage(m_pGamePanelInfo);
}

void VInternetDlg::UpdateServer(serveritem_t &server)
{
	m_pFavoriteGames->UpdateServer(server); 
}

void VInternetDlg::OnDeleteServer(int chosenPanel)
{
	Panel *delPanel =m_pTabPanel->GetPage(chosenPanel);
	m_pTabPanel->DeletePage(delPanel);

	InvalidateLayout();
	Repaint();

}

vgui::PropertySheet *VInternetDlg::GetTabPanel() 
{
	return m_pTabPanel;
}


void VInternetDlg::OnOpenContextMenu()
{
//	CServerContextMenu *menu = VInternetDlg::GetInstance()->GetContextMenu();
		// no selected rows, so don't display default stuff in menu
	if( m_pTabPanel->GetActiveTab()->IsCursorOver() || 
		m_pFavoriteGames->IsCursorOver() )
	{
		m_pContextMenu->ShowMenu(this, -1, false, false, false,false);
	}
}	








void VInternetDlg::OnTick()
{

//FIX ME!!!
	return;

/*

	// get the latest raw messages
	IBinaryBuffer *buf;
	CNetAddress address;
	while ((buf = m_pNet->GetIncomingRawData(address)) != NULL)
	{
		//ReceivedRawData(buf, address);
		buf->Release();
	}

	// get all the latest messages
	IReceiveMessage *recv;
	while ((recv = m_pNet->GetIncomingData()) != NULL)
	{

		// make sure the message is valid
		if (!CheckMessageValidity(recv))
			return;

		// record the reception
	//	m_iLastReceivedTime = m_iTime;

		// find the message id in the dispatch table
		int dataName = recv->GetMsgID();



		switch(dataName) 
		{
		case TSVC_CHALLENGE:
			{
				int ChallengeKey;
				int status = COnlineStatus::ONLINE;
				int heartbeatRate =10000;//GetHeartBeatRate();
			
				recv->ReadInt("challenge", ChallengeKey);
				recv->ReadUInt("sessionID", m_iSessionID);
				// respond to the challenge
				ISendMessage *reply = CreateServerMessage(TCLS_RESPONSE);
				reply->SetSessionID( m_iSessionID );
			


				reply->WriteInt("challenge", ChallengeKey);
				reply->WriteUInt("sessionID", m_iSessionID);
				reply->WriteInt("status", status);
				reply->WriteInt("build", 1994);
				reply->WriteInt("hrate", heartbeatRate);	// heartbeat rate to expect

				//m_iPreviousHeartBeatRateSentToServer = heartbeatRate;

				// reset the login timeout
				//m_iLoginTimeout = system()->getTimeMillis() + COnlineStatus::SERVERCONNECT_TIMEOUT;

				m_pNet->SendMessage(reply, NET_RELIABLE);
				
				}
			break;

		case TSVC_LOGINOK:
			{
				int newStatus;
				recv->ReadInt("status", newStatus);
				m_bLoggedIn=true;

				SearchForFriend(0, "ar@cfgn.net", "", "", "");

			}
			break;
		case TSVC_FRIENDSFOUND:
			{
				//char name[60];

				int serverID,sessionID;
				recv->ReadInt("uid",m_iRemoteUID);	
				recv->ReadInt("serverid",serverID);
				recv->ReadInt("sessionID",sessionID);



				// create the message to the server
				ISendMessage *msg = CreateServerMessage(TCLS_ROUTETOFRIEND);

				// write in the redirection info
				msg->WriteInt("rID", TCL_MESSAGE);
				msg->WriteUInt("rUserID", m_iRemoteUID);
				msg->WriteUInt("rSessionID", sessionID);
				msg->WriteUInt("rServerID", serverID);
				msg->WriteBlob("rData", "Hello", 5);

				m_pNet->SendMessage(msg, NET_RELIABLE);

				// lets log off
				msg = CreateServerMessage(TCLS_HEARTBEAT);
				msg->WriteInt("status", COnlineStatus::OFFLINE);	
				m_pNet->SendMessage(msg, NET_RELIABLE);

			//	m_pNet->Shutdown(true);
			//	m_pNet->deleteThis();
	//			SendStatusToServer(COnlineStatus::OFFLINE);

			}
			break;

		default:
			{
				while(recv->AdvanceField()) 
				{
					char data[512];
					const char *nm=recv->GetFieldName();
					recv->ReadString(nm, data, 512);

				}
			}

			break;
	

		//	{ TSVC_CHALLENGE,	CServerSession::ReceivedMsg_Challenge },
		//	{ TSVC_LOGINOK,		CServerSession::ReceivedMsg_LoginOK },
		//	{ TSVC_LOGINFAIL,	CServerSession::ReceivedMsg_LoginFail },
		//	{ TSVC_DISCONNECT,	CServerSession::ReceivedMsg_Disconnect },
		//	{ TSVC_FRIENDS,		CServerSession::ReceivedMsg_Friends },
		//	{ TSVC_FRIENDUPDATE, CServerSession::ReceivedMsg_FriendUpdate },
		//	{ TSVC_GAMEINFO,	CServerSession::ReceivedMsg_GameInfo },
		//	{ TSVC_HEARTBEAT,	CServerSession::ReceivedMsg_Heartbeat },
		//	{ TSVC_PINGACK,		CServerSession::ReceivedMsg_PingAck },
		

		//default:
		//	break;

		}
		//ReceivedData(recv);
		m_pNet->ReleaseMessage(recv);
	}


	// get the latest fails
	while ((recv = m_pNet->GetFailedMessage()) != NULL)
	{
		

		m_pNet->ReleaseMessage(recv);
	}

	// now let it update itself
	m_pNet->RunFrame();	
	*/
}

void VInternetDlg::SearchForFriend(unsigned int uid, const char *email, const char *username, const char *firstname, const char *lastname)
{
	ISendMessage *msg = CreateServerMessage(TCLS_FRIENDSEARCH);
	msg->WriteUInt("uid", uid);
	msg->WriteString("Email", email);
	msg->WriteString("UserName", username);
	msg->WriteString("FirstName", firstname);
	msg->WriteString("LastName", lastname);

	m_pNet->SendMessage(msg, NET_RELIABLE);
}

ISendMessage *VInternetDlg::CreateServerMessage(int msgID)
{
	ISendMessage *msg = m_pNet->CreateMessage(msgID);
	msg->SetNetAddress(GetServerAddress());
	msg->SetSessionID(m_iSessionID);
	msg->SetEncrypted(true);

	return msg;
}

CNetAddress VInternetDlg::GetServerAddress()
{
	return m_iServerAddr;// m_pNet->GetNetAddress("tracker.valvesoftware.com:1200");
}


//-----------------------------------------------------------------------------
// Purpose: Sends the first pack in the login sequence
//-----------------------------------------------------------------------------
void VInternetDlg::SendInitialLogin()
{
//	assert(m_iLoginState == LOGINSTATE_WAITINGTORECONNECT || m_iLoginState == LOGINSTATE_DISCONNECTED);

	m_iSessionID = 0;

	// stop searching for alternate servers
//	m_bServerSearch = false;

//	int desiredStatus = COnlineStatus::ONLINE;


	// setup the login message
/*	ISendMessage *loginMsg = m_pNet->CreateMessage(TCLS_LOGIN);
	loginMsg->SetNetAddress(GetServerAddress());
	loginMsg->SetEncrypted(true);
	loginMsg->SetSessionID(0);

//	const char *adr= GetServerAddress().ToStaticString();

	loginMsg->WriteUInt("uid", 36283);
	loginMsg->WriteString("email", "alfred@mazuma.net.au");
	loginMsg->WriteString("password", "mrorange");
	loginMsg->WriteInt("status", desiredStatus);

	m_pNet->SendMessage(loginMsg, NET_RELIABLE);
*/
	// set the current status to be a connecting message
//	m_iStatus = COnlineStatus::CONNECTING;

//	m_iLoginState = LOGINSTATE_AWAITINGCHALLENGE;
	// record the time (for timeouts)
//	m_iLoginTimeout = system()->getTimeMillis() + COnlineStatus::SERVERCONNECT_TIMEOUT;


}


//-----------------------------------------------------------------------------
// Purpose: Checks to see if the current message is valid
//			replies with a message telling the sender if it's not
//-----------------------------------------------------------------------------
bool VInternetDlg::CheckMessageValidity(IReceiveMessage *dataBlock)
{
	int msgID = dataBlock->GetMsgID();
	if (msgID == TSVC_FRIENDS || msgID == TSVC_GAMEINFO || msgID == TSVC_HEARTBEAT || msgID == TSVC_FRIENDUPDATE)
	{
		// see if the server really knows us
		if (/*m_iStatus < COnlineStatus::ONLINE ||*/ m_iSessionID != dataBlock->SessionID())
		{
			// the server thinks we're still logged on to it
			// tell the server we're actually logged off from it
			ISendMessage *msg = m_pNet->CreateReply(TCLS_HEARTBEAT, dataBlock);
			// tell it we're the sessionID it thinks we are
			msg->SetSessionID(dataBlock->SessionID());
			msg->WriteInt("status", 0);
			m_pNet->SendMessage(msg, NET_RELIABLE);
			return false;
		}
	}

	return true;
}




//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t VInternetDlg::m_MessageMap[] =
{
//	MAP_MESSAGE( VInternetDlg, "PageChanged", OnGameListChanged ),
	MAP_MESSAGE_INT( VInternetDlg, "Manage", OnManageServer, "serverID" ),
	MAP_MESSAGE_PARAMS( VInternetDlg, "CvarChangeValue", OnPlayerDialog ),
	MAP_MESSAGE_INT( VInternetDlg, "DeleteServer", OnDeleteServer, "panelid" ),
	MAP_MESSAGE( VInternetDlg, "OpenContextMenu", OnOpenContextMenu ),
};
IMPLEMENT_PANELMAP(VInternetDlg, vgui::Frame);
