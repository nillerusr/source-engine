//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Code to support the uploading of user submitted items
//
//=============================================================================


#ifndef itemtest_H
#define itemtest_H


#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "tier1/fmtstr.h"
#include "tier1/KeyValues.h"
#include "tier1/utlmap.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/refcount.h"
#include "tier1/smartptr.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeDag;
class CDmElement;
class CDmeMaterial;
class CDmeModel;
class CItemTestManifest;
class CTargetVMT;
class CTargetQC;
class CTargetMDL;

#ifdef DECLARE_LOGGING_CHANNEL
DECLARE_LOGGING_CHANNEL( LOG_ITEMTEST );
#endif

enum ItemtestLogLevel_t
{
	kItemtest_Log_Info,
	kItemtest_Log_Warning,
	kItemtest_Log_Error,
};

//=============================================================================
//
//=============================================================================
class CItemLog
{
public:
	CItemLog( CItemLog *pItemLog = NULL ) : m_pItemLog( pItemLog ) { }

	void SetItemLog( CItemLog *pItemLog ) { m_pItemLog = pItemLog; }

	void Msg( const char *pszFormat, ... ) const;
	void Warning( const char *pszFormat, ... ) const;
	void Error( const char *pszFormat, ... ) const;
	virtual void Log( ItemtestLogLevel_t nLogLevel, const char *pszMessage ) const;

public:
	CItemLog *m_pItemLog;
};

//=============================================================================
//
//=============================================================================
class CItemUpload
{
public:
	// Returns the Steam AccountID as an 0x prefixed hex string, true on success, false on failure
	static bool GetSteamId( CUtlString &sSteamId );

	// Returns $VPROJECT, true on success, false on failure
	static bool GetVProjectDir( CUtlString &sVProjectDir );

	// Returns FileName( GetVProjectDir() ), true on success, false on failure
	static bool GetVMod( CUtlString &sVMod );

	// Returns $SOURCESDK_content/GetVMod(), true on success, false on failure
	static bool GetContentDir( CUtlString &sSDKContentDir );

	// Gets the SourceSDK path from the path of the executable
	static bool GetSourceSDKFromExe( CUtlString &sSourceSDK, CUtlString &sSourceSDKBin );

	static bool GetBinDirectory( CUtlString &sSDKBinDir );
	static bool RunCommandLine( const char *pszCmdLine, const char *pszWorkingDir, CItemLog *pLog );
	static bool FileExists( const char *pszFilename );
	static bool CopyFiles( const char *pszSourceDir, const char *pszPattern, const char *pszDestDir );
	static bool CopyFile( const char *pszSourceFile, const char *pszDestFile );

	static bool CreateDirectory( const char *pszDirectory );

	static bool GetDevMode() { return m_bDev; }
	static void SetDevMode( bool bDev ) { m_bDev = bDev; }

	static bool IgnoreEnvironmentVariables() { return m_bIgnoreEnvVars; }
	static void SetIgnoreEnvironmentVariables( bool bIgnore ) { m_bIgnoreEnvVars = bIgnore; }

	static void ForceSteamID( const char *pszSteamID ) { m_szForcedSteamID = pszSteamID; }
	static const char *GetForcedSteamID( void ) { return m_szForcedSteamID; }

	static bool GetP4() { return m_bP4; }
	static void SetP4( bool bP4 ) { m_bP4 = bP4; }

	static bool IsSameFile( const char *szPath1, const char *szPath2 );
	static bool GetCurrentExecutableFileName( CUtlString &sCurrentExecutableFileName );
	static bool GetSteamAppInstallLocation( CUtlString &sSteamAppInstallLocation, int nAppId );

	static bool InitManifest( void );
	static CItemTestManifest *Manifest( void ) { return m_pItemTestManifest; }

	// Remove punctuation and other special characters from an item name
	static bool SanitizeName( const char *pszName, CUtlString &sCleanName );

protected:
	static bool m_bDev;
	static bool m_bIgnoreEnvVars;
	static bool m_bP4;
	static CUtlString m_szForcedSteamID;
	static CItemTestManifest *m_pItemTestManifest;
};

int GetClassCount();
const char *GetClassString( int i );
const char *GetClassString( const char *pszClassString );
int GetClassIndex( const char *pszClassString );

typedef CUtlVector< CUtlString > ExtensionList;

//=============================================================================
//
//=============================================================================
class CItemTestManifest
{
public:
	CItemTestManifest( const char *pszManifestFile, CItemLog *pItemLog );

	bool		IsValid( void ) { return m_pManifestKV != NULL; }

	int			GetNumClasses( void ) { return m_vecClasses.Count(); }
	const char	*GetClass( int nClass ) { return (nClass >= 0 && nClass < m_vecClasses.Count()) ? m_vecClasses[nClass].Get() : NULL; }
	const char	*GetClassVMTTemplate( int nClass ) { return (nClass >= 0 && nClass < m_vecClassTemplates.Count()) ? m_vecClassTemplates[nClass] : NULL; }
	bool		HasClassVMTTemplates() { return m_vecClassTemplates.Count() > 0; }

	int			GetNumMaterialTypes( void ) { return m_vecMaterialTypes.Count(); }
	const char	*GetMaterialType( int nMaterial ) { return (nMaterial >= 0 && nMaterial < m_vecMaterialTypes.Count()) ? m_vecMaterialTypes[nMaterial].pszMaterialType : NULL; }
	int			GetMaterialType( const char *pszMaterialType );
	int			GetDefaultMaterialType( void ) { return m_nDefaultMaterialType; }

	int			GetNumMaterialSkins( void ) { return max(m_vecMaterialSkins.Count(), 1); }
	const char	*GetMaterialSkin( int nMaterial ) { return (nMaterial >= 0 && nMaterial < m_vecMaterialSkins.Count()) ? m_vecMaterialSkins[nMaterial].pszMaterialSkin : NULL; }
	int			GetMaterialSkin( const char *pszMaterialSkin );
	const char	*GetMaterialSkinFilenameAppend( int nMaterial ) { return (nMaterial >= 0 && nMaterial < m_vecMaterialSkins.Count()) ? m_vecMaterialSkins[nMaterial].pszFilenameAppend : ""; }
	int			GetDefaultMaterialSkin( void ) { return m_nDefaultMaterialSkin; }

	int			GetNumTextureTypes( void ) { return m_vecTextureTypes.Count(); }
	const char	*GetTextureType( int nTexture ) { return (nTexture >= 0 && nTexture < m_vecTextureTypes.Count()) ? m_vecTextureTypes[nTexture].pszTextureType : NULL; }
	bool		IsTextureTypeRequired( int nTexture ) { return (nTexture >= 0 && nTexture < m_vecTextureTypes.Count()) ? !m_vecTextureTypes[nTexture].bOptional : false; }
	int			GetTextureType( const char *pszTextureType );
	KeyValues	*GetTextureAddToVTEXConfig( const char *pszTextureType );

	int			GetNumIconTypes( void ) { return m_vecIconTypes.Count(); }
	const char	*GetIconType( int nIcon ) { return (nIcon >= 0 && nIcon < m_vecIconTypes.Count()) ? m_vecIconTypes[nIcon].pszIconType : NULL; }
	int			GetIconType( const char *pszIconType );
	const char	*GetIconFilenameAppend( int nIcon ) { return (nIcon >= 0 && nIcon < m_vecIconTypes.Count()) ? m_vecIconTypes[nIcon].pszFilenameAppend : ""; }
	bool		GetIconDimensions( int nIcon, int &nWidth, int &nHeight );
	KeyValues	*GetIconAddToVTEXConfig( int nIcon ) { return (nIcon >= 0 && nIcon < m_vecIconTypes.Count()) ? m_vecIconTypes[nIcon].pkvAddToVTEXConfig : NULL; }
	KeyValues	*GetIconVMTTemplate( int nIcon ) { return (nIcon >= 0 && nIcon < m_vecIconTypes.Count()) ? m_vecIconTypes[nIcon].pkvVMTTemplate : NULL; }

	ExtensionList *GetMDLExtensions( void ) { return &m_vecMDLExtensions; }
	ExtensionList *GetAnimationMDLExtentions( void ) { return &m_vecAnimationMDLExtensions; }
	const char	*GetVMTVarForTextureType( const char *pszTexture );
	const char	*GetItemDirectory( void ) { return m_strItemDirectoryOverride.IsEmpty() ? m_pItemDirectory : m_strItemDirectoryOverride.String(); }
	const char	*GetAnimationDirectory( void ) { return m_strAnimationDirectoryOverride.IsEmpty() ? m_pAnimationDirectory : m_strAnimationDirectoryOverride.String(); }
	const char	*GetIconDirectory( void ) { return m_strIconDirectoryOverride.IsEmpty() ? m_pIconDirectory : m_strIconDirectoryOverride.String(); }
	const char	*GetZipSourceDirectory( void ) { return m_pZipSourceDirectory; }
	const char	*GetZipOutputDirectory( void ) { return m_pZipOutputDirectory; }
	const char	*GetQCTemplate( void ) const { return m_pQCTemplate; }
	const char	*GetQCITemplate( void ) const { return m_pQCITemplate; }
	int			GetQCLODDistance( int nLOD ) { return (nLOD > 0 && nLOD <= m_vecQCLODDistances.Count()) ? m_vecQCLODDistances[nLOD-1] : nLOD; }
	bool		UseTerseMessages( void ) { return m_bTerseMessages; }
	bool		GetItemPathUsesSteamId( void ) { return m_bItemPathUsesSteamId; }

	void		SetItemDirectoryOverride( const char *pszItemDirOverride ) { m_strItemDirectoryOverride = pszItemDirOverride; }
	void		SetAnimationDirectoryOverride( const char *pszAnimationDirOverride ) { m_strAnimationDirectoryOverride = pszAnimationDirOverride; }
	void		SetIconDirectoryOverride( const char *pszIconDirOverride ) { m_strIconDirectoryOverride = pszIconDirOverride; }

private:
	bool		ParseStringsFromManifest( KeyValues *pKV, const char *pszKeyName, CUtlVector< CUtlString > &vecList );

private:
	CItemLog	*m_pItemLog;

	KeyValues	*m_pManifestKV;

	struct material_type_manifest_t
	{
		const char	*pszMaterialType;
	};
	struct material_skin_manifest_t
	{
		const char	*pszMaterialSkin;
		const char	*pszFilenameAppend;
	};
	struct texture_type_manifest_t
	{
		const char	*pszTextureType;
		KeyValues	*pkvAddToVTEXConfig;
		bool		 bOptional;
	};
	struct icon_type_manifest_t
	{
		const char	*pszIconType;
		const char	*pszFilenameAppend;
		int			nWidth;
		int			nHeight;
		KeyValues	*pkvAddToVTEXConfig;
		KeyValues	*pkvVMTTemplate;
	};

	CUtlString	m_strItemDirectoryOverride;
	CUtlString	m_strAnimationDirectoryOverride;
	CUtlString	m_strIconDirectoryOverride;

	const char	*m_pItemDirectory;
	const char	*m_pAnimationDirectory;
	const char	*m_pIconDirectory;
	const char	*m_pZipSourceDirectory;
	const char	*m_pZipOutputDirectory;
	const char  *m_pQCTemplate;
	const char	*m_pQCITemplate;
	CUtlVector< int > m_vecQCLODDistances;
	CUtlVector< CUtlString > m_vecClasses;
	CUtlVector<material_type_manifest_t> m_vecMaterialTypes;
	CUtlVector<material_skin_manifest_t> m_vecMaterialSkins;
	CUtlVector<texture_type_manifest_t> m_vecTextureTypes;
	CUtlVector<icon_type_manifest_t> m_vecIconTypes;
	CUtlVector<const char *> m_vecClassTemplates;
	CUtlMap< CUtlString, CUtlString > m_vecVMTTextureRemaps;
	ExtensionList m_vecMDLExtensions;
	ExtensionList m_vecAnimationMDLExtensions;
	bool		  m_bTerseMessages;
	bool          m_bItemPathUsesSteamId;
	int			  m_nDefaultMaterialType;
	int			  m_nDefaultMaterialSkin;
};

//=============================================================================
//
//=============================================================================
template < class T >
class CItemUploadGame : public CItemUpload
{
public:
	static const Vector &GetBipHead( int i );
	static const RadianEuler &GetBipHeadRotation( int i );

	static const Vector s_vBipHead[];
	static const RadianEuler s_eBipHead[];

};


//=============================================================================
// See: http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
//=============================================================================
class CItemUploadTF : public CItemUploadGame< CItemUploadTF >
{
public:
	// These map to indices in s_szClassStrings
	enum ClassType_t
	{
		kDemo,
		kEngineer,
		kHeavy,
		kMedic,
		kPyro,
		kScout,
		kSniper,
		kSoldier,
		kSpy,
		kClassCount
	};

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CAsset;

//=============================================================================
//
//=============================================================================
class CTargetBase : public CItemLog
{
public:
	virtual ~CTargetBase() {}

	// Is this target valid?  A target is valid if all of it's inputs are valid and the settings make sense
	// This function can do any sort of validity checking required.  Not called IsValid to avoid confusion
	// with CSmartPtr::IsValid which most CTargetBase items are wrapped in.  If returns true, sMsg
	// string is unchanged, if returns false, sMsg contains description of first encountered problem.
	virtual bool IsOk( CUtlString &sMsg ) const = 0;

	// Does the output of this go into the content tree or the game tree
	virtual bool IsContent() const { return false; }

	// Does the output of this go under the "models" path
	virtual bool IsModelPath() const { return true; }

	// Get item directory path for this target
	virtual const char *GetItemDirectory() const;

	// Returns a string indicating the type of target, i.e. MDL, QC, DMX, VMT, VTF, TGA, etc...
	virtual const char *GetTypeString() const = 0;

	// Return the number of files that are output from this CTarget, essentially the number of extensions defined
	int GetOutputCount() const;

	// Flags specifying how a path should be returned.  See flag usage below.
	enum PathFlags
	{
		PATH_FLAG_FILE =		0x20,		// If set the filename is included in the returned path
		PATH_FLAG_EXTENSION =	0x10,		// If set and PATH_FLAG_FILE, the returned path will include the extension on the filename
		PATH_FLAG_PATH =		0x08,		// If set the pathname is included in the returned path
		PATH_FLAG_ABSOLUTE =	0x04,		// If set and PATH_FLAG_PATH, the returned path will be absolute (default relative)
		PATH_FLAG_MODELS =		0x02,		// if not set and PATH_FLAG_PATH & !PATH_FLAG_ABSOLUTE, the returned path will include the 'models' prefix, !MODELS implies !PREFIX
		PATH_FLAG_PREFIX =		0x01,		// If not set and PATH_FLAG_PATH & !PATH_FLAG_ABSOLUTE, the returned path will include the prefix (materials | materialsrc)
		PATH_FLAG_ALL =			0x0ffff,
		PATH_FLAG_ZIP =			0x10000
	};

	// Gets the specified output path with the specified flags.  See flag usage below.  sOutputPath cleared on error.
	bool GetOutputPath( CUtlString &sOutputPath, int nIndex = 0, uint nPathFlags = PATH_FLAG_ALL ) const;

	// Appends all output paths to the passed array with the specified flags.  See flag usage below.  If bRecurse is true, calls it for each output CTargetBase as well
	bool GetOutputPaths( CUtlVector< CUtlString > &sOutputPaths, uint nPathFlags = PATH_FLAG_ALL, bool bRecurse = false ) const;

	// First compile all inputs and then compile this target which produces the expected output files
	// Can do all sorts of things, like load and save files or call vtex, studiomdl, etc...
	virtual bool Compile();

	// Returns the output filenames for this target and optionally for all inputs recursively, either relative or absolute
	bool GetOutputPaths( CUtlVector< CUtlString > &sOutputPaths, bool bRelative, bool bRecurse, bool bExtension, bool bPrefix = true ) const;

	// Returns the input filenames for this target and optionally for all inputs recursively, either relative or absolute
	virtual bool GetInputPaths( CUtlVector< CUtlString > &sInputPaths, bool bRelative = true, bool bRecurse = true, bool bExtension = true );

	// Gets all of the CTargetBase inputs for this CTargetBase
	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const
	{
		AssertMsg( 0, "Implement CTargetBase::GetInputs for derived class\n" );
		return false;
	}

	// Returns the extensions for the outputs of this CTargetBase.  Extensions should include the .
	virtual const ExtensionList *GetExtensionsAndCount( void ) const
	{
		return NULL;
	}

	// Returns a prefix between the mod directory and the relative path, used for 'materials' & 'materialsrc'
	virtual const char *GetPrefix() const { return NULL; }

	// The name of this target without any extension, i.e. the filename where it should be written
	virtual void GetName( CUtlString &sName ) const;
	void SetCustomOutputName( const char *pszCustomOutputName ) { m_strCustomOutputName = pszCustomOutputName; }
	const CUtlString &GetCustomOutputName() const { return m_strCustomOutputName; }

	virtual void SetNameSuffix( const char *pszSuffix ) { m_sNameSuffix = pszSuffix; }
	virtual const char *GetNameSuffix() const { return m_sNameSuffix.Get(); }

	virtual void SetCustomRelativeDir( const char *pszCustomRelativeDir ) { m_sCustomRelativeDir = pszCustomRelativeDir; }
	virtual const char *GetCustomRelativeDir() const { return m_sCustomRelativeDir.IsEmpty() ? NULL : m_sCustomRelativeDir.Get(); }

	// The name of the asset that owns this target
	virtual const CUtlString &GetAssetName() const;

	// The asset that owns this target
	CAsset *Asset() const;

	virtual void UpdateManifest( KeyValues *pKv ) = 0;

	void SetIgnoreP4( bool bIgnoreP4 ) { m_bIgnoreP4 = bIgnoreP4; }

	KeyValues *GetCustomKeyValues() { return m_kvCustomKeys; }

	void SetCustomModPath( const char *pszCustomModPath ) { m_sCustomModPath = pszCustomModPath; }
	const char *GetCustomModPath() const { return m_sCustomModPath.IsEmpty() ? NULL : m_sCustomModPath.String(); }

protected:
	friend class CAsset;
	friend class CRefCountAccessor;

	CTargetBase( CAsset *pAsset, const CTargetBase *pTargetParent );

	void AddRef()
	{
		++m_nRefCount;
	}

	void Release()
	{
		Assert( m_nRefCount > 0 );
		--m_nRefCount;

		if ( m_nRefCount == 0 )
		{
			delete this;
		}
	}

	virtual bool CheckFile( const char *pszFilename ) const;

	// Gets the directory name for this asset.  See PathFlags_t above.
	// PATH_FLAG_FILE & PATH_FLAG_EXTENSION are not meaningful for this function
	virtual bool GetDirName( CUtlString &sDirName, uint nPathFlags = PATH_FLAG_ALL ) const;

	// Create the output directory for this target if necessary
	bool CreateOutputDirectory() const;

	void AddOrEditP4File( const char *pszFilePath );

private:
	CAsset *m_pAsset;					// The asset that owns this target
	const CTargetBase *m_pTargetParent;
	CUtlString m_sNameSuffix;
	CUtlString m_sCustomRelativeDir;
	CUtlString m_strCustomOutputName;	// Name of output file
	int m_nRefCount;
	bool m_bIgnoreP4;
	KeyValuesAD m_kvCustomKeys;
	CUtlString m_sCustomModPath;
};


//=============================================================================
//
//=============================================================================
class CTargetTGA : public CTargetBase
{
public:
	virtual ~CTargetTGA();

	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual bool IsContent() const { return true; }
	virtual bool IsModelPath() const;

	virtual const char *GetTypeString() const { return "TGA"; }

	virtual bool Compile();

	virtual bool GetInputPaths( CUtlVector< CUtlString > &sInputPaths, bool bRelative = true, bool bRecurse = true, bool bExtension = true );

	// No CTargetBase inputs for CTargetTGA
	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const { return true; }

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual const char *GetPrefix() const;
	virtual void GetName( CUtlString &sName ) const;

	void Clear();

	// An input texture file... TGA, PSD... something else?
	bool SetInputFile( const char *pszFilename );

	const CUtlString &GetInputFile() const;

	int GetWidth() const { return m_nWidth; }
	int GetHeight() const { return m_nHeight; }
	int GetChannelCount() const { return m_nChannelCount; }
	bool HasAlpha() const { return m_bAlpha; }

	virtual void UpdateManifest( KeyValues *pKv );

	void SetNoNiceFiltering( bool bNoNice ) { m_bNoNiceFiltering = bNoNice; }
	void SetChannelCount( int nChannels ) { m_nChannelCount = nChannels; }

protected:
	friend class CAsset;
	friend class CTargetVMT;

public:
	// Made public so we can load and validate assets before building them
	CTargetTGA( CAsset *pAsset, const CTargetVMT *pTargetVMT );
protected:

	template < class T > static T NearestPowerOfTwo( T nVal );

	CUtlString m_sInputFile;	// Arbitrary input file
	CUtlString m_sFileBase;		// Name of file without extension or path
	CUtlString m_sExtension;	// Extension of m_sInputFile

	mutable CUtlString m_sPrefix;

	enum {
		IMAGE_FILE_UNKNOWN,
		IMAGE_FILE_TGA,
		IMAGE_FILE_PSD,
		IMAGE_FILE_VTF
	} m_nSrcImageType;
	int m_nWidth;
	int m_nHeight;
	int m_nChannelCount;
	bool m_bNoNiceFiltering;
	bool m_bAlpha;
	bool m_bPowerOfTwo;

	const CTargetVMT *m_pTargetVMT;

};


//=============================================================================
//
//=============================================================================
class CTargetVTF : public CTargetBase
{
public:
	virtual ~CTargetVTF();

	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const { return m_pTargetTGA->IsOk( sMsg ); }

	virtual bool IsContent() const { return false; }
	virtual bool IsModelPath() const;

	virtual const char *GetTypeString() const { return "VTF"; }

	virtual bool Compile();

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const;

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual const char *GetPrefix() const;

	virtual void GetName( CUtlString &sName ) const;

	virtual void SetNameSuffix( const char *pszSuffix ) { m_pTargetTGA->SetNameSuffix( pszSuffix ); }
	virtual const char *GetNameSuffix() const { return m_pTargetTGA->GetNameSuffix(); }

	bool SetInputFile( const char *pszFilename ) { return m_pTargetTGA->SetInputFile( pszFilename ); }

	const CUtlString &GetInputFile() const { return m_pTargetTGA->GetInputFile(); }

	bool HasAlpha() const { return m_pTargetTGA->HasAlpha(); }

	virtual void UpdateManifest( KeyValues *pKV )
	{
		m_pTargetTGA->UpdateManifest( pKV );
	}

	CTargetTGA *GetTargetTGA( void ) const { return m_pTargetTGA.IsValid() ? m_pTargetTGA.GetObject() : NULL; }

protected:
	friend class CAsset;

	CTargetVTF( CAsset *pAsset, const CTargetVMT *pTargetVMT );

	const CTargetVMT *m_pTargetVMT;
	CSmartPtr< CTargetTGA > m_pTargetTGA;
};

#define kInvalidMaterialType			-1
#define kInvalidMaterialSkin			-1
#define LEGACY_MATERIALTYPE_PRIMARY		1
#define LEGACY_MATERIALTYPE_SECONDARY	2

//=============================================================================
//
//=============================================================================
class CTargetVMT : public CTargetBase
{
public:
	enum ColorAlpha_t
	{
		kNoColorAlpha,
		kTransparency,
		kPaintable,
		kColorSpecPhong
	};

	static const char *ColorAlphaTypeToString( ColorAlpha_t nColorAlphaType )
	{
		static const char *pszColorAlphaTypeStr[] = {
			"NoColorAlpha",
			"Transparency",
			"Paintable",
			"ColorSpecPhong"
		};

		if ( nColorAlphaType < 0 || nColorAlphaType >= ARRAYSIZE( pszColorAlphaTypeStr ) )
		{
			return "Unknown";
		}

		return pszColorAlphaTypeStr[ nColorAlphaType ];
	}

	static ColorAlpha_t StringToColorAlphaType( const char *pszUserData )
	{
		if ( StringHasPrefix( pszUserData, "T" ) )
			return CTargetVMT::kTransparency;

		if ( StringHasPrefix( pszUserData, "P" ) )
			return CTargetVMT::kPaintable;

		if ( StringHasPrefix( pszUserData, "S" ) )
			return CTargetVMT::kColorSpecPhong;

		return CTargetVMT::kNoColorAlpha;
	}

	enum NormalAlpha_t
	{
		kNoNormalAlpha,
		kNormalSpecPhong
	};

	static const char *NormalAlphaTypeToString( NormalAlpha_t nNormalAlphaType )
	{
		static const char *pszNormalAlphaTypeStr[] = {
			"NoNormalAlpha",
			"NormalSpecPhong"
		};

		if ( nNormalAlphaType < 0 || nNormalAlphaType >= ARRAYSIZE( pszNormalAlphaTypeStr ) )
		{
			return "Unknown";
		}

		return pszNormalAlphaTypeStr[ nNormalAlphaType ];
	}

	static NormalAlpha_t StringToNormalAlphaType( const char *pszUserData )
	{
		if ( StringHasPrefix( pszUserData, "N" ) )
			return CTargetVMT::kNormalSpecPhong;

		if ( StringHasPrefix( pszUserData, "S" ) )
			return CTargetVMT::kNormalSpecPhong;

		return CTargetVMT::kNoNormalAlpha;
	}

	const char *MaterialTypeToString( int nMaterialType );
	static int StringToMaterialType( const char *pszUserData );

	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual bool IsContent() const { return false; }

	virtual const char *GetTypeString() const { return "VMT"; }

	virtual bool Compile();

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const;

	virtual const char *GetPrefix() const { return "materials"; }

	virtual void GetName( CUtlString &sName ) const;

	void SetMaterialId( const char *pszMaterialId );

	virtual void GetMaterialId( CUtlString &sMaterialId ) const { sMaterialId = m_sMaterialId; }
	const CUtlString &GetMaterialId() const { return m_sMaterialId; }

	bool SetTargetVTF( const char *pszTextureType, const char *pszFilename, int nSkinIndex = CItemUpload::Manifest()->GetDefaultMaterialSkin() );

	void SetColorAlphaType( ColorAlpha_t nColorAlphaType ) { m_nColorAlphaType = nColorAlphaType; }
	ColorAlpha_t GetColorAlphaType() const { return m_nColorAlphaType; }

	void SetNormalAlphaType( NormalAlpha_t nNormalAlphaType ) { m_nNormalAlphaType = nNormalAlphaType; }
	NormalAlpha_t GetNormalAlphaType() const { return m_nNormalAlphaType; }

	bool SetMaterialType( int nMaterialType );
	int GetMaterialType() const;

	void SetDuplicate( int nMaterialType );
	bool GetDuplicate() const { return m_bDuplicate; }

	void SetTargetResolution( int nWidth, int nHeight ) { m_nTargetWidth = nWidth; m_nTargetHeight = nHeight; }
	int  GetTargetWidth( void ) const { return m_nTargetWidth; }
	int  GetTargetHeight( void ) const { return m_nTargetHeight; }

	int GetNumTargetVTFS( int nSkinIndex )
	{
		if ( nSkinIndex >= 0 && nSkinIndex < m_vecTargetVTFs.Count() )
		{
			return m_vecTargetVTFs[nSkinIndex].Count();
		}
		return 0;
	}

	CTargetVTF *GetTargetVTF( int iVTF, int nSkinIndex = 0 ) 
	{ 
		if ( nSkinIndex >= 0 && nSkinIndex < m_vecTargetVTFs.Count() )
		{
			if ( iVTF >= 0 && iVTF < m_vecTargetVTFs[nSkinIndex].Count() )
				return m_vecTargetVTFs[nSkinIndex][iVTF].IsValid() ? m_vecTargetVTFs[nSkinIndex][iVTF].GetObject() : NULL;
		}
		return NULL;
	}

	const char *GetTextureTypeForTGA( CTargetTGA *pTGA ) const
	{
		FOR_EACH_VEC( m_vecTargetVTFs, i )
		{
			FOR_EACH_VEC( m_vecTargetVTFs[i], j )
			{
				if ( m_vecTargetVTFs[i][j].IsValid() && m_vecTargetVTFs[i][j]->GetTargetTGA() == pTGA )
					return CItemUpload::Manifest()->GetTextureType(j);
			}
		}
		return NULL;
	}
	virtual KeyValues *GetTextureAddToVTEXConfigForTGA( CTargetTGA *pTGA ) const
	{
		const char *pszTextureType = GetTextureTypeForTGA( pTGA );
		if ( pszTextureType )
		{
			return CItemUpload::Manifest()->GetTextureAddToVTEXConfig( pszTextureType );
		}
		return NULL;
	}

	void SetVMTKV( const KeyValues *pKV, int nSkinIndex = 0 );
	KeyValues *GetVMTKV( int nSkinIndex );

	void CreateLegacyTemplate( KeyValues *pVMTKV );

	virtual void UpdateManifest( KeyValues *pKV );

protected:
	friend class CAsset;

	CTargetVMT( CAsset *pAsset, const CTargetBase *pTargetParent );

	virtual ~CTargetVMT();

	KeyValues *m_pVMTKV;

	mutable ExtensionList m_vecExtensions;
	CUtlVector< CUtlVector< CSmartPtr< CTargetVTF > > > m_vecTargetVTFs;
	ColorAlpha_t m_nColorAlphaType;

	NormalAlpha_t m_nNormalAlphaType;

	int m_nMaterialType;
	bool m_bDuplicate;

	int m_nTargetWidth;
	int m_nTargetHeight;

	CUtlString m_sMaterialId;

	bool SetTargetVTF( CSmartPtr< CTargetVTF > &pTargetVTF, const char *pszFilename, const char *pszSuffix ) const;
};

//=============================================================================
//
//=============================================================================
class CTargetIcon : public CTargetVMT
{
public:
	virtual bool IsModelPath() const { return false; }

	virtual const char *GetItemDirectory() const { return CItemUpload::Manifest()->GetIconDirectory(); }

	virtual KeyValues *GetTextureAddToVTEXConfigForTGA( CTargetTGA *pTGA ) const { return CItemUpload::Manifest()->GetIconAddToVTEXConfig( m_nIconType ); }

	bool SetTargetVTF( const char *pszFilename );

protected:
	friend class CAsset;

	CTargetIcon( CAsset *pAsset, int nIconType );

	int m_nIconType;
};


//=============================================================================
// Inputs OBJ, SMD, DMX -> Outputs DMX in proper place with proper name
//=============================================================================
class CTargetDMX : public CTargetBase
{
public:
	virtual ~CTargetDMX();

	// From CItemBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual const char *GetItemDirectory() const
	{
		return m_strQCITemplate.IsEmpty() ? CItemUpload::Manifest()->GetItemDirectory() : CItemUpload::Manifest()->GetAnimationDirectory();
	}

	virtual bool IsContent() const { return true; }

	virtual const char *GetTypeString() const { return "DMX"; }

	virtual bool Compile();

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual bool GetInputPaths( CUtlVector< CUtlString > &sInputPaths, bool bRelative = true, bool bRecurse = true, bool bExtension = true );

	// No CTargetBase inputs for this target
	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const { return true; }

	virtual void GetName( CUtlString &sName ) const;

	int GetPolyCount() const;
	int GetTriangleCount() const;
	int GetVertexCount() const;

	bool GetAnimationFrameInfo( float& flFrameRate, int& nFrameCount ) const;

	bool AreAllMeshesSkinned() const;

	const CUtlString &GetInputFile() const;

	void SetLod( int nLod ) { m_nLod = nLod; }
	int GetLod() const { return m_nLod; }

	void SetQCITemplate( const char *pszQCITemplate ) { m_strQCITemplate = pszQCITemplate; }

	virtual void UpdateManifest( KeyValues *pKV )
	{
		CFmtStr sTmp;
		sTmp.sprintf( "lod%d", GetLod() );
		KeyValues *pLodSubKey = new KeyValues( sTmp.Access() );
		pKV->AddSubKey( pLodSubKey );
		pLodSubKey->SetString( "filename", GetInputFile().Get() );
		CUtlString sOutName;
		if ( GetOutputPath( sOutName, 0, PATH_FLAG_PATH | PATH_FLAG_FILE | PATH_FLAG_PREFIX | PATH_FLAG_MODELS | PATH_FLAG_EXTENSION ) )
		{
			pLodSubKey->SetString( "out_filename", sOutName );
		}
		pLodSubKey->SetInt( "polyCount", GetPolyCount() );
		pLodSubKey->SetInt( "vertexCount", GetVertexCount() );
	}

	void SetSoundScriptFilePath( const char *pszSoundScriptFile ) { m_strSoundScriptFile = pszSoundScriptFile; }
	const char *GetSoundScriptFilePath() const { return m_strSoundScriptFile.String(); }
	bool WriteVCD( CUtlBuffer &vcdBuf );
	void SetAnimationLoopStartTime( float flStartTime ) { m_flAnimationLoopStartTime = flStartTime; }

protected:
	friend class CAsset;
	friend class CTargetQC;

	CTargetDMX( CAsset *pAsset, const CTargetQC *pTargetQC );

	void Clear();

	// An input geometry file, SMD, OBJ or DMX
	bool SetInputFile( const char *pszFilename );

	// Reloads the existing file, called on compile in case user changed something
	bool ReloadFile();

	bool IsInputObj() const;	// Simple test based on extension checking
	bool IsInputSmd() const;	// Simple test based on extension checking
	bool IsInputDmx() const;	// Simple test based on extension checking
	bool IsInputFbx() const;	// Simple test based on extension checking

	CDmElement *LoadObj();
	CDmElement *LoadSmd();
	CDmElement *LoadDmx();
	CDmElement *LoadFbx();

	void ReplaceMaterials() const;

	void SkinToBipHead();

	void SkinToBipHead_R( CDmeModel *pDstModel, CDmeDag *pSrcDag, const Vector &vBipHead );

	bool OutputQCIFile();
	void OutputSounds( CUtlBuffer &buf, int nIndentLevel, CDmElement *pExportedSounds );

	// Dependencies/Inputs
	CUtlString m_sInputFile;	// Arbitrary input file
	CUtlString m_sExtension;	// Extension of m_sInputFile

	CDmElement *m_pDmRoot;		// Root of loaded DMX file

	CUtlVector< CSmartPtr< CTargetVMT > > m_targetVmtList;

	int m_nLod;					// The LOD index
	CUtlString m_strQCITemplate; // QCI Template for Animation DMX
	float m_flAnimationLoopStartTime;
	CUtlString m_strSoundScriptFile;
};


//=============================================================================
//
//=============================================================================
class CTargetVCD : public CTargetBase
{
public:
	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const { return true; }

	virtual bool IsContent() const { return false; }

	virtual const char *GetTypeString() const { return "VCD"; }

	virtual const char *GetPrefix() const { return "scenes"; }

	virtual bool IsModelPath() const { return false; }

	virtual bool Compile();

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const { return true; }

	virtual void UpdateManifest( KeyValues *pKV ) {}

	void SetInputFile( const char *pszVCD ) { m_strVCDPath = pszVCD; }

protected:
	friend class CAsset;

	CTargetVCD( CAsset *pAsset, const CTargetQC *pTargetQC );
	const CTargetQC *m_pTargetQC;
	CUtlString m_strVCDPath;
};


//=============================================================================
//
//=============================================================================
class CTargetQC : public CTargetBase
{
public:
	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual bool IsContent() const { return true; }

	virtual const char *GetTypeString() const { return "QC"; }

	virtual bool Compile();

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const;

	void SetQCTemplate( const char *pszQCTemplate );
	const char *GetQCTemplate();

	void SetQCITemplate( const char *pszQCITemplate ) { m_strQCITemplate = pszQCITemplate; }
	const char *GetQCITemplate() { return m_strQCITemplate.IsEmpty() ? NULL : m_strQCITemplate.Get(); }

	// Add a DMX Target to a QC which is a pathname to a geometry file (SMD, OBJ or DMX) in some arbitrary location
	int TargetDMXCount() const;
	int AddTargetDMX( const char *pszGeometryFile );
	bool SetTargetDMX( int nLOD, const char *pszGeometryFile );
	bool RemoveTargetDMX( int nLOD );
	CSmartPtr< CTargetDMX > GetTargetDMX( int nLOD ) const;

	// VCDs
	CSmartPtr< CTargetVCD > GetTargetVCD();

	virtual void UpdateManifest( KeyValues *pKV )
	{
		for ( int i = 0; i < TargetDMXCount(); ++i )
		{
			GetTargetDMX( i )->UpdateManifest( pKV );
		}
	}

protected:
	friend class CAsset;

	CTargetQC( CAsset *pAsset, const CTargetMDL *pTargetMDL );

	enum
	{
		kATexture,		// i.e. Red
		kBTexture,		// i.e. Blue
		kCTexture,		// i.e. Green
		kDTexture,		// i.e. Yellow
		kCommonTexture,
		kTextureCount
	};

	// Dependencies/Inputs
	CUtlVector< CSmartPtr< CTargetDMX > > m_TargetDMXs;	// 0 = LOD0, 1 = LOD1
	CSmartPtr< CTargetVCD > m_TargetVCD;

	CUtlBuffer m_QCTemplate;
	CUtlString m_strQCITemplate;
};


//=============================================================================
//
//=============================================================================
class CTargetMDL : public CTargetBase
{
public:
	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual bool IsContent() const { return false; }

	virtual const char *GetTypeString() const { return "MDL"; }

	virtual bool Compile();

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const;

	CSmartPtr< CTargetQC > GetTargetQC() const { return m_pTargetQC; }

	virtual void UpdateManifest( KeyValues *pKV )
	{
		GetTargetQC()->UpdateManifest( pKV );
	}

protected:
	friend class CAsset;

	CTargetMDL( CAsset *pAsset, const CTargetBase *pTargetParent );

	// Dependencies/Inputs
	CSmartPtr< CTargetQC > m_pTargetQC;
};


//=============================================================================
//
//=============================================================================
class CAsset : public CTargetBase
{
public:
	CAsset();

	CAsset( const char *pszName, bool *pbOk = NULL );

	virtual ~CAsset();

	// From CTargetBase
	virtual bool IsOk( CUtlString &sMsg ) const;

	virtual bool IsContent() const { return true; }

	virtual const char *GetTypeString() const { return "ZIP"; }
	
	// This compiles all the inputs but doesn't create the final archive
	bool CompilePreview();
	virtual bool PostCompilePreview();

	virtual bool Compile();
	virtual bool PostCompile();

	virtual bool GetInputs( CUtlVector< CTargetBase * > &inputs ) const;

	virtual const ExtensionList *GetExtensionsAndCount( void ) const;

	virtual const CUtlString &GetAssetName() const;

	// Returns the
	bool GetDirectory( CUtlString &sDirectory, const char *pszPrefix ) const;

	// Returns <class>/<steamid>/<name>, false if there's something wrong
	bool GetRelativeDir( CUtlString &sRelativeDir, const char *pszPrefix, const CTargetBase *pTarget ) const;

	// Returns GetSDKContentDir()/GetRelativeDir() or GetVProjectDir()/GetRelativeDir()
	bool GetAbsoluteDir( CUtlString &sAbsoluteDir, const char *pszPrefix, const CTargetBase *pTarget ) const;

	// Get the files that were built during the compile process
	const CUtlVector< CUtlString > &GetBuiltFiles() const { return m_sAbsPaths; }
	const CUtlVector< CUtlString > &GetRelativePathBuiltFiles() const { return m_sRelPaths; }
	const CUtlVector< CUtlString > &GetModFiles() const { return m_sModOutputs; }
	void AddModOutput( const char *pszCustomModOutput ) { m_sModOutputs.AddToTail( pszCustomModOutput ); }

	bool SetName( const char *pszName );
	bool IsNameValid() const;

	const char *GetSteamId() const;
	bool IsSteamIdValid() const;

	void SetSkinToBipHead( bool bSkinToBipHead ) { m_bSkinToBipHead = bSkinToBipHead; }
	bool SkinToBipHead() const { return m_bSkinToBipHead; }

	virtual const char *GetClass() const = 0;
	virtual KeyValues *GetAdditionalManifestData( void ) = 0;

	bool SetTargetIcon( int nIcon, const char *pszIconFile );
	CSmartPtr< CTargetIcon > GetTargetIcon( int nIcon ) const { return m_vecTargetIcons[ nIcon ]; }

	int AddModel();
	int GetNumModels() const { return m_vecModels.Count(); }
	int GetCurrentModel() const { return m_nCurrentModel; }
	bool SetCurrentModel( int nModel );
	void RemoveModels();

	int TargetDMXCount() const;
	int AddTargetDMX( const char *pszGeometryFile );
	bool SetTargetDMX( int nLOD, const char *pszGeometryFile );
	bool RemoveTargetDMX( int nLOD );
	CSmartPtr< CTargetDMX > GetTargetDMX( int nLOD );

	CSmartPtr< CTargetMDL > GetTargetMDL() const;

	CSmartPtr< CTargetQC > GetTargetQC() const;

	int GetTargetVMTCount() const;
	CTargetVMT *GetTargetVMT( int nIndex ) const;

	template < class T, class C > CSmartPtr< T > NewTarget( const C *pTargetParent )
	{
		return CSmartPtr< T >( new T( this, pTargetParent ) );
	}

	CSmartPtr< CTargetVMT > FindOrAddMaterial( const char *pszMaterial, int nMaterialType );

	CSmartPtr< CTargetVMT > FindMaterial( const char *pszMaterial );

	bool Mkdir( const char *pszPrefix, const CTargetBase *pTarget );

	void CreateManifest( CUtlBuffer &manifestBuf );

	virtual void UpdateManifest( KeyValues *pKV )
	{
		KeyValues *pkvAdditional = GetAdditionalManifestData();
		if ( pkvAdditional )
		{
			pkvAdditional->CopySubkeys( pKV );
		}

		pKV->SetString( "name", GetAssetName().Get() );
		pKV->SetString( "class", GetClass() );
		pKV->SetString( "steamId", GetSteamId() );

		for ( int i = 0; i < TargetDMXCount(); ++i )
		{
			GetTargetDMX( i )->UpdateManifest( pKV );
		}

		for ( int i = 0; i < GetTargetVMTCount(); ++i )
		{
			CFmtStr sTmp;
			sTmp.sprintf( "vmt%d", i );
			KeyValues *pSubKey = new KeyValues( sTmp.Access() );
			pKV->AddSubKey( pSubKey );
			GetTargetVMT( i )->UpdateManifest( pSubKey );
		}
	}

	void SetArchivePath( const char *pszArchivePath ) { m_sArchivePath = pszArchivePath; }
	const char *GetArchivePath() const { return m_sArchivePath; }

	const char* CheckRedundantOutputFilePath( const char* pszInputFilePath, const char* pszVTEXConfig, const char* pszOutputFilePath );

	void ExcludeFileExtension( const char *pszExtension ) { m_sExcludeFileExtensions.AddToTail( pszExtension ); }

	void SetBuildScenesImage( bool bShouldBuildScenesImage ) { m_bShouldBuildScenesImage = bShouldBuildScenesImage; }

protected:
	CUtlString m_sSteamId;
	CUtlString m_sName;
	bool m_bSkinToBipHead;

	// build scenes.image
	bool BuildScenesImage();
	bool m_bShouldBuildScenesImage;

	// Called only by CTargetVMT destructor
	friend class CTargetVMT;
	bool RemoveMaterial( const char *pszMaterial );

	// This owns the pointers for all of the targets in the Asset
	CUtlVector< CSmartPtr< CTargetBase > > m_targetStore;

	// Dependencies/Inputs
	CUtlVector< CSmartPtr< CTargetMDL > > m_vecModels;
	int m_nCurrentModel;

	CUtlMap< CUtlString, CTargetVMT * > m_vmtMap;

	CUtlVector< CSmartPtr< CTargetIcon > > m_vecTargetIcons;

	// The path to the ZIP archive
	CUtlString m_sArchivePath;

	// Files that were built during the compile process
	CUtlVector< CUtlString > m_sAbsPaths;
	CUtlVector< CUtlString > m_sRelPaths;
	CUtlVector< CUtlString > m_sModOutputs;

	// Sometime we don't want to include some extensions in the zip file
	CUtlVector< CUtlString > m_sExcludeFileExtensions;

	struct CompileOutputFile_t
	{
		CUtlString m_strInputFilePath;
		CUtlString m_strVTEXConfig;
		CUtlString m_strOutputFilePath;
	};
	CUtlVector< CompileOutputFile_t > m_CompileOutputFiles;
};


//=============================================================================
//
// Instantiate a game specific asset class
//
// e.g.: typedef CAssetGame< CItemUploadTF > CAssetTF;
//
//=============================================================================
template < typename T >
class CAssetGame : public CAsset
{
	typedef CAsset BaseClass;

public:
	CAssetGame()
	: CAsset()
	{
		m_sClass = GetClassString( "" );
		m_pkvAdditionalManifestData = NULL;
	}

	CAssetGame( const char *pszClass, const char *pszName, bool *pbOk /* = NULL */ )
	: CAsset( pszName, pbOk )
	{
		const char *pszSanitizedClass = GetClassString( pszClass );
		if ( pszSanitizedClass )
		{
			m_sClass = pszSanitizedClass;
		}
		else
		{
			m_sClass = GetClassString( "" );
		}
	}

	bool SetClass( const char *pszClass )
	{
		const char *pszRealClass = GetClassString( pszClass );

		if ( pszRealClass )
		{
			m_sClass = pszRealClass;
		}
		else
		{
			m_sClass = pszClass;
			Assert( !IsClassValid() );
		}

		return IsClassValid();
	}

	virtual const char *GetClass() const { return m_sClass.Get(); }

	bool IsClassValid() const
	{
		if ( m_sClass.Length() <= 0 || GetClassString( m_sClass ) == NULL )
			return false;

		return true;
	}

	void SetAdditionalManifestData( KeyValues *pKV )
	{
		if ( m_pkvAdditionalManifestData )
		{
			m_pkvAdditionalManifestData->deleteThis();
			m_pkvAdditionalManifestData = NULL;
		}

		m_pkvAdditionalManifestData = pKV->MakeCopy();
	}

	KeyValues *GetAdditionalManifestData( void )
	{
		return m_pkvAdditionalManifestData;
	}

	virtual bool IsOk( CUtlString &sMsg ) const
	{
		if ( !IsClassValid() )
		{
			sMsg = "Invalid Class";
			return false;
		}

		return BaseClass::IsOk( sMsg );
	}

	const Vector &GetBipHead() const
	{
		const int nClassIndex = GetClassIndex( m_sClass );
		return CItemUploadGame< T >::GetBipHead( nClassIndex );
	}

	const RadianEuler &GetBipHeadRotation() const
	{
		const int nClassIndex = GetClassIndex( m_sClass );
		return CItemUploadGame< T >::GetBipHeadRotation( nClassIndex );
	}

protected:
	CUtlString	m_sClass;
	KeyValues	*m_pkvAdditionalManifestData;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
typedef CAssetGame< CItemUploadTF > CAssetTF;


//-----------------------------------------------------------------------------
// Get[Input|Output]Path(s) flag usage
//-----------------------------------------------------------------------------
//
// Get the name of the nth output file
// For example:
//
//	FILE	EXTEN	PATH	ABS		MODELS	PREFIX	OutputName
//	true	true	true	true	true	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	true	true	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	true	false	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	true	false	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo.vmt
//
//	true	true	true	false	true	true	materials/models/player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	false	true	false	models/player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	false	false	true	player/items/pyro/0x00017714/foo/foo.vmt
//	true	true	true	false	false	false	player/items/pyro/0x00017714/foo/foo.vmt
//
//	true	true	false	true	true	true	foo.vmt
//	true	true	false	true	true	false	foo.vmt
//	true	true	false	true	false	true	foo.vmt
//	true	true	false	true	false	false	foo.vmt
//
//	true	true	false	false	true	true	foo.vmt
//	true	true	false	false	true	false	foo.vmt
//	true	true	false	false	false	true	foo.vmt
//	true	true	false	false	false	false	foo.vmt
//
//	true	false	true	true	true	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	true	false	true	true	true	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	true	false	true	true	false	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	true	false	true	true	false	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//
//	true	false	true	false	true	true	materials/models/player/items/pyro/0x00017714/foo/foo
//	true	false	true	false	true	false	models/player/items/pyro/0x00017714/foo/foo
//	true	false	true	false	false	true	player/items/pyro/0x00017714/foo/foo
//	true	false	true	false	false	false	player/items/pyro/0x00017714/foo/foo
//
//	true	false	false	true	true	true	foo
//	true	false	false	true	true	false	foo
//	true	false	false	true	false	true	foo
//	true	false	false	true	false	false	foo
//
//	true	false	false	false	true	true	foo
//	true	false	false	false	true	false	foo
//	true	false	false	false	false	true	foo
//	true	false	false	false	false	false	foo
//
//	false	true	true	true	true	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo
//	false	true	true	true	true	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo
//	false	true	true	true	false	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo
//	false	true	true	true	false	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo
//
//	false	true	true	false	true	true	materials/models/player/items/pyro/0x00017714/foo
//	false	true	true	false	true	false	models/player/items/pyro/0x00017714/foo
//	false	true	true	false	false	true	player/items/pyro/0x00017714/foo
//	false	true	true	false	false	false	player/items/pyro/0x00017714/foo
//
//	false	true	false	true	true	true	- FAILURE -
//	false	true	false	true	true	false	- FAILURE -
//	false	true	false	true	false	true	- FAILURE -
//	false	true	false	true	false	false	- FAILURE -
//
//	false	true	false	false	true	true	- FAILURE -
//	false	true	false	false	true	false	- FAILURE -
//	false	true	false	false	false	true	- FAILURE -
//	false	true	false	false	false	false	- FAILURE -
//
//	false	false	true	true	true	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	false	false	true	true	true	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	false	false	true	true	false	true	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//	false	false	true	true	false	false	d:/dev/main/game/tf/materials/models/player/items/pyro/0x00017714/foo/foo
//
//	false	false	true	false	true	true	materials/models/player/items/pyro/0x00017714/foo/foo
//	false	false	true	false	true	false	models/player/items/pyro/0x00017714/foo/foo
//	false	false	true	false	false	true	player/items/pyro/0x00017714/foo/foo
//	false	false	true	false	false	false	player/items/pyro/0x00017714/foo/foo
//
//	false	false	false	true	true	true	- FAILURE -
//	false	false	false	true	true	false	- FAILURE -
//	false	false	false	true	false	true	- FAILURE -
//	false	false	false	true	false	false	- FAILURE -
//
//	false	false	false	false	true	true	- FAILURE -
//	false	false	false	false	true	false	- FAILURE -
//	false	false	false	false	false	true	- FAILURE -
//	false	false	false	false	false	false	- FAILURE -
//


#endif // itemtest_H
