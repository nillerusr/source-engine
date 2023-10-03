// ChildView.h : interface of the CChildView class
//
/////////////////////////////////////////////////////////////////////////////
//  See Copyright Notice in gmMachine.h

#if !defined(AFX_CHILDVIEW_H__43C2AE53_727B_457A_B67D_FB0A1B5D1BE4__INCLUDED_)
#define AFX_CHILDVIEW_H__43C2AE53_727B_457A_B67D_FB0A1B5D1BE4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "gmDebugger.h"
#include "NetServer.h"
#include "ScintillaEdit.h"
#include <list.>


#define GM_DEBUGGER_PORT  49001

// Fwd decls
class GMLangSettings;

struct gmdBreakPoint
{
  int m_enabled;
  int m_allThreads;
  int m_responseId;
  int m_lineNumber;
  int m_threadId;
  int m_sourceId;
};
/////////////////////////////////////////////////////////////////////////////
// CChildView window

class CChildView : public CWnd
{
// Construction
public:
	CChildView();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildView)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CChildView();

  //
  // GM Debug Members
  //

  nServer m_networkServer;        // network server

  CListCtrl m_locals;             // local variable window
  CListCtrl m_callstack;          // callstack window
  CEdit m_outputWindow;           // output message window
  ScintillaEdit m_scintillaEdit;  // source window
  HMODULE m_scintillaDll;
  GMLangSettings * m_langSettings;

  std::list<CString> m_outputLines;

  // Source 
  struct Source
  {
    char * m_source;
    unsigned int m_id;
    Source * m_next;
  };

  int m_currentCallFrame;
  Source * m_sources;             // list of source code retrieved from the gm machine being debugged
  unsigned int m_sourceId;        // source id of the source code currently being viewed, 0 for none
  int m_currentDebugThread;       // thread id of the current context thread
  int m_currentPos;               // execute pos. -1 for invalid.
  gmdBreakPoint m_breakPoint;     // last break point command
  int m_responseId;               // current rolling response id
  char * m_unpackBuffer;          // current message unpacking buffer
  int m_unpackBufferSize;         // current message unpacking buffer size
  bool m_connected;               // is our network port open???
  bool m_debugging;
  int m_lineNumberOnSourceRcv;

  gmDebuggerSession m_session;

  // Threads
  struct ThreadInfo
  {
    int m_id;
    int m_state;
  };
  std::list<ThreadInfo> m_threads; // current running threads.

  void Log(const char * a_message);
  bool SetSource(unsigned int a_sourceId, const char * a_source = NULL);
  bool Open(unsigned short a_port);
  void SetLine(int a_line);
  void ClearCurrentContext();
  void Disconnect();

  // threads
  CListCtrl m_threadsWindow;      // thread window
  void FindAddThread(int a_threadId, int a_state, bool a_select = false); // 
  void RemoveThread(int a_threadId);
  void UpdateThreadWindow();

  BOOL OnIdle();

	// Generated message map functions
protected:
	//{{AFX_MSG(CChildView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnStepInto();
	afx_msg void OnUpdateStepInto(CCmdUI* pCmdUI);
	afx_msg void OnStepOut();
	afx_msg void OnUpdateStepOut(CCmdUI* pCmdUI);
	afx_msg void OnStepOver();
	afx_msg void OnUpdateStepOver(CCmdUI* pCmdUI);
	afx_msg void OnGo();
	afx_msg void OnUpdateGo(CCmdUI* pCmdUI);
	afx_msg void OnStopDebugging();
	afx_msg void OnUpdateStopDebugging(CCmdUI* pCmdUI);
	afx_msg void OnToggleBreakpoint();
	afx_msg void OnUpdateToggleBreakpoint(CCmdUI* pCmdUI);
  afx_msg void OnNMDblclkCallstack(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnNMDblclkThreadstack(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnNMClkThreadstack(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBreakAllThreads();
	afx_msg void OnUpdateBreakAllThreads(CCmdUI* pCmdUI);
	afx_msg void OnResumeAllThreads();
	afx_msg void OnUpdateResumeAllThreads(CCmdUI* pCmdUI);
	afx_msg void OnBreakCurrentThread();
	afx_msg void OnUpdateBreakCurrentThread(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDVIEW_H__43C2AE53_727B_457A_B67D_FB0A1B5D1BE4__INCLUDED_)
