// ChildView.cpp : implementation of the CChildView class
//
//  See Copyright Notice in gmMachine.h

#include "StdAfx.h"
#include "gmd.h"
#include "ChildView.h"
#include "GMLangSettings.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define LOCALS_ID     10
#define CALLSTACK_ID  11
#define THREADS_ID    12


void gmDebuggerBreak(gmDebuggerSession * a_session, int a_threadId, int a_sourceId, int a_lineNumber)
{
  CChildView * view = (CChildView *) a_session->m_user;
  gmMachineGetContext(a_session, a_threadId, 0);
}


void gmDebuggerRun(gmDebuggerSession * a_session, int a_threadId)
{
  CChildView * view = (CChildView *) a_session->m_user;
  char buffer[128];
  sprintf(buffer, "thread %d started.\r\n", a_threadId);
  view->Log(buffer);
  view->FindAddThread(a_threadId, 0);
}


void gmDebuggerStop(gmDebuggerSession * a_session, int a_threadId)
{
  CChildView * view = (CChildView *) a_session->m_user;
  char buffer[128];
  sprintf(buffer, "thread %d stopped.\r\n", a_threadId);
  view->Log(buffer);
  if(a_threadId == view->m_currentDebugThread) 
    view->ClearCurrentContext();
  view->RemoveThread(a_threadId);
}


void gmDebuggerSource(gmDebuggerSession * a_session, int a_sourceId, const char * a_sourceName, const char * a_source)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->SetSource(a_sourceId, a_source);
  if(view->m_lineNumberOnSourceRcv != -1)
  {
    view->SetLine(view->m_lineNumberOnSourceRcv);
    view->m_lineNumberOnSourceRcv = -1;
  }
}


#include <windows.h>
void gmDebuggerException(gmDebuggerSession * a_session, int a_threadId)
{
  // play exception sound.
  //PlaySound("critical");
  //PlaySound(MAKEINTRESOURCE(IDR_WAV_ERROR), NULL, SND_ASYNC | SND_RESOURCE);
  CChildView * view = (CChildView *) a_session->m_user;
  gmMachineGetContext(a_session, a_threadId, 0);
}


void gmDebuggerBeginContext(gmDebuggerSession * a_session, int a_threadId, int a_callFrame)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->m_currentDebugThread = a_threadId;
  view->FindAddThread(view->m_currentDebugThread, 4, true);
  view->m_callstack.DeleteAllItems();
  view->m_locals.DeleteAllItems();
  view->m_currentCallFrame = a_callFrame;
}


void gmDebuggerContextCallFrame(gmDebuggerSession * a_session, int a_callFrame, const char * a_functionName, int a_sourceId, int a_lineNumber, const char * a_thisSymbol, const char * a_thisValue, int a_thisId)
{
  CChildView * view = (CChildView *) a_session->m_user;
  char buffer[256];
  _snprintf(buffer, 256, "%s (%d)", a_functionName, a_lineNumber);
  view->m_callstack.InsertItem(view->m_callstack.GetItemCount(), buffer);

  if(view->m_currentCallFrame == a_callFrame)
  {
    // add "this"
    int index = view->m_locals.GetItemCount();
    view->m_locals.InsertItem(index, a_thisSymbol);
    view->m_locals.SetItemText(index, 1, a_thisValue);

    // do we have the source code?
    view->m_lineNumberOnSourceRcv = -1;
    if(!view->SetSource(a_sourceId))
    {
      // request source
      gmMachineGetSource(a_session, a_sourceId);
      view->m_lineNumberOnSourceRcv = a_lineNumber;
    }
    else
    {
      // update the position cursor.
      view->SetLine(a_lineNumber);
    }
  }
}


void gmDebuggerContextVariable(gmDebuggerSession * a_session, const char * a_varSymbol, const char * a_varValue, int a_varId)
{
  CChildView * view = (CChildView *) a_session->m_user;
  int index = view->m_locals.GetItemCount();
  view->m_locals.InsertItem(index, a_varSymbol);
  view->m_locals.SetItemText(index, 1, a_varValue);
}


void gmDebuggerEndContext(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
}


void gmDebuggerBeginSourceInfo(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
}


void gmDebuggerSourceInfo(gmDebuggerSession * a_session, int a_sourceId, const char * a_sourceName)
{
  CChildView * view = (CChildView *) a_session->m_user;
}


void gmDebuggerEndSourceInfo(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
}


void gmDebuggerBeginThreadInfo(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
  while(!view->m_threads.empty()) view->m_threads.pop_front();
}


void gmDebuggerThreadInfo(gmDebuggerSession * a_session, int a_threadId, int a_threadState)
{
  CChildView * view = (CChildView *) a_session->m_user;
  CChildView::ThreadInfo info;
  info.m_id = a_threadId;
  info.m_state = a_threadState;
  view->m_threads.push_front(info);
}


void gmDebuggerEndThreadInfo(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->UpdateThreadWindow();
}


void gmDebuggerError(gmDebuggerSession * a_session, const char * a_error)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->Log(a_error);
}


void gmDebuggerMessage(gmDebuggerSession * a_session, const char * a_message)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->Log(a_message);
}


void gmDebuggerAck(gmDebuggerSession * a_session, int a_response, int a_posNeg)
{
  CChildView * view = (CChildView *) a_session->m_user;
  if(a_response == view->m_responseId && a_posNeg)
  {
    if(view->m_breakPoint.m_enabled)
    {
      view->m_scintillaEdit.MarkerAdd(view->m_breakPoint.m_lineNumber-1, 0);
    }
    else
    {
      view->m_scintillaEdit.MarkerDelete(view->m_breakPoint.m_lineNumber-1, 0);
    }
  }
}


void gmDebuggerQuit(gmDebuggerSession * a_session)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->Disconnect();
}


void SendMachineMessage(gmDebuggerSession * a_session, const void * a_command, int a_len)
{
  CChildView * view = (CChildView *) a_session->m_user;
  view->m_networkServer.nSendMessage((const char *) a_command, a_len);
}

const void * PumpMachineMessage(gmDebuggerSession * a_session, int &a_len)
{
  CChildView * view = (CChildView *) a_session->m_user;
  return view->m_networkServer.PumpMessage(a_len);
}



/////////////////////////////////////////////////////////////////////////////
// CChildView

CChildView::CChildView()
{
  m_session.m_pumpMessage = PumpMachineMessage;
  m_session.m_sendMessage = SendMachineMessage;
  m_session.m_user = this;
  m_scintillaDll = NULL;
  m_currentDebugThread = 0;
  m_currentPos = -1;
  m_responseId = 100;
  m_sources = NULL;
  m_sourceId = 0;
  m_unpackBuffer = NULL;
  m_unpackBufferSize = 0;
  m_connected = false;
  m_debugging = false;
  m_lineNumberOnSourceRcv = -1;
}

CChildView::~CChildView()
{
  if(m_scintillaDll)
    FreeLibrary(m_scintillaDll);

  // free the source code
  Source * source = m_sources;
  while(source)
  {
    m_sources = source->m_next;
    delete[] source->m_source;
    delete source;
    source = m_sources;
  }

  if(m_unpackBuffer)
    delete[] m_unpackBuffer;
}


BEGIN_MESSAGE_MAP(CChildView,CWnd )
  //{{AFX_MSG_MAP(CChildView)
  ON_WM_CREATE()
  ON_WM_DESTROY()
  ON_WM_SIZE()
  ON_WM_PAINT()
  ON_COMMAND(ID_STEP_INTO, OnStepInto)
  ON_UPDATE_COMMAND_UI(ID_STEP_INTO, OnUpdateStepInto)
  ON_COMMAND(ID_STEP_OUT, OnStepOut)
  ON_UPDATE_COMMAND_UI(ID_STEP_OUT, OnUpdateStepOut)
  ON_COMMAND(ID_STEP_OVER, OnStepOver)
  ON_UPDATE_COMMAND_UI(ID_STEP_OVER, OnUpdateStepOver)
  ON_COMMAND(ID_GO, OnGo)
  ON_UPDATE_COMMAND_UI(ID_GO, OnUpdateGo)
  ON_COMMAND(ID_STOP_DEBUGGING, OnStopDebugging)
  ON_UPDATE_COMMAND_UI(ID_STOP_DEBUGGING, OnUpdateStopDebugging)
  ON_COMMAND(ID_TOGGLE_BREAKPOINT, OnToggleBreakpoint)
  ON_UPDATE_COMMAND_UI(ID_TOGGLE_BREAKPOINT, OnUpdateToggleBreakpoint)
  ON_NOTIFY(NM_DBLCLK, CALLSTACK_ID, OnNMDblclkCallstack)
  ON_NOTIFY(NM_DBLCLK, THREADS_ID, OnNMDblclkThreadstack)
  ON_NOTIFY(NM_CLICK, THREADS_ID, OnNMClkThreadstack)
  ON_COMMAND(ID_BREAK_ALL_THREADS, OnBreakAllThreads)
	ON_UPDATE_COMMAND_UI(ID_BREAK_ALL_THREADS, OnUpdateBreakAllThreads)
	ON_COMMAND(ID_RESUME_ALL_THREADS, OnResumeAllThreads)
	ON_UPDATE_COMMAND_UI(ID_RESUME_ALL_THREADS, OnUpdateResumeAllThreads)
	ON_COMMAND(ID_BREAK_CURRENT_THREAD, OnBreakCurrentThread)
	ON_UPDATE_COMMAND_UI(ID_BREAK_CURRENT_THREAD, OnUpdateBreakCurrentThread)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
  if (!CWnd::PreCreateWindow(cs))
    return FALSE;

  cs.dwExStyle |= WS_EX_CLIENTEDGE;
  cs.style &= ~WS_BORDER;
  cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
    ::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);

  return TRUE;
}

void CChildView::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// OnCreate
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int CChildView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CWnd::OnCreate(lpCreateStruct) == -1)
    return -1;

  //
  // Create Scintilla
  //

  m_scintillaDll = LoadLibrary("SciLexer.dll");
  if (!m_scintillaDll)
    return -1;

  HWND hwnd = ::CreateWindowEx(0, "Scintilla", NULL, WS_CHILD | WS_VISIBLE |WS_TABSTOP, 0, 0, 100, 100, m_hWnd, NULL, AfxGetInstanceHandle(), NULL);

  if (!hwnd)
    return -1;

  m_scintillaEdit.Attach(hwnd);

  m_scintillaEdit.MarkerDefine(0, ScintillaEdit::MS_Circle);
  m_scintillaEdit.MarkerDefine(1, ScintillaEdit::MS_Arrow);
  m_scintillaEdit.MarkerSetFGColour(0, RGB(0,0,0));
  m_scintillaEdit.MarkerSetGGColour(0, RGB(255,0,0));

  m_scintillaEdit.MarkerSetFGColour(1, RGB(0,0,0));
  m_scintillaEdit.MarkerSetGGColour(1, RGB(0,0,255));
  m_scintillaEdit.SetEOLMode(ScintillaEdit::EOL_CrLf);

  m_scintillaEdit.SetReadOnly(true);

  #if 1
  // Show line numbers
  // Code based on Scite
  // TODO, put markers to left of line numbers if possible
  {
    int lineCount = m_scintillaEdit.SendMessage(SCI_GETLINECOUNT,(WPARAM)0, (LPARAM)0);
    int lineNumWidth = 1;
    int lineNumbersWidth = 4;
    while (lineCount >= 10) 
    {
		  lineCount /= 10;
		  ++lineNumWidth;
	  }
	  if (lineNumWidth < lineNumbersWidth) 
    {
	    lineNumWidth = lineNumbersWidth;
	  }

		// The 4 here allows for spacing: 1 pixel on left and 3 on right.
		int pixelWidth = 4 + lineNumWidth * m_scintillaEdit.SendMessage(SCI_TEXTWIDTH, (WPARAM)STYLE_LINENUMBER, (LPARAM)"9");
		m_scintillaEdit.SendMessage(SCI_SETMARGINWIDTHN, (WPARAM)0, (LPARAM)pixelWidth);
	}
  #endif

  

  m_langSettings = new GMLangSettings;
  m_langSettings->Apply(m_scintillaEdit);

  //
  // Create Output Window
  //

  if(!m_outputWindow.Create(
    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_LEFT | ES_AUTOVSCROLL | ES_READONLY
    , CRect(0,0,0,0), this, 0)) return -1;
  m_outputWindow.ShowScrollBar(SB_VERT, TRUE);

  //
  // Create Locals Window
  //

  if(!m_locals.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, CRect(0,0,0,0), this, LOCALS_ID)) 
    return -1;

  m_locals.InsertColumn(0, "Variable");
  m_locals.InsertColumn(1, "Value");
  m_locals.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  //
  // Create Callstack Window
  //

  if(!m_callstack.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, CRect(0,0,0,0), this, CALLSTACK_ID)) 
    return -1;

  m_callstack.InsertColumn(0, "Call stack");
  m_callstack.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  //
  // Create Thread Window
  //

  if(!m_threadsWindow.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, CRect(0,0,0,0), this, THREADS_ID)) 
    return -1;

  m_threadsWindow.InsertColumn(0, "Thread ID");
  m_threadsWindow.InsertColumn(1, "Status");
  m_threadsWindow.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // start listening for network connections.
  Open(GM_DEBUGGER_PORT);

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// OnDestroy
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::OnDestroy() 
{
  delete m_langSettings;

  CWnd::OnDestroy();

  if (m_scintillaEdit)
  {
    HWND hwnd = m_scintillaEdit.Detach();
    ::SendMessage(hwnd, WM_DESTROY, 0, 0);
  }
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// OnDestroy
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::OnSize(UINT nType, int cx, int cy) 
{
  CWnd::OnSize(nType, cx, cy);

  CRect rect;
  GetClientRect(rect);

  CRect scintillaRect(rect);
  CRect outputRect(rect);
  CRect localRect(rect), callstackRect(rect);
  CRect threadRect(rect);

  scintillaRect.right = (int) (0.7f * (float) rect.right);
  callstackRect.left = scintillaRect.right;
  localRect.left = scintillaRect.right;
  threadRect.left = scintillaRect.right;

  outputRect.right = scintillaRect.right;
  scintillaRect.bottom = (int) (0.85f * (float) rect.bottom);
  outputRect.top = scintillaRect.bottom;

  localRect.bottom = (int) (0.333333f * (float) rect.bottom); 
  callstackRect.top = localRect.bottom;
  callstackRect.bottom = (int) (0.666666f * (float) rect.bottom);
  threadRect.top = callstackRect.bottom;
  
  m_scintillaEdit.MoveWindow(scintillaRect);
  m_outputWindow.MoveWindow(outputRect);
  m_locals.MoveWindow(localRect);
  m_callstack.MoveWindow(callstackRect);
  m_threadsWindow.MoveWindow(threadRect);

  // Set column widths on windows.
  int width;
  m_locals.GetWindowRect(rect);
  width = rect.Width()/2;
  m_locals.SetColumnWidth(0, width);
  m_locals.SetColumnWidth(1, width);

  m_callstack.GetWindowRect(rect);
  width = rect.Width();
  m_callstack.SetColumnWidth(0, width);

  m_threadsWindow.GetWindowRect(rect);
  width = rect.Width()/2;
  m_threadsWindow.SetColumnWidth(0, width);
  m_threadsWindow.SetColumnWidth(1, width);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Log
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::Log(const char * a_message)
{
  if(m_outputLines.size() > 20)
  {
    m_outputLines.pop_front();
  }
  m_outputLines.push_back(CString(a_message));

  std::list<CString>::iterator it;

  CString text = "";
  for(it = m_outputLines.begin(); it != m_outputLines.end(); ++it)
  {
    text += *it;
  }

  m_outputWindow.SetWindowText(text);
  int nlines = m_outputWindow.GetLineCount();
  m_outputWindow.LineScroll(nlines);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SetSource
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

bool CChildView::SetSource(unsigned int a_sourceId, const char * a_source)
{
  if(a_sourceId == m_sourceId) return true;

  // do we have the source
  Source * source = m_sources;
  while(source)
  {
    if(source->m_id == a_sourceId)
    {
      m_scintillaEdit.SetReadOnly(false);
      m_scintillaEdit.SetText(source->m_source);
      m_scintillaEdit.SetReadOnly(true);
      m_sourceId = a_sourceId;
      return true;
    }
    source = source->m_next;
  }

  // we dont have the source, add it
  if(a_source)
  {
    m_scintillaEdit.SetReadOnly(false);
    m_scintillaEdit.SetText(a_source);
    m_scintillaEdit.SetReadOnly(true);
    m_sourceId = a_sourceId;

    int len = strlen(a_source) + 1;
    source = new Source;
    source->m_source = new char[len];
    memcpy(source->m_source, a_source, len);
    source->m_id = a_sourceId;
    source->m_next = m_sources;
    m_sources = source;
    return true;
  }
  return false;
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SetLine
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::SetLine(int a_line)
{
  if(m_currentPos >= 0)
  {
    m_scintillaEdit.MarkerDelete(m_currentPos, 1);
    m_currentPos = -1;
  }
  m_currentPos = a_line - 1;
  m_scintillaEdit.MarkerAdd(m_currentPos, 1);

  // center the source view around the cursor
  int topLine = m_scintillaEdit.GetFirstVisibleLine();
  int visLines = m_scintillaEdit.GetLinesOnScreen();
  int scrollLines = 0;
  int centre = (topLine + (visLines >> 1));
  int lq = centre - (visLines >> 2);
  int hq = centre + (visLines >> 2);

  if(m_currentPos < lq)
    scrollLines = m_currentPos - centre;
  else if(m_currentPos > hq)
    scrollLines = m_currentPos - centre;
  m_scintillaEdit.LineScroll(0, scrollLines);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// FindAddThread
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::FindAddThread(int a_threadId, int a_state, bool a_select)
{
  if(a_threadId <= 0) return;
  std::list<ThreadInfo>::iterator it;

  char buffer[128];
  sprintf(buffer, "%d", a_threadId);
  const char * state = "";

  switch(a_state)
  {
    case 0 : state = "running"; break;
    case 1 : state = "blocked"; break;
    case 2 : state = "sleeping"; break;
    case 3 : state = "exception"; break;
    case 4 : state = "debug"; break;
    default : break;
  }

  // find in list
 
  for(it = m_threads.begin(); it != m_threads.end(); ++it)
  {
    if(it->m_id == a_threadId)
    {
      // update state
      if(it->m_state != a_state || a_select)
      {
        if(it->m_state != a_state) it->m_state = a_state;
        ThreadInfo &info = *it;
        LVFINDINFO findInfo;
        findInfo.flags = LVFI_PARAM;
        findInfo.lParam = (LPARAM)&info;
        int index = m_threadsWindow.FindItem(&findInfo);
        ASSERT(index != -1);
        if(it->m_state != a_state) m_threadsWindow.SetItemText(index, 1, state);
        if(a_select) 
        { 
          m_threadsWindow.SetSelectionMark(index);
          m_threadsWindow.SetItemState(index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
      }
      return;
    }
  }

  // add
  ThreadInfo info;
  info.m_id = a_threadId;
  info.m_state = a_state;
  m_threads.push_front(info);
  m_threadsWindow.InsertItem(0, buffer);
  m_threadsWindow.SetItemText(0, 1, state);
  m_threadsWindow.SetItemData(0, (DWORD)&m_threads.front());
  if(a_select) 
  { 
    m_threadsWindow.SetSelectionMark(0);
    m_threadsWindow.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
  }
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// RemoveThread
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::RemoveThread(int a_threadId)
{
  if(a_threadId <= 0) return;
  std::list<ThreadInfo>::iterator it;
  
  for(it = m_threads.begin(); it != m_threads.end(); ++it)
  {
    if(it->m_id == a_threadId)
    {
      ThreadInfo &info = *it;

      LVFINDINFO findInfo;
      findInfo.flags = LVFI_PARAM;
      findInfo.lParam = (LPARAM)&info;
      int index = m_threadsWindow.FindItem(&findInfo);
      ASSERT(index != -1);
      m_threadsWindow.DeleteItem(index);

      m_threads.erase(it);
      return;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UpdateThreadWindow
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::UpdateThreadWindow()
{

  m_threadsWindow.DeleteAllItems();
  std::list<ThreadInfo>::iterator it;

  for(it = m_threads.begin(); it != m_threads.end(); ++it)
  {
    char buffer[128];
    sprintf(buffer, "%d", it->m_id);
    const char * state = "";

    switch(it->m_state)
    {
      case 0 : state = "running"; break;
      case 1 : state = "blocked"; break;
      case 2 : state = "sleeping"; break;
      case 3 : state = "exception"; break;
      case 4 : state = "broken"; break;
      default : break;
    }

    m_threadsWindow.InsertItem(0, buffer);
    m_threadsWindow.SetItemText(0, 1, state);
    m_threadsWindow.SetItemData(0, (DWORD)&*it);

    if(m_currentDebugThread == it->m_id)
    {
      m_threadsWindow.SetSelectionMark(0);
      m_threadsWindow.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
  }
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Open()
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

bool CChildView::Open(unsigned short a_port)
{
  Disconnect();
  if(m_networkServer.Open(a_port))
  {
    m_connected = true;
    char buffer[256];
    sprintf(buffer, "Listening for gmMachines on port %d\r\n", GM_DEBUGGER_PORT);
    Log(buffer);
    return true;
  }
  return false;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// ClearCurrentContext()
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::ClearCurrentContext()
{
  int sel = m_threadsWindow.GetSelectionMark();
  if(sel != -1)
  {
    m_threadsWindow.SetItemState(sel, 0, LVIS_SELECTED | LVIS_FOCUSED);
  }

  m_currentDebugThread = 0;
  m_threadsWindow.SetSelectionMark(-1);
  m_currentPos = -1;

  // clear the windows
  SetSource(0, ""); // hackorama
  m_locals.DeleteAllItems();
  m_callstack.DeleteAllItems();
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Disconnect()
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::Disconnect()
{
  while(!m_threads.empty()) m_threads.pop_front();
  m_threadsWindow.DeleteAllItems();
  ClearCurrentContext();
  if(m_connected)
  {
    m_networkServer.Close();
    m_connected = false;
  }
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// OnIdle()
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

BOOL CChildView::OnIdle()
{
  static int ut = 0;

  // process network messages
  if(m_networkServer.IsConnected())
  {
    m_debugging = true;
    // pump messages from server
    m_session.Update();

    if(--ut <= 0)
    {
      ut = 500;
      gmMachineGetThreadInfo(&m_session);
    }

    return TRUE;
  }
  else
  {
    if(m_debugging)
    {
      m_debugging = false;
      Open(GM_DEBUGGER_PORT);
    }
  }
  return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Handlers
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CChildView::OnStepInto() 
{
	if(m_currentDebugThread && m_debugging)
  {
    // send run command
    gmMachineStepInto(&m_session, m_currentDebugThread);
  }

}

void CChildView::OnUpdateStepInto(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}

void CChildView::OnStepOut() 
{
	if(m_currentDebugThread && m_debugging)
  {
    // send run command
    gmMachineStepOut(&m_session, m_currentDebugThread);
  }
}

void CChildView::OnUpdateStepOut(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}

void CChildView::OnStepOver() 
{
	if(m_currentDebugThread && m_debugging)
  {
    // send run command
    gmMachineStepOver(&m_session, m_currentDebugThread);
  }
}

void CChildView::OnUpdateStepOver(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}

void CChildView::OnGo() 
{
	if(m_currentDebugThread && m_debugging)
  {
    // send run command
    gmMachineRun(&m_session, m_currentDebugThread);
  }
}

void CChildView::OnUpdateGo(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}

void CChildView::OnStopDebugging() 
{
  if(m_connected && m_debugging)
  {
    gmMachineQuit(&m_session);
    Sleep(500); // wait for quit message to post (hack)
    Disconnect();
  }
}

void CChildView::OnUpdateStopDebugging(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_debugging != 0));
}

void CChildView::OnToggleBreakpoint() 
{
  // add a break point at the current line.
  int line = m_scintillaEdit.GetCurrentPos();
  line = m_scintillaEdit.LineFromPosition(line);
  bool enabled = true;

  // do we have a break point at this line already???
  if(m_scintillaEdit.MarkerGet(line) & 0x01) 
    enabled = false;

  if(m_currentDebugThread && m_debugging)
  {

    m_breakPoint.m_enabled = enabled;
    m_breakPoint.m_allThreads = true;
    m_breakPoint.m_responseId = ++m_responseId;
    m_breakPoint.m_sourceId = m_sourceId;
    m_breakPoint.m_lineNumber = line + 1;
    m_breakPoint.m_threadId = 0;

    gmMachineSetBreakPoint(&m_session, m_responseId, m_sourceId, line + 1, 0, enabled ? 1 : 0);
  }
}

void CChildView::OnUpdateToggleBreakpoint(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}


void CChildView::OnBreakAllThreads() 
{
  if(m_connected && m_debugging)
  {
    std::list<ThreadInfo>::iterator it;
    for(it = m_threads.begin(); it != m_threads.end(); ++it)
    {
      gmMachineBreak(&m_session, it->m_id);
    }
  }
}

void CChildView::OnUpdateBreakAllThreads(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_debugging != 0));
}

void CChildView::OnResumeAllThreads() 
{
  if(m_connected && m_debugging)
  {
    std::list<ThreadInfo>::iterator it;
    for(it = m_threads.begin(); it != m_threads.end(); ++it)
    {
      // send run command
      gmMachineRun(&m_session, it->m_id);
    }
  }
}

void CChildView::OnUpdateResumeAllThreads(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_debugging != 0));
}

  
void CChildView::OnNMDblclkCallstack(NMHDR *pNMHDR, LRESULT *pResult)
{
  int selected = m_callstack.GetSelectionMark();
  if (selected != -1 && m_currentDebugThread > 0)
  {
    gmMachineGetContext(&m_session, m_currentDebugThread, selected);
  }

  *pResult = 0;
}

void CChildView::OnNMDblclkThreadstack(NMHDR *pNMHDR, LRESULT *pResult)
{
  // they have selected a thread, break
  *pResult = 0;
}


void CChildView::OnNMClkThreadstack(NMHDR *pNMHDR, LRESULT *pResult)
{
  int selected = m_threadsWindow.GetSelectionMark();
  if(selected != -1)
  {
    DWORD user = m_threadsWindow.GetItemData(selected);
    if(user)
    {

      ThreadInfo * info = (ThreadInfo *) user;
      gmMachineGetContext(&m_session, info->m_id, 0);
    }
  }
  *pResult = 0;
}


void CChildView::OnBreakCurrentThread() 
{
  if(m_currentDebugThread != 0)
  {
    gmMachineBreak(&m_session, m_currentDebugThread);
  }
}

void CChildView::OnUpdateBreakCurrentThread(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable((m_currentDebugThread != 0));
}
