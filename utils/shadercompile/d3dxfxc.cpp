//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: D3DX command implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#include "shadercompile.h"

#include "d3dxfxc.h"
#include "cmdsink.h"

// Required to compile using D3DX* routines in the same process
#include <d3dx9shader.h>
#include "dx_proxy/dx_proxy.h"

#include <tier0/icommandline.h>
#include <tier1/strtools.h>

#define D3DXSHADER_MICROCODE_BACKEND_OLD_DEPRECATED ( 1 << 25 )

namespace InterceptFxc
{

	// The command that is intercepted by this namespace routines
	static const char *s_pszCommand = "fxc.exe ";
	static size_t s_uCommandLen = strlen( s_pszCommand );

	namespace Private
	{
		//
		// Response implementation
		//
		class CResponse : public CmdSink::IResponse
		{
		public:
			explicit CResponse( LPD3DXBUFFER pShader, LPD3DXBUFFER pListing, HRESULT hr );
			~CResponse( void );

		public:
			virtual bool Succeeded( void ) { return m_pShader && (m_hr == D3D_OK); }
			virtual size_t GetResultBufferLen( void ) { return ( Succeeded() ? m_pShader->GetBufferSize() : 0 ); }
			virtual const void * GetResultBuffer( void ) { return ( Succeeded() ? m_pShader->GetBufferPointer() : NULL ); }
			virtual const char * GetListing( void ) { return (const char *) ( m_pListing ? m_pListing->GetBufferPointer() : NULL ); }

		protected:
			LPD3DXBUFFER m_pShader;
			LPD3DXBUFFER m_pListing;
			HRESULT m_hr;
		};

		CResponse::CResponse( LPD3DXBUFFER pShader, LPD3DXBUFFER pListing, HRESULT hr ) :
			m_pShader(pShader),
			m_pListing(pListing),
			m_hr(hr)
		{
			NULL;
		}

		CResponse::~CResponse( void )
		{
			if ( m_pShader )
				m_pShader->Release();

			if ( m_pListing )
				m_pListing->Release();
		}

		//
		// Perform a fast shader file compilation.
		//		TODO: avoid writing "shader.o" and "output.txt" files to avoid extra filesystem access.
		//
		// @param pszFilename		the filename to compile (e.g. "debugdrawenvmapmask_vs20.fxc")
		// @param pMacros			null-terminated array of macro-defines
		// @param pszModel			shader model for compilation
		//
		void FastShaderCompile( const char *pszFilename, const D3DXMACRO *pMacros, const char *pszModel, CmdSink::IResponse **ppResponse )
		{
			LPD3DXBUFFER pShader = NULL; // NOTE: Must release the COM interface later
			LPD3DXBUFFER pErrorMessages = NULL; // NOTE: Must release COM interface later
			
			// DxProxyModule
			static DxProxyModule s_dxModule;

			// X360TEMP: This needs to be moved to an external semantic (or fixed)
			bool bIsX360 = false;
			for ( int i=0; ;i++ )
			{
				if ( !pMacros[i].Name )
				{
					break;
				}
				if ( V_stristr( pMacros[i].Name, "_X360" ) && atoi( pMacros[i].Definition ) )
				{
					bIsX360 = true;
					break;
				}
			}
			
			HRESULT hr = s_dxModule.D3DXCompileShaderFromFile( pszFilename, pMacros, NULL /* LPD3DXINCLUDE */,
				"main",	pszModel, 0, &pShader, &pErrorMessages,
				NULL /* LPD3DXCONSTANTTABLE *ppConstantTable */ );

			if ( ppResponse )
			{
				*ppResponse = new CResponse( pShader, pErrorMessages, hr );
			}
			else
			{
				if ( pShader )
				{
					pShader->Release();
				}

				if ( pErrorMessages )
				{
					pErrorMessages->Release();
				}
			}
		}

	}; // namespace Private

	//
	// Completely mimic the behaviour of "fxc.exe" in the specific cases related
	// to shader compilations.
	//
	// @param pCommand       the command in form
	//		"fxc.exe /DSHADERCOMBO=1 /DTOTALSHADERCOMBOS=4 /DCENTROIDMASK=0 /DNUMDYNAMICCOMBOS=4 /DFLAGS=0x0 /DNUM_BONES=1 /Dmain=main /Emain /Tvs_2_0 /DSHADER_MODEL_VS_2_0=1 /D_X360=1 /nologo /Foshader.o debugdrawenvmapmask_vs20.fxc>output.txt 2>&1"
	//
	void ExecuteCommand( const char *pCommand, CmdSink::IResponse **ppResponse )
	{
		// Expect that the command passed is exactly "fxc.exe"
		Assert( !strncmp( pCommand, s_pszCommand, s_uCommandLen ) );
		pCommand += s_uCommandLen;

		// A duplicate portion of memory for modifications
		void *bufEditableCommand = alloca( strlen( pCommand ) + 1 );
		char *pEditableCommand = strcpy( (char *) bufEditableCommand, pCommand );

		// Macros to be defined for D3DX
		CUtlVector<D3DXMACRO> macros;

		// Shader model (determined when parsing "/D" flags)
		const char *pszShaderModel = NULL;

		// Iterate over the command line and find all "/D...=..." settings
		for ( char *pszFlag = pEditableCommand;
			( pszFlag = strstr( pszFlag, "/D" ) ) != NULL;
			/* advance inside */ )
		{
			// Make sure this is a command-line flag (basic check for preceding space)
			if ( pszFlag > pEditableCommand &&
				pszFlag[-1] &&
				' ' != pszFlag[-1] )
			{
				++ pszFlag;
				continue;
			}

			// Name is immediately after "/D"
			char *pszFlagName = pszFlag + 2; // 2 = length of "/D"
			// Value will be determined later
			char *pszValue = "";

			if ( char *pchEq = strchr( pszFlag, '=' ) )
			{
				// Value is after '=' sign
				*pchEq = 0;
				pszValue = pchEq + 1;
				pszFlag = pszValue;
			}

			if ( char *pchSpace = strchr( pszFlag, ' ' ) )
			{
				// Space is designating the end of the flag
				*pchSpace = 0;
				pszFlag = pchSpace + 1;
			}
			else
			{
				// Reached end of command line
				pszFlag = "";
			}

			// Shader model extraction
			if ( !strncmp(pszFlagName, "SHADER_MODEL_", 13) )
			{
				pszShaderModel = pszFlagName + 13;
			}

			// Add the macro definition to the macros array
			int iMacroIdx = macros.AddToTail();
			D3DXMACRO &m = macros[iMacroIdx];

			// Fill the macro data
			m.Name = pszFlagName;
			m.Definition = pszValue;
		}

		// Add a NULL-terminator
		{
			D3DXMACRO nullTerminatorMacro = { NULL, NULL };
			macros.AddToTail( nullTerminatorMacro );
		}

		// Convert shader model to lowercase
		char chShaderModel[20] = {0};
		if(pszShaderModel)
		{
			Q_strncpy( chShaderModel, pszShaderModel, sizeof(chShaderModel) - 1 );
		}
		Q_strlower( chShaderModel );

		// Determine the file name (at the end of the command line before redirection)
		char const *pszFilename = "";
		if ( const char *pchCmdRedirect = strstr( pCommand, ">output.txt " ) )
		{
			size_t uCmdEndOffset = ( pchCmdRedirect - pCommand );

			pEditableCommand[uCmdEndOffset] = 0;
			pszFilename = &pEditableCommand[uCmdEndOffset];

			while ( pszFilename > pEditableCommand &&
				pszFilename[-1] &&
				' ' != pszFilename[-1] )
			{
				-- pszFilename;
			}
		}

		// Compile the stuff
		Private::FastShaderCompile( pszFilename, macros.Base(), chShaderModel, ppResponse );
	}

	bool TryExecuteCommand( const char *pCommand, CmdSink::IResponse **ppResponse )
	{
		{
			static bool s_bNoIntercept = ( CommandLine()->FindParm("-nointercept") != 0 );
			static int s_dummy = ( Msg( s_bNoIntercept ?
				"[shadercompile] Using old slow technique - runs 'fxc.exe'.\n" :
				"[shadercompile] Using new faster Vitaliy's implementation.\n" ), 1 );
			if ( !s_bNoIntercept && !strncmp(pCommand, InterceptFxc::s_pszCommand, InterceptFxc::s_uCommandLen) )
			{
				// Trap "fxc.exe" so that we did not spawn extra process every time
				InterceptFxc::ExecuteCommand( pCommand, ppResponse );
				return true;
			}
		}

		return false;
	}

}; // namespace InterceptFxc




