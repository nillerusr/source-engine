//============ Copyright (c) Valve Corporation, All rights reserved. ==========++;
//
//=============================================================================


// Valve includes
#include "appframework/appframework.h"
#include "appframework/tier3app.h"
#include "filesystem.h"
#include "icommandline.h"
#include "mathlib/mathlib.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "studio.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vstdlib/iprocessutils.h"
#include "tier2/fileutils.h"


// Last include
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CFmtStr PrettyPrintFloat( const float &flVal, float flEps = 0.000005f )
{
	if ( FloatMakePositive( flVal ) <= flEps )
	{
		return CFmtStr( "%.6f", 0.0f );
	}

	return CFmtStr( "%.6f", flVal );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CFmtStr PrettyPrintVector( const Vector &v )
{
	return CFmtStr( "%s %s %s",
		PrettyPrintFloat( v.x ).Get(),
		PrettyPrintFloat( v.y ).Get(),
		PrettyPrintFloat( v.z ).Get() );
}


//-----------------------------------------------------------------------------
// A src bone transform transforms pre-compiled data (.dmx or .smd files, for example)
// into post-compiled data (.mdl or .ani files)
//-----------------------------------------------------------------------------
static const mstudiosrcbonetransform_t *GetSrcBoneTransform( const studiohdr_t *pStudioHdr, int nBoneIndex )
{
	if ( !pStudioHdr || nBoneIndex < 0 || nBoneIndex >= pStudioHdr->numbones )
		return false;

	const char *pszBoneName = pStudioHdr->pBone( nBoneIndex )->pszName();
	const int nSrcBoneTransformCount = pStudioHdr->NumSrcBoneTransforms();
	for ( int i = 0; i < nSrcBoneTransformCount; ++i )
	{
		const mstudiosrcbonetransform_t *pSrcBoneTransform = pStudioHdr->SrcBoneTransform( i );
		if ( pSrcBoneTransform && !Q_stricmp( pSrcBoneTransform->pszName(), pszBoneName ) )
			return pSrcBoneTransform;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
struct CommandLineBinaryOpt_t
{
	const char *m_pszShortName;
	const char *m_pszLongName;
	const char *m_pszDoc;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CMdlInfoApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	CMdlInfoApp();

	// Methods of IApplication
	virtual bool Create() OVERRIDE;
	virtual bool PreInit() OVERRIDE;
	virtual int Startup() OVERRIDE;
	virtual int Main() OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void PostShutdown() OVERRIDE;
	virtual void Destroy() OVERRIDE {};

	static const CommandLineBinaryOpt_t s_binaryOptions[];

	bool m_bOptAll;
	bool m_bOptVerbose;

	static const char kOptHelp[];
	static const char kOptVerbose[];

	static const char kOptInput[];

	static const char kOptGeneral[];
	static const char kOptBones[];
	static const char kOptAttachments[];
	static const char kOptAnimation[];
	static const char kOptSequence[];
	static const char kOptMesh[];
	static const char kOptTexture[];
	static const char kOptSkin[];

	static bool CommandLineHasOpt( const char *pszOpt );

	bool DoIt( const char *pszFileIn );
	void HandleOptions();
	static MDLHandle_t GetMDLHandle( const char *pszFilename );
	void PrintHelp();

	void DumpGeneral( MDLHandle_t hMdl, CStudioHdr &cStudioHdr ) const;
	void DumpBones( CStudioHdr &cStudioHdr ) const;
	void DumpAttachments( CStudioHdr &cStudioHdr ) const;
	void DumpAnimationList( CStudioHdr &cStudioHdr ) const;
	void DumpSequenceList( CStudioHdr &cStudioHdr ) const;
	void DumpMesh( CStudioHdr &cStudioHdr ) const;
	void DumpTexture( CStudioHdr &cStudioHdr ) const;
	void DumpSkin( CStudioHdr &cStudioHdr ) const;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char CMdlInfoApp::kOptHelp[] = "-help";
const char CMdlInfoApp::kOptVerbose[] = "-verbose";

const char CMdlInfoApp::kOptInput[] = "-input";

const char CMdlInfoApp::kOptGeneral[] = "-general";
const char CMdlInfoApp::kOptBones[] = "-bones";
const char CMdlInfoApp::kOptAttachments[] = "-attachments";
const char CMdlInfoApp::kOptAnimation[] = "-animation";
const char CMdlInfoApp::kOptSequence[] = "-sequence";
const char CMdlInfoApp::kOptMesh[] = "-mesh";
const char CMdlInfoApp::kOptTexture[] = "-texture";
const char CMdlInfoApp::kOptSkin[] = "-skin";

const CommandLineBinaryOpt_t CMdlInfoApp::s_binaryOptions[] =
{
	{ "-h", kOptHelp, "Print usage information" },
	{ "-v", kOptVerbose, "Print extra junk" },

	{ "-i", kOptInput, "Specifies the input MDL file" },

	{ "-g", kOptGeneral, "Print general information section" },
	{ "-b", kOptBones, "Print bones/skeleton section" },
	{ "-at", kOptAttachments, "Print attachments section" },
	{ "-a", kOptAnimation, "Print animation section" },
	{ "-s", kOptSequence, "Print sequence section" },
	{ "-m", kOptMesh, "Print mesh section" },
	{ "-t", kOptTexture, "Print texture section" },
	{ "-k", kOptSkin, "Print sKin section" }
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CMdlInfoApp );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CMdlInfoApp::CMdlInfoApp()
: m_bOptAll( true )
, m_bOptVerbose( false )
{
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CMdlInfoApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },

		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) )
		return false;

	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
	{
		Warning( "Create: Unable to connect to material system interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMdlInfoApp::PreInit()
{
	MathLib_Init();

	if ( !BaseClass::PreInit() )
		return false;

	CreateInterfaceFn factory = GetFactory();

	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pMDLCache )
	{
		Warning( "Error! mdlinfo is missing a required interface!\n" );
		return false;
	}

	SetupSearchPaths( NULL, false, true );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMdlInfoApp::Startup()
{
	if ( BaseClass::Startup() < 0 )
		return -1;

	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->ModInit();
	}

	if ( g_pDataCache )
	{
		g_pDataCache->SetSize( 64 * 1024 * 1024 );
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMdlInfoApp::Main()
{
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLineHasOpt( kOptHelp ) )
	{
		PrintHelp();
		return 0;
	}

	const char *pszOptFileIn = CommandLine()->ParmValue( "-i" );
	if ( !pszOptFileIn )
		pszOptFileIn = CommandLine()->ParmValue( "-input" );

	if ( pszOptFileIn )
		return DoIt( pszOptFileIn );

	Warning( "Error! Missing -i <file>.mdl\n\n" );

	PrintHelp();

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::Shutdown()
{
	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->ModShutdown();
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::PostShutdown()
{
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMdlInfoApp::CommandLineHasOpt( const char *pszOpt )
{
	for ( int ii = 0; ii < ARRAYSIZE( s_binaryOptions ); ++ii )
	{
		if ( !V_stricmp( pszOpt, s_binaryOptions[ii].m_pszShortName ) || !V_stricmp( pszOpt, s_binaryOptions[ii].m_pszLongName ) )
		{
			if ( CommandLine()->FindParm( s_binaryOptions[ii].m_pszShortName ) > 0 || CommandLine()->FindParm( s_binaryOptions[ii].m_pszLongName ) > 0 )
				return true;

			return false;
		}
	}

	if ( CommandLine()->FindParm( pszOpt ) > 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMdlInfoApp::DoIt( const char *pszFileIn )
{
	HandleOptions();

	MDLHandle_t hMdl = GetMDLHandle( pszFileIn );
	if ( hMdl == MDLHANDLE_INVALID )
		return false;

	// Get the model
	studiohdr_t *pHdr = g_pMDLCache->GetStudioHdr( hMdl );
	if ( !pHdr )
		return false;

	CStudioHdr cStudioHdr( pHdr, g_pMDLCache );

	DumpGeneral( hMdl, cStudioHdr );
	DumpBones( cStudioHdr );
	DumpAttachments( cStudioHdr );
	DumpAnimationList( cStudioHdr );
	DumpSequenceList( cStudioHdr );
	DumpMesh( cStudioHdr );
	DumpTexture( cStudioHdr );
	DumpSkin( cStudioHdr );

	g_pMDLCache->Release( hMdl );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::HandleOptions()
{
	if (
		CommandLineHasOpt( kOptGeneral ) ||
		CommandLineHasOpt( kOptBones ) ||
		CommandLineHasOpt( kOptAttachments ) ||
		CommandLineHasOpt( kOptAnimation ) ||
		CommandLineHasOpt( kOptSequence ) ||
		CommandLineHasOpt( kOptMesh ) ||
		CommandLineHasOpt( kOptTexture ) ||
		CommandLineHasOpt( kOptSkin ) )
	{
		m_bOptAll = false;
	}

	if ( CommandLineHasOpt( kOptVerbose ) )
	{
		m_bOptVerbose = true;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
MDLHandle_t CMdlInfoApp::GetMDLHandle( const char *pszFilename )
{
	if ( pszFilename == NULL )
		return MDLHANDLE_INVALID;

	char szFullPath[ MAX_PATH ];
	if ( !GenerateFullPath( pszFilename, NULL, szFullPath, ARRAYSIZE( szFullPath ) ) )
	{
		V_strncpy( szFullPath, pszFilename, ARRAYSIZE( szFullPath ) );
	}

	MDLHandle_t hMdl = g_pMDLCache->FindMDL( szFullPath );
	if ( hMdl == MDLHANDLE_INVALID )
	{
		Error( "Couldn't Load MDL %s via g_pMDLCache\n", pszFilename );
	}
	else
	{
		studiohdr_t *pHdr = g_pMDLCache->GetStudioHdr( hMdl );

		if ( !pHdr || !V_strcmp( "error.mdl", pHdr->pszName() ) )
		{
			Error( "Couldn't Load MDL %s via g_pMDLCache, got error model instead\n", pszFilename );
			g_pMDLCache->Release( hMdl );
			hMdl = MDLHANDLE_INVALID;
		}
	}

	return hMdl;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::PrintHelp()
{
	Msg( "\n" );
	Msg( "NAME\n" );
	Msg( "    mdlinfo - Prints information about a VALVe MDL file\n" );
	Msg( "\n" );
	Msg( "SYNOPSIS\n" );
	Msg( "    mdlinfo [ options ] < -i filename.mdl >\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < ARRAYSIZE( s_binaryOptions ); ++ii )
	{
		const CommandLineBinaryOpt_t &opt = s_binaryOptions[ii];
		Msg( "    %s | %s : %s\n", opt.m_pszShortName, opt.m_pszLongName, opt.m_pszDoc );
	}

	Msg( "\n" );
	Msg( "DESCRIPTION\n" );
	Msg( "    Prints information about a VALVe MDL file\n" );
	Msg( "    If no sections are specified, all are printed\n" );
	Msg( "    Multiple section specifiers can be specified but print order is fixed\n" );
	Msg( "\n" );
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpGeneral( MDLHandle_t hMdl, CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptGeneral ) ) )
		return;

	const studiohdr_t *pHdr = cStudioHdr.GetRenderHdr();

	Msg( "//\n" );
	Msg( "// MDL General Info\n" );
	Msg( "//\n" );
	Msg( "\n" );

	Msg( "// MDL:           %s\n", cStudioHdr.pszName() );
	Msg( "// Filename:      %s\n", g_pMDLCache->GetModelName( hMdl ) );
	Msg( "// Id:            0x%08x\n", pHdr->id );
	Msg( "// Version:       0x%08x\n", pHdr->version );
	Msg( "// Checksum:      0x%08x\n", pHdr->checksum );
	Msg( "// EyePosition:   %s\n", PrettyPrintVector( pHdr->eyeposition ).Get() );
	Msg( "// IllumPosition: %s\n", PrettyPrintVector( pHdr->illumposition ).Get() );
	Msg( "// Hull Min:      %s\n", PrettyPrintVector( pHdr->hull_min ).Get() );
	Msg( "// Hull Max:      %s\n", PrettyPrintVector( pHdr->hull_max ).Get() );
	Msg( "// Bone Count:    %d\n", cStudioHdr.numbones() );

	const char *pszSurfaceProp = cStudioHdr.pszSurfaceProp();
	if ( pszSurfaceProp )
	{
		Msg( "// SurfaceProp:   %s\n", pszSurfaceProp );
	}

	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void PrintBoneFlags( const mstudiobone_t *pBone )
{
	CFmtStrMax sBoneFlags;

	struct BoneFlag_t
	{
		int m_nMask;
		const char *m_pzsName;
	};

	static const BoneFlag_t boneFlags[] =
	{
		{ BONE_PHYSICALLY_SIMULATED, "BONE_PHYSICALLY_SIMULATED" },
		{ BONE_PHYSICS_PROCEDURAL, "BONE_PHYSICS_PROCEDURAL" },
		{ BONE_ALWAYS_PROCEDURAL, "BONE_ALWAYS_PROCEDURAL" },
		{ BONE_SCREEN_ALIGN_SPHERE, "BONE_SCREEN_ALIGN_SPHERE" },
		{ BONE_SCREEN_ALIGN_CYLINDER, "BONE_SCREEN_ALIGN_CYLINDER" },
		{ BONE_USED_BY_HITBOX, "BONE_USED_BY_HITBOX" },
		{ BONE_USED_BY_ATTACHMENT, "BONE_USED_BY_ATTACHMENT" },
		{ BONE_USED_BY_VERTEX_LOD0, "BONE_USED_BY_VERTEX_LOD0" },
		{ BONE_USED_BY_VERTEX_LOD1, "BONE_USED_BY_VERTEX_LOD1" },
		{ BONE_USED_BY_VERTEX_LOD2, "BONE_USED_BY_VERTEX_LOD2" },
		{ BONE_USED_BY_VERTEX_LOD3, "BONE_USED_BY_VERTEX_LOD3" },
		{ BONE_USED_BY_VERTEX_LOD4, "BONE_USED_BY_VERTEX_LOD4" },
		{ BONE_USED_BY_VERTEX_LOD5, "BONE_USED_BY_VERTEX_LOD5" },
		{ BONE_USED_BY_VERTEX_LOD6, "BONE_USED_BY_VERTEX_LOD6" },
		{ BONE_USED_BY_VERTEX_LOD7, "BONE_USED_BY_VERTEX_LOD7" },
		{ BONE_USED_BY_BONE_MERGE, "BONE_USED_BY_BONE_MERGE" },
		{ BONE_FIXED_ALIGNMENT, "BONE_FIXED_ALIGNMENT" },
		{ BONE_HAS_SAVEFRAME_POS, "BONE_HAS_SAVEFRAME_POS" },
		{ BONE_HAS_SAVEFRAME_ROT, "BONE_HAS_SAVEFRAME_ROT" },
	};

	Msg( "//   + Bone Flags: 0x%08x\n", pBone->flags );

	for ( int ii = 0; ii < ARRAYSIZE( boneFlags ); ++ii )
	{
		if ( pBone->flags & boneFlags[ii].m_nMask )
		{
			Msg( "//     - 0x%08x %s\n", boneFlags[ii].m_nMask, boneFlags[ii].m_pzsName );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpBones( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptBones ) ) )
		return;

	const float flEps = 1.0e-5;

	static const Quaternion qIdentity( 0.0f, 0.0f, 0.0f, 1.0f );

	const int nBoneCount = cStudioHdr.numbones();

	Msg( "//\n" );
	Msg( "// Define Bones, Bone Count: %d\n", nBoneCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nBoneCount; ++ii )
	{
		const mstudiobone_t *pBone = cStudioHdr.pBone( ii );
		const mstudiobone_t *pParentBone = pBone->parent < ii && pBone->parent >= 0 ? cStudioHdr.pBone( pBone->parent ) : NULL;
		const mstudiosrcbonetransform_t *pSrcBoneTransform = GetSrcBoneTransform( cStudioHdr.GetRenderHdr(), ii );

		bool bRealigned = false;
		Vector vAlignment;
		QAngle aAlignment;

		if ( pSrcBoneTransform )
		{
			MatrixAngles( pSrcBoneTransform->posttransform, aAlignment, vAlignment );

			if ( !QAnglesAreEqual( QAngle( 0.0f, 0.0f, 0.0f ), aAlignment, flEps ) || !VectorsAreEqual( Vector( 0.0f, 0.0f, 0.0f ), vAlignment, flEps ) )
			{
				bRealigned = true;
			}
		}

		if ( m_bOptVerbose )
		{
			Msg( "// * %3d: %s\n", ii, pBone->pszName() );
			if ( pParentBone )
			{
				Msg( "//   + Parent: %3d: %s\n", pBone->parent, pParentBone->pszName() );
			}

			if ( bRealigned )
			{
				Msg( "//   + REALIGNED\n" );
			}

			if ( pBone->flags & BONE_ALWAYS_PROCEDURAL )
			{
				static const char *s_ProcType[] = { "<ERROR", "AXISINTERP", "QUATINTERP", "AIMATBONE", "AIMATTACH", "JIGGLE" };
				Msg( "//   + Procedural Type: %s\n", s_ProcType[pBone->proctype] );
			}

			const char *pszSurfaceProp = pBone->pszSurfaceProp();
			if ( pszSurfaceProp )
			{
				Msg( "//   + Surface Prop: %s\n", pszSurfaceProp );
			}

			PrintBoneFlags( pBone );
		}

		const QAngle aRot = pBone->rot.ToQAngle();

		if ( pBone->flags & BONE_ALWAYS_PROCEDURAL )
		{
		}
		CFmtStrMax sTmp( "%s$definebone \"%s\" \"%s\" %s %s %s %s %s %s",
			( pBone->flags & BONE_ALWAYS_PROCEDURAL ) ? "// procedural " : "",
			pBone->pszName(),
			pParentBone ? pParentBone->pszName() : "",
			PrettyPrintFloat( pBone->pos.x ).Get(),
			PrettyPrintFloat( pBone->pos.y ).Get(),
			PrettyPrintFloat( pBone->pos.z ).Get(),
			PrettyPrintFloat( aRot.x ).Get(),
			PrettyPrintFloat( aRot.y ).Get(),
			PrettyPrintFloat( aRot.z ).Get() );

		if ( bRealigned )
		{
			sTmp.AppendFormat( " %s %s %s %s %s %s",
				PrettyPrintFloat( vAlignment.x ).Get(),
				PrettyPrintFloat( vAlignment.y ).Get(),
				PrettyPrintFloat( vAlignment.z ).Get(),
				PrettyPrintFloat( aAlignment.x ).Get(),
				PrettyPrintFloat( aAlignment.y ).Get(),
				PrettyPrintFloat( aAlignment.z ).Get() );
		}

		sTmp.Append( "\n" );

		Msg( sTmp.Get() );

		if ( m_bOptVerbose )
		{
			Msg( "\n" );
		}
	}

	if ( !m_bOptVerbose )
	{
		Msg( "\n" );
	}

	bool bSaveFrame = false;

	for ( int ii = 0; ii < nBoneCount; ++ii )
	{
		const mstudiobone_t *pBone = cStudioHdr.pBone( ii );
		const bool bPosition = ( pBone->flags & BONE_HAS_SAVEFRAME_POS ) != 0;
		const bool bRotation = ( pBone->flags & BONE_HAS_SAVEFRAME_POS ) != 0;
		if ( bPosition || bRotation )
		{
			bSaveFrame = true;

			CFmtStr sTmp( "$BoneSaveFrame \"%s\"", pBone->pszName() );

			if ( bPosition )
			{
				sTmp.Append( " position" );
			}

			if ( bRotation )
			{
				sTmp.Append( " rotation" );
			}
			sTmp.Append( "\n" );
			Msg( sTmp.Get() );
		}
	}

	if ( bSaveFrame )
	{
		Msg( "\n" );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpAttachments( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptAttachments ) ) )
		return;

	const int nAtttachmentCount = cStudioHdr.GetNumAttachments();

	Msg( "//\n" );
	Msg( "// Attachment Count: %d\n", nAtttachmentCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nAtttachmentCount; ++ii )
	{
		const mstudioattachment_t &attachment = cStudioHdr.pAttachment( ii );

		CFmtStrMax sTmp( "$attachment \"%s\"",
			attachment.pszName() );

		sTmp.Append( "\n" );

		Msg( sTmp.Get() );
	}

	if ( !m_bOptVerbose )
	{
		Msg( "\n" );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpAnimationList( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptAnimation ) ) )
		return;

	int nAnimCount = 0;
	virtualmodel_t *pVirtualModel = cStudioHdr.GetVirtualModel();
	if ( pVirtualModel )
	{
		nAnimCount = pVirtualModel->m_anim.Count();
	}
	else
	{
		nAnimCount = cStudioHdr.GetRenderHdr()->numlocalanim;
	}

	Msg( "//\n" );
	Msg( "// Animation Count: %d\n", nAnimCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nAnimCount; ++ii )
	{
		const mstudioanimdesc_t &animDesc = cStudioHdr.pAnimdesc( ii );
		const studiohdr_t *pHdr = cStudioHdr.pAnimStudioHdr( ii );

		if ( pHdr != cStudioHdr.GetRenderHdr() )
		{
			Msg( "// %3d: %-50s [Included From %s]\n", ii, animDesc.pszName(), pHdr->pszName() );
		}
		else
		{
			Msg( "// %3d: %-50s\n", ii, animDesc.pszName() );
		}
	}

	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpSequenceList( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptSequence ) ) )
		return;

	const int nSeqCount = cStudioHdr.GetNumSeq();

	Msg( "//\n" );
	Msg( "// Sequence Count: %d\n", nSeqCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nSeqCount; ++ii )
	{
		const mstudioseqdesc_t &seqDesc = cStudioHdr.pSeqdesc( ii );
		const studiohdr_t *pHdr = cStudioHdr.pSeqStudioHdr( ii );

		if ( pHdr != cStudioHdr.GetRenderHdr() )
		{
			Msg( "// %3d: %-50s %-50s [Included From %s]\n", ii, seqDesc.pszLabel(), seqDesc.pszActivityName(), pHdr->pszName() );
		}
		else
		{
			Msg( "// %3d: %-50s %-50s\n", ii, seqDesc.pszLabel(), seqDesc.pszActivityName() );
		}
	}

	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpMesh( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptMesh ) ) )
		return;

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpTexture( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptTexture ) ) )
		return;

	const studiohdr_t *pHdr = cStudioHdr.GetRenderHdr();

	const int nTextureCount = pHdr->numtextures;

	Msg( "//\n" );
	Msg( "// Texture Count: %d\n", nTextureCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nTextureCount; ++ii )
	{
		const mstudiotexture_t *pTexture = pHdr->pTexture( ii );
		Msg( "// Texture: %2d: %s\n", ii, pTexture->pszName() );
	}

	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMdlInfoApp::DumpSkin( CStudioHdr &cStudioHdr ) const
{
	if ( !( m_bOptAll || CommandLineHasOpt( kOptSkin ) ) )
		return;

	const studiohdr_t *pHdr = cStudioHdr.GetRenderHdr();

	const int nSkinCount = pHdr->numskinfamilies;

	Msg( "//\n" );
	Msg( "// Skin Count: %d\n", nSkinCount );
	Msg( "//\n" );
	Msg( "\n" );

	for ( int ii = 0; ii < nSkinCount; ++ii )
	{
		const short *pSkinRef = pHdr->pSkinref( ii * pHdr->numskinref );
		CFmtStrMax sTmp( "// Skin: %2d: [", ii );
		for ( int jj = 0; jj < pHdr->numskinref; ++jj )
		{
			const int nTextureIndex = pSkinRef[jj];
			sTmp.AppendFormat( " %3d", nTextureIndex );
		}
		sTmp.Append( " ]\n" );
		Msg( sTmp.Get() );
	}

	Msg( "\n" );
}