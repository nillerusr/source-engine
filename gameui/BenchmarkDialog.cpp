//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BenchmarkDialog.h"
#include "EngineInterface.h"
#include "BasePanel.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "filesystem.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBenchmarkDialog::CBenchmarkDialog(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	Button *button = new Button(this, "RunButton", "RunButton");
	button->SetCommand(new KeyValues("RunBenchmark"));
	SetSizeable(false);
	SetDeleteSelfOnClose(true);

	LoadControlSettings("Resource/BenchmarkDialog.res");
}

//-----------------------------------------------------------------------------
// Purpose: Launches the benchmark
//-----------------------------------------------------------------------------
void CBenchmarkDialog::RunBenchmark()
{
	// apply settings
	BasePanel()->ApplyOptionsDialogSettings();

	// launch the map
	engine->ClientCmd_Unrestricted("disconnect\n");
	engine->ClientCmd_Unrestricted("wait\n");
	engine->ClientCmd_Unrestricted("wait\n");
	engine->ClientCmd_Unrestricted("maxplayers 1\n");
	engine->ClientCmd_Unrestricted("progress_enable\n");
	engine->ClientCmd_Unrestricted("map test_hardware\n");

	// close this dialog
	Close();
}


//-----------------------------------------------------------------------------
// Purpose: Displays benchmark results
//-----------------------------------------------------------------------------
class CBenchmarkResultsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBenchmarkResultsDialog, vgui::Frame );
public:
	CBenchmarkResultsDialog( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
	{
		SetTitle("#GameUI_BenchmarkResults_Title", true);
		SetDeleteSelfOnClose(true);
		SetSizeable(false);

		m_pUploadCheck = new CheckButton( this, "UploadCheck", "#GameUI_BenchmarkResults_UploadNow" );

		LoadControlSettings("Resource/BenchmarkResultsDialog.res");
		
		m_pUploadCheck->SetSelected( true );
		MoveToCenterOfScreen();
	}

	virtual void Activate()
	{
		BaseClass::Activate();

		KeyValues *kv = new KeyValues( "Benchmark" );
		if ( kv->LoadFromFile( g_pFullFileSystem, "results/results.txt", "MOD" ) )
		{
			// get the framerate
			char szFrameRate[32];
			Q_snprintf( szFrameRate, sizeof(szFrameRate), "%.2f", kv->GetFloat("framerate") );
			SetDialogVariable( "framerate", szFrameRate );
		}
		else
		{
			Close();
		}
		kv->deleteThis();
	}

	void OnKeyCodePressed( KeyCode code )
	{
		if ( code == KEY_XBUTTON_B )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

private:
	virtual void OnClose()
	{
		if ( m_pUploadCheck->IsSelected() )
		{
			engine->ClientCmd_Unrestricted( "bench_upload\n" );
		}
		BaseClass::OnClose();
	}

	vgui::CheckButton *m_pUploadCheck;
};

//-----------------------------------------------------------------------------
// Purpose: Launches the stats dialog
//-----------------------------------------------------------------------------
CON_COMMAND_F( bench_showstatsdialog, "Shows a dialog displaying the most recent benchmark results.", FCVAR_CHEAT )
{
	static vgui::DHANDLE<CBenchmarkResultsDialog> g_BenchmarkResultsDialog;

	if (!g_BenchmarkResultsDialog.Get())
	{
		g_BenchmarkResultsDialog = new CBenchmarkResultsDialog( BasePanel(), "BenchmarkResultsDialog" );
	}

	g_BenchmarkResultsDialog->Activate();
}
