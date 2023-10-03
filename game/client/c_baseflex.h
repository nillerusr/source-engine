//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
// Client-side CBasePlayer

#ifndef C_STUDIOFLEX_H
#define C_STUDIOFLEX_H
#pragma once


#include "c_baseanimating.h"
#include "c_baseanimatingoverlay.h"
#include "sceneentity_shared.h"

#include "UtlVector.h"

//-----------------------------------------------------------------------------
// Purpose: Item in list of loaded scene files
//-----------------------------------------------------------------------------
class CFlexSceneFile
{
public:
	enum
	{
		MAX_FLEX_FILENAME = 128,
	};

	char			filename[ MAX_FLEX_FILENAME ];
	void			*buffer;
};

// For phoneme emphasis track
struct Emphasized_Phoneme;
class CSentence;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_BaseFlex : public C_BaseAnimatingOverlay
{
	DECLARE_CLASS( C_BaseFlex, C_BaseAnimatingOverlay );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

					C_BaseFlex();
	virtual			~C_BaseFlex();

	virtual void Spawn();

	virtual IClientModelRenderable*	GetClientModelRenderable();

	virtual void InitPhonemeMappings();

	void		SetupMappings( char const *pchFileRoot );

	virtual CStudioHdr *OnNewModel( void );

	virtual void	StandardBlendingRules( CStudioHdr *hdr, Vector pos[], QuaternionAligned q[], float currentTime, int boneMask );

	virtual void OnThreadedDrawSetup();

	// model specific
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual	void	SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	virtual bool	UsesFlexDelayedWeights();

	virtual bool	GetSoundSpatialization( SpatializationInfo_t& info );

	virtual void	GetToolRecordingState( KeyValues *msg );

	// Called at the lowest level to actually apply a flex animation
	void			AddFlexAnimation( CSceneEventInfo *info );

	void			SetFlexWeight( LocalFlexController_t index, float value );
	float			GetFlexWeight( LocalFlexController_t index );

	// Look up flex controller index by global name
	LocalFlexController_t				FindFlexController( const char *szName );

public:
	Vector			m_viewtarget;
	CInterpolatedVar< Vector >	m_iv_viewtarget;
	// indexed by model local flexcontroller
	float			m_flexWeight[MAXSTUDIOFLEXCTRL];
	CInterpolatedVarArray< float, MAXSTUDIOFLEXCTRL >	m_iv_flexWeight;

	int				m_blinktoggle;

	static int		AddGlobalFlexController( char *szName );
	static char const *GetGlobalFlexControllerName( int idx );

	// bah, this should be unified with all prev/current stuff.

public:

	// Keep track of what scenes are being played
	void				StartChoreoScene( CChoreoScene *scene );
	void				RemoveChoreoScene( CChoreoScene *scene );

	// Start the specifics of an scene event
	virtual bool		StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, C_BaseEntity *pTarget );

	virtual bool		ProcessSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	// Remove all playing events
	void				ClearSceneEvents( CChoreoScene *scene, bool canceled );

	// Stop specifics of event
	virtual	bool		ClearSceneEvent( CSceneEventInfo *info, bool fastKill, bool canceled );

	// Add the event to the queue for this actor
	void				AddSceneEvent( CChoreoScene *scene, CChoreoEvent *event, C_BaseEntity *pTarget = NULL, bool bClientSide = false, C_SceneEntity *pSceneEntity = NULL );

	// Remove the event from the queue for this actor
	void				RemoveSceneEvent( CChoreoScene *scene, CChoreoEvent *event, bool fastKill );

	// Checks to see if the event should be considered "completed"
	bool				CheckSceneEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	// Checks to see if a event should be considered "completed"
	virtual bool		CheckSceneEventCompletion( CSceneEventInfo *info, float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	int					FlexControllerLocalToGlobal( const flexsettinghdr_t *pSettinghdr, int key );
	void				EnsureTranslations( const flexsettinghdr_t *pSettinghdr );

	// For handling scene files
	const void			*FindSceneFile( const char *filename );

	static void			InvalidateFlexCaches();
	bool				IsFlexCacheValid() const;

private:
	Vector	SetViewTarget( CStudioHdr *pStudioHdr, const float *pGlobalFlexWeight );
	void	RunFlexRules( CStudioHdr *pStudioHdr, const float *pGlobalFlexWeight, float *dest );

	// Manipulation of events for the object
	// Should be called by think function to process all scene events
	// The default implementation resets m_flexWeight array and calls
	//  AddSceneEvents
	void ProcessSceneEvents( bool bFlexEvents, float *pGlobalFlexWeight );

	// Assumes m_flexWeight array has been set up, this adds the actual currently playing
	//  expressions to the flex weights and adds other scene events as needed
	bool ProcessSceneEvent( float *pGlobalFlexWeight, bool bFlexEvents, CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	bool RequestStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	bool ProcessFlexAnimationSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessFlexSettingSceneEvent( float *pGlobalFlexWeight, CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	void AddFlexSetting( float *pGlobalFlexWeight, const char *expr, float scale, 
		const flexsettinghdr_t *pSettinghdr, bool newexpression );

	// Array of active SceneEvents, in order oldest to newest
	CUtlVector < CSceneEventInfo >		m_SceneEvents;
	CUtlVector < CChoreoScene * >		m_ActiveChoreoScenes;

	bool				HasSceneEvents() const;

private:
// Mapping for each loaded scene file used by this actor
	struct FS_LocalToGlobal_t
	{
		explicit FS_LocalToGlobal_t() :
			m_Key( 0 ),
			m_nCount( 0 ),
			m_Mapping( 0 )
		{
		}

		explicit FS_LocalToGlobal_t( const flexsettinghdr_t *key ) :
			m_Key( key ),
			m_nCount( 0 ),
			m_Mapping( 0 )
		{
		}		

		void SetCount( int count )
		{
			Assert( !m_Mapping );
			Assert( count > 0 );
			m_nCount = count;
			m_Mapping = new int[ m_nCount ];
			Q_memset( m_Mapping, 0, m_nCount * sizeof( int ) );
		}

		FS_LocalToGlobal_t( const FS_LocalToGlobal_t& src )
		{
			m_Key = src.m_Key;
			delete m_Mapping;
			m_Mapping = new int[ src.m_nCount ];
			Q_memcpy( m_Mapping, src.m_Mapping, src.m_nCount * sizeof( int ) );

			m_nCount = src.m_nCount;
		}

		~FS_LocalToGlobal_t()
		{
			delete m_Mapping;
			m_nCount = 0;
			m_Mapping = 0;
		}

		const flexsettinghdr_t	*m_Key;
		int						m_nCount;
		int						*m_Mapping;	
	};

	static bool FlexSettingLessFunc( const FS_LocalToGlobal_t& lhs, const FS_LocalToGlobal_t& rhs );
	
	CUtlRBTree< FS_LocalToGlobal_t, unsigned short > m_LocalToGlobal;

	float			m_blinktime;
	int				m_prevblinktoggle;

	int						m_iBlink;
	LocalFlexController_t	m_iEyeUpdown;
	LocalFlexController_t	m_iEyeRightleft;
	int				m_iMouthAttachment;

	float			*m_flFlexDelayedWeight;

	int					m_iMostRecentFlexCounter;
	Vector				m_CachedViewTarget;
	CUtlVector< float > m_CachedFlexWeights;
	CUtlVector< float > m_CachedDelayedFlexWeights;

	// shared flex controllers
	static int		g_numflexcontrollers;
	static char		*g_flexcontroller[MAXSTUDIOFLEXCTRL*4]; // room for global set of flexcontrollers
	static float	s_pGlobalFlexWeight[MAXSTUDIOFLEXCTRL*4];

protected:

	enum
	{
		PHONEME_CLASS_WEAK = 0,
		PHONEME_CLASS_NORMAL,
		PHONEME_CLASS_STRONG,

		NUM_PHONEME_CLASSES
	};

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	struct Emphasized_Phoneme
	{
		// Global fields, setup at start
		char			classname[ 64 ];
		bool			required;
		// Global fields setup first time tracks played
		bool			basechecked;
		const flexsettinghdr_t *base;
		const flexsetting_t *exp;

		// Local fields, processed for each sentence
		bool			valid;
		float			amount;
	};

	Emphasized_Phoneme m_PhonemeClasses[ NUM_PHONEME_CLASSES ];

private:

	C_BaseFlex( const C_BaseFlex & ); // not defined, not accessible

	const flexsetting_t *FindNamedSetting( const flexsettinghdr_t *pSettinghdr, const char *expr );

	void			ProcessVisemes( Emphasized_Phoneme *classes, float *pGlobalFlexWeight );
	void			AddVisemesForSentence( float *pGlobalFlexWeight, Emphasized_Phoneme *classes, float emphasis_intensity, CSentence *sentence, float t, float dt, bool juststarted );
	void			AddViseme( float *pGlobalFlexWeight, Emphasized_Phoneme *classes, float emphasis_intensity, int phoneme, float scale, bool newexpression );
	bool			SetupEmphasisBlend( Emphasized_Phoneme *classes, int phoneme );
	void			ComputeBlendedSetting( Emphasized_Phoneme *classes, float emphasis_intensity );

#ifdef HL2_CLIENT_DLL
public:

	Vector			m_vecLean;
	CInterpolatedVar< Vector >	m_iv_vecLean;
	Vector			m_vecShift;
	CInterpolatedVar< Vector >	m_iv_vecShift;
#endif
};


//-----------------------------------------------------------------------------
// Do we have active expressions?
//-----------------------------------------------------------------------------
inline bool C_BaseFlex::HasSceneEvents() const
{
	return m_SceneEvents.Count() != 0;
}


EXTERN_RECV_TABLE(DT_BaseFlex);

float *GetVisemeWeights( int phoneme );

#endif // C_STUDIOFLEX_H




