//  See Copyright Notice in gmMachine.h

#include "NetClient.h"
#include <windows.h>
#include <process.h> // Requires Multi threaded library for _beginthread and _endthread
#include <stddef.h>
#include <stdlib.h>
#include <conio.h>
#include <winsock.h> // Requires Ws2_32.lib
#include <math.h>

#undef SendMessage // stupid windows

// These two are for MSVS 2005 security consciousness until safe std lib funcs are available
#pragma warning(disable : 4996) // Deprecated functions
#define _CRT_SECURE_NO_DEPRECATE // Allow old unsecure standard library functions, Disable some 'warning C4996 - function was deprecated'


struct nPacket
{
  int id; // id == 0x4fe27d9a
  int len;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// QUEUE
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct nQueueNode
{
  char * m_buffer;
  int m_len;
  nQueueNode * m_next;
};

class nQueue
{
public:
  nQueue()
  {
    m_queue = NULL;
    m_lastDeQueue = NULL;
    m_mutex = CreateMutex(NULL, FALSE, NULL);
  }

  ~nQueue()
  {
    WaitForSingleObject(m_mutex, INFINITE);

    int a;
    while(DeQueue(a));
    DeQueue(a);

    ReleaseMutex(m_mutex);

    // destroy mutex... todo
  }

  bool EnQueue(const char * a_buffer, int a_len)
  {
    WaitForSingleObject(m_mutex, INFINITE);

    nQueueNode * node = new nQueueNode;
    node->m_len = a_len;
    node->m_buffer = new char[a_len];
    memcpy(node->m_buffer, a_buffer, a_len);
    node->m_next = NULL;

    // add to end of list
    nQueueNode ** n = &m_queue;
    while(*n) n = &(*n)->m_next;
    *n = node;

    ReleaseMutex(m_mutex);

    return true;
  }

  const char * DeQueue(int &a_len)
  {
    const char * ret = NULL;
    
    WaitForSingleObject(m_mutex, INFINITE);

    if(m_lastDeQueue)
    {
      delete[] m_lastDeQueue->m_buffer;
      delete m_lastDeQueue;
      m_lastDeQueue = NULL;
    }

    if(m_queue)
    {
      m_lastDeQueue = m_queue;
      m_queue = m_queue->m_next;
      a_len = m_lastDeQueue->m_len;
      ret = m_lastDeQueue->m_buffer;
    }

    ReleaseMutex(m_mutex);

    return ret;
  }

  bool IsEmpty() { return (m_queue == NULL); }

private:

  HANDLE m_mutex;

  nQueueNode * m_lastDeQueue;
  nQueueNode * m_queue;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLIENT
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct nClientData
{
  SOCKET client;
  nQueue messages;
  CRITICAL_SECTION criticalSection;
  bool threadAlive; 
};



nClient::nClient()
{
  nClientData * cd = new nClientData;
  cd->threadAlive = false;
  cd->client = INVALID_SOCKET;
  InitializeCriticalSection(&cd->criticalSection);
  m_data = cd;
}



nClient::~nClient()
{
  Close();
  nClientData * cd = (nClientData *) m_data;
  DeleteCriticalSection(&cd->criticalSection);
  delete cd;
}



void nClient::Close()
{
  nClientData * cd = (nClientData *) m_data;
  EnterCriticalSection(&cd->criticalSection);

  if(cd->client != INVALID_SOCKET)
  {
    closesocket(cd->client);
    cd->client = INVALID_SOCKET;
  }

  LeaveCriticalSection(&cd->criticalSection);

  // wait for the thread to die.
  while(cd->threadAlive) 
  {
    _sleep(0);
  }

  WSACleanup();
}



bool nClient::IsConnected()
{
  bool result = false;
  nClientData * cd = (nClientData *) m_data;
  EnterCriticalSection(&cd->criticalSection);
  if(cd->client != INVALID_SOCKET)
  {
    result = true;
  }
  LeaveCriticalSection(&cd->criticalSection);
  return result;
}



bool nClient::SendMessage(const char * a_buffer, int a_len)
{
  bool res = false;
  nClientData * cd = (nClientData *) m_data;
  nPacket packet;
  packet.id = 0x4fe27d9a;
  packet.len = a_len;

  EnterCriticalSection(&cd->criticalSection);
  if(cd->client != INVALID_SOCKET)
  {
    send(cd->client, (const char *) &packet, sizeof(nPacket), 0);
    send(cd->client, (const char *) a_buffer, a_len, 0);
    res = true;
  }
  LeaveCriticalSection(&cd->criticalSection);
  return res;
}



const char * nClient::PumpMessage(int &a_len)
{
  nClientData * cd = (nClientData *) m_data;
  const char * buffer = cd->messages.DeQueue(a_len);
  return buffer;
}



void nClientThread(void * param)
{
  nClientData * cd = (nClientData *) param;
  EnterCriticalSection(&cd->criticalSection);
  cd->threadAlive = true;
  SOCKET client = cd->client;
  LeaveCriticalSection(&cd->criticalSection);

  char * dbuffer = NULL;
  char buffer[4096];
  char * sbp;
  int state = 0; // 0 searching for packet, 1 getting message
  int need = sizeof(nPacket);

  // packet header
  nPacket packet;
  char * dbp = (char *) &packet;

  // packet data
  int dbufferSize = 0, n;

  // read loop
  for(;;)
  {
    // read
    n = recv(client, buffer, 4096, 0);
    if(n == SOCKET_ERROR || n == 0) break;
    sbp = buffer;

    // consume
    while(n > 0)
    {
      int have = (n > need) ? need : n;
      need -= have;
      n -= have;
      memcpy(dbp, sbp, have);
      sbp += have;
      dbp += have;

      // can we change state?
      if(need == 0)
      {
        if(state == 0)
        {
          if(packet.id != 0x4fe27d9a) goto terror;
          state = 1;
          need = packet.len;

          // allocate the dbuffer
          if(need > dbufferSize)
          {
            if(dbuffer) { delete[] dbuffer; }
            dbufferSize = need + 512;
            dbuffer = new char[dbufferSize];
          }
          dbp = dbuffer;
        }
        else if(state == 1)
        {
          cd->messages.EnQueue(dbuffer, packet.len);
          dbp = (char *) &packet;
          need = sizeof(nPacket);
          state = 0;
        }
      }
    }
  }

terror:

  if(dbuffer) { delete[] dbuffer; }

  EnterCriticalSection(&cd->criticalSection);
  cd->threadAlive = false;
  LeaveCriticalSection(&cd->criticalSection);

  _endthread();
}



bool nClient::Connect(const char * a_server, short a_port)
{
	WSADATA wsaData;
	struct hostent *hp;
	unsigned int addr;
	struct sockaddr_in server;

	int wsaret=WSAStartup(0x101,&wsaData);
	if(wsaret)	
		return false;

	SOCKET conn;

	conn = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(conn==INVALID_SOCKET)
		return false;

	addr=inet_addr(a_server);
	hp=gethostbyaddr((char*)&addr,sizeof(addr),AF_INET);

  if(hp==NULL)
	{
		closesocket(conn);
		return false;
	}

  server.sin_addr.s_addr=*((unsigned long*)hp->h_addr);
	server.sin_family=AF_INET;
	server.sin_port=htons((u_short) a_port);

	if(connect(conn,(struct sockaddr*)&server,sizeof(server)))
	{
		closesocket(conn);
		return false;	
	}

  nClientData * cd = (nClientData *) m_data;
  EnterCriticalSection(&cd->criticalSection);
  cd->client = conn;
  LeaveCriticalSection(&cd->criticalSection);

  _beginthread(nClientThread, 0, cd);
  _sleep(0);

  return true;
}

