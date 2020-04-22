//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "EventProperties.h"
#include "mdlviewer.h"
#include "choreoevent.h"
#include "choreoscene.h"
#include "mathlib/mathlib.h"
#include "choreochannel.h"
#include "choreoactor.h"
#include "filesystem.h"
#include "scriplib.h"

#include "eventproperties_expression.h"
#include "eventproperties_face.h"
#include "eventproperties_firetrigger.h"
#include "eventproperties_flexanimation.h"
#include "eventproperties_generic.h"
#include "eventproperties_gesture.h"
#include "eventproperties_interrupt.h"
#include "eventproperties_lookat.h"
#include "eventproperties_moveto.h"
#include "eventproperties_permitresponses.h"
#include "eventproperties_sequence.h"
#include "eventproperties_speak.h"
#include "eventproperties_subscene.h"

void CBaseEventPropertiesDialog::PopulateTagList( CEventParams *params )
{
	CChoreoScene *scene = params->m_pScene;
	if ( !scene )
		return;

	HWND control = GetControl( IDC_TAGS );
	if ( control )
	{
		SendMessage( control, CB_RESETCONTENT, 0, 0 ); 
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)va( "\"%s\" \"%s\"", params->m_szTagName, params->m_szTagWav ) );

		for ( int i = 0; i < scene->GetNumActors(); i++ )
		{
			CChoreoActor *a = scene->GetActor( i );
			if ( !a )
				continue;

			for ( int j = 0; j < a->GetNumChannels(); j++ )
			{
				CChoreoChannel *c = a->GetChannel( j );
				if ( !c )
					continue;

				for ( int k = 0 ; k < c->GetNumEvents(); k++ )
				{
					CChoreoEvent *e = c->GetEvent( k );
					if ( !e )
						continue;

					if ( e->GetNumRelativeTags() <= 0 )
						continue;

					// add each tag to combo box
					for ( int t = 0; t < e->GetNumRelativeTags(); t++ )
					{
						CEventRelativeTag *tag = e->GetRelativeTag( t );
						if ( !tag )
							continue;

						SendMessage( control, CB_ADDSTRING, 0, (LPARAM)va( "\"%s\" \"%s\"", tag->GetName(), e->GetParameters() ) ); 
					}
				}
			}
		}
	}
}

#include "mapentities.h"
#include "utldict.h"

struct CMapEntityData
{
	CMapEntityData()
	{
		origin.Init();
		angles.Init();
	}

	Vector origin;
	QAngle angles;
};

class CMapEntities : public IMapEntities
{
public:
	CMapEntities();
	~CMapEntities();

	virtual void	CheckUpdateMap( char const *mapname );
	virtual bool	LookupOrigin( char const *name, Vector& origin, QAngle& angles )
	{
		int idx = FindNamedEntity( name );
		if ( idx == -1 )
		{
			origin.Init();
			angles.Init();
			return false;
		}

		CMapEntityData *e = &m_Entities[ idx ];
		Assert( e );
		origin = e->origin;
		angles = e->angles;
		return true;
	}

	virtual int		Count( void );
	virtual char const *GetName( int number );

	int	FindNamedEntity( char const *name );

private:
	char		m_szCurrentMap[ 1024 ];

	CUtlDict< CMapEntityData, int >	m_Entities;
};

static CMapEntities g_MapEntities;
// Expose to rest of tool
IMapEntities *mapentities = &g_MapEntities;

CMapEntities::CMapEntities()
{
	m_szCurrentMap[ 0 ] = 0;
}

CMapEntities::~CMapEntities()
{
	m_Entities.RemoveAll();
}

int CMapEntities::FindNamedEntity( char const *name )
{
	char lowername[ 128 ];
	strcpy( lowername, name );
	_strlwr( lowername );

	int index = m_Entities.Find( lowername );
	if ( index == m_Entities.InvalidIndex() )
		return -1;

	return index;
}

#include "bspfile.h"

void CMapEntities::CheckUpdateMap( char const *mapname )
{
	if ( !mapname || !mapname[ 0 ] )
		return;

	if ( !stricmp( mapname, m_szCurrentMap ) )
		return;

	// Latch off the name of the map
	Q_strncpy( m_szCurrentMap, mapname, sizeof( m_szCurrentMap ) );

	// Load names from map
	m_Entities.RemoveAll();

	FileHandle_t hfile = filesystem->Open( mapname, "rb" );
	if ( hfile == FILESYSTEM_INVALID_HANDLE )
		return;

	dheader_t header;
	filesystem->Read( &header, sizeof( header ), hfile );

	// Check the header
	if ( header.ident != IDBSPHEADER ||
		 header.version < MINBSPVERSION || header.version > BSPVERSION )
	{
		Con_ErrorPrintf( "BSP file %s is wrong version (%i), expected (%i)\n",
			mapname,
			header.version, 
			BSPVERSION );

		filesystem->Close( hfile );
		return;
	}

	// Find the LUMP_PAKFILE offset
	lump_t *entlump = &header.lumps[ LUMP_ENTITIES ];
	if ( entlump->filelen <= 0 )
	{
		Con_ErrorPrintf( "BSP file %s is missing entity lump\n", mapname );

		// It's empty or only contains a file header ( so there are no entries ), so don't add to search paths
		filesystem->Close( hfile );
		return;
	}

	// Seek to correct position
	filesystem->Seek( hfile, entlump->fileofs, FILESYSTEM_SEEK_HEAD );

	char *buffer = new char[ entlump->filelen + 1 ];
	Assert( buffer );

	filesystem->Read( buffer, entlump->filelen, hfile );

	filesystem->Close( hfile );

	buffer[ entlump->filelen ] = 0;

	// Now we have entity buffer, now parse it
	ParseFromMemory( buffer, entlump->filelen );

	while ( 1 )
	{
		if (!GetToken (true))
			break;

		if (Q_stricmp (token, "{") )
			Error ("ParseEntity: { not found");
		
		char name[ 256 ];
		char origin[ 256 ];
		char angles[ 256 ];

		name[ 0 ] = 0;
		origin[ 0 ] = 0;
		angles[ 0 ] = 0;

		do
		{
			char key[ 256 ];
			char value[ 256 ];

			if (!GetToken (true))
			{
				Error ("ParseEntity: EOF without closing brace");
			}

			if (!Q_stricmp (token, "}") )
				break;

			Q_strncpy( key, token, sizeof( key ) );

			GetToken (false);

			Q_strncpy( value, token, sizeof( value ) );

			// Con_Printf( "Parsed %s -- %s\n", key, value );

			if ( !Q_stricmp( key, "name" ) )
			{
				Q_strncpy( name, value, sizeof( name ) );
			}
			if ( !Q_stricmp( key, "targetname" ) )
			{
				Q_strncpy( name, value, sizeof( name ) );
			}
			if ( !Q_stricmp( key, "origin" ) )
			{
				Q_strncpy( origin, value, sizeof( origin ) );
			}
			if ( !Q_stricmp( key, "angles" ) )
			{
				Q_strncpy( angles, value, sizeof( angles ) );
			}

		} while (1);

		if ( name[ 0 ] )
		{
			if ( FindNamedEntity( name ) == - 1 )
			{
				CMapEntityData ent;
				
				float org[3];
				if ( origin[ 0 ] )
				{
					if ( 3 == sscanf( origin, "%f %f %f", &org[ 0 ], &org[ 1 ], &org[ 2 ] ) )
					{
						ent.origin = Vector( org[ 0 ], org[ 1 ], org[ 2 ] );

						// Con_Printf( "read %f %f %f for entity %s\n", org[0], org[1], org[2], name );
					}
				}
				if ( angles[ 0 ] )
				{
					if ( 3 == sscanf( angles, "%f %f %f", &org[ 0 ], &org[ 1 ], &org[ 2 ] ) )
					{
						ent.angles = QAngle( org[ 0 ], org[ 1 ], org[ 2 ] );

						// Con_Printf( "read %f %f %f for entity %s\n", org[0], org[1], org[2], name );
					}
				}

				m_Entities.Insert( name, ent );
			}
		}
	}

	delete[] buffer;
}

int CMapEntities::Count( void )
{
	return m_Entities.Count();
}

char const *CMapEntities::GetName( int number )
{
	if ( number < 0 || number >= (int)m_Entities.Count() )
		return NULL;

	return m_Entities.GetElementName( number );
}

bool NameLessFunc( const char *const& name1, const char *const& name2 )
{
	if ( Q_stricmp( name1, name2 ) < 0 )
		return true;
	return false;
}

void CBaseEventPropertiesDialog::SetDialogTitle( CEventParams *params, char const *eventname, char const *desc )
{
	char sz[ 256 ];
	Q_snprintf( sz, sizeof( sz ), " : %s", eventname  );
	Q_strncat( params->m_szDialogTitle, sz, sizeof(  params->m_szDialogTitle ), COPY_ALL_CHARACTERS );
	Q_snprintf( sz, sizeof( sz ), "%s:", desc );

	// Set dialog title
	SetWindowText( m_hDialog, params->m_szDialogTitle );
	// Set type name field
	SetDlgItemText( m_hDialog, IDC_TYPENAME, sz );
	// Set event name
	SetDlgItemText( m_hDialog, IDC_EVENTNAME, params->m_szName );
}

void CBaseEventPropertiesDialog::ShowControlsForEventType( CEventParams *params )
{
	// Special processing for various settings
	if ( !params->m_bHasEndTime )
	{
		ShowWindow( GetControl( IDC_ENDTIME ), SW_HIDE );
	}

	if ( params->m_bFixedLength )
	{
		ShowWindow( GetControl( IDC_ENDTIME ), SW_HIDE );
		ShowWindow( GetControl( IDC_CHECK_ENDTIME ), SW_HIDE );
	}
}

void CBaseEventPropertiesDialog::InitControlData( CEventParams *params )
{
	SetDlgItemText( m_hDialog, IDC_STARTTIME, va( "%f", params->m_flStartTime ) );
	SetDlgItemText( m_hDialog, IDC_ENDTIME, va( "%f", params->m_flEndTime ) );

	SendMessage( GetControl( IDC_CHECK_ENDTIME ), BM_SETCHECK, 
		( WPARAM ) params->m_bHasEndTime ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	if ( GetControl( IDC_CHECK_RESUMECONDITION ) != (HWND)0 )
	{
		SendMessage( GetControl( IDC_CHECK_RESUMECONDITION ), BM_SETCHECK, 
			( WPARAM ) params->m_bResumeCondition ? BST_CHECKED : BST_UNCHECKED,
			( LPARAM )0 );
	}
	
	SendMessage( GetControl( IDC_CHECK_DISABLED ), BM_SETCHECK, 
		( WPARAM ) params->m_bDisabled ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );

	PopulateTagList( params );
}

BOOL CBaseEventPropertiesDialog::InternalHandleMessage( CEventParams *params, HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, bool& handled )
{
	handled = false;
	switch(uMsg)
	{
	default:
		break;
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		default:
			break;
		case IDC_CHECK_DISABLED:
			{
				params->m_bDisabled = SendMessage( GetControl( IDC_CHECK_DISABLED ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				handled = true;
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

void CBaseEventPropertiesDialog::PopulateNamedActorList( HWND wnd, CEventParams *params )
{
	int i;

	char const *mapname = NULL;
	if ( params->m_pScene )
	{
		mapname = params->m_pScene->GetMapname();
	}

	CUtlRBTree< char const *, int >		m_SortedNames( 0, 0, NameLessFunc );

	if ( mapname )
	{
		g_MapEntities.CheckUpdateMap( mapname );

		for ( i = 0; i < g_MapEntities.Count(); i++ )
		{
			char const *name = g_MapEntities.GetName( i );
			if ( name && name[ 0 ] )
			{
				m_SortedNames.Insert( name );
			}
		}
	}

	for ( i = 0 ; i < params->m_pScene->GetNumActors() ; i++ )
	{
		CChoreoActor *actor = params->m_pScene->GetActor( i );
		if ( actor && actor->GetName() && actor->GetName()[0] )
		{
			if ( m_SortedNames.Find( actor->GetName() ) == m_SortedNames.InvalidIndex() )
			{
				m_SortedNames.Insert( actor->GetName() );
			}
		}
	}

	i = m_SortedNames.FirstInorder();
	while ( i != m_SortedNames.InvalidIndex() )
	{
		char const *name = m_SortedNames[ i ];
		if ( name && name[ 0 ] )
		{
			SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)name ); 
		}

		i = m_SortedNames.NextInorder( i );
	}

	/*
	// Note have to do this here, after posting data to the control, since we are storing a raw string pointer in m_SortedNames!!!
	if ( allActors )
	{
		allActors->deleteThis();
	}
	*/

	// These events can also be directed at another player or named target, too
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!player" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!enemy" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!self" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!friend" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!speechtarget" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target1" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target2" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target3" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target4" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target5" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target6" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target7" );
	SendMessage( wnd, CB_ADDSTRING, 0, (LPARAM)"!target8" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static void
//-----------------------------------------------------------------------------
void CBaseEventPropertiesDialog::ParseTags( CEventParams *params )
{
	strcpy( params->m_szTagName, "" );
	strcpy( params->m_szTagWav, "" );

	if ( params->m_bUsesTag )
	{
		// Parse out the two tokens
		char selectedText[ 512 ];
		selectedText[ 0 ] = 0;
		HWND control = GetControl( IDC_TAGS );
		if ( control )
		{
			SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( selectedText ), (LPARAM)selectedText );
		}

		ParseFromMemory( selectedText, strlen( selectedText ) );
		if ( TokenAvailable() )
		{
			GetToken( false );
			char tagname[ 256 ];
			strcpy( tagname, token );
			if ( TokenAvailable() )
			{
				GetToken( false );
				char wavename[ 256 ];
				strcpy( wavename, token );

				// Valid
				strcpy( params->m_szTagName, tagname );
				strcpy( params->m_szTagWav, wavename );

			}
			else
			{
				params->m_bUsesTag = false;
			}
		}
		else
		{
			params->m_bUsesTag = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static void
//-----------------------------------------------------------------------------
void CBaseEventPropertiesDialog::UpdateTagRadioButtons( CEventParams *params )
{
	if ( params->m_bUsesTag )
	{
		SendMessage( GetControl( IDC_RELATIVESTART ), BM_SETCHECK, ( WPARAM )BST_CHECKED, (LPARAM)0 );
		SendMessage( GetControl( IDC_ABSOLUTESTART ), BM_SETCHECK, ( WPARAM )BST_UNCHECKED, (LPARAM)0 );
	}
	else
	{
		SendMessage( GetControl( IDC_ABSOLUTESTART ), BM_SETCHECK, ( WPARAM )BST_CHECKED, (LPARAM)0 );
		SendMessage( GetControl( IDC_RELATIVESTART ), BM_SETCHECK, ( WPARAM )BST_UNCHECKED, (LPARAM)0 );
	}
}

void CBaseEventPropertiesDialog::GetSplineRect( HWND placeholder, RECT& rcOut )
{
	GetWindowRect( placeholder, &rcOut );
	RECT rcDlg;
	GetWindowRect( m_hDialog, &rcDlg );

	OffsetRect( &rcOut, -rcDlg.left, -rcDlg.top );
}

void CBaseEventPropertiesDialog::DrawSpline( HDC hdc, HWND placeholder, CChoreoEvent *e )
{
	RECT rcOut;

	GetSplineRect( placeholder, rcOut );

	HBRUSH bg = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
	FillRect( hdc, &rcOut, bg );
	DeleteObject( bg );

	if ( !e )
		return;

	// Draw spline
	float range = ( float )( rcOut.right - rcOut.left );
	if ( range <= 1.0f )
		return;

	float height = ( float )( rcOut.bottom - rcOut.top );

	HPEN pen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNTEXT ) );
	HPEN oldPen = (HPEN)SelectObject( hdc, pen );

	float duration = e->GetDuration();
	float starttime = e->GetStartTime();

	for ( int i = 0; i < (int)range; i++ )
	{
		float frac = ( float )i / ( range - 1 );

		float scale = 1.0f - e->GetIntensity( starttime + frac * duration );

		int h = ( int ) ( scale * ( height - 1 ) );

		if ( i == 0 )
		{
			MoveToEx( hdc, rcOut.left + i, rcOut.top + h, NULL );
		}
		else
		{
			LineTo( hdc, rcOut.left + i, rcOut.top + h );
		}
	}

	SelectObject( hdc, oldPen );

	HBRUSH frame = CreateSolidBrush( GetSysColor( COLOR_BTNSHADOW ) );
	InflateRect( &rcOut, 1, 1 );
	FrameRect( hdc, &rcOut, frame );
	DeleteObject( frame );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int EventProperties( CEventParams *params )
{
	int iret = 1;
	switch ( params->m_nType )
	{
	default:
		break;
	case CChoreoEvent::EXPRESSION:
		return EventProperties_Expression( params );
	case CChoreoEvent::LOOKAT:
		return EventProperties_LookAt( params );
	case CChoreoEvent::MOVETO:
		return EventProperties_MoveTo( params );
	case CChoreoEvent::SPEAK:
		return EventProperties_Speak( params );
	case CChoreoEvent::GESTURE:
		return EventProperties_Gesture( params );
	case CChoreoEvent::SEQUENCE:
		return EventProperties_Sequence( params );
	case CChoreoEvent::FACE:
		return EventProperties_Face( params );
	case CChoreoEvent::FIRETRIGGER:
		return EventProperties_FireTrigger( params );
	case CChoreoEvent::FLEXANIMATION:
		return EventProperties_FlexAnimation( params );
	case CChoreoEvent::SUBSCENE:
		return EventProperties_SubScene( params );
	case CChoreoEvent::INTERRUPT:
		return EventProperties_Interrupt( params );
	case CChoreoEvent::PERMIT_RESPONSES:
		return EventProperties_PermitResponses( params );
	case CChoreoEvent::GENERIC:
		return EventProperties_Generic( params );
	}

	return iret;
}
