//========= Copyright Valve Corporation, All rights reserved. ================================== //
//
// Purpose: Defines a texture compositor which uses simple operations and shaders to 
// create complex procedural textures. 
//
//============================================================================================== //

#ifndef CTEXTURECOMPOSITOR_H
#define CTEXTURECOMPOSITOR_H

#include "materialsystem/itexturecompositor.h"
#include "materialsystem/combineoperations.h"

class CTCStage;

// ------------------------------------------------------------------------------------------------
struct RenderTarget_t
{
	int m_nWidth;
	int m_nHeight;
	ITexture* m_pRT;
};

// ------------------------------------------------------------------------------------------------
class CTextureCompositor : public ITextureCompositor
{
public:
	CTextureCompositor( int _width, int _height, int nTeam, const char* pCompositeName, uint64 nRandomSeed, uint32 nTexCompositeCreateFlags );

	virtual int AddRef() OVERRIDE;
	virtual int Release() OVERRIDE;
	virtual int GetRefCount() const OVERRIDE { return m_nReferenceCount; }

	virtual void Update() OVERRIDE;
	virtual ITexture* GetResultTexture() const OVERRIDE;
	virtual ECompositeResolveStatus GetResolveStatus() const OVERRIDE;
	virtual void ScheduleResolve() OVERRIDE;

	void Resolve();

	void Error( bool _retry, PRINTF_FORMAT_STRING const char* _debugDevMsg, ... );

	void SetRootStage( CTCStage* _rootStage ); 

	ITexture* AllocateCompositorRenderTarget( );
	void ReleaseCompositorRenderTarget( ITexture* _tex );

	int GetTeamNumber() const { return m_nTeam; }
	const CUtlString& GetName() const { return m_CompositeName; }
	uint32 GetCreateFlags() const { return m_nTexCompositeCreateFlags; }
	void GetTextureName( char* pOutBuffer, int nBufferLen ) const;
	void GetSeed( uint32* pOutHi, uint32* pOutLo ) const;

	void SetTemplate( const char* pTemplate ) { m_TemplateName = pTemplate; }
	bool UsesTemplate() const { return m_TemplateName.IsEmpty() == false; }
	const CUtlString& GetTemplateName() const { return m_TemplateName; }

protected:
	virtual ~CTextureCompositor();
	void Restart();

private:
	void Shutdown();

	CInterlockedInt m_nReferenceCount;

	int m_nWidth;
	int m_nHeight;
	int m_nTeam;
	uint64 m_nRandomSeed;
	CTCStage* m_pRootStage;
	ECompositeResolveStatus m_ResolveStatus;

	// Did an error occur
	bool m_bError;
	// And is it fatal, or should we try again?
	bool m_bFatal;

	CUtlVector<RenderTarget_t> m_RenderTargetPool;
	int m_nRenderTargetsAllocated;
	int m_nCompositePaintKitId;

	CUtlString m_CompositeName;
	CUtlString m_TemplateName;
	uint32 m_nTexCompositeCreateFlags;
	bool m_bHasTeamSpecifics;
};

// ------------------------------------------------------------------------------------------------
class CTextureCompositorTemplate
{
public:
	static CTextureCompositorTemplate* Create( const char* pName, KeyValues* pTmplDesc );
	~CTextureCompositorTemplate();

	/* const */ KeyValues* GetKV() /* const */ { return m_pKV; }
	const CUtlString& GetName() const { return m_Name; }

	bool ResolveDependencies() const;
	bool HasDependencyCycles(); // Not const because we update m_bCheckedForCycles

	void SetImplementsName( const CUtlString& implementsName ) { m_ImplementsName = implementsName; }
	bool ImplementsTemplate() const { return m_ImplementsName.IsEmpty() == false; }
	const CUtlString& GetImplementsName() const { return m_ImplementsName; }

	void SetCheckedForCycles( bool checked ) { m_bCheckedForCycles = checked; }
	bool HasCheckedForCycles() const { return m_bCheckedForCycles; }

private:
	CTextureCompositorTemplate( const char* pName, KeyValues* pKV ) 
	: m_pKV( pKV ) 
	, m_Name( pName )
	, m_bCheckedForCycles( false )
	{ }

	KeyValues* m_pKV;

	// Our own name
	CUtlString m_Name;

	// If we are an implementation of another template, what is that template's name?
	CUtlString m_ImplementsName;

	// Have we checked this template and it's hierarchy for cycles? If so we can early out on future checks.
	bool m_bCheckedForCycles;
};

// ------------------------------------------------------------------------------------------------
const char *GetCombinedMaterialName( ECombineOperation eMaterial );
CTextureCompositor* CreateTextureCompositor( int _w, int _h, const char* pCompositeName, int nTeamNum, uint64 _randomSeed, KeyValues* _stageDesc, uint32 texCompositeCreateFlags );

#endif /* CTEXTURECOMPOSITOR_H */
