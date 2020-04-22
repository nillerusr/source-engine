//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//-----------------------------------------------------------------------------
// Purpose: Skips ahead in demo to specified frame/time
//-----------------------------------------------------------------------------
class CDemoActionSkipAhead : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual bool Update( const DemoActionTimingContext& tc );
	virtual void FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void SetSkipToTick( int tick );
	void SetSkipToTime( float t );
	
private:

	bool		m_bUsingSkipTick;
	int			m_nSkipToTick;
	float		m_flSkipToTime;

	friend class CBaseActionSkipAheadDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Simply stops playback of demo
//-----------------------------------------------------------------------------
class CDemoActionStopPlayback : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Update( const DemoActionTimingContext& tc );
	virtual void			FireAction( void );

private:

	friend class CBaseActionStopPlaybackDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Skips ahead in demo to specified frame/time
//-----------------------------------------------------------------------------
class CDemoActionPlayCommands : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual void FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetCommandStream( char const *stream );
	char const	*GetCommandStream( void ) const;

private:

	enum
	{
		MAX_COMMAND_STREAM = 256,
	};

	char		m_szCommandStream[ MAX_COMMAND_STREAM ];
};

//-----------------------------------------------------------------------------
// Purpose: Screen fade actions
//-----------------------------------------------------------------------------
class CDemoActionScreenFadeStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual void FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	ScreenFade_t *GetScreenFade( void );

private:
	ScreenFade_t		fade;

	friend class CBaseActionScreenFadeStartDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Text message actions
//-----------------------------------------------------------------------------
class CDemoActionTextMessageStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual void			FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	client_textmessage_t *GetTextMessage( void );

	void						SetMessageText( char const *text );
	char const					*GetMessageText( void ) const;

	void						SetFontName( char const *font );
	char const					*GetFontName( void ) const;

private:
	enum
	{
		MAX_MESSAGE_TEXT = 512,
		MAX_FONT_NAME = 64,
	};

	char						m_szMessageText[ MAX_MESSAGE_TEXT ];
	char						m_szVguiFont[ MAX_FONT_NAME ];
	client_textmessage_t		message;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CDemoActionCDTrackStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual void			FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetTrack( int track );
	int			GetTrack( void ) const;

private:
	int			m_nCDTrack;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CDemoActionCDTrackStop : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual void			FireAction( void );

private:
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionPlaySoundStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual void			FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetSoundName( char const *name );
	char const	*GetSoundName( void ) const;

private:

	enum
	{
		MAX_SOUND_NAME = 128,
	};

	char		m_szSoundName[ MAX_SOUND_NAME ];
};

//-----------------------------------------------------------------------------
// Purpose: Base for actions that go on until a specified stop frame/time
//-----------------------------------------------------------------------------
class CBaseDemoActionWithStopTime : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	virtual bool Init( KeyValues *pInitData );
	virtual bool Update( const DemoActionTimingContext& tc );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetStopTick( int tick );
	void		SetStopTime( float time );
	
private:

	bool		m_bUsingStopTick;
	int			m_nStopTick;
	float		m_flStopTime;

	friend class CBaseActionWithStopTimeDialog;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionChangePlaybackRate : public CBaseDemoActionWithStopTime
{
	typedef CBaseDemoActionWithStopTime BaseClass;

public:
	CDemoActionChangePlaybackRate();

	virtual bool Init( KeyValues *pInitData );
	virtual void FireAction( void );
	virtual void	OnActionFinished( void );

	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetPlaybackRate( float rate );
	float		GetPlaybackRate( void ) const;

private:

	float		m_flPlaybackRate;
	float		m_flSavePlaybackRate;

	friend class CBaseActionChangePlaybackRateDialog;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionPausePlayback : public CBaseDemoActionWithStopTime
{
	typedef CBaseDemoActionWithStopTime BaseClass;

public:
	CDemoActionPausePlayback();

	virtual bool Init( KeyValues *pInitData );
	virtual void FireAction( void );

	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );

	void		SetPauseTime( float t );
	float		GetPauseTime( void ) const;

private:

	float		m_flPauseTime;

	friend class CBaseActionPauseDialog;
};

class CDemoActionZoom : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;
public:
	CDemoActionZoom();

	virtual bool Init( KeyValues *pInitData );
	virtual bool Update( const DemoActionTimingContext& tc );
	virtual void FireAction( void );
	virtual void SaveKeysToBuffer( int depth, CUtlBuffer& buf );
private:

	bool			m_bSpline;
	bool			m_bStayout;

	float			m_flFinalFOV;
	float			m_flFOVRateOut;  // degress per second
	float			m_flFOVRateIn;	 // degrees per second
	float			m_flHoldTime;

// for playback
	float			m_flFOVStartTime;
	float			m_flOriginalFOV;

	friend class CBaseActionZoomDialog;
};