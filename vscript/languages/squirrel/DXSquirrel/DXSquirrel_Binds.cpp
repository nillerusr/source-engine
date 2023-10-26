#include "stdafx.h"
#include "sqplus.h"
#include "DXSquirrel_Binds.h"

WNDCLASSEX gWC;
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
IDirect3D9 * gpD3D = NULL;
BOOL DXSquirrel_Initialize()
{
	WNDCLASSEX t = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, 
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("DXSquirrel"), NULL };
	gWC = t;
	RegisterClassEx( &gWC );
	SquirrelVM::Init();
	if( NULL == ( gpD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return FALSE;

	//initializes classes
	_INIT_STATIC_NAMESPACE(DX);
	_INIT_CLASS(Device);
	_INIT_CLASS(Vector3);
	_INIT_CLASS(Matrix);
	_INIT_CLASS(BaseMesh);
	_INIT_CLASS(Mesh);
	//
	return TRUE;
}

void DXSquirrel_Shutdown()
{
	if(gpD3D) {
		gpD3D->Release();
	}
}
