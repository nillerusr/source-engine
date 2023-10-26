//
// ScriptSys.cpp
//

#include "gmCall.h"
#include "ScriptSys.h"
#include "ScriptObj.h"
#include "GameObj.h"


// Init statics and constants
ScriptSys* ScriptSys::s_instance = NULL;
const int ScriptSys::DEBUGGER_DEFAULT_PORT = 49001;
const char* ScriptSys::DEBUGGER_DEFAULT_IP = "127.0.0.1"; // localhost


void ScriptSys::Init()
{
  GM_ASSERT( !s_instance ); // Just have one instance for this example
  
  s_instance = new ScriptSys;

  // Register Game Object type and bindings
  ScriptObj::RegisterScriptBindings();
  GameObj::RegisterScriptBindings();
}


void ScriptSys::Destroy()
{
  delete s_instance;
  s_instance = NULL;
}


void ScriptSys::DebuggerSendMessage(gmDebugSession * a_session, const void * a_command, int a_len)
{
  nClient * client = (nClient *) a_session->m_user;
  client->SendMessage((const char *) a_command, a_len);
}


const void* ScriptSys::DebuggerPumpMessage(gmDebugSession * a_session, int &a_len)
{
  nClient * client = (nClient *) a_session->m_user;
  return client->PumpMessage(a_len);
}


ScriptSys::ScriptSys()
{
  m_machine = new gmMachine;

  //Set machine callbacks
  gmMachine::s_machineCallback = ScriptSysCallback_Machine;
  gmMachine::s_printCallback = ScriptSysCallback_Print;

  // Init debugger
  gmBindDebugLib(m_machine); // Register debugging library

  m_debuggerIP = DEBUGGER_DEFAULT_IP;
  m_debuggerPort = DEBUGGER_DEFAULT_PORT;
  m_debugSession.m_sendMessage = DebuggerSendMessage;
  m_debugSession.m_pumpMessage = DebuggerPumpMessage;
  m_debugSession.m_user = &m_debugClient;

  if(m_debugClient.Connect(m_debuggerIP, ((short) m_debuggerPort)))
  {
    m_debugSession.Open(m_machine);
    fprintf(stderr, "Debug session opened"GM_NL);
  }
  m_machine->SetDebugMode(true);
}


ScriptSys::~ScriptSys()
{
  // End debugger session if any
  m_debugSession.Close();
  m_debugClient.Close();

  // For debugging
  _gmDumpLeaks();

  delete m_machine;
}


void __cdecl ScriptSys::LogError(const char *a_str, ...)
{
  // WARNING This is not safe for longer strings, should use non-ansi vsprintnf, string type, or similar.
  const int MAX_CHARS = 512;
  char buffer[MAX_CHARS];

  va_list args;
  va_start(args, a_str);
  
  vsprintf(buffer, a_str, args);
  
  va_end(args);

  fprintf(stderr, "ERROR: %s", a_str);
}


GameObj* ScriptSys::GetGameObjFromThreadId(int a_threadId)
{
  ScriptObj* scriptObj;

  if(m_mapThreadGameObjs.GetAt(a_threadId, scriptObj))
  {
    return scriptObj->GetGameObj();
  }
  return NULL;
}


void ScriptSys::AssociateThreadIdWithGameObj(int a_threadId, GameObj& a_gameObj)
{
  ScriptObj* scriptObj = a_gameObj.GetScriptObj();
  
  scriptObj->AddThreadId(a_threadId);
  m_mapThreadGameObjs.SetAt(a_threadId, scriptObj);
}


void ScriptSys::DisassociateThreadIdWithGameObj(int a_threadId)
{
  ScriptObj* scriptObj;

  if(m_mapThreadGameObjs.RemoveAt(a_threadId, scriptObj))
  {
    scriptObj->RemoveThreadId(a_threadId);
  }
}


void ScriptSys::RemoveThreadIdButDontTouchGameObj(int a_threadId)
{
  m_mapThreadGameObjs.RemoveAt(a_threadId);
}


void ScriptSys::SetTableNull(const char* a_memberName, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable newVar;
  newVar.Nullify();
  table->Set(m_machine, a_memberName, newVar);
}


void ScriptSys::SetTableInt(const char* a_memberName, int a_int, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable newVar;
  newVar.SetInt(a_int);
  table->Set(m_machine, a_memberName, newVar);
}


void ScriptSys::SetTableFloat(const char* a_memberName, float a_float, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable newVar;
  newVar.SetFloat(a_float);
  table->Set(m_machine, a_memberName, newVar);
}


void ScriptSys::SetTableString(const char* a_memberName, const char* a_string, int a_strLength, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable newVar;
  newVar.SetString(m_machine->AllocStringObject(a_string, a_strLength));
  table->Set(m_machine, a_memberName, newVar);
}

void ScriptSys::SetTableGameObj(const char* a_memberName, GameObj* a_gameObj, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  GM_ASSERT(a_gameObj);
  gmTableObject* table = a_table;
  gmVariable newVar;
  newVar.SetUser(a_gameObj->GetScriptObj()->GetUserObject());
  table->Set(m_machine, a_memberName, newVar);
}


gmTableObject* ScriptSys::SetTableTable(const char* a_memberName, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmMachine* machine = m_machine;
  gmVariable newVar;
  gmTableObject* newTable = machine->AllocTableObject();
  newVar.SetTable(newTable);
  table->Set(machine, a_memberName, newVar);
  return newTable; //Return the table so we can potentially put things in it
}


bool ScriptSys::GetTableInt(const char* a_memberName, int& a_int, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable stringName;
  gmVariable retVar;

  stringName.SetString(m_machine->AllocStringObject(a_memberName));
  retVar = table->Get(stringName);
  if(retVar.m_type == GM_INT)
  {
    a_int = retVar.m_value.m_int;
    return true;
  }
  return false;
}


bool ScriptSys::GetTableFloat(const char* a_memberName, float& a_float, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable stringName;
  gmVariable retVar;

  stringName.SetString(m_machine->AllocStringObject(a_memberName));
  retVar = table->Get(stringName);
  if(retVar.m_type == GM_FLOAT)
  {
    a_float = retVar.m_value.m_float;
    return true;
  }
  return false;
}


bool ScriptSys::GetTableString(const char* a_memberName, String& a_string, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable stringName;
  gmVariable retVar;

  stringName.SetString(m_machine->AllocStringObject(a_memberName));
  retVar = table->Get(stringName);
  if(retVar.m_type == GM_STRING)
  {
    gmStringObject* stringObj = (gmStringObject*)GM_MOBJECT(m_machine, retVar.m_value.m_ref);
    a_string = stringObj->GetString();
    return true;
  }
  return false;
}


bool ScriptSys::GetTableGameObj(const char* a_memberName, GameObj*& a_gameObj, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable stringName;
  gmVariable retVar;

  stringName.SetString(m_machine->AllocStringObject(a_memberName));
  retVar = table->Get(stringName);
  if(retVar.m_type == ScriptObj::GMTYPE_GAMEOBJ)
  {
    gmUserObject* userObj = (gmUserObject*)GM_MOBJECT(m_machine, retVar.m_value.m_ref);
    a_gameObj = ((ScriptObj*)userObj->m_user)->GetGameObj();
    return true;
  }
  return false;
}


bool ScriptSys::GetTableTable(const char* a_memberName, gmTableObject*& a_retTable, gmTableObject* a_table)
{
  GM_ASSERT(a_table);
  gmTableObject* table = a_table;
  gmVariable stringName;
  gmVariable retVar;

  stringName.SetString(m_machine->AllocStringObject(a_memberName));
  retVar = table->Get(stringName);
  if(retVar.m_type == GM_TABLE)
  {
    a_retTable = (gmTableObject*)GM_MOBJECT(m_machine, retVar.m_value.m_ref);
    return true;
  }
  return false;
}


void GM_CDECL ScriptSys::ScriptSysCallback_Print(gmMachine* a_machine, const char* a_string)
{
  printf("%s\n", a_string);
}


bool GM_CDECL ScriptSys::ScriptSysCallback_Machine(gmMachine* a_machine, gmMachineCommand a_command, const void* a_context)
{
  switch(a_command)
  {
    case MC_THREAD_EXCEPTION:
    {
      ScriptSys::Get()->LogAnyMachineErrorMessages();
      break;
    }
    case MC_COLLECT_GARBAGE:
    {
/* // Old code
#if GM_USE_INCGC
      gmGarbageCollector* gc = a_machine->GetGC();

      for(unsigned int objIndex = 0; objIndex<ScriptSys::Get()->m_allScriptObjs.Count(); ++objIndex)
      {
        ScriptObj* scriptObj = ScriptSys::Get()->m_allScriptObjs[objIndex];
        gc->GetNextObject(scriptObj->GetTableObject());
      }
#else //GM_USE_INCGC
      gmuint32 mark = *(gmuint32*)a_context;

      for(unsigned int objIndex = 0; objIndex<ScriptSys::Get()->m_allScriptObjs.Count(); ++objIndex)
      {
        ScriptObj* scriptObj = ScriptSys::Get()->m_allScriptObjs[objIndex];

        if(scriptObj->GetTableObject()->NeedsMark(mark))
        {
          scriptObj->GetTableObject()->Mark(a_machine, mark);
        }
      }
#endif //GM_USE_INCGC
*/
      break;
    }
    case MC_THREAD_CREATE: // Called when a thread is created.  a_context is the thread.
    {
      break;
    }
    case MC_THREAD_DESTROY: // Called when a thread is destroyed.  a_context is the thread that is about to die
    {
      gmThread* thread = (gmThread*)a_context;
      ScriptSys::Get()->DisassociateThreadIdWithGameObj(thread->GetId());
      break;
    }
  }
  return false;
}



bool ScriptSys::ExecuteFile(const char* a_fileName)
{
  FILE* scriptFile = NULL;
  char* fileString = NULL;
  int fileSize = 0;

  GM_ASSERT(m_machine);

  if( !(scriptFile = fopen(a_fileName, "rb")) )
  {
    return false;
  }

  fseek(scriptFile, 0, SEEK_END);
  fileSize = ftell(scriptFile);
  fseek(scriptFile, 0, SEEK_SET);
  fileString = new char [fileSize+1];
  fread(fileString, fileSize, 1, scriptFile);
  fileString[fileSize] = 0; // Terminating null
  fclose(scriptFile);

  int threadId = GM_INVALID_THREAD;
  int errors = m_machine->ExecuteString(fileString, &threadId, true, a_fileName);
  if(errors)
  {
    LogAnyMachineErrorMessages();
  }

  delete [] fileString;

  return true;
}


bool ScriptSys::ExecuteString(const char* a_string)
{
  GM_ASSERT(m_machine);

  int threadId = GM_INVALID_THREAD;
  int errors = m_machine->ExecuteString(a_string, &threadId, true);
  if (errors)
  {
    LogAnyMachineErrorMessages();
  }

  return true;
}


int ScriptSys::Execute(unsigned int a_deltaTimeMS)
{
  int numThreads = m_machine->Execute(a_deltaTimeMS);
  
  if(m_debugClient.IsConnected())
  {
    m_debugSession.Update();
  }
  else
  {
    m_debugSession.Close();
  }

  return numThreads;
}


void ScriptSys::LogAnyMachineErrorMessages()
{
  bool first = true;
  const char * message;
  while((message = m_machine->GetLog().GetEntry(first)))
  {
    LogError("%s"GM_NL, message);
  }
  m_machine->GetLog().Reset();
}