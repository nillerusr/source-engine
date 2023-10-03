//  See Copyright Notice in gmMachine.h

#include "gmmachine.h"
#include "gmthread.h"
#include "gmdebug.h"
#include "gmstreambuffer.h"

// libs

#include "gmmathlib.h"
#include "gmstringlib.h"
#include "gmarraylib.h"
#include "gmsystemlib.h"
#include "gmvector3lib.h"

#include "timer.h"
#include "NetClient.h"
#include <math.h>

#define GM_DEBUGGER_PORT  49001

//
// globals
//

#undef GetObject

gmMachine * g_machine = NULL;
Timer g_timer, g_timer1;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// gmMachine exception handler
//

bool GM_CDECL gmeMachineCallback(gmMachine * a_machine, gmMachineCommand a_command, const void * a_context)
{
  if(a_command == MC_THREAD_EXCEPTION)
  {
    bool first = true;
    const char * entry;
    while((entry = a_machine->GetLog().GetEntry(first)))
    {
      fprintf(stderr, "%s", entry);
    }
    a_machine->GetLog().Reset();
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// gmeLib
//

static POINT s_screenSize;
static HANDLE s_hConsole;

static void gmeInit(gmMachine * a_machine)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	DWORD dwConSize; 

  s_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(s_hConsole, &csbi); 
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y; 
	s_screenSize.x = csbi.dwSize.X;
	s_screenSize.y = csbi.dwSize.Y;

  gmTableObject * table = a_machine->AllocTableObject();
  a_machine->GetGlobals()->Set(a_machine, "CA", gmVariable(GM_TABLE, table->GetRef()));

  table->Set(a_machine, "F_BLUE", gmVariable(GM_INT, FOREGROUND_BLUE));
  table->Set(a_machine, "F_GREEN", gmVariable(GM_INT, FOREGROUND_GREEN));
  table->Set(a_machine, "F_RED", gmVariable(GM_INT, FOREGROUND_RED));
  table->Set(a_machine, "F_INTENSITY", gmVariable(GM_INT, FOREGROUND_INTENSITY));
  table->Set(a_machine, "B_BLUE", gmVariable(GM_INT, BACKGROUND_BLUE));
  table->Set(a_machine, "B_GREEN", gmVariable(GM_INT, BACKGROUND_GREEN));
  table->Set(a_machine, "B_RED", gmVariable(GM_INT, BACKGROUND_RED));
  table->Set(a_machine, "B_INTENSITY", gmVariable(GM_INT, BACKGROUND_INTENSITY));

  table->Set(a_machine, "LEADING_BYTE", gmVariable(GM_INT, COMMON_LVB_LEADING_BYTE));
  table->Set(a_machine, "TRAILING_BYTE", gmVariable(GM_INT, COMMON_LVB_TRAILING_BYTE));
  table->Set(a_machine, "GRID_HORIZONTAL", gmVariable(GM_INT, COMMON_LVB_GRID_HORIZONTAL));
  table->Set(a_machine, "GRID_LVERTICAL", gmVariable(GM_INT, COMMON_LVB_GRID_LVERTICAL));
  table->Set(a_machine, "GRID_RVERTICAL", gmVariable(GM_INT, COMMON_LVB_GRID_RVERTICAL));
  table->Set(a_machine, "REVERSE_VIDEO", gmVariable(GM_INT, COMMON_LVB_REVERSE_VIDEO));
  table->Set(a_machine, "UNDERSCORE", gmVariable(GM_INT, COMMON_LVB_UNDERSCORE));
}

static int GM_CDECL gmfCXY(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(x, 0);
  GM_CHECK_INT_PARAM(y, 1);
	COORD point;
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x > s_screenSize.x) x = s_screenSize.x;
  if(y > s_screenSize.y) y = s_screenSize.y;
  point.X = (short) x; point.Y = (short) y;
	SetConsoleCursorPosition(s_hConsole, point);
  return GM_OK;
}

static int GM_CDECL gmfCXYTEXT(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(3);
  GM_CHECK_INT_PARAM(x, 0);
  GM_CHECK_INT_PARAM(y, 1);
  GM_CHECK_STRING_PARAM(str, 2);
	COORD point;
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x > s_screenSize.x) x = s_screenSize.x;
  if(y > s_screenSize.y) y = s_screenSize.y;
  point.X = (short) x; point.Y = (short) y;
	SetConsoleCursorPosition(s_hConsole, point);
  printf("%s", str);
  return GM_OK;
}

static int GM_CDECL gmfCCLR(gmThread * a_thread)
{
	COORD coordScreen = { 0, 0 }; 
	DWORD cCharsWritten; 
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	DWORD dwConSize; 
	GetConsoleScreenBufferInfo(s_hConsole, &csbi); 
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y; 
	FillConsoleOutputCharacter(s_hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten); 
	GetConsoleScreenBufferInfo(s_hConsole, &csbi); 
	FillConsoleOutputAttribute(s_hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten); 
	SetConsoleCursorPosition(s_hConsole, coordScreen); 
  return GM_OK;
}

static int GM_CDECL gmfCURSOR(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(state, 0);
  GM_CHECK_INT_PARAM(size, 1);
  CONSOLE_CURSOR_INFO info;
  if(size < 1) size = 1; if(size > 100) size = 100;
  info.bVisible = state;
  info.dwSize = size;
  SetConsoleCursorInfo(s_hConsole, &info);
  return GM_OK;
}

static int GM_CDECL gmfCATTRIB(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(param, 0);
  SetConsoleTextAttribute(s_hConsole, (short) param);
  return GM_OK;
}

int GM_CDECL gmfTimer(gmThread * a_thread)
{
  a_thread->PushFloat(g_timer1.Tick());
  return GM_OK;
}

int GM_CDECL gmfIsPressed(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(key, 0);
  a_thread->PushInt(GetAsyncKeyState(key));
  return GM_OK;
}

static gmFunctionEntry s_gmeLib[] = 
{ 
  {"TICK", gmfTimer},
  {"CLS", gmfCCLR},
  {"XY", gmfCXY},
  {"XYTEXT", gmfCXYTEXT},
  {"CURSOR", gmfCURSOR},
  {"CATTRIB", gmfCATTRIB},
  {"ISPRESSED", gmfIsPressed},
};


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// debug callbacks
//

#if GMDEBUG_SUPPORT


void SendDebuggerMessage(gmDebugSession * a_session, const void * a_command, int a_len)
{
  nClient * client = (nClient *) a_session->m_user;
  client->SendMessage((const char *) a_command, a_len);
}


const void * PumpDebuggerMessage(gmDebugSession * a_session, int &a_len)
{
  nClient * client = (nClient *) a_session->m_user;
  return client->PumpMessage(a_len);
}


#endif 

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// gmeSetEnv
//
void GM_CDECL gmdSetEnv(gmMachine * a_machine, const char * a_env, gmTableObject * a_vars)
{
  char * t = (char *) alloca(strlen(a_env) + 1);
  strcpy(t, a_env);
  char * r = t;
  while(*r != '\0' && *r != '=') ++r;
  if(*r == '=')
  {
    *r = '\0';
    ++r;
  }
  else r = "";

  a_vars->Set(a_machine, gmVariable(GM_STRING, a_machine->AllocStringObject(t)->GetRef()),
                         gmVariable(GM_STRING, a_machine->AllocStringObject(r)->GetRef()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// main
//
void main(int argc, char * argv[], char * envp[])
{
  bool debug = false, envvars = false, keypress = false, keypressOnError = false;
  char * filename = "";
  char * ip = "127.0.0.1"; // localhost
  int port = GM_DEBUGGER_PORT;
  char * script = NULL;
#if GMDEBUG_SUPPORT
  nClient client;
  gmDebugSession session;

  session.m_sendMessage = SendDebuggerMessage;
  session.m_pumpMessage = PumpDebuggerMessage;
  session.m_user = &client;
#endif

  //
  // process command line arguments
  //
  
  if(argc <= 1)
  {
#if GMDEBUG_SUPPORT
    fprintf(stderr, "args: filename [-d<ip> for debug] [-e for env vars] [-ke for keypress on error] [-k for keypress on exit]"GM_NL);
#else //GMDEBUG_SUPPORT
    fprintf(stderr, "args: filename [-e for env vars] [-ke for keypress on error] [-k for keypress on exit]"GM_NL);
#endif //GMDEBUG_SUPPORT

    return;
  }

  filename = argv[1];
  int argstart = 2;

  int i;
  for(i = 2; i < argc; ++i)
  {
    if(argv[i][0] == '-' && argv[i][1] == 'd')
    {
      debug = true;
      // read the ip
      if(argv[i][2])
      {
        ip = argv[i] + 2;
      }
      argstart = i+1;
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'e')
    {
      envvars = true;
      argstart = i+1;
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'k' && argv[i][2] == 'e')
    {
      keypressOnError = true;
      argstart = i+1;
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'k')
    {
      keypress = true;
      argstart = i+1;
    }
  }

  //
  // load the file
  //

  FILE * fp = fopen(filename, "rb");
  if(fp)
  {
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);
    script = new char[size + 1];
    fread(script, 1, size, fp);
    script[size] = 0;
    fclose(fp);
  }
  if(script == NULL)
  {
    fprintf(stderr, "could not open file %s"GM_NL, filename);
    if(keypress || keypressOnError)
    {
      getchar();
    }
    return;
  }

  //
  // start the machine
  //

  gmMachine::s_machineCallback = gmeMachineCallback;

  g_machine = new gmMachine;
  g_machine->SetDesiredByteMemoryUsageHard(128*1024);
  g_machine->SetDesiredByteMemoryUsageSoft(g_machine->GetDesiredByteMemoryUsageHard() * 9 / 10);
  g_machine->SetAutoMemoryUsage(true);
  g_machine->SetDebugMode(debug);

  //
  // bind the process arguments to table "arg"
  //

  gmTableObject * argTable = g_machine->AllocTableObject();
  g_machine->GetGlobals()->Set(g_machine, "arg",
                               gmVariable(GM_TABLE, argTable->GetRef()));
  for(i = argstart; i < argc; ++i)
  {
    argTable->Set(g_machine, gmVariable(GM_INT, i - argstart), gmVariable(GM_STRING, g_machine->AllocStringObject(argv[i])->GetRef()));
  }

  //
  // bind the process arguments to table "env"
  //

  if(envvars)
  {
    gmTableObject * envTable = g_machine->AllocTableObject();
    g_machine->GetGlobals()->Set(g_machine, "env", gmVariable(GM_TABLE, envTable->GetRef()));
    for(i = argstart; envp[i]; ++i)
    {
      gmdSetEnv(g_machine, envp[i], envTable);
    }
  }

  //
  // bind the default libs
  //

  gmBindMathLib(g_machine);
  gmBindStringLib(g_machine);
  gmBindArrayLib(g_machine);
  gmBindSystemLib(g_machine);
  gmBindVector3Lib(g_machine);
  gmeInit(g_machine);


#if GMDEBUG_SUPPORT
  gmBindDebugLib(g_machine);
#endif
  g_machine->RegisterLibrary(s_gmeLib, sizeof(s_gmeLib) / sizeof(s_gmeLib[0]));

  //
  // compile the script
  //


  int errors = g_machine->ExecuteString(script, NULL, false, filename);
  if(errors)
  {
    bool first = true;
    const char * message;
    while((message = g_machine->GetLog().GetEntry(first)))
    {
      fprintf(stderr, "%s"GM_NL, message);
    }
    g_machine->GetLog().Reset();

    delete g_machine;
    delete[] script;

    if(keypress || keypressOnError)
    {
      getchar();
    }

    return;
  }

  //
  // start the remote debug thread, which will connect to the debugger if it exists
  //
  if(debug && port)
  {
#if GMDEBUG_SUPPORT
    if(client.Connect(ip, ((short) port)))
    {
      session.Open(g_machine);
      fprintf(stderr, "debug session opened"GM_NL);
    }
#endif
  }

  //
  // execute loop
  //

  g_timer.Init();
  g_timer1.Init();

  float timeAcc = 0.0f;
  gmuint32 idt = 0;
  while(g_machine->Execute(idt))
  {
    //
    // update time
    //
    timeAcc += (g_timer.Tick() * 1000.0f);
    if(timeAcc > 1.0f)
    {
      idt = (gmuint32) floorf(timeAcc);
      timeAcc -= (float) idt;
    }
    else idt = 0;

    //
    // update remote debug status
    //

    if(debug && port)
    {
#if GMDEBUG_SUPPORT
      if(client.IsConnected())
      {
        session.Update();
      }
      else
      {
        session.Close();
      }
#endif
    }
#ifdef WIN32
    // Just give the OS a chance to update and run more smoothly.
    Sleep(0);
#endif //WIN32
  }

  //
  // clean up the debug comms thread
  //

  if(debug && port)
  {
#if GMDEBUG_SUPPORT
    session.Close();
#endif
  }
 
#if GMDEBUG_SUPPORT
  client.Close();
#endif
  delete[] script;
  delete g_machine;

  _gmDumpLeaks();

  if(keypress)
  {
    getchar();
  }
};

