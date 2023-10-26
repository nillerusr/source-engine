#include "stdafx.h"
#include <Windows.h>
#include <mmsystem.h>
#include <strsafe.h>

#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx9.lib")

#include "sqplus.h"

LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch( msg ) {
  case WM_CLOSE:
    PostQuitMessage( 0 );
    return 0;
  }
  return DefWindowProc( hWnd, msg, wParam, lParam );
}

BOOL DXSquirrel_Initialize();
void DXSquirrel_Shutdown();

INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT ) {
  if(!DXSquirrel_Initialize()) {
    MessageBox(NULL,_SC("Error initilizing the app"),_SC("DXSquirrel"),MB_OK);
    return -1;
  }
  try {
    SquirrelObject main = SquirrelVM::CompileScript(_SC("dxsquirrel.nut"));
    SquirrelVM::RunScript(main);
  }
  catch(SquirrelError &e) {
    MessageBox(NULL,e.desc,_SC("DXSquirrel"),MB_OK);
    return -2;
  }
  DXSquirrel_Shutdown();
  return 0;
}



