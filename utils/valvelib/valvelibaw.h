//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_VALVELIBAW_H__9C6B3FB5_1D05_4B77_A095_DC8F516057DE__INCLUDED_)
#define AFX_VALVELIBAW_H__9C6B3FB5_1D05_4B77_A095_DC8F516057DE__INCLUDED_

// valvelibaw.h : header file
//

class CDialogChooser;
class IBuildProject;

// All function calls made by mfcapwz.dll to this custom AppWizard (except for
//  GetCustomAppWizClass-- see valvelib.cpp) are through this class.  You may
//  choose to override more of the CCustomAppWiz virtual functions here to
//  further specialize the behavior of this custom AppWizard.
class CValvelibAppWiz : public CCustomAppWiz
{
public:
	virtual CAppWizStepDlg* Next(CAppWizStepDlg* pDlg);
	virtual CAppWizStepDlg* Back(CAppWizStepDlg* pDlg);
		
	virtual void InitCustomAppWiz();
	virtual void ExitCustomAppWiz();
	virtual void CustomizeProject(IBuildProject* pProject);

protected:
	CDialogChooser* m_pChooser;
};

// This declares the one instance of the CValvelibAppWiz class.  You can access
//  m_Dictionary and any other public members of this class through the
//  global Valvelibaw.  (Its definition is in valvelibaw.cpp.)
extern CValvelibAppWiz Valvelibaw;

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VALVELIBAW_H__9C6B3FB5_1D05_4B77_A095_DC8F516057DE__INCLUDED_)
