//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "shake.h"
#include "tmessage.h"
#include "cl_demoaction.h"
#include "cl_demoactionmanager.h"
#include "cl_demoactioneditors.h"
#include "cl_demoaction_types.h"
#include "cl_demoeditorpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*action - 
//			newaction - 
//-----------------------------------------------------------------------------
CBaseActionEditDialog::CBaseActionEditDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
: vgui::Frame( parent, CBaseDemoAction::NameForType( action->GetType() ) ), 
	m_pEditor( parent ),
	m_pAction( action ),
	m_bNewAction( newaction )
{
	if ( m_bNewAction )
	{
		SetTitle( va( "New %s Action", CBaseDemoAction::NameForType( action->GetType() ) ), true );
	}
	else
	{
		SetTitle( va( "Edit %s Action", CBaseDemoAction::NameForType( action->GetType() ) ), true );
	}
	m_pOK = new vgui::Button( this, "OK", "OK" );
	m_pCancel = new vgui::Button( this, "Cancel", "Cancel" );

	m_pActionName = new vgui::TextEntry( this, "ActionName" );
	m_pStart = new vgui::TextEntry( this, "ActionStart" );

	m_pStartType = new vgui::ComboBox( this, "ActionStartType", (int)NUM_TIMING_TYPES, false );

	for ( int i = 0; i < (int)NUM_TIMING_TYPES; i++ )
	{
		m_pStartType->AddItem( CBaseDemoAction::NameForTimingType( (DEMOACTIONTIMINGTYPE)i ), NULL );
	}

	SetSizeable( false );
	SetMoveable( true );
}
	
static bool g_BaseActionEditSaveChained = false;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionEditDialog::Init( void )
{
	// Fill in data from passed in action
	m_pActionName->SetText( m_pAction->GetActionName() );
	m_pStartType->ActivateItem( (int)m_pAction->GetTimingType() );

	switch ( m_pAction->GetTimingType() )
	{
	default:
	case ACTION_USES_NEITHER:
		{
			m_pStart->SetText( "" );
		}
		break;
	case ACTION_USES_TICK:
		{
			m_pStart->SetText( va( "%i", m_pAction->GetStartTick() ) );
		}
		break;
	case ACTION_USES_TIME:
		{
			m_pStart->SetText( va( "%.3f", m_pAction->GetStartTime() ) );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionEditDialog::OnSaveChanges( void )
{
	bool bret = false;
	// No baseclass chain
	g_BaseActionEditSaveChained = true;

	char actionname[ 512 ];
	m_pActionName->GetText( actionname, sizeof( actionname ) );
	if ( Q_strcmp( m_pAction->GetActionName(), actionname ) )
	{
		bret = true;
		m_pAction->SetActionName( actionname );
	}

	char starttext[ 512 ];
	m_pStart->GetText( starttext, sizeof( starttext ) );
	char starttype[ 512 ];
	m_pStartType->GetText( starttype, sizeof( starttype ) );

	DEMOACTIONTIMINGTYPE timingType = CBaseDemoAction::TimingTypeForName( starttype );

	if ( timingType != m_pAction->GetTimingType() )
	{
		bret = true;
		m_pAction->SetTimingType( timingType );
	}

	switch ( timingType )
	{
	default:
	case ACTION_USES_NEITHER:
		{
		}
		break;
	case ACTION_USES_TICK:
		{
			int tick = atoi( starttext );
			if ( tick != m_pAction->GetStartTick() )
			{
				m_pAction->SetStartTick( tick );
				bret = true;
			}
		}
		break;
	case ACTION_USES_TIME:
		{
			float t = (float)atof( starttext );
			if ( t != m_pAction->GetStartTime() )
			{
				m_pAction->SetStartTime( t );
				bret = true;
			}
		}
		break;
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionEditDialog::OnClose()
{
	if ( m_bNewAction )
	{
		demoaction->AddAction( m_pAction );
	}

	g_BaseActionEditSaveChained = false;
	if ( OnSaveChanges() || m_bNewAction )
	{
		demoaction->SetDirty( true );
		m_pEditor->OnRefresh();
	}
	Assert( g_BaseActionEditSaveChained );
	MarkForDeletion();
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionEditDialog::OnCancel()
{
	if ( m_bNewAction )
	{
		delete m_pAction;
	}
	// Nothing, just delete
	MarkForDeletion();
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *commands - 
//-----------------------------------------------------------------------------
void CBaseActionEditDialog::OnCommand( char const *commands )
{
	if ( !Q_strcasecmp( commands, "OK" ) )
	{
		OnClose();
	}
	else if ( !Q_strcasecmp( commands, "Cancel" ) )
	{
		OnCancel();
	}
	else
	{
		BaseClass::OnCommand( commands );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*action - 
//			newaction - 
//-----------------------------------------------------------------------------
CBaseActionWithTargetDialog::CBaseActionWithTargetDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
: CBaseActionEditDialog( parent, action, newaction )
{
	m_pActionTarget = new vgui::TextEntry( this, "ActionTarget" );
}

//-----------------------------------------------------------------------------
// Purpose: Slam text with text from action
//-----------------------------------------------------------------------------
void CBaseActionWithTargetDialog::Init( void )
{
	BaseClass::Init();

	m_pActionTarget->SetText( m_pAction->GetActionTarget() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionWithTargetDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char actiontarget[ 512 ];
	m_pActionTarget->GetText( actiontarget, sizeof( actiontarget ) );

	if ( Q_strcmp( m_pAction->GetActionTarget(), actiontarget ) )
	{
		bret = true;
		m_pAction->SetActionTarget( actiontarget );
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionSkipAheadDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionSkipAheadDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionSkipAhead	*GetAction( void ) { return static_cast< CDemoActionSkipAhead * >( m_pAction ); }

	vgui::ComboBox	*m_pSkipType;
	vgui::TextEntry	*m_pSkip;
};

CBaseActionSkipAheadDialog::CBaseActionSkipAheadDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: CBaseActionEditDialog( parent, action, newaction )
{
	m_pSkip = new vgui::TextEntry( this, "ActionSkip" );

	m_pSkipType = new vgui::ComboBox( this, "ActionSkipType", (int)2, false );
	for ( int i = 1; i < (int)NUM_TIMING_TYPES; i++ )
	{
		m_pSkipType->AddItem( CBaseDemoAction::NameForTimingType( (DEMOACTIONTIMINGTYPE)i ), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionSkipAheadDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionSkipAheadDialog.res" );

	BaseClass::Init();

	if ( GetAction()->m_bUsingSkipTick )
	{
		m_pSkipType->SetText( "TimeUseTick" );
		m_pSkip->SetText( va( "%i", GetAction()->m_nSkipToTick ) );
	}
	else
	{
		m_pSkipType->SetText( "TimeUseClock" );
		m_pSkip->SetText( va( "%.3f", GetAction()->m_flSkipToTime ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionSkipAheadDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char skiptype[ 512 ];
	m_pSkipType->GetText( skiptype, sizeof( skiptype ) );

	char skipto[ 512 ];
	m_pSkip->GetText( skipto, sizeof( skipto ) );

	float fskip = (float)atof( skipto );
	int	 iskip = (int)atoi( skipto );

	if ( !Q_strcasecmp( skiptype, "TimeUseTick" ) )
	{
		if ( GetAction()->m_nSkipToTick != iskip )
		{
			bret = true;
			GetAction()->SetSkipToTick( iskip );
			GetAction()->SetSkipToTime( -1.0f );
		}
	}
	else
	{
		if ( GetAction()->m_flSkipToTime != fskip )
		{
			bret = true;
			GetAction()->SetSkipToTime( fskip );
			GetAction()->SetSkipToTick( -1 );
		}
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_SKIPAHEAD, CBaseActionSkipAheadDialog );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionStopPlaybackDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionStopPlaybackDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
		: BaseClass( parent, action, newaction )
	{
	}

	virtual void	Init( void )
	{
		LoadControlSettings( "resource\\BaseActionStopPlaybackDialog.res" );

		BaseClass::Init();
	}
private:
	CDemoActionStopPlayback	*GetAction( void ) { return static_cast< CDemoActionStopPlayback * >( m_pAction ); }
};

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_STOPPLAYBACK, CBaseActionStopPlaybackDialog );



//-----------------------------------------------------------------------------
// Purpose: Screen Fade
//-----------------------------------------------------------------------------
class CBaseActionScreenFadeStartDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionScreenFadeStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
		: BaseClass( parent, action, newaction )
	{
		m_pDuration = new vgui::TextEntry( this, "ScreenFadeDuration" );
		m_pHoldTime = new vgui::TextEntry( this, "ScreenFadeHoldTime" );

		m_pFFADE_IN = new vgui::CheckButton( this, "ScreenFadeFFADE_IN", "Fade in" );
		m_pFFADE_OUT = new vgui::CheckButton( this, "ScreenFadeFFADE_OUT", "Fade out" );
		m_pFFADE_MODULATE = new vgui::CheckButton( this, "ScreenFadeFFADE_MODULATE", "Modulate" );
		m_pFFADE_STAYOUT = new vgui::CheckButton( this, "ScreenFadeFFADE_STAYOUT", "Stay out" );
		m_pFFADE_PURGE = new vgui::CheckButton( this, "ScreenFadeFFADE_Purge", "Purge" );

		m_pColor = new vgui::TextEntry( this, "ScreenFadeColor" );
	}

	virtual void	Init( void );

	virtual bool	OnSaveChanges( void );

private:
	CDemoActionScreenFadeStart	*GetAction( void ) { return static_cast< CDemoActionScreenFadeStart * >( m_pAction ); }

	bool CheckFlagDifference( vgui::CheckButton *check, bool oldval, int flag );

	vgui::TextEntry	*m_pDuration;
	vgui::TextEntry	*m_pHoldTime;

	vgui::CheckButton *m_pFFADE_IN;
	vgui::CheckButton *m_pFFADE_OUT;
	vgui::CheckButton *m_pFFADE_MODULATE;
	vgui::CheckButton *m_pFFADE_STAYOUT;
	vgui::CheckButton *m_pFFADE_PURGE;

	vgui::TextEntry	*m_pColor;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionScreenFadeStartDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionScreenFadeStartDialog.res" );

	BaseClass::Init();

	ScreenFade_t const *f = GetAction()->GetScreenFade();

	float duration = f->duration * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	float holdTime = f->holdTime * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	int fadein = f->fadeFlags & FFADE_IN;
	int fadeout = f->fadeFlags & FFADE_OUT;
	int fademodulate = f->fadeFlags & FFADE_MODULATE;
	int fadestayout = f->fadeFlags & FFADE_STAYOUT;
	int fadepurge = f->fadeFlags & FFADE_PURGE;
	int r = f->r;
	int g = f->g;
	int b = f->b;
	int a = f->a;

	m_pDuration->SetText( va( "%.3f", duration ) );
	m_pHoldTime->SetText( va( "%.3f", holdTime ) );
	m_pColor->SetText( va( "%i %i %i %i", r, g, b, a ) );
	m_pFFADE_IN->SetSelected( fadein != 0 );
	m_pFFADE_OUT->SetSelected( fadeout != 0 );
	m_pFFADE_MODULATE->SetSelected( fademodulate != 0 );
	m_pFFADE_STAYOUT->SetSelected( fadestayout != 0 );
	m_pFFADE_PURGE->SetSelected( fadepurge != 0 );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionScreenFadeStartDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	// Grab current settings
	ScreenFade_t *f = GetAction()->GetScreenFade();

	float duration = f->duration * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	float holdTime = f->holdTime * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	bool fadein = f->fadeFlags & FFADE_IN;
	bool fadeout = ( f->fadeFlags & FFADE_OUT ) != 0;
	bool fademodulate = ( f->fadeFlags & FFADE_MODULATE ) != 0;
	bool fadestayout = ( f->fadeFlags & FFADE_STAYOUT ) != 0;
	bool fadepurge = ( f->fadeFlags & FFADE_PURGE ) != 0;
	int r = f->r;
	int g = f->g;
	int b = f->b;
	int a = f->a;

	char sz[ 512 ];
	m_pDuration->GetText( sz, sizeof( sz ) );
	if ( (float)atof( sz ) != duration )
	{
		bret = true;
		f->duration = (unsigned short)((float)(1<<SCREENFADE_FRACBITS) * (float)atof( sz ) );
	}
	m_pHoldTime->GetText( sz, sizeof( sz ) );
	if ( (float)atof( sz ) != holdTime )
	{
		bret = true;
		f->holdTime = (unsigned short)((float)(1<<SCREENFADE_FRACBITS) * (float)atof( sz ) );
	}

	int rr, gg, bb, aa;
	m_pColor->GetText( sz, sizeof( sz ) );
	if ( 4 == sscanf( sz, "%i %i %i %i", &rr, &gg, &bb, &aa ) )
	{
		rr = clamp( rr, 0, 255 );
		gg = clamp( gg, 0, 255 );
		bb = clamp( bb, 0, 255 );
		aa = clamp( aa, 0, 255 );

		if ( rr != r || gg != g || bb != b || aa != a )
		{
			bret = true;
			f->r = rr;
			f->g = gg;
			f->b = bb;
			f->a = aa;
		}
	}

	if ( CheckFlagDifference( m_pFFADE_IN, fadein, FFADE_IN ) )
	{
		bret = true;
	}
	if ( CheckFlagDifference( m_pFFADE_OUT, fadeout, FFADE_OUT ) )
	{
		bret = true;
	}
	if ( CheckFlagDifference( m_pFFADE_MODULATE, fademodulate, FFADE_MODULATE ) )
	{
		bret = true;
	}
	if ( CheckFlagDifference( m_pFFADE_STAYOUT, fadestayout, FFADE_STAYOUT ) )
	{
		bret = true;
	}
	if ( CheckFlagDifference( m_pFFADE_PURGE, fadepurge, FFADE_PURGE ) )
	{
		bret = true;
	}
	
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *check - 
//			oldval - 
//			flag - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionScreenFadeStartDialog::CheckFlagDifference( vgui::CheckButton *check, bool oldval, int flag )
{
	bool bret = false;
	if ( check->IsSelected() != oldval )
	{
		ScreenFade_t *f = GetAction()->GetScreenFade();

		bret = true;
		if ( check->IsSelected() )
		{
			f->fadeFlags |= flag;
		}
		else
		{
			f->fadeFlags &= ~flag;
		}
	}
	return bret;
}


DECLARE_DEMOACTIONEDIT( DEMO_ACTION_SCREENFADE_START, CBaseActionScreenFadeStartDialog );

//-----------------------------------------------------------------------------
// Purpose: Screen Fade
//-----------------------------------------------------------------------------
class CBaseActionTextMessageStartDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionTextMessageStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
		: BaseClass( parent, action, newaction )
	{
		m_pFadeInTime = new vgui::TextEntry( this, "TextMessageFadeInTime" );
		m_pFadeOutTime = new vgui::TextEntry( this, "TextMessageFadeOutTime" );
		m_pHoldTime = new vgui::TextEntry( this, "TextMessageHoldTime" );
		m_pFXTime = new vgui::TextEntry( this, "TextMessageFXTime" );

		m_pMessageText = new vgui::TextEntry( this, "TextMessageText" );
		m_pFontName = new vgui::ComboBox( this, "TextMessageFont", 6, false );

		m_pX = new vgui::TextEntry( this, "TextMessageX" );
		m_pY = new vgui::TextEntry( this, "TextMessageY" );

		m_pColor1 = new vgui::TextEntry( this, "TextMessageColor1" );
		m_pColor2 = new vgui::TextEntry( this, "TextMessageColor2" );

		m_pEffectType = new vgui::ComboBox( this, "TextMessageEffect", 3, false );
	}

	virtual void	Init( void );

	virtual bool	OnSaveChanges( void );

private:
	CDemoActionTextMessageStart	*GetAction( void ) { return static_cast< CDemoActionTextMessageStart * >( m_pAction ); }

	void	FillInFonts();

	bool	SaveDifferingFloat( vgui::TextEntry *control, float *curval );
	bool	SaveDifferingInt( vgui::TextEntry *control, int *curval );
	bool	SaveDifferingColor( vgui::TextEntry *control, byte *r, byte *g, byte *b, byte *a );

	enum
	{
		FADEINOUT = 0,
		FLICKER,
		WRITEOUT,

		NUM_EFFECT_TYPES
	};

	struct EffectType
	{
		char const *name;
	};

	static EffectType s_EffectTypes[];

	static int		EffectTypeForName( char const *name );
	static char const *NameForEffectType( int type );

	vgui::TextEntry	*m_pFadeInTime;
	vgui::TextEntry	*m_pFadeOutTime;
	vgui::TextEntry	*m_pHoldTime;
	vgui::TextEntry	*m_pFXTime;

	vgui::TextEntry	*m_pMessageText;
	vgui::ComboBox	*m_pFontName;

	vgui::ComboBox *m_pEffectType;

	vgui::TextEntry	*m_pColor1;
	vgui::TextEntry	*m_pColor2;

	vgui::TextEntry *m_pX;
	vgui::TextEntry *m_pY;
};

CBaseActionTextMessageStartDialog::EffectType CBaseActionTextMessageStartDialog::s_EffectTypes[] =
{
	{ "FADEINOUT" },
	{ "FLICKER" },
	{ "WRITEOUT " }
};

int CBaseActionTextMessageStartDialog::EffectTypeForName( char const *name )
{
	int c = NUM_EFFECT_TYPES;
	int i;
	for ( i = 0; i < c; i++ )
	{
		if ( !Q_strcasecmp( s_EffectTypes[ i ].name, name ) )
			return i;
	}
	Assert( 0 );
	return 0;
}

char const *CBaseActionTextMessageStartDialog::NameForEffectType( int type )
{
	Assert( type >= 0 && type < NUM_EFFECT_TYPES );

	return s_EffectTypes[ type ].name;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionTextMessageStartDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionTextMessageStartDialog.res" );

	BaseClass::Init();

	client_textmessage_t *tm = GetAction()->GetTextMessage();

	m_pX->SetText( va( "%f", tm->x ) );
	m_pY->SetText( va( "%f", tm->y ) );

	m_pFadeInTime->SetText( va( "%.3f", tm->fadein ) );
	m_pFadeOutTime->SetText( va( "%.3f", tm->fadeout ) );
	m_pHoldTime->SetText( va( "%.3f", tm->holdtime ) );
	m_pFXTime->SetText( va( "%.3f", tm->fxtime ) );

	m_pColor1->SetText( va( "%i %i %i %i", tm->r1, tm->g1, tm->b1, tm->a1 ) );
	m_pColor2->SetText( va( "%i %i %i %i", tm->r2, tm->g2, tm->b2, tm->a2 ) );

	m_pMessageText->SetText( GetAction()->GetMessageText() );

	FillInFonts();

	m_pFontName->SetText( GetAction()->GetFontName() );

	int c = NUM_EFFECT_TYPES;
	int i;
	for ( i = 0; i < c ; i++ )
	{
		m_pEffectType->AddItem( NameForEffectType( i ), NULL );
	}

	m_pEffectType->SetText( NameForEffectType( tm->effect ) );
}

void CBaseActionTextMessageStartDialog::FillInFonts()
{
	m_pFontName->AddItem( "TextMessageDefault", NULL );

	KeyValues *schemeFile = new KeyValues( "Fonts" );
	if ( !schemeFile )
		return;

	if ( schemeFile->LoadFromFile( g_pFileSystem, "resource/SourceScheme.res" ) )
	{
		// Iterate fonts
		for (	KeyValues *kv = schemeFile->FindKey("Fonts", true)->GetFirstSubKey(); 
				kv != NULL; 
				kv = kv->GetNextKey() )
		{
			m_pFontName->AddItem( kv->GetName(), NULL );
		}
	}

	schemeFile->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *control - 
//			*curval - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionTextMessageStartDialog::SaveDifferingFloat( vgui::TextEntry *control, float *curval )
{
	bool bret = false;

	Assert( curval && control );

	char sz[ 512 ];
	control->GetText( sz, sizeof( sz ) );

	float fcontrol = (float)atof( sz );
	if ( fcontrol != *curval )
	{
		*curval = fcontrol;
		bret = true;
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *control - 
//			*curval - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionTextMessageStartDialog::SaveDifferingInt( vgui::TextEntry *control, int *curval )
{
	bool bret = false;

	Assert( curval && control );

	char sz[ 512 ];
	control->GetText( sz, sizeof( sz ) );

	int icontrol = atoi( sz );
	if ( icontrol != *curval )
	{
		*curval = icontrol;
		bret = true;
	}

	return bret;
}

bool CBaseActionTextMessageStartDialog::SaveDifferingColor( vgui::TextEntry *control, byte *r, byte *g, byte *b, byte *a )
{
	bool bret = false;

	Assert( r && g && b && a && control );

	char sz[ 512 ];
	control->GetText( sz, sizeof( sz ) );

	int rr, gg, bb, aa;
	if ( sscanf( sz, "%i %i %i %i", &rr, &gg, &bb, &aa ) == 4 )
	{
		if ( *r != rr )
		{
			bret = true;
			*r = rr;
		}
		if ( *g != gg )
		{
			bret = true;
			*g = gg;
		}
		if ( *b != bb )
		{
			bret = true;
			*b = bb;
		}
		if ( *a != aa )
		{
			bret = true;
			*a = aa;
		}
	}

	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseActionTextMessageStartDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	client_textmessage_t *tm = GetAction()->GetTextMessage();
	bret |= SaveDifferingFloat( m_pFadeInTime, &tm->fadein );
	bret |= SaveDifferingFloat( m_pFadeOutTime, &tm->fadeout );
	bret |= SaveDifferingFloat( m_pHoldTime, &tm->holdtime );
	bret |= SaveDifferingFloat( m_pFXTime, &tm->fxtime );

	bret |= SaveDifferingFloat( m_pX, &tm->x );
	bret |= SaveDifferingFloat( m_pY, &tm->y );

	bret |= SaveDifferingColor( m_pColor1, &tm->r1, &tm->g1, &tm->b1, &tm->a1 );
	bret |= SaveDifferingColor( m_pColor2, &tm->r2, &tm->g2, &tm->b2, &tm->a2 );

	char sz[ 1024 ];
	m_pEffectType->GetText( sz, sizeof( sz ) );
	int iEffect = EffectTypeForName( sz );
	if ( iEffect != tm->effect )
	{
		tm->effect = iEffect;
		bret = true;
	}

	m_pMessageText->GetText( sz, sizeof( sz ) );
	if ( Q_strcasecmp( sz, GetAction()->GetMessageText() ) )
	{
		GetAction()->SetMessageText( sz );
		bret = true;
	}

	m_pFontName->GetText( sz, sizeof( sz ) );
	if ( Q_strcasecmp( sz, GetAction()->GetFontName() ) )
	{
		GetAction()->SetFontName( sz );
		bret = true;
	}
	return bret;
}


DECLARE_DEMOACTIONEDIT( DEMO_ACTION_TEXTMESSAGE_START, CBaseActionTextMessageStartDialog );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionPlayCommandsDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionPlayCommandsDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionPlayCommands	*GetAction( void ) { return static_cast< CDemoActionPlayCommands * >( m_pAction ); }

	vgui::TextEntry	*m_pCommands;
};

CBaseActionPlayCommandsDialog::CBaseActionPlayCommandsDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: CBaseActionEditDialog( parent, action, newaction )
{
	m_pCommands = new vgui::TextEntry( this, "Commands" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionPlayCommandsDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionPlayCommandsDialog.res" );

	BaseClass::Init();

	m_pCommands->SetText( GetAction()->GetCommandStream() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionPlayCommandsDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char commands[ 512 ];
	m_pCommands->GetText( commands, sizeof( commands ) );

	if ( Q_strcasecmp( commands, GetAction()->GetCommandStream() ) )
	{
		bret = true;
		GetAction()->SetCommandStream( commands );
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_PLAYCOMMANDS, CBaseActionPlayCommandsDialog );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionCDTrackStartDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionCDTrackStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionCDTrackStart	*GetAction( void ) { return static_cast< CDemoActionCDTrackStart * >( m_pAction ); }

	vgui::TextEntry	*m_pTrackNumber;
};

CBaseActionCDTrackStartDialog::CBaseActionCDTrackStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: CBaseActionEditDialog( parent, action, newaction )
{
	m_pTrackNumber = new vgui::TextEntry( this, "TrackNumber" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionCDTrackStartDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionCDTrackStartDialog.res" );

	BaseClass::Init();

	m_pTrackNumber->SetText( va( "%i", GetAction()->GetTrack() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionCDTrackStartDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char track[ 512 ];
	m_pTrackNumber->GetText( track, sizeof( track ) );
	int itrack = atoi( track );

	if ( itrack != GetAction()->GetTrack() )
	{
		bret = true;
		GetAction()->SetTrack( itrack );
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_PLAYCDTRACK_START, CBaseActionCDTrackStartDialog );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionPlaySoundStartDialog : public CBaseActionEditDialog
{
	DECLARE_CLASS_SIMPLE( CBaseActionPlaySoundStartDialog, CBaseActionEditDialog );

public:
	CBaseActionPlaySoundStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );

	virtual void	OnCommand( char const *command );

private:
	CDemoActionPlaySoundStart	*GetAction( void ) { return static_cast< CDemoActionPlaySoundStart * >( m_pAction ); }

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	vgui::TextEntry	*m_pSoundName;
	vgui::Button	*m_pChooseSound;

	vgui::DHANDLE< vgui::FileOpenDialog > m_hFileOpenDialog;
};

CBaseActionPlaySoundStartDialog::CBaseActionPlaySoundStartDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: CBaseActionEditDialog( parent, action, newaction )
{
	m_pSoundName = new vgui::TextEntry( this, "SoundName" );
	m_pChooseSound = new vgui::Button( this, "ChooseSound", "Choose..." );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionPlaySoundStartDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionPlaySoundStartDialog.res" );

	BaseClass::Init();

	m_pSoundName->SetText( GetAction()->GetSoundName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionPlaySoundStartDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char soundname[ 512 ];
	m_pSoundName->GetText( soundname, sizeof( soundname ) );

	if ( Q_strcasecmp( soundname, GetAction()->GetSoundName() ) )
	{
		bret = true;
		GetAction()->SetSoundName( soundname );
	}

	return bret;
}

void CBaseActionPlaySoundStartDialog::OnFileSelected( char const *fullpath )
{
	if ( !fullpath || !fullpath[ 0 ] )
		return;

	char relativepath[ 512 ];
	g_pFileSystem->FullPathToRelativePath( fullpath, relativepath, sizeof( relativepath ) );

	Q_FixSlashes( relativepath );

	char *soundname = relativepath;
	if ( !Q_strnicmp( relativepath, "sound\\", strlen( "sound\\" ) ) )
	{
		soundname += strlen( "sound\\" );
	}

	m_pSoundName->SetText( soundname );

	if ( m_hFileOpenDialog )
	{
		m_hFileOpenDialog->MarkForDeletion();
	}
}

void CBaseActionPlaySoundStartDialog::OnCommand( char const *command )
{
	if ( !Q_strcasecmp( command, "choosesound" ) )
	{
		if ( !m_hFileOpenDialog.Get() )
		{
			m_hFileOpenDialog = new vgui::FileOpenDialog( this, "Choose .wav file", true );
		}
		if ( m_hFileOpenDialog )
		{
			char startPath[ MAX_PATH ];
			Q_strncpy( startPath, com_gamedir, sizeof( startPath ) );
			Q_FixSlashes( startPath );
			m_hFileOpenDialog->SetStartDirectory( va( "%s/sound", startPath ) );
			m_hFileOpenDialog->DoModal( false );
		}
		return;
	}
	
	BaseClass::OnCommand( command );
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_PLAYSOUND_START, CBaseActionPlaySoundStartDialog );

class CBaseActionWithStopTimeDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;
public:
	CBaseActionWithStopTimeDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );

private:
	CBaseDemoActionWithStopTime	*GetAction( void ) { return static_cast< CBaseDemoActionWithStopTime * >( m_pAction ); }

	vgui::ComboBox	*m_pStopType;
	vgui::TextEntry	*m_pStop;
};

CBaseActionWithStopTimeDialog::CBaseActionWithStopTimeDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: CBaseActionEditDialog( parent, action, newaction )
{
	m_pStop = new vgui::TextEntry( this, "ActionStop" );
	m_pStopType = new vgui::ComboBox( this, "ActionStopType", (int)2, false );
	for ( int i = 1; i < (int)NUM_TIMING_TYPES; i++ )
	{
		m_pStopType->AddItem( CBaseDemoAction::NameForTimingType( (DEMOACTIONTIMINGTYPE)i ), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionWithStopTimeDialog::Init( void )
{
	BaseClass::Init();

	if ( GetAction()->m_bUsingStopTick )
	{
		m_pStopType->SetText( "TimeUseTick" );
		m_pStop->SetText( va( "%i", GetAction()->m_nStopTick ) );
	}
	else
	{
		m_pStopType->SetText( "TimeUseClock" );
		m_pStop->SetText( va( "%.3f", GetAction()->m_flStopTime ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionWithStopTimeDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char stoptype[ 512 ];
	m_pStopType->GetText( stoptype, sizeof( stoptype ) );

	char stop[ 512 ];
	m_pStop->GetText( stop, sizeof( stop ) );

	float fstop = (float)atof( stop );
	int	 istop = (int)atoi( stop );

	if ( !Q_strcasecmp( stoptype, "TimeUseTick" ) )
	{
		if ( GetAction()->m_nStopTick != istop )
		{
			bret = true;
			GetAction()->SetStopTick( istop );
			GetAction()->SetStopTime( -1.0f );
		}
	}
	else
	{
		if ( GetAction()->m_flStopTime != fstop )
		{
			bret = true;
			GetAction()->SetStopTime( fstop );
			GetAction()->SetStopTick( -1 );
		}
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionChangePlaybackRateDialog : public CBaseActionWithStopTimeDialog
{
	typedef CBaseActionWithStopTimeDialog BaseClass;

public:
	CBaseActionChangePlaybackRateDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionChangePlaybackRate	*GetAction( void ) { return static_cast< CDemoActionChangePlaybackRate * >( m_pAction ); }

	vgui::TextEntry	*m_pRate;
};

CBaseActionChangePlaybackRateDialog::CBaseActionChangePlaybackRateDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: BaseClass( parent, action, newaction )
{
	m_pRate = new vgui::TextEntry( this, "PlaybackRate" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionChangePlaybackRateDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionPlaybackRateDialog.res" );

	BaseClass::Init();

	m_pRate->SetText( va( "%f", GetAction()->GetPlaybackRate() ) );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionChangePlaybackRateDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char rate[ 512 ];
	m_pRate->GetText( rate, sizeof( rate ) );

	float frate = (float)atof( rate );

	if ( GetAction()->GetPlaybackRate() != frate )
	{
		bret = true;
		GetAction()->SetPlaybackRate( frate );
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_CHANGEPLAYBACKRATE, CBaseActionChangePlaybackRateDialog );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionPauseDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionPauseDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionPausePlayback	*GetAction( void ) { return static_cast< CDemoActionPausePlayback * >( m_pAction ); }

	vgui::TextEntry	*m_pPauseTime;
};

CBaseActionPauseDialog::CBaseActionPauseDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: BaseClass( parent, action, newaction )
{
	m_pPauseTime = new vgui::TextEntry( this, "PauseTime" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionPauseDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionPauseDialog.res" );

	BaseClass::Init();

	m_pPauseTime->SetText( va( "%f", GetAction()->GetPauseTime() ) );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionPauseDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char pausetime[ 512 ];
	m_pPauseTime->GetText( pausetime, sizeof( pausetime ) );

	float ftime = (float)atof( pausetime );

	if ( GetAction()->GetPauseTime() != ftime )
	{
		bret = true;
		GetAction()->SetPauseTime( ftime );
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_PAUSE, CBaseActionPauseDialog );


// Fonts
/*
		// if a font fails to load then the subsequent fonts will replace
		"Default"
		"DefaultUnderline"
		"DefaultSmall"
		"DefaultVerySmall"
		"DefaultLarge"
		"Marlett"
		"Trebuchet24"
		"Trebuchet20"
		"Trebuchet18"
		"DefaultFixed"
*/


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseActionZoomDialog : public CBaseActionEditDialog
{
	typedef CBaseActionEditDialog BaseClass;

public:
	CBaseActionZoomDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction );

	virtual void	Init( void );

	// Returns true if changes were effected
	virtual bool	OnSaveChanges( void );
private:
	CDemoActionZoom	*GetAction( void ) { return static_cast< CDemoActionZoom * >( m_pAction ); }

	vgui::CheckButton	*m_pSpline;
	vgui::CheckButton	*m_pStayout;

	vgui::TextEntry	*m_pFinalFOV;
	vgui::TextEntry	*m_pOutRate;
	vgui::TextEntry	*m_pInRate;
	vgui::TextEntry	*m_pHoldTime;
};

CBaseActionZoomDialog::CBaseActionZoomDialog( CDemoEditorPanel *parent, CBaseDemoAction *action, bool newaction )
	: BaseClass( parent, action, newaction )
{
	m_pFinalFOV = new vgui::TextEntry( this, "ZoomFOV" );
	m_pOutRate = new vgui::TextEntry( this, "ZoomOut" );
	m_pInRate = new vgui::TextEntry( this, "ZoomIn" );
	m_pHoldTime = new vgui::TextEntry( this, "ZoomHold" );

	m_pSpline = new vgui::CheckButton( this, "ZoomSpline", "Spline" );
	m_pStayout = new vgui::CheckButton( this, "ZoomStayout", "Stay Out" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseActionZoomDialog::Init( void )
{
	LoadControlSettings( "resource\\BaseActionZoomDialog.res" );

	BaseClass::Init();

	m_pFinalFOV->SetText( va( "%f", GetAction()->m_flFinalFOV ) );
	m_pOutRate->SetText( va( "%f", GetAction()->m_flFOVRateOut ) );
	m_pInRate->SetText( va( "%f", GetAction()->m_flFOVRateIn ) );
	m_pHoldTime->SetText( va( "%f", GetAction()->m_flHoldTime ) );

	m_pSpline->SetSelected( GetAction()->m_bSpline );
	m_pStayout->SetSelected( GetAction()->m_bStayout );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if changes were effected
//-----------------------------------------------------------------------------
bool CBaseActionZoomDialog::OnSaveChanges( void )
{
	bool bret = BaseClass::OnSaveChanges();

	char sz[ 512 ];
	m_pFinalFOV->GetText( sz, sizeof( sz ) );
	float f = (float)atof( sz );

	if ( GetAction()->m_flFinalFOV != f )
	{
		bret = true;
		GetAction()->m_flFinalFOV = f;
	}

	m_pOutRate->GetText( sz, sizeof( sz ) );
	f = (float)atof( sz );

	if ( GetAction()->m_flFOVRateOut != f )
	{
		bret = true;
		GetAction()->m_flFOVRateOut = f;
	}

	m_pInRate->GetText( sz, sizeof( sz ) );
	f = (float)atof( sz );

	if ( GetAction()->m_flFOVRateIn != f )
	{
		bret = true;
		GetAction()->m_flFOVRateIn = f;
	}

	m_pHoldTime->GetText( sz, sizeof( sz ) );
	f = (float)atof( sz );

	if ( GetAction()->m_flHoldTime != f )
	{
		bret = true;
		GetAction()->m_flHoldTime = f;
	}

	if ( m_pSpline->IsSelected() != GetAction()->m_bSpline )
	{
		bret = true;
		GetAction()->m_bSpline = m_pSpline->IsSelected();
	}

	if ( m_pStayout->IsSelected() != GetAction()->m_bStayout )
	{
		bret = true;
		GetAction()->m_bStayout = m_pStayout->IsSelected();
	}

	return bret;
}

DECLARE_DEMOACTIONEDIT( DEMO_ACTION_ZOOM, CBaseActionZoomDialog );