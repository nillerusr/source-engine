//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "toolutils/savewindowpositions.h"
#include "iregistry.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/ToolWindow.h"
#include "vgui/ISurface.h"
#include "vgui_controls/PropertySheet.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlbuffer.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: This will save the bounds and the visibility state of UI elements registered during startup
// FIXME:  Preserve Z order?
//-----------------------------------------------------------------------------
class CWindowPositionMgr : public IWindowPositionMgr
{
public:
	// Inherited from IWindowPositionMgr
	virtual void	SavePositions( char const *filename, char const *key );
	virtual bool	LoadPositions( char const *filename, Panel *parent, vgui::IToolWindowFactory *factory, char const *key, bool force = false );
	virtual void	RegisterPanel( char const *saveName, Panel *panel, bool contextMenu );
	virtual void	UnregisterPanel( vgui::Panel *panel );

private:
	struct LoadInfo_t
	{
		CUtlSymbol		m_Name;
		PHandle			m_hPanel;
		bool			m_bLoaded;
		bool			m_bContextMenu;
	};

	LoadInfo_t	*Find( Panel *panel );
	LoadInfo_t	*Find( char const *panelName );

	CUtlVector< LoadInfo_t > m_Panels;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CWindowPositionMgr g_WindowPositionMgr;
IWindowPositionMgr *windowposmgr = &g_WindowPositionMgr;

CWindowPositionMgr::LoadInfo_t *CWindowPositionMgr::Find( Panel *panel )
{
	if ( !panel )
		return NULL;

	int c = m_Panels.Count();
	for ( int i = 0; i < c; ++i )
	{
		LoadInfo_t *info = &m_Panels[ i ];
		if ( info->m_hPanel.Get() == panel )
			return info;
	}
	return NULL;
}

CWindowPositionMgr::LoadInfo_t *CWindowPositionMgr::Find( char const *panelName )
{
	if ( !panelName )
		return NULL;

	int c = m_Panels.Count();
	for ( int i = 0; i < c; ++i )
	{
		LoadInfo_t *info = &m_Panels[ i ];
		if ( !Q_stricmp( info->m_Name.String(), panelName ) )
			return info;
	}
	return NULL;
}

static void BufPrint( CUtlBuffer& buf, int level, char const *fmt, ... )
{
	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ) - 1, fmt, argptr );
	va_end( argptr );
	string[ sizeof( string ) - 1 ] = 0;

	while ( --level >= 0 )
	{
		buf.Printf( "    " );
	}
	buf.Printf( "%s", string );
}

void CWindowPositionMgr::SavePositions( char const *filename, char const *key )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buf.Printf( "%s\n", key );
	buf.Printf( "{\n" );

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float flOOW = (sw != 0.0f) ? 1.0f / (float)sw : 1.0f;
	float flOOH = (sh != 0.0f) ? 1.0f / (float)sh : 1.0f;

	int c = ToolWindow::GetToolWindowCount();

	for ( int i = 0 ; i < c; ++i )
	{
		ToolWindow *tw = ToolWindow::GetToolWindow( i );
		Assert( tw );
		if ( !tw )
			continue;

		BufPrint( buf, 1, "toolwindow\n" );
		BufPrint( buf, 1, "{\n" );

		// Get panel bounds
		int x, y, w, h;
		tw->GetBounds( x, y, w, h );

		float fx = (float)x * flOOW;
		float fy = (float)y * flOOH;
		float fw = (float)w * flOOW;
		float fh = (float)h * flOOH;
		BufPrint( buf, 2, "bounds \"%.10f %.10f %.10f %.10f\"\n", fx, fy, fw, fh );

		// Now iterate the actual contained panels
		PropertySheet *sheet = tw->GetPropertySheet();
		Assert( sheet );
		if ( sheet )
		{
			int subCount = sheet->GetNumPages();
			Assert( subCount > 0 );
			if ( subCount > 0 )
			{
				BufPrint( buf, 2, "windows\n" );
				BufPrint( buf, 2, "{\n" );

				for ( int s = 0 ; s < subCount; ++s )
				{
					Panel *subPanel = sheet->GetPage( s );
					if ( !subPanel )
						continue;

					LoadInfo_t *info = Find( subPanel );
					if ( !info )
						continue;

					BufPrint( buf, 3, "panel \"%s\"\n", info->m_Name.String() );
				}

				BufPrint( buf, 2, "}\n" );
			}
		}

		BufPrint( buf, 1, "}\n" );
	}

	buf.Printf( "}\n" );

	if ( g_pFullFileSystem->FileExists( filename, "DEFAULT_WRITE_PATH" ) &&
		!g_pFullFileSystem->IsFileWritable( filename, "DEFAULT_WRITE_PATH" ) )
	{
		Warning( "IFM window layout file '%s' is read-only!!!\n", filename );
	}

	FileHandle_t h = g_pFullFileSystem->Open( filename, "wb", "DEFAULT_WRITE_PATH" );
	if ( FILESYSTEM_INVALID_HANDLE != h )
	{
		g_pFullFileSystem->Write( buf.Base(), buf.TellPut(), h );
		g_pFullFileSystem->Close( h );
	}
}

bool CWindowPositionMgr::LoadPositions( char const *filename, vgui::Panel *parent, vgui::IToolWindowFactory *factory, char const *key, bool force /*=false*/ )
{
	bool success = false;

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );

	KeyValues *kv = new KeyValues( key );
	if ( kv->LoadFromFile( g_pFullFileSystem, filename, "GAME" ) )
	{
		// Walk through tools
		for ( KeyValues *tw = kv->GetFirstSubKey(); tw != NULL; tw = tw->GetNextKey() )
		{
			if ( Q_stricmp( tw->GetName(), "toolwindow" ) )
				continue;

			// read bounds
			float fx, fy, fw, fh;
			int x, y, w, h;
			char const *bounds = tw->GetString( "bounds", "" );
			if ( !bounds || !bounds[ 0 ] )
				continue;

			if ( 4 != sscanf( bounds, "%f %f %f %f", &fx, &fy, &fw, &fh ) )
				continue;

			x = (int)( sw * fx + 0.5f );
			y = (int)( sh * fy + 0.5f );
			w = (int)( sw * fw + 0.5f );
			h = (int)( sh * fh + 0.5f );

			w = clamp( w, 0, sw );
			h = clamp( h, 0, sh );

			// Now load pages
			KeyValues *pages = tw->FindKey( "windows", false );
			if ( !pages )
				continue;

			ToolWindow *newTool = factory->InstanceToolWindow( parent, true, NULL, NULL, false );
			newTool->SetBounds( x, y, w, h );

			for ( KeyValues *page = pages->GetFirstSubKey(); page != NULL; page = page->GetNextKey() )
			{
				if ( Q_stricmp( page->GetName(), "panel" ) )
					continue;

				char const *pageName = page->GetString();
				if ( !pageName || !pageName[ 0 ] )
					continue;

				LoadInfo_t *info = Find( pageName );
				if ( !info )
					continue;

				newTool->AddPage( info->m_hPanel.Get(), info->m_Name.String(), info->m_bContextMenu );
				success = true;
			}

			// If we didn't successfully create something, delete the tool
			if ( !success )
			{
				delete newTool;
			}
		}
	}
	kv->deleteThis();

	return success;
}

void CWindowPositionMgr::RegisterPanel( char const *saveName, Panel *panel, bool contextMenu )
{
	char const *panelName = panel->GetName();
	if ( !panelName || !panelName[ 0 ] )
	{
		Warning( "CWindowPositionMgr::RegisterPanel:  Panel has NULL or blank name!!!\n" );
		return;
	}

	LoadInfo_t info;
	info.m_hPanel = panel;
	info.m_Name = saveName;
	info.m_bLoaded = false;
	info.m_bContextMenu = contextMenu;

	m_Panels.AddToTail( info );
}

void CWindowPositionMgr::UnregisterPanel( vgui::Panel *panel )
{
	int c = m_Panels.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		if ( m_Panels[ i ].m_hPanel.Get() != panel )
			continue;

		m_Panels.Remove( i );
		break;
	}
}
