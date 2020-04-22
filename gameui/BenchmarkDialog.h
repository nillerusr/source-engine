//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BENCHMARKDIALOG_H
#define BENCHMARKDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: Benchmark launch dialog
//-----------------------------------------------------------------------------
class CBenchmarkDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBenchmarkDialog, vgui::Frame );
public:
	CBenchmarkDialog(vgui::Panel *parent, const char *name);

	void OnKeyCodePressed( vgui::KeyCode code )
	{
		if ( code == KEY_XBUTTON_B || code == STEAMCONTROLLER_B )
		{
			Close();
		}
		else if ( code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
		{
			RunBenchmark();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}
	
private:
	MESSAGE_FUNC( RunBenchmark, "RunBenchmark" );
};


#endif // BENCHMARKDIALOG_H
