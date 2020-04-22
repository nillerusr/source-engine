//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FACEPOSER_MODELS_H
#define FACEPOSER_MODELS_H
#ifdef _WIN32
#pragma once
#endif

class StudioModel;

#include "mxbitmaptools.h"

typedef unsigned int CRC32_t;

class IFaceposerModels
{
public:
						IFaceposerModels();
	virtual				~IFaceposerModels();

	virtual int			Count( void ) const;
	virtual char const	*GetModelName( int index );
	virtual char const	*GetModelFileName( int index );
	virtual int			GetActiveModelIndex( void ) const;
	virtual char const	*GetActiveModelName( void );
	virtual StudioModel *GetActiveStudioModel( void );
	virtual void		ForceActiveModelIndex( int index );
	virtual void		UnForceActiveModelIndex();
	virtual int			FindModelByFilename( char const *filename );

	virtual int			LoadModel( char const *filename );
	virtual void		FreeModel( int index );

	virtual void		CloseAllModels( void );

	virtual StudioModel *GetStudioModel( int index );
	virtual CStudioHdr	*GetStudioHeader( int index );
	virtual int			GetIndexForStudioModel( StudioModel *model );

	virtual int			GetModelIndexForActor( char const *actorname );
	virtual StudioModel *GetModelForActor( char const *actorname );

	virtual char const *GetActorNameForModel( int modelindex );
	virtual void		SetActorNameForModel( int modelindex, char const *actorname );

	virtual int			CountVisibleModels( void );
	virtual void		ShowModelIn3DView( int modelindex, bool show );
	virtual bool		IsModelShownIn3DView( int modelindex );

	virtual void		SaveModelList( void );
	virtual void		LoadModelList( void );

	virtual void		ReleaseModels( void );
	virtual void		RestoreModels( void );
	
	//virtual void		RefreshModels( void );

	virtual void		CheckResetFlexes( void );
	virtual void		ClearOverlaysSequences( void );

	virtual mxbitmapdata_t *GetBitmapForSequence( int modelindex, int sequence );

	virtual void		RecreateAllAnimationBitmaps( int modelindex );
	virtual void		RecreateAnimationBitmap( int modelindex, int sequence );

	virtual void		CreateNewBitmap( int modelindex, char const *pchBitmapFilename, int sequence, int nSnapShotSize, bool bZoomInOnFace, class CExpression *pExpression, mxbitmapdata_t *bitmap );

	virtual int			CountActiveSources();

	virtual void		SetSolveHeadTurn( int solve );

	virtual void		ClearModelTargets( bool force = false );

private:
	class CFacePoserModel
	{
	public:
		CFacePoserModel( char const *modelfile, StudioModel *model );
		~CFacePoserModel();

		void LoadBitmaps();
		void FreeBitmaps();
		mxbitmapdata_t *GetBitmapForSequence( int sequence );

		const char *GetBitmapChecksum( int sequence );
		CRC32_t GetBitmapCRC( int sequence );
		const char *GetBitmapFilename( int sequence );
		void		RecreateAllAnimationBitmaps();
		void		RecreateAnimationBitmap( int sequence, bool reconcile );


		void SetActorName( char const *actorname )
		{
			strcpy( m_szActorName, actorname );
		}

		char const *GetActorName( void ) const
		{
			return m_szActorName;
		}

		StudioModel *GetModel( void ) const
		{
			return m_pModel;
		}

		char const *GetModelFileName( void ) const
		{
			return m_szModelFileName;
		}

		char const	*GetShortModelName( void ) const
		{
			return m_szShortName;
		}

		void SetVisibleIn3DView( bool visible )
		{
			m_bVisibileIn3DView = visible;
		}

		bool GetVisibleIn3DView( void ) const
		{
			return m_bVisibileIn3DView;
		}

		// For material system purposes
		void Release( void );
		void Restore( void );

		void Refresh( void )
		{
			// Forces a reload from disk
			Release();
			Restore();
		}

		void		CreateNewBitmap( char const *pchBitmapFilename, int sequence, int nSnapShotSize, bool bZoomInOnFace, class CExpression *pExpression, mxbitmapdata_t *bitmap );

	private:

		void		LoadBitmapForSequence( mxbitmapdata_t *bitmap, int sequence );

		void		ReconcileAnimationBitmaps();
		void		BuildValidChecksums( CUtlRBTree< CRC32_t > &tree );

		enum
		{
			MAX_ACTOR_NAME = 64,
			MAX_MODEL_FILE = 128,
			MAX_SHORT_NAME = 32,
		};

		char			m_szActorName[ MAX_ACTOR_NAME ];
		char			m_szModelFileName[ MAX_MODEL_FILE ];
		char			m_szShortName[ MAX_SHORT_NAME ];
		StudioModel		*m_pModel;
		bool			m_bVisibileIn3DView;

		struct AnimBitmap
		{
			AnimBitmap()
			{
				needsload = false;
				bitmap = 0;
			}
			bool			needsload;
			mxbitmapdata_t *bitmap;
		};

		CUtlVector< AnimBitmap * >	m_AnimationBitmaps;
		bool			m_bFirstBitmapLoad;
	};

	CFacePoserModel *GetEntry( int index );

	CUtlVector< CFacePoserModel * > m_Models;

	int					m_nLastRenderFrame;
	int					m_nForceModelIndex;
};

extern IFaceposerModels *models;

void EnableStickySnapshotMode( void );
void DisableStickySnapshotMode( void );

#endif // FACEPOSER_MODELS_H
