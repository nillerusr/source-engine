#include "cbase.h"
#include "c_asw_objective.h"
#include "hud_element_helper.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_hud_objective.h"
#include <vgui/ILocalize.h>
#include "c_asw_marker.h"
#include "asw_hud_minimap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Objective, DT_ASW_Objective, CASW_Objective)
	RecvPropString( RECVINFO( m_ObjectiveTitle ) ),
	RecvPropString( RECVINFO( m_ObjectiveDescription1 ) ),
	RecvPropString( RECVINFO( m_ObjectiveDescription2 ) ),
	RecvPropString( RECVINFO( m_ObjectiveDescription3 ) ),
	RecvPropString( RECVINFO( m_ObjectiveDescription4 ) ),
	RecvPropString( RECVINFO( m_ObjectiveImage ) ),
	RecvPropString( RECVINFO( m_ObjectiveMarkerName ) ),	
	RecvPropString( RECVINFO( m_ObjectiveInfoIcon1 ) ),
	RecvPropString( RECVINFO( m_ObjectiveInfoIcon2 ) ),
	RecvPropString( RECVINFO( m_ObjectiveInfoIcon3 ) ),
	RecvPropString( RECVINFO( m_ObjectiveInfoIcon4 ) ),
	RecvPropString( RECVINFO( m_ObjectiveInfoIcon5 ) ),
	RecvPropString( RECVINFO( m_ObjectiveIcon ) ),
	RecvPropString( RECVINFO( m_MapMarkings ) ),	
	RecvPropBool( RECVINFO( m_bComplete ) ),	
	RecvPropBool( RECVINFO( m_bFailed ) ),
	RecvPropBool( RECVINFO( m_bOptional ) ),
	RecvPropBool( RECVINFO( m_bDummy ) ),
	RecvPropBool( RECVINFO( m_bVisible ) ),
	RecvPropInt( RECVINFO( m_Priority ) ),
END_RECV_TABLE()

C_ASW_Objective::C_ASW_Objective()
{
	m_ObjectiveTitle[0]=NULL;
	m_ObjectiveDescription1[0]=NULL;
	m_ObjectiveDescription2[0]=NULL;
	m_ObjectiveDescription3[0]=NULL;
	m_ObjectiveDescription4[0]=NULL;
	m_ObjectiveIcon[0] = '\0';
	m_ObjectiveImage[0]=NULL;
	m_ObjectiveMarkerName[0]=NULL;
	m_bComplete = false;
	m_bDummy = false;
	m_ObjectiveIconTextureID = -1;
	m_iNumMapMarks = 0;
}

C_ASW_Objective::~C_ASW_Objective()
{
}

void C_ASW_Objective::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( gpGlobals->curtime + 1.0f );
	}
}

const wchar_t* C_ASW_Objective::GetObjectiveTitle()
{
	static wchar_t wbuffer[255];
	if ( m_ObjectiveTitle[0] == '#' )
	{
		wchar_t* pTitle = g_pVGuiLocalize->Find(m_ObjectiveTitle);
		if (pTitle)
			return pTitle;
	}

	g_pVGuiLocalize->ConvertANSIToUnicode(m_ObjectiveTitle, wbuffer, sizeof(wbuffer));
	return wbuffer;
}

ObjectiveMapMark* C_ASW_Objective::GetMapMarkings( void )
{
	return m_MapMarks;
}

const char* C_ASW_Objective::GetDescription(int i)
{
	switch ( i )
	{
	case 0:
		return m_ObjectiveDescription1;
	case 1:
		return m_ObjectiveDescription2;
	case 2:
		return m_ObjectiveDescription3;
	case 3:
		return m_ObjectiveDescription4;
	}
	return NULL;
}

void C_ASW_Objective::ClientThink( void )
{
	UpdateMarkings();

	SetNextClientThink( gpGlobals->curtime + 1.0f );
}

// returns the next word from a string
int C_ASW_Objective::FindNextToken(const char* pText)
{
	int iTextLength = strlen(pText);
	for ( int i = 0; i < iTextLength; i++ )
	{
		if ( pText[i]==' ' || pText[i]=='\0' )
		{
			szTokenBuffer[i] = '\0';
			//if (pText[i] != '\0')
			//Msg("FindNextToken: Tokenbuffer = %s (ptext: %s)\n", szTokenBuffer, pText + i+1);
			return i+1;
		}
		szTokenBuffer[i] = pText[i];
	}
	szTokenBuffer[iTextLength] = '\0';
	//Msg("FindNextToken end of string. Tokenbuffer = %s\n", szTokenBuffer);
	return iTextLength+1;
}

void C_ASW_Objective::UpdateMarkings( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	m_iNumMapMarks = 0;

	// get the objective's map marking string
	CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );

	if ( pMap )
	{
		bool bMarkers = false;

		for ( int i = 0; i < IObjectiveMarkerList::AutoList().Count(); ++i )
		{
			C_ASW_Marker *pMarker = static_cast< C_ASW_Marker* >( IObjectiveMarkerList::AutoList()[ i ] );

			if ( V_strcmp( pMarker->GetObjectiveName(), GetEntityName() ) == 0 )
			{
				Assert( m_iNumMapMarks < ASW_NUM_MAP_MARKS );

				m_MapMarks[ m_iNumMapMarks ].type = MMT_BRACKETS;

				int nHalfWidth = pMarker->GetMapWidth() / 2;
				int nHalfHeight = pMarker->GetMapHeight() / 2;

				Vector vUpperLeft = pMarker->GetAbsOrigin() + Vector( -nHalfWidth, nHalfHeight, 0.0f );
				Vector vLowerRight = pMarker->GetAbsOrigin() + Vector( nHalfWidth, -nHalfHeight, 0.0f );

				Vector2D v2DUpperLeft = pMap->WorldToMapTexture( vUpperLeft );
				Vector2D v2DLowerRight = pMap->WorldToMapTexture( vLowerRight );

				m_MapMarks[ m_iNumMapMarks ].x = v2DUpperLeft.x;
				m_MapMarks[ m_iNumMapMarks ].y = v2DUpperLeft.y;
				m_MapMarks[ m_iNumMapMarks ].w = v2DLowerRight.x - v2DUpperLeft.x;
				m_MapMarks[ m_iNumMapMarks ].h = v2DLowerRight.y - v2DUpperLeft.y;

				m_MapMarks[ m_iNumMapMarks ].bComplete = pMarker->IsComplete();
				m_MapMarks[ m_iNumMapMarks ].bEnabled = pMarker->IsEnabled();

				bMarkers = true;

				m_iNumMapMarks++;
			}
		}

		if ( bMarkers )
		{
			// Already gathered brackets from makers
			return;
		}
	}

	int nStrLen = V_strlen( m_MapMarkings );
	bool bFoundMark = true;
	int pos = 0;
	while ( bFoundMark && m_iNumMapMarks < ASW_NUM_MAP_MARKS && pos < nStrLen )
	{
		pos += FindNextToken( &m_MapMarkings[ pos ] );
		if ( !Q_stricmp(szTokenBuffer, "BRACKETS") )
		{
			m_MapMarks[ m_iNumMapMarks ].type = MMT_BRACKETS;
			pos += FindNextToken( &m_MapMarkings[ pos ] );
			m_MapMarks[ m_iNumMapMarks ].x = atoi( szTokenBuffer );
			pos += FindNextToken( &m_MapMarkings[ pos ] );
			m_MapMarks[ m_iNumMapMarks ].y = atoi( szTokenBuffer );
			pos += FindNextToken( &m_MapMarkings[ pos ] );
			m_MapMarks[ m_iNumMapMarks ].w = atoi( szTokenBuffer );
			pos += FindNextToken( &m_MapMarkings[ pos ] );
			m_MapMarks[ m_iNumMapMarks ].h = atoi( szTokenBuffer );

			m_MapMarks[ m_iNumMapMarks ].bComplete = false;
			m_MapMarks[ m_iNumMapMarks ].bEnabled = true;

			m_iNumMapMarks++;
		}
		else
		{
			bFoundMark = false;
		}
	}
}

bool C_ASW_Objective::KeyValue( const char *szKeyName, const char *szValue )
{	
	if ( FStrEq( szKeyName, "objectivetitle" ) )
	{
		Q_strncpy( m_ObjectiveTitle, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectivedescription1" ) )
	{
		Q_strncpy( m_ObjectiveDescription1, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectivedescription2" ) )
	{
		Q_strncpy( m_ObjectiveDescription2, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectivedescription3" ) )
	{
		Q_strncpy( m_ObjectiveDescription3, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectivedescription4" ) )
	{
		Q_strncpy( m_ObjectiveDescription4, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveimage" ) )
	{
		Q_strncpy( m_ObjectiveImage, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectivemarkername" ) )
	{
		Q_strncpy( m_ObjectiveMarkerName, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveinfoicon1" ) )
	{
		Q_strncpy( m_ObjectiveInfoIcon1, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveinfoicon2" ) )
	{
		Q_strncpy( m_ObjectiveInfoIcon2, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveinfoicon3" ) )
	{
		Q_strncpy( m_ObjectiveInfoIcon3, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveinfoicon4" ) )
	{
		Q_strncpy( m_ObjectiveInfoIcon4, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveinfoicon5" ) )
	{
		Q_strncpy( m_ObjectiveInfoIcon5, szValue, 255 );
		return true;
	}
	if ( FStrEq( szKeyName, "objectiveicon" ) )
	{
		Q_strncpy( m_ObjectiveIcon, szValue, 255 );
		return true;
	}
	

	return BaseClass::KeyValue( szKeyName, szValue );
}

const char* C_ASW_Objective::GetObjectiveIconName()
{
	return m_ObjectiveIcon;
}

int C_ASW_Objective::GetObjectiveIconTextureID()
{
	if (m_ObjectiveIconTextureID == -1)
	{
		if (m_ObjectiveIcon[0] != '\0')
		{
			m_ObjectiveIconTextureID = vgui::surface()->CreateNewTextureID();		
			vgui::surface()->DrawSetTextureFile( m_ObjectiveIconTextureID, m_ObjectiveIcon, true, false);
		}
		else
		{
			return -1;
		}
	}
	return m_ObjectiveIconTextureID;
}

const char* C_ASW_Objective::GetInfoIcon(int i)
{
	switch (i)
	{
	case 0:
		return m_ObjectiveInfoIcon1;
	case 1:
		return m_ObjectiveInfoIcon2;
	case 2:
		return m_ObjectiveInfoIcon3;
	case 3:
		return m_ObjectiveInfoIcon4;
	case 4:
		return m_ObjectiveInfoIcon5;
	}
	return NULL;
}

// allows this objective to paint its status on the HUD
void C_ASW_Objective::PaintObjective(float &current_y)
{

}