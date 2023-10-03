#ifndef _NETCLIENT_H_
#define _NETCLIENT_H_

//  See Copyright Notice in gmMachine.h

#undef SendMessage // windows clash

//
// nClient
//
class nClient
{
public:
  nClient();
  ~nClient();

  bool Connect(const char * a_server, short a_port);
  void Close();
  bool IsConnected(); 

  bool SendMessage(const char * a_buffer, int a_len);
  const char * PumpMessage(int &a_len);

private:

  void * m_data;
};


#endif //_NETCLIENT_H_
