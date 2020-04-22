//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IFILESYSTEMOPENDIALOG_H
#define IFILESYSTEMOPENDIALOG_H
#ifdef _WIN32
#pragma once
#endif



#define FILESYSTEMOPENDIALOG_VERSION	"FileSystemOpenDlg003"


class IFileSystem;


abstract_class IFileSystemOpenDialog
{
public:
	// You must call this first to set the hwnd.
	virtual void Init( CreateInterfaceFn factory, void *parentHwnd ) = 0;

	// Call this to free the dialog.
	virtual void Release() = 0;

	// Use these to configure the dialog.
	virtual void AddFileMask( const char *pMask ) = 0;
	virtual void SetInitialDir( const char *pDir, const char *pPathID = NULL ) = 0;
	virtual void SetFilterMdlAndJpgFiles( bool bFilter ) = 0;
	virtual void GetFilename( char *pOut, int outLen ) const = 0;	// Get the filename they chose.

	// Call this to make the dialog itself. Returns true if they clicked OK and false 
	// if they canceled it.
	virtual bool DoModal() = 0;

	// This uses the standard windows file open dialog.
	virtual bool DoModal_WindowsDialog() = 0;
};


#endif // IFILESYSTEMOPENDIALOG_H
