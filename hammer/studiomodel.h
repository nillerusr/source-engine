//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef STUDIOMODEL_H
#define STUDIOMODEL_H

#ifdef _WIN32
#pragma once
#endif

#ifndef byte
typedef unsigned char byte;
#endif // byte

#include "hammer_mathlib.h"
#include "studio.h"
#include "utlvector.h"
#include "datacache/imdlcache.h"
#include "FileChangeWatcher.h"


class StudioModel;
class CMaterial;
class CRender3D;
class CRender2D;


struct ModelCache_t
{
	StudioModel *pModel;
	char *pszPath;
	int nRefCount;
};


//-----------------------------------------------------------------------------
// Purpose: Defines an interface to a cache of studio models.
//-----------------------------------------------------------------------------
class CStudioModelCache
{
	public:

		static StudioModel *FindModel(const char *pszModelPath);
		static StudioModel *CreateModel(const char *pszModelPath);
		static void AddRef(StudioModel *pModel);
		static void Release(StudioModel *pModel);
		static void AdvanceAnimation(float flInterval);

	protected:

		static BOOL AddModel(StudioModel *pModel, const char *pszModelPath);
		static void RemoveModel(StudioModel *pModel);

		static ModelCache_t m_Cache[1024];
		static int m_nItems;
};


// Calling these will monitor the filesystem for changes to model files and automatically 
// incorporate changes to the models.
void InitStudioFileChangeWatcher();
void UpdateStudioFileChangeWatcher();


class StudioModel
{
public:

	static bool				Initialize( void );
	static void				Shutdown( void ); // garymcthack - need to call this.

	StudioModel(void);
	~StudioModel(void);

	void					FreeModel ();
	bool					LoadModel( const char *modelname );
	bool					PostLoadModel ( const char *modelname );
	void					DrawModel3D( CRender3D *pRender, float flAlpha, bool bWireframe);
	void					DrawModel2D( CRender2D *pRender, float flAlpha, bool bWireFrame);
	void					AdvanceFrame( float dt );

	void					ExtractBbox( Vector &mins, Vector &maxs );
	void					ExtractClippingBbox( Vector &mins, Vector &maxs );
	void					ExtractMovementBbox( Vector &mins, Vector &maxs );
	void					RotateBbox( Vector &Mins, Vector &Maxs, const QAngle &Angles );

	int						SetSequence( int iSequence );
	int						GetSequence( void );
	int						GetSequenceCount( void );
	void					GetSequenceName( int nIndex, char *szName );
	void					GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed );

	int						SetBodygroup( int iGroup, int iValue );
	int						SetSkin( int iValue );
	void					SetOrigin( float x, float y, float z );
	void					GetOrigin( float &x, float &y, float &z );
	void					SetOrigin( const Vector &v );
	void					GetOrigin( Vector &v );
	void					SetAngles( QAngle& pfAngles );
	bool					IsTranslucent();

private:
	CStudioHdr				*m_pStudioHdr;
	CStudioHdr				*GetStudioHdr() const;
	studiohdr_t*			GetStudioRenderHdr() const;
	studiohwdata_t*			GetHardwareData();

	// entity settings
	Vector					m_origin;
	QAngle					m_angles;	
	int						m_sequence;			// sequence index
	float					m_cycle;			// pos in animation cycle
	int						m_bodynum;			// bodypart selection	
	int						m_skinnum;			// skin group selection
	byte					m_controller[MAXSTUDIOBONECTRLS];	// bone controllers
	float					m_poseParameter[MAXSTUDIOPOSEPARAM];		// animation blending
	byte					m_mouth;			// mouth position
	char*					m_pModelName;		// model file name

	Vector					*m_pPosePos;
	Quaternion				*m_pPoseAng;

	// internal data
	MDLHandle_t				m_MDLHandle;
	mstudiomodel_t			*m_pModel;	

	void					SetUpBones ( bool bUpdatePose, matrix3x4_t*	pBoneToWorld );
	void					SetupModel ( int bodypart );

	void					LoadStudioRender( void );

	bool					LoadVVDFile( const char *modelname );
	bool					LoadVTXFile( const char *modelname );
};


extern Vector g_vright;		// needs to be set to viewer's right in order for chrome to work
extern float g_lambert;		// modifier for pseudo-hemispherical lighting


#endif // STUDIOMODEL_H
