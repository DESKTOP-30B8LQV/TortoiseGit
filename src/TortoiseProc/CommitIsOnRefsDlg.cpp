// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2011-2016 - TortoiseGit

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
#include "stdafx.h"
#include "resource.h"
#include "CommitIsOnRefsDlg.h"
#include "StringUtils.h"
#include "BrowseRefsDlg.h"
#include "RefLogDlg.h"
#include "LogDlg.h"
#include "LoglistUtils.h"

// CCommitIsOnRefsDlg dialog

IMPLEMENT_DYNAMIC(CCommitIsOnRefsDlg, CResizableStandAloneDialog)

CCommitIsOnRefsDlg::CCommitIsOnRefsDlg(CWnd* pParent /*=nullptr*/)
	: CResizableStandAloneDialog(CCommitIsOnRefsDlg::IDD, pParent)
	, m_bTerminating(false)
{
}

CCommitIsOnRefsDlg::~CCommitIsOnRefsDlg()
{
}

void CCommitIsOnRefsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_REF_LEAFS, m_cRefList);
	DDX_Control(pDX, IDC_FILTER, m_cFilter);
	DDX_Control(pDX, IDC_COMMIT, m_cRevEdit);
}

BEGIN_MESSAGE_MAP(CCommitIsOnRefsDlg, CResizableStandAloneDialog)
	ON_BN_CLICKED(IDC_SELREF, OnBnClickedSelRevBtn)
	ON_EN_CHANGE(IDC_FILTER, OnEnChangeEditFilter)
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CCommitIsOnRefsDlg message handlers

void CCommitIsOnRefsDlg::OnCancel()
{
	m_bTerminating = true;

	DestroyWindow();
}

BOOL CCommitIsOnRefsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	AddAnchor(IDC_FILTER, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_STATIC_FILTER, BOTTOM_LEFT);
	AddAnchor(IDC_SELREF, TOP_RIGHT);
	AddAnchor(IDC_COMMIT, TOP_LEFT);
	AddAnchor(IDC_LIST_REF_LEAFS, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_LOG, TOP_RIGHT);
	AddOthersToAnchor();

	EnableSaveRestore(L"CommitIsOnRefsDlg");

	CImageList* imagelist = new CImageList();
	imagelist->Create(IDB_BITMAP_REFTYPE, 16, 3, RGB(255, 255, 255));
	m_cRefList.SetImageList(imagelist, LVSIL_SMALL);

	CRect rect;
	m_cRefList.GetClientRect(&rect);

	this->m_cRefList.InsertColumn(0, L"Ref", 0, rect.Width() - 50);
	RefreshList();
	return FALSE;
}

void CCommitIsOnRefsDlg::RefreshList()
{
	m_RefList.clear();
	if (g_Git.GetRefsCommitIsOn(m_RefList, m_rev.m_CommitHash, true, true, CGit::BRANCH_ALL))
		MessageBox(g_Git.GetGitLastErr(_T("Could not get all refs.")), _T("TortoiseGit"), MB_ICONERROR);

	AddToList();
}

void CCommitIsOnRefsDlg::AddToList()
{
	m_cRefList.DeleteAllItems();

	CString filter;
	m_cFilter.GetWindowText(filter);

	int item = 0;
	for (size_t i = 0; i < m_RefList.size(); ++i)
	{
		int nImage = -1;
		CString ref = m_RefList[i];
		if (CStringUtils::StartsWith(ref, L"refs/tags/"))
			nImage = 0;
		else if (CStringUtils::StartsWith(ref, L"refs/remotes/"))
			nImage = 2;
		else if (CStringUtils::StartsWith(ref, L"refs/heads/"))
			nImage = 1;

		if (ref.Find(filter) >= 0)
			m_cRefList.InsertItem(item++, ref, nImage);
	}
}

void CCommitIsOnRefsDlg::OnEnChangeEditFilter()
{
	SetTimer(IDT_FILTER, 1000, nullptr);
}

void CCommitIsOnRefsDlg::OnTimer(UINT_PTR nIDEvent)
{
	if( nIDEvent == IDT_FILTER)
	{
		KillTimer(IDT_FILTER);
		AddToList();
	}

	__super::OnTimer(nIDEvent);
}

void CCommitIsOnRefsDlg::OnBnClickedSelRevBtn()
{
	INT_PTR entry = m_cSelRev.GetCurrentEntry();
	if (entry == 0) /* Browse Refence*/
	{
		{
			CString str = CBrowseRefsDlg::PickRef();
			if (str.IsEmpty())
				return;

			if (FillRevFromString(str))
				return;

			m_cRevEdit.SetWindowText(str);
		}
	}

	if (entry == 1) /*Log*/
	{
		CLogDlg dlg;
		CString revision;
		m_cRevEdit.GetWindowText(revision);
		dlg.SetParams(CTGitPath(), CTGitPath(), revision, revision, 0);
		dlg.SetSelect(true);
		if (dlg.DoModal() == IDOK)
		{
			if (dlg.GetSelectedHash().empty())
				return;

			if (FillRevFromString(dlg.GetSelectedHash().at(0).ToString()))
				return;

			m_cRevEdit.SetWindowText(dlg.GetSelectedHash().at(0).ToString());
		}
		else
			return;
	}

	if (entry == 2) /*RefLog*/
	{
		CRefLogDlg dlg;
		if (dlg.DoModal() == IDOK)
		{
			if (FillRevFromString(dlg.m_SelectedHash))
				return;

			m_cRevEdit.SetWindowText(dlg.m_SelectedHash);
		}
		else
			return;
	}

	SetDlgItemText(IDC_FIRSTURL, m_rev.m_CommitHash.ToString().Left(8) + _T(": ") + m_rev.GetSubject());
	if (!m_rev.m_CommitHash.IsEmpty())
		m_tooltips.AddTool(IDC_FIRSTURL,
		CLoglistUtils::FormatDateAndTime(m_rev.GetAuthorDate(), DATE_SHORTDATE) + _T("  ") + m_rev.GetAuthorName());

	/*InterlockedExchange(&m_bThreadRunning, TRUE);
	if (!AfxBeginThread(DiffThreadEntry, this))
	{
		InterlockedExchange(&m_bThreadRunning, FALSE);
		CMessageBox::Show(GetSafeHwnd(), IDS_ERR_THREADSTARTFAILED, IDS_APPNAME, MB_OK | MB_ICONERROR);
	}*/
	KillTimer(IDT_INPUT);
}
