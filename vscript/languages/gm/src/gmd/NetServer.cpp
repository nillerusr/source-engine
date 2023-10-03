//  See Copyright Notice in gmMachine.h

#include "StdAfx.h"
#include "NetServer.h"
#include <windows.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>
#include <conio.h>
#include <winsock.h>
#include <math.h>

#undef SendMessage // stupid windows

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
// SERVER
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct nServerData
{
  SOCKET server;
  SOCKET client;
  nQueue messages;
  CRITICAL_SECTION criticalSection;
  bool threadAlive; 
};



nServer::nServer()
{
  nServerData * sd = new nServerData;
  sd->threadAlive = false;
  sd->server = sd->client = INVALID_SOCKET;
  InitializeCriticalSection(&sd->criticalSection);
  m_data = sd;
}



nServer::~nServer()
{
  Done();
}



void nServer::Close()
{
  nServerData * sd = (nServerData *) m_data;
  EnterCriticalSection(&sd->criticalSection);
  bool wait = false;

  if(sd->client != INVALID_SOCKET)
  {
    wait = true;
    closesocket(sd->client);
    sd->client = INVALID_SOCKET;
  }
  if(sd->server != INVALID_SOCKET)
  {
    closesocket(sd->server);
    sd->server = INVALID_SOCKET;
  }

  LeaveCriticalSection(&sd->criticalSection);

  // wait for the thread to die.
  while(wait && sd->threadAlive) 
  {
    _sleep(0);
  }

  WSACleanup();
}



void nServer::Done()
{
  if(m_data)
  {
    Close();
    nServerData * sd = (nServerData *) m_data;
    DeleteCriticalSection(&sd->criticalSection);
    delete sd;
    m_data = NULL;
  }
}




bool nServer::IsConnected()
{
  bool result = false;
  nServerData * sd = (nServerData *) m_data;
  EnterCriticalSection(&sd->criticalSection);
  if(sd->client != INVALID_SOCKET)
  {
    result = true;
  }
  LeaveCriticalSection(&sd->criticalSection);
  return result;
}



bool nServer::nSendMessage(const char * a_buffer, int a_len)
{
  bool res = false;
  nServerData * sd = (nServerData *) m_data;
  nPacket packet;
  packet.id = 0x4fe27d9a;
  packet.len = a_len;

  EnterCriticalSection(&sd->criticalSection);
  if(sd->client != INVALID_SOCKET)
  {
    send(sd->client, (const char *) &packet, sizeof(nPacket), 0);
    send(sd->client, (const char *) a_buffer, a_len, 0);
    res = true;
  }
  LeaveCriticalSection(&sd->criticalSection);
  return res;
}



const char * nServer::PumpMessage(int &a_len)
{
  nServerData * sd = (nServerData *) m_data;
  const char * buffer = sd->messages.DeQueue(a_len);
  return buffer;
}


//
// nServerThread
//

void nServerThread(void * param)
{
  nServerData * sd = (nServerData *) param;
  EnterCriticalSection(&sd->criticalSection);
  sd->threadAlive = true;
  SOCKET client = INVALID_SOCKET;
  SOCKET server = sd->server;
  LeaveCriticalSection(&sd->criticalSection);

  sockaddr_in from;
  int fromlen = sizeof(from), n;
  char * dbuffer = NULL;

  while(true)
  {
    client = accept(server, (struct sockaddr*) &from, &fromlen);

    if(client != INVALID_SOCKET)
    {
      EnterCriticalSection(&sd->criticalSection);
      sd->client = client;
      LeaveCriticalSection(&sd->criticalSection);

      char buffer[4096];
      char * sbp;
      int state = 0; // 0 searching for packet, 1 getting message
      int need = sizeof(nPacket);

      // packet header
      nPacket packet;
      char * dbp = (char *) &packet;

      // packet data
      int dbufferSize = 0;

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
              sd->messages.EnQueue(dbuffer, packet.len);
              dbp = (char *) &packet;
              need = sizeof(nPacket);
              state = 0;
            }
          }
        }
      }

      break;
    }
  }  

terror:

  if(dbuffer) { delete[] dbuffer; }

  EnterCriticalSection(&sd->criticalSection);
  sd->threadAlive = false;
  LeaveCriticalSection(&sd->criticalSection);

  _endthread();
}



bool nServer::Open(short a_port)
{
  nServerData * sd = (nServerData *) m_data;

  WSADATA wsaData;
  sockaddr_in local;
  int wsaret = WSAStartup(0x101, &wsaData);
  if(wsaret != 0)
  {
    return false;
  }

  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port=htons((u_short) a_port);
  sd->server = socket(AF_INET, SOCK_STREAM, 0);

  if(sd->server == INVALID_SOCKET)
  {
    return false;
  }

  if(bind(sd->server, (sockaddr*) &local, sizeof(local)) != 0)
  {
    closesocket(sd->server);
    sd->server = INVALID_SOCKET;
    return false;
  }

  if(listen(sd->server, 10) != 0)
  {
    closesocket(sd->server);
    sd->server = INVALID_SOCKET;
    return false;
  }

  // start the listener thread.
  _beginthread(nServerThread, 0, sd);
  _sleep(0);

  return true;
}
