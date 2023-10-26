#include "stdafx.h"
#include "sqplusWin32.h"
#include "DXSquirrel_Binds.h"

extern WNDCLASSEX gWC;
extern IDirect3D9 * gpD3D;

_MEMBER_FUNCTION_IMPL(Device,constructor)
{
	StackHandler sa(v);

	//PARSES the params
	SquirrelObject params = sa.GetObjectHandle(2);

	D3DPRESENT_PARAMETERS d3dpp; 
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
	//defaults
	d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.BackBufferWidth = 640;
	d3dpp.BackBufferHeight = 480;
	//windowed
	if(params.Exists(_T("Windowed"))) {
		d3dpp.Windowed = params.GetBool(_T("Windowed"))? TRUE : FALSE;
	}
	if(params.Exists(_T("SwapEffect"))) {
		d3dpp.SwapEffect = (D3DSWAPEFFECT)params.GetInt(_T("SwapEffect"));
	}
	if(params.Exists(_T("BackBufferFormat"))) {
		d3dpp.BackBufferFormat = (D3DFORMAT)params.GetInt(_T("BackBufferFormat"));
	}
	if(params.Exists(_T("EnableAutoDepthStencil"))) {
		d3dpp.EnableAutoDepthStencil = params.GetBool(_T("EnableAutoDepthStencil"))? TRUE : FALSE;
	}
	if(params.Exists(_T("AutoDepthStencilFormat"))) {
		d3dpp.AutoDepthStencilFormat = (D3DFORMAT)params.GetInt(_T("AutoDepthStencilFormat"));
	}
	if(params.Exists(_T("BackBufferWidth"))) {
		d3dpp.BackBufferWidth = params.GetInt(_T("BackBufferWidth"));
	}
	if(params.Exists(_T("BackBufferHeight"))) {
		d3dpp.BackBufferHeight = params.GetInt(_T("BackBufferHeight"));
	}
	

	//create the window
	HWND hWnd = CreateWindow( _T("DXSquirrel"), _T("DXSquirrel"), 
                             0, CW_USEDEFAULT, CW_USEDEFAULT, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight,
                             NULL, NULL, gWC.hInstance, NULL );

	if(!hWnd) return sa.ThrowError(_T("Error creating the window"));
	EnableWindow(hWnd,TRUE);
	ShowWindow(hWnd,SW_SHOW);
	//d3dpp.hDeviceWindow = hWnd;
	//PARSES the params
	IDirect3DDevice9 *pDev;
	HRESULT hr;
	if(FAILED(hr = gpD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
									  &d3dpp, &pDev))) {
										  return sa.ThrowError(_T("Error initializing the device"));
									  }
	

	return construct_RefCounted(pDev);
}

_MEMBER_FUNCTION_IMPL(Device,BeginScene)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	if(FAILED(self->BeginScene())) {
		return sa.ThrowError(_T("BeginScene failed"));
	}
	return 0;
}

_MEMBER_FUNCTION_IMPL(Device,EndScene)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	if(FAILED(self->EndScene())) {
		return sa.ThrowError(_T("EndScene failed"));
	}
	return 0;
}

//params (this,flags,[color],[z],[stencil])
_MEMBER_FUNCTION_IMPL(Device,Clear)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	FLOAT z = 1.0f;
	DWORD color = 0xFF000000;
	DWORD stencil = 0;
	INT params = sa.GetParamCount();
	if(params > 2) {
		color = (DWORD)sa.GetInt(3);
	}
	if(params > 3) {
		z = sa.GetFloat(4);
	}
	if(params > 4) {
		stencil = (DWORD)sa.GetInt(4);
	}
	HRESULT hr;
	if(FAILED(hr = self->Clear(0,NULL,(DWORD)sa.GetInt(2),color,z,stencil)))
	{
		return sa.ThrowError(_T("Clear failed"));
	}
	
	return 0;
}

_MEMBER_FUNCTION_IMPL(Device,Present)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	self->Present(NULL,NULL,NULL,NULL);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Device,SetTransform)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	_CHECK_INST_PARAM(mat,3,D3DXMATRIX,Matrix);
	self->SetTransform((D3DTRANSFORMSTATETYPE)sa.GetInt(2),mat);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Device,SetRenderState)
{
	StackHandler sa(v);
	_CHECK_SELF(IDirect3DDevice9,Device);
	self->SetRenderState((D3DRENDERSTATETYPE)sa.GetInt(2),(DWORD)sa.GetInt(3));
	return 0;
}

_BEGIN_CLASS(Device)
_MEMBER_FUNCTION(Device,constructor,2,_T("xt"))
_MEMBER_FUNCTION(Device,BeginScene,0,NULL)
_MEMBER_FUNCTION(Device,EndScene,0,NULL)
_MEMBER_FUNCTION(Device,Present,0,NULL)
_MEMBER_FUNCTION(Device,Clear,-2,_T("xnnnn"))
_MEMBER_FUNCTION(Device,SetTransform,3,_T("xnx"))
_MEMBER_FUNCTION(Device,SetRenderState,3,_T("xnn|b"))
_END_CLASS(Device)