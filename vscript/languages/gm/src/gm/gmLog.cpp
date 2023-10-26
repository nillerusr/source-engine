/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmLog.h"
#include "gmMem.h"

// Must be last header
#include "memdbgon.h"

gmLog::gmLog() :
  m_mem(1, GMLOG_CHAINSIZE)
{
  m_first = NULL;
  m_last = NULL;
  m_curr = NULL;

  m_memApproxLimit = -1;
}



gmLog::~gmLog()
{
}



void gmLog::Reset()
{
  m_first = NULL;
  m_last = NULL;
  m_curr = NULL;
  m_mem.Reset();
}



void gmLog::ResetAndFreeMemory()
{
  m_first = NULL;
  m_last = NULL;
  m_curr = NULL;
  m_mem.ResetAndFreeMemory();
}



void GM_CDECL gmLog::LogEntry(const char * a_format, ...)
{
  va_list ap;
  char buffer[GMLOG_CHAINSIZE];

  va_start(ap, a_format);
	_gmvsnprintf(buffer, GMLOG_CHAINSIZE, a_format, ap);
	va_end(ap);
  strcat(buffer, GM_NL);

  if( (m_memApproxLimit > 0) && (m_mem.GetSystemMemUsed() > (unsigned int)m_memApproxLimit) )
  {
    m_mem.Reset();
  }

  // add to entry list
  Entry * entry = (Entry *) m_mem.AllocBytes(sizeof(Entry) + sizeof(int), GM_DEFAULT_ALLOC_ALIGNMENT);
  if(entry != NULL)
  {
    char * text = (char *) m_mem.AllocBytes(strlen(buffer) + 1, GM_DEFAULT_ALLOC_ALIGNMENT);
    if(text)
    {
      strcpy(text, buffer);
      entry->m_text = text;
      entry->m_next = NULL;
      if(m_last)
      {
        m_last->m_next = entry;
        m_last = entry;
      }
      else
      {
        m_first = m_last = entry;
      }
    }
  }
}



const char * gmLog::GetEntry(bool &a_first)
{
  if(a_first == true)
  {
    a_first = false;
    m_curr = m_first;
  }

  if(m_curr)
  {
    const char * text = m_curr->m_text;
    m_curr = m_curr->m_next;
    return text;
  }

  return NULL;
}


void gmLog::SetMemLimit(int a_limit)
{
  m_memApproxLimit = a_limit;
}

