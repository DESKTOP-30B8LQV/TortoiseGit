// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2016 - TortoiseGit

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#pragma once
#include "StandAloneDlg.h"
#include "registry.h"
#include "Git.h"
#include "ACEdit.h"
#include "MenuButton.h"
#include "FilterEdit.h"

// CCommitIsOnRefsDlg dialog

#define IDT_FILTER		101
#define IDT_INPUT		102

class CCommitIsOnRefsDlg : public CResizableStandAloneDialog
{
	DECLARE_DYNAMIC(CCommitIsOnRefsDlg)

public:
	CCommitIsOnRefsDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CCommitIsOnRefsDlg();

	bool IsTerminating() { return m_bTerminating; }
	void RefreshList();

// Dialog Data
	enum { IDD = IDD_COMMITISONREFS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	afx_msg void OnEnChangeEditFilter();
	afx_msg void OnBnClickedSelRevBtn();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	DECLARE_MESSAGE_MAP()

	bool			m_bTerminating;
	STRING_VECTOR	m_RefList;

	void AddToList();
	int FillRevFromString(const CString& str)
	{
		GitRev gitrev;
		if (gitrev.GetCommit(str))
		{
			MessageBox(gitrev.GetLastErr(), L"TortoiseGit", MB_ICONERROR);
			return -1;
		}
		m_rev = gitrev;
		return 0;
	}
	GitRev m_rev;

public:
	CListCtrl			m_cRefList;
	CACEdit				m_cRevEdit;
	CMenuButton			m_cSelRev;
	CFilterEdit			m_cFilter;
};
