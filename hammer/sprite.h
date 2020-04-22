//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef SPRITE_H
#define SPRITE_H
#pragma once

#include "mathlib/mathlib.h"
#include "materialsystem/imaterialsystem.h"

class CTexture;
class CMaterial;
class IMaterialVar;
class CRender;
class CRender3D;


// must match definition in modelgen.h
enum synctype_t
{
	ST_SYNC=0, 
	ST_RAND 
};

#define SPR_VP_PARALLEL_UPRIGHT		0
#define SPR_FACING_UPRIGHT			1
#define SPR_VP_PARALLEL				2
#define SPR_ORIENTED				3
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL					0
#define SPR_ADDITIVE				1
#define SPR_INDEXALPHA				2
#define SPR_ALPHATEST				3


//-----------------------------------------------------------------------------
// From engine\GL_MODEL.H:
//-----------------------------------------------------------------------------

class CSpriteModel
{
	public:

		CSpriteModel(void);
		~CSpriteModel(void);
			
		bool LoadSprite(const char *pszSpritePath);

		int GetFrameCount(void);
		int GetWidth() const;
		int GetHeight() const;
		int GetType() const;

		void Bind( CRender* pRender, int frame );
		void GetRect( float& left, float& up,  float& right, float& down ) const;
		void SetRenderMode( const int mode );

		void SetMaterialPrimitiveType( const MaterialPrimitiveType_t type );
		void SetOrigin( const Vector &v );
		void GetOrigin( Vector &v );
		void SetAngles( const QAngle& pfAngles );
		void SetScale( const float fScale );
		void SetInvert( const bool b );
		inline void SetTextureExtent( Vector2D TexUL, Vector2D TexLR ) { m_TexUL = TexUL; m_TexLR = TexLR; }
		inline void SetExtent( Vector2D UL, Vector2D LR ) { m_UL = UL; m_LR = LR; }
		void DrawSprite3D( CRender3D *pRender, unsigned char color[3] );

	protected:
		void GetSpriteAxes(QAngle& Angles, int type, Vector& forward, Vector& right, Vector& up, Vector& ViewUp, Vector& ViewRight, Vector& ViewForward);

		Vector			m_Origin;
		QAngle			m_Angles;
		float			m_fScale;
		MaterialPrimitiveType_t	m_MaterialPrimitiveType;

		CMaterial*		m_pMaterial;
		IMaterialVar*	m_pFrameVar;
		IMaterialVar*	m_pRenderModeVar;
		int				m_NumFrames;
		int				m_Type;
		int				m_Width;
		int				m_Height;
		bool			m_bInvert;

		Vector2D		m_TexUL, m_TexLR;
		Vector2D		m_UL, m_LR;
};


//-----------------------------------------------------------------------------
// inline methods
//-----------------------------------------------------------------------------

inline int CSpriteModel::GetWidth() const
{
	return m_Width;
}

inline int CSpriteModel::GetHeight() const
{
	return m_Height;
}

inline int CSpriteModel::GetType() const
{
	return m_Type;
}

inline void CSpriteModel::GetRect( float& left, float& up, float& right, float& down ) const
{
	left = m_UL.x;
	right = m_LR.x;
	up = m_UL.y;
	down = m_LR.y;
}

//-----------------------------------------------------------------------------
// Sprite cache
//-----------------------------------------------------------------------------

struct SpriteCache_t
{
	CSpriteModel *pSprite;
	char *pszPath;
	int nRefCount;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#define SPRITE_CACHE_SIZE	1024

class CSpriteCache
{
	public:

		static CSpriteModel *CreateSprite(const char *pszSpritePath);
		static void AddRef(CSpriteModel *pSprite);
		static void Release(CSpriteModel *pSprite);

	protected:

		static bool AddSprite(CSpriteModel *pSprite, const char *pszSpritePath);
		static void RemoveSprite(CSpriteModel *pSprite);

		static SpriteCache_t m_Cache[SPRITE_CACHE_SIZE];
		static int m_nItems;
};


#endif // SPRITE_H

