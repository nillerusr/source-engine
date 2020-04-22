//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "mapinfo.h"


class CSurroundTest : public CPointEntity
{
public:
	DECLARE_CLASS( CSurroundTest, CPointEntity );

	void	FireCorrectOutput( inputdata_t &inputdata );
	void	Spawn( void );

private:
	
	COutputEvent m_On2Speakers;
	COutputEvent m_On4Speakers;
	COutputEvent m_On51Speakers;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_surroundtest, CSurroundTest );

BEGIN_DATADESC( CSurroundTest )
	DEFINE_INPUTFUNC( FIELD_VOID, "FireCorrectOutput", FireCorrectOutput ),
	DEFINE_OUTPUT( m_On2Speakers, "On2Speakers" ),
	DEFINE_OUTPUT( m_On4Speakers, "On4Speakers" ),
	DEFINE_OUTPUT( m_On51Speakers, "On51Speakers" ),
END_DATADESC()

enum
{
	SND_SURROUND_HEADPHONES = 0,
	SND_SURROUND_2SPEAKERS = 2,
	SND_SURROUND_4SPEAKERS = 4,
	SND_SURROUND_51SPEAKERS, 
};

void CSurroundTest::FireCorrectOutput( inputdata_t &inputdata )
{
	ConVar const *pSurroundCVar = cvar->FindVar( "snd_surround_speakers" );

	if ( pSurroundCVar )
	{
		int iSetting = pSurroundCVar->GetInt();
		
		if ( iSetting == SND_SURROUND_HEADPHONES || iSetting == SND_SURROUND_2SPEAKERS )
		{
			m_On2Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_4SPEAKERS )
		{
			m_On4Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_51SPEAKERS )
		{
			m_On51Speakers.FireOutput( this, this );
		}
	}
}

void CSurroundTest::Spawn( void )
{
	BaseClass::Spawn();
}