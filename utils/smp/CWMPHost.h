//========= Copyright Valve Corporation, All rights reserved. ============//
// CWMPHost.h : Declaration of the CWMPHost
//

#include "resource.h"       // main symbols
#include <oledlg.h>

// if this file isn't found, set your path to include the wmp sdk include directory
// download the sdk from http://www.microsoft.com/windows/windowsmedia/mp10/sdk.aspx
#include "wmp.h"
#include "CWMPEventDispatch.h"


/////////////////////////////////////////////////////////////////////////////
// CWMPHost

class CWMPHost : public CWindowImpl< CWMPHost, CWindow, CWinTraits< WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE > >
{
public:
    DECLARE_WND_CLASS_EX(NULL, 0, 0)

    BEGIN_MSG_MAP(CWMPHost)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnClick)
		MESSAGE_HANDLER(WM_MBUTTONDOWN, OnClick)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, OnClick)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLeftDoubleClick)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_SYSKEYDOWN, OnSysKeyDown)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnErase)
        MESSAGE_HANDLER(WM_NCACTIVATE, OnNCActivate)

        COMMAND_ID_HANDLER(ID_HALF_SIZE, OnVideoScale)
        COMMAND_ID_HANDLER(ID_FULL_SIZE, OnVideoScale)
        COMMAND_ID_HANDLER(ID_DOUBLE_SIZE, OnVideoScale)
        COMMAND_ID_HANDLER(ID_STRETCH_TO_FIT, OnVideoScale)
    END_MSG_MAP()

    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnErase(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& bHandled);
    LRESULT OnSize(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnContextMenu(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnClick(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnLeftDoubleClick(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnSysKeyDown(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnKeyDown(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */);
	LRESULT OnNCActivate(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */);

    LRESULT OnVideoScale(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
 
    CAxWindow                   m_wndView;
    CComPtr<IConnectionPoint>   m_spConnectionPoint;
    DWORD                       m_dwAdviseCookie;
	HMENU						m_hPopupMenu;
};


/////////////////////////////////////////////////////////////////////////////
// event logging - this should really be in its own smp.h or something...

enum EventType_t
{
	ET_APPLAUNCH,
	ET_APPEXIT,
	ET_CLOSE,
	ET_FADEOUT,

	ET_MEDIABEGIN,
	ET_MEDIAEND,

	ET_JUMPHOME,
	ET_JUMPEND,

	ET_PLAY,
	ET_PAUSE,
	ET_STOP,
	ET_SCRUBFROM,
	ET_SCRUBTO,
	ET_STEPFWD,
	ET_STEPBCK,
	ET_JUMPFWD,
	ET_JUMPBCK,
	ET_REPEAT,

	ET_MAXIMIZE,
	ET_MINIMIZE,
	ET_RESTORE,
};
