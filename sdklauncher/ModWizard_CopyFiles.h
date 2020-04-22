//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MODWIZARD_COPYFILES_H
#define MODWIZARD_COPYFILES_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ProgressBar.h>
#include "utlvector.h"
#include "configs.h"


// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard sub panel 3.
// This panel asks for the directory to install in and the mod name.
// --------------------------------------------------------------------------------------------------------------------- //

namespace vgui
{

	class CModWizardSubPanel_CopyFiles : public WizardSubPanel
	{
	public:
		typedef WizardSubPanel BaseClass;

	public:
		CModWizardSubPanel_CopyFiles( Panel *parent, const char *panelName );

		// Called to store the settings it'll use to copy all the files over.
		void GetReady( const char *pOutputDirName, const char *pOutputModGamedirName, const char *modName ) ;
		
		virtual WizardSubPanel* GetNextSubPanel();
		virtual void OnDisplayAsNext();
		virtual void OnTick();


	protected:
		class CFileCopyInfo
		{
		public:
			CFileCopyInfo( const char *pIn, const char *pOut )
			{
				Q_strncpy( m_InFilename, pIn, sizeof( m_InFilename ) );
				Q_strncpy( m_OutFilename, pOut, sizeof( m_OutFilename ) );
			}

			char m_InFilename[MAX_PATH];
			char m_OutFilename[MAX_PATH];
		};

	protected:

		bool BuildCopyFiles_R( const char *pSourceDir, const char *pMask, const char *pOutputDirName );

		bool BuildCopyFilesForMappings( char **pMappings, int nMappings );
		bool HandleSpecialFileCopy( CFileCopyInfo *pInfo, bool &bErrorStatus );
		bool HandleReplacements_GenericVCProj( CFileCopyInfo *pInfo, bool &bErrorStatus );
		virtual bool BuildCopyFilesForMod_HL2() = 0;
		virtual bool BuildCopyFilesForMod_HL2MP() = 0;
		virtual bool BuildCopyFilesForMod_FromScratch() = 0;
		virtual bool BuildCopyFilesForMod_SourceCodeOnly() = 0;
		virtual bool HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus ) = 0;
		virtual bool HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus ) = 0;
		// Right now only one of these files gets modified, but keeping it here for expansion in the future.
		virtual bool HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus ) = 0;

	protected:	

		CUtlVector<CFileCopyInfo> m_FileCopyInfos;
		int m_iCurCopyFile;	// -1 at the beginning.


		Label *m_pLabel;
		Label *m_pFinishedLabel;
		ProgressBar *m_pProgressBar;

		char m_OutputDirName[MAX_PATH];		// c:\mymod
		char m_OutModGamedirName[MAX_PATH];	// c:\mymod\mymod
		char m_ModName[MAX_PATH];			// mymod
		ModType_t m_ModType;
	};

	class CModWizardSubPanel_CopyFiles_Source2006 : public CModWizardSubPanel_CopyFiles
	{

	public:
		CModWizardSubPanel_CopyFiles_Source2006( Panel *parent, const char *panelName );

	private:

		bool BuildCopyFilesForMod_HL2();
		bool BuildCopyFilesForMod_HL2MP();
		bool BuildCopyFilesForMod_FromScratch();
		bool BuildCopyFilesForMod_SourceCodeOnly();
		bool HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus );
		bool HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus );
		bool HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus ) { return false; } // Ep1 will never do this.

	};

	class CModWizardSubPanel_CopyFiles_Source2007 : public CModWizardSubPanel_CopyFiles
	{
	public:
		CModWizardSubPanel_CopyFiles_Source2007( Panel *parent, const char *panelName );

	private:

		bool BuildCopyFilesForMod_HL2();
		bool BuildCopyFilesForMod_HL2MP();
		bool BuildCopyFilesForMod_FromScratch();
		bool BuildCopyFilesForMod_SourceCodeOnly();
		bool HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus );
		bool HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus );

		// Right now only one of these files gets modified, but keeping it here for expansion in the future.
		bool HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus );

	};

	class CModWizardSubPanel_CopyFiles_Source2009 : public CModWizardSubPanel_CopyFiles
	{
	public:
		CModWizardSubPanel_CopyFiles_Source2009( Panel *parent, const char *panelName );

	private:

		bool BuildCopyFilesForMod_HL2();
		bool BuildCopyFilesForMod_HL2MP();
		bool BuildCopyFilesForMod_FromScratch();
		bool BuildCopyFilesForMod_SourceCodeOnly();
		bool HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus );
		bool HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus );

		// Right now only one of these files gets modified, but keeping it here for expansion in the future.
		bool HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus );

	};
}


#endif // MODWIZARD_COPYFILES_H
