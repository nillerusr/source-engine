#ifndef _NETSERVER_H_
#define _NETSERVER_H_

//  See Copyright Notice in gmMachine.h

//
// nServer
//
class nServer
{
public:

  nServer();
  ~nServer();

  bool Open(short a_port);
  void Close();
  void Done();
  bool IsConnected(); 

  bool nSendMessage(const char * a_buffer, int a_len);
  const char * PumpMessage(int &a_len);

private:

  void * m_data;
};

#endif //_NETSERVER_H_