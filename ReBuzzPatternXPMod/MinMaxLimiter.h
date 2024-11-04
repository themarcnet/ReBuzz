#pragma once

#include "resource.h"
#include "afxwin.h"
#include "ScrollWnd.h"
#include "PatEd.h"
#include "INI.h"

class CEditorWnd;

//The original source has members and methods missing
//This version is simply the original, with the missing stuff added.
//NOTE: If any other source file includes MinMaxLimiter.h, it MUST include this version, not the original.
class CMinMaxLimiterDialog : public CDialog
{
	DECLARE_DYNAMIC(CMinMaxLimiterDialog)

public:
	CMinMaxLimiterDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMinMaxLimiterDialog();

// Dialog Data
	enum { IDD = IDD_MINMAXLIMITER_DIALOG };

	CEditorWnd *pew;
	CMICallbacks *pCB;

private:
	CIniReader m_IniReader;
	CIniReader mc_IniReader;
	BOOL IniOK;
	BOOL InicOK;

protected:

	DECLARE_MESSAGE_MAP()
public:
	CComboBox *cbOctaveMin;
	CComboBox *cbOctaveMax;
	CComboBox *cbNoteMin;
	CComboBox *cbNoteMax;
	CComboBox* cbPreset;

	virtual BOOL OnInitDialog();
	void SaveParams();
	void OnBnClickedSave();


protected:
	virtual void OnOK();
	virtual void OnCancel();

private:
	void InitPresetCombo();

public:
//	afx_msg void OnCbnEditupdateCombo1();
	afx_msg void OnComboPresetSelect();

};
