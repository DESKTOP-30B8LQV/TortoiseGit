// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2003-2009, 2015 - TortoiseSVN
// Copyright (C) 2008-2016 - TortoiseGit

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

#include "stdafx.h"
#include "TortoiseProc.h"
#include "cursor.h"
#include "StatGraphDlg.h"
#include "LogDlg.h"
#include "MessageBox.h"
#include "registry.h"
#include "AppUtils.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "TempFile.h"
#include "IconMenu.h"
#include "BrowseRefsDlg.h"
#include "SmartHandle.h"
#include "LogOrdering.h"
#include "ClipboardHelper.h"

#define WM_TGIT_REFRESH_SELECTION   (WM_APP + 1)

IMPLEMENT_DYNAMIC(CLogDlg, CResizableStandAloneDialog)
CLogDlg::CLogDlg(CWnd* pParent /*=nullptr*/)
	: CResizableStandAloneDialog(CLogDlg::IDD, pParent)
	, m_wParam(0)
	, m_nSortColumn(0)
	, m_bFollowRenames(false)
	, m_bSelect(false)
	, m_bSelectionMustBeSingle(true)
	, m_bShowTags(true)
	, m_bShowLocalBranches(true)
	, m_bShowRemoteBranches(true)
	, m_bNoMerges(false)
	, m_iHidePaths(0)
	, m_bWalkBehavior(FALSE)
	, m_bFirstParent(false)
	, m_bWholeProject(FALSE)
	, m_iCompressedGraph(0)
	, m_bShowWC(true)

	, m_bSelectionMustBeContinuous(false)

	, m_bCancelled(FALSE)
	, m_pNotifyWindow(nullptr)

	, m_bAscending(FALSE)
	, m_hAccel(nullptr)
	, m_bNavigatingWithSelect(false)
{
	m_bFilterWithRegex = !!CRegDWORD(_T("Software\\TortoiseGit\\UseRegexFilter"), FALSE);
	m_bFilterCaseSensitively = !!CRegDWORD(L"Software\\TortoiseGit\\FilterCaseSensitively", FALSE);
	m_bAsteriskLogPrefix = !!CRegDWORD(L"Software\\TortoiseGit\\AsteriskLogPrefix", TRUE);
	m_LogList.m_SelectedFilters = CRegDWORD(_T("Software\\TortoiseGit\\SelectedLogFilters"), LOGFILTER_ALL);

	CString str;
	str=g_Git.m_CurrentDir;
	str.Replace(_T(":"),_T("_"));
	str=CString(_T("Software\\TortoiseGit\\LogDialog\\AllBranch\\"))+str;

	m_regbAllBranch=CRegDWORD(str,FALSE);

	m_AllBranchType = (AllBranchType)(DWORD)m_regbAllBranch;
	switch (m_AllBranchType)
	{
	case AllBranchType::None:
		m_bAllBranch = FALSE;
		break;
	case AllBranchType::AllBranches:
		m_bAllBranch = TRUE;
		break;
	default:
		m_bAllBranch = BST_INDETERMINATE;
	}

	str = g_Git.m_CurrentDir;
	str.Replace(_T(":"),_T("_"));
	m_regShowWholeProject = CRegDWORD(_T("Software\\TortoiseGit\\TortoiseProc\\ShowWholeProject\\") + str, FALSE);
	m_bWholeProject = m_regShowWholeProject;
	m_regbShowTags = CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\ShowTags\\") + str, TRUE);
	m_bShowTags = !!m_regbShowTags;
	m_regbShowLocalBranches = CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\ShowLocalBranches\\") + str, TRUE);
	m_bShowLocalBranches = !!m_regbShowLocalBranches;
	m_regbShowRemoteBranches = CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\ShowRemoteBranches\\") + str, TRUE);
	m_bShowRemoteBranches = !!m_regbShowRemoteBranches;

	m_bShowUnversioned = CRegDWORD(_T("Software\\TortoiseGit\\AddBeforeCommit"), TRUE);
	
	m_bShowGravatar = !!CRegDWORD(_T("Software\\TortoiseGit\\EnableGravatar"), FALSE);
	m_regbShowGravatar = CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\ShowGravatar\\") + str, m_bShowGravatar);
	m_bShowGravatar = !!m_regbShowGravatar;
	m_bShowDescribe = !!CRegDWORD(_T("Software\\TortoiseGit\\ShowDescribe"));
	m_bShowBranchRevNo = !!CRegDWORD(_T("Software\\TortoiseGit\\ShowBranchRevisionNumber"), FALSE);
}

CLogDlg::~CLogDlg()
{
	m_regbAllBranch = (DWORD)m_AllBranchType;
	m_regbShowTags = m_bShowTags;
	m_regbShowLocalBranches = m_bShowLocalBranches;
	m_regbShowRemoteBranches = m_bShowRemoteBranches;
	m_regbShowGravatar = m_bShowGravatar;
}

void CLogDlg::DoDataExchange(CDataExchange* pDX)
{
	CResizableStandAloneDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LOGLIST, m_LogList);
	DDX_Control(pDX, IDC_LOGMSG, m_ChangedFileListCtrl);
	DDX_Control(pDX, IDC_PROGRESS, m_LogProgress);
	DDX_Control(pDX, IDC_SPLITTERTOP, m_wndSplitter1);
	DDX_Control(pDX, IDC_SPLITTERBOTTOM, m_wndSplitter2);
	DDX_Text(pDX, IDC_SEARCHEDIT, m_LogList.m_sFilterText);
	DDX_Control(pDX, IDC_DATEFROM, m_DateFrom);
	DDX_Control(pDX, IDC_DATETO, m_DateTo);
	DDX_Control(pDX, IDC_LOG_JUMPTYPE, m_JumpType);
	DDX_Control(pDX, IDC_LOG_JUMPUP, m_JumpUp);
	DDX_Control(pDX, IDC_LOG_JUMPDOWN, m_JumpDown);
	DDX_Text(pDX, IDC_LOGINFO, m_sLogInfo);
	DDX_Check(pDX, IDC_LOG_ALLBRANCH,m_bAllBranch);
	DDX_Check(pDX, IDC_WHOLE_PROJECT, m_bWholeProject);
	DDX_Check(pDX, IDC_WALKBEHAVIOUR, m_bWalkBehavior);
	DDX_Control(pDX, IDC_SEARCHEDIT, m_cFilter);
	DDX_Control(pDX, IDC_STATIC_REF, m_staticRef);
	DDX_Control(pDX, IDC_PIC_AUTHOR, m_gravatar);
	DDX_Control(pDX, IDC_FILTER, m_cFileFilter);
}

BEGIN_MESSAGE_MAP(CLogDlg, CResizableStandAloneDialog)
	ON_WM_CONTEXTMENU()
	ON_WM_SETCURSOR()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LOGLIST, OnLvnItemchangedLoglist)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LOGMSG, OnLvnItemchangedLogmsg)
	ON_NOTIFY(EN_LINK, IDC_MSGVIEW, OnEnLinkMsgview)
	ON_EN_VSCROLL(IDC_MSGVIEW, OnEnscrollMsgview)
	ON_EN_HSCROLL(IDC_MSGVIEW, OnEnscrollMsgview)
	ON_BN_CLICKED(IDC_STATBUTTON, OnBnClickedStatbutton)

	ON_MESSAGE(WM_TGIT_REFRESH_SELECTION, OnRefreshSelection)
	ON_MESSAGE(WM_FILTEREDIT_INFOCLICKED, OnClickedInfoIcon)
	ON_MESSAGE(WM_FILTEREDIT_CANCELCLICKED, OnClickedCancelFilter)

	ON_MESSAGE(MSG_LOAD_PERCENTAGE,OnLogListLoading)

	ON_EN_CHANGE(IDC_SEARCHEDIT, OnEnChangeSearchedit)
	ON_WM_TIMER()
	ON_NOTIFY(DTN_DATETIMECHANGE, IDC_DATETO, OnDtnDatetimechangeDateto)
	ON_NOTIFY(DTN_DATETIMECHANGE, IDC_DATEFROM, OnDtnDatetimechangeDatefrom)
	ON_CBN_SELCHANGE(IDC_LOG_JUMPTYPE, &CLogDlg::OnCbnSelchangeJumpType)
	ON_COMMAND(IDC_LOG_JUMPUP, &CLogDlg::OnBnClickedJumpUp)
	ON_COMMAND(IDC_LOG_JUMPDOWN, &CLogDlg::OnBnClickedJumpDown)
	ON_COMMAND(ID_GO_UP, &CLogDlg::OnBnClickedJumpUp)
	ON_COMMAND(ID_GO_DOWN, &CLogDlg::OnBnClickedJumpDown)
	ON_COMMAND(ID_EXITCLEARFILTER, OnExitClearFilter)
	ON_BN_CLICKED(IDC_WALKBEHAVIOUR, OnBnClickedWalkBehaviour)
	ON_BN_CLICKED(IDC_VIEW, OnBnClickedView)
	ON_BN_CLICKED(IDC_WHOLE_PROJECT, OnBnClickShowWholeProject)
	ON_NOTIFY(LVN_COLUMNCLICK,IDC_LOGLIST, OnLvnColumnclick)
	ON_COMMAND(MSG_FETCHED_DIFF, OnBnClickedHidepaths)
	ON_BN_CLICKED(IDC_LOG_ALLBRANCH, OnBnClickedAllBranch)
	ON_EN_CHANGE(IDC_FILTER, OnEnChangeFileFilter)

	ON_NOTIFY(DTN_DROPDOWN, IDC_DATEFROM, &CLogDlg::OnDtnDropdownDatefrom)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_REFRESH, &CLogDlg::OnBnClickedRefresh)
	ON_STN_CLICKED(IDC_STATIC_REF, &CLogDlg::OnBnClickedBrowseRef)
	ON_COMMAND(ID_LOGDLG_REFRESH, &CLogDlg::OnBnClickedRefresh)
	ON_COMMAND(ID_GO_BACKWARD_SELECT, &CLogDlg::GoBackAndSelect)
	ON_COMMAND(ID_GO_FORWARD_SELECT, &CLogDlg::GoForwardAndSelect)
	ON_COMMAND(ID_GO_BACKWARD, &CLogDlg::GoBack)
	ON_COMMAND(ID_GO_FORWARD, &CLogDlg::GoForward)
	ON_COMMAND(ID_SELECT_SEARCHFIELD, &CLogDlg::OnSelectSearchField)
	ON_COMMAND(ID_LOGDLG_FIND, &CLogDlg::OnFind)
	ON_COMMAND(ID_LOGDLG_FOCUSFILTER, &CLogDlg::OnFocusFilter)
	ON_COMMAND(ID_EDIT_COPY, &CLogDlg::OnEditCopy)
	ON_MESSAGE(MSG_REFLOG_CHANGED, OnRefLogChanged)
	ON_REGISTERED_MESSAGE(TaskBarButtonCreated, OnTaskbarBtnCreated)

	ON_REGISTERED_MESSAGE(CGitStatusListCtrl::GITSLNM_ITEMCHANGED, &CLogDlg::OnFileListCtrlItemChanged)
	ON_WM_MOVE()
	ON_WM_MOVING()
	ON_WM_SIZING()
	ON_WM_CTLCOLOR()
	ON_WM_PAINT()
END_MESSAGE_MAP()

enum JumpType
{
	JumpType_AuthorEmail,
	JumpType_CommitterEmail,
	JumpType_MergePoint,
	JumpType_Parent1,
	JumpType_Parent2,
	JumpType_Tag,
	JumpType_TagFF,
	JumpType_Branch,
	JumpType_BranchFF,
	JumpType_History,
};

void CLogDlg::SetParams(const CTGitPath& orgPath, const CTGitPath& path, CString hightlightRevision, CString range, DWORD limit, int limitScale/*=-1*/)
{
	m_orgPath = orgPath;
	m_path = path;
	m_hightlightRevision = hightlightRevision;

	if (range == GIT_REV_ZERO)
		range = _T("HEAD");

	if (!(range.IsEmpty() || range == _T("HEAD")))
	{
		m_bAllBranch = BST_UNCHECKED;
		m_AllBranchType = AllBranchType::None;
	}

	SetRange(range);

	if (limitScale == CFilterData::SHOW_NO_LIMIT || (!range.IsEmpty() && m_LogList.m_Filter.m_NumberOfLogsScale != CFilterData::SHOW_LAST_N_COMMITS))
		m_LogList.m_Filter.m_NumberOfLogsScale = CFilterData::SHOW_NO_LIMIT;
	if (limitScale >= CFilterData::SHOW_LAST_N_COMMITS && limit > 0)
	{
		// limitation from command line argument, so override the filter.
		m_LogList.m_Filter.m_NumberOfLogs = limit;
		m_LogList.m_Filter.m_NumberOfLogsScale = limitScale;
	}

	if (::IsWindow(m_hWnd))
		UpdateData(FALSE);
}

void CLogDlg::SetFilter(const CString& findstr, LONG findtype, bool findregex)
{
	m_LogList.m_sFilterText = findstr;
	if (findtype)
		m_LogList.m_SelectedFilters = findtype;
	m_LogList.m_bFilterWithRegex = m_bFilterWithRegex = findregex;
}

BOOL CLogDlg::OnInitDialog()
{
	CString temp;
	CResizableStandAloneDialog::OnInitDialog();
	CAppUtils::MarkWindowAsUnpinnable(m_hWnd);

	// Let the TaskbarButtonCreated message through the UIPI filter. If we don't
	// do this, Explorer would be unable to send that message to our window if we
	// were running elevated. It's OK to make the call all the time, since if we're
	// not elevated, this is a no-op.
	CHANGEFILTERSTRUCT cfs = { sizeof(CHANGEFILTERSTRUCT) };
	typedef BOOL STDAPICALLTYPE ChangeWindowMessageFilterExDFN(HWND hWnd, UINT message, DWORD action, PCHANGEFILTERSTRUCT pChangeFilterStruct);
	CAutoLibrary hUser = AtlLoadSystemLibraryUsingFullPath(_T("user32.dll"));
	if (hUser)
	{
		ChangeWindowMessageFilterExDFN *pfnChangeWindowMessageFilterEx = (ChangeWindowMessageFilterExDFN*)GetProcAddress(hUser, "ChangeWindowMessageFilterEx");
		if (pfnChangeWindowMessageFilterEx)
		{
			pfnChangeWindowMessageFilterEx(m_hWnd, TaskBarButtonCreated, MSGFLT_ALLOW, &cfs);
		}
	}
	m_pTaskbarList.Release();
	if (FAILED(m_pTaskbarList.CoCreateInstance(CLSID_TaskbarList)))
		m_pTaskbarList = nullptr;

	m_hAccel = LoadAccelerators(AfxGetResourceHandle(),MAKEINTRESOURCE(IDR_ACC_LOGDLG));

	// use the state of the "stop on copy/rename" option from the last time
	UpdateData(FALSE);

	// set the font to use in the log message view, configured in the settings dialog
	CAppUtils::CreateFontForLogs(m_logFont);
	GetDlgItem(IDC_MSGVIEW)->SetFont(&m_logFont);
	// make the log message rich edit control send a message when the mouse pointer is over a link
	GetDlgItem(IDC_MSGVIEW)->SendMessage(EM_SETEVENTMASK, NULL, ENM_LINK | ENM_SCROLL);

	// "unrelated paths" should be in gray color
	m_iHidePaths = 2;

	// set up the columns
	m_LogList.DeleteAllItems();

	m_LogList.m_Path=m_path;
	m_LogList.m_hasWC = !GitAdminDir::IsBareRepo(g_Git.m_CurrentDir);
	m_LogList.m_bShowWC = m_LogList.m_hasWC && m_bShowWC;
	m_LogList.InsertGitColumn();

	if (m_bWholeProject)
		m_LogList.m_Path.Reset();

	m_ChangedFileListCtrl.Init(GITSLC_COLEXT | GITSLC_COLSTATUS |GITSLC_COLADD|GITSLC_COLDEL, _T("LogDlg"), (GITSLC_POPALL ^ (GITSLC_POPIGNORE|GITSLC_POPRESTORE)), false, m_LogList.m_hasWC, GITSLC_COLEXT | GITSLC_COLSTATUS | GITSLC_COLADD| GITSLC_COLDEL);

	GetDlgItem(IDC_LOGLIST)->UpdateData(FALSE);

	SetDlgTitle();

	CheckRegexpTooltip();

	SetSplitterRange();

	// the filter control has a 'cancel' button (the red 'X'), we need to load its bitmap
	m_cFilter.SetCancelBitmaps(IDI_CANCELNORMAL, IDI_CANCELPRESSED);
	m_cFilter.SetInfoIcon(IDI_LOGFILTER);
	m_cFilter.SetValidator(this);

	AdjustControlSize(IDC_LOG_ALLBRANCH);
	AdjustControlSize(IDC_WHOLE_PROJECT);

	GetClientRect(m_DlgOrigRect);
	m_LogList.GetClientRect(m_LogListOrigRect);
	GetDlgItem(IDC_MSGVIEW)->GetClientRect(m_MsgViewOrigRect);
	m_ChangedFileListCtrl.GetClientRect(m_ChgOrigRect);

	m_Brush.CreateSolidBrush(RGB(174, 200, 255));

	m_DateFrom.SendMessage(DTM_SETMCSTYLE, 0, MCS_WEEKNUMBERS|MCS_NOTODAY|MCS_NOTRAILINGDATES|MCS_NOSELCHANGEONNAV);
	m_DateTo.SendMessage(DTM_SETMCSTYLE, 0, MCS_WEEKNUMBERS | MCS_NOTRAILINGDATES | MCS_NOSELCHANGEONNAV);

	m_staticRef.SetURL(CString());

	AddMainAnchors();
	SetFilterCueText();

	m_LogList.m_ShowMask &= ~CGit::LOG_INFO_LOCAL_BRANCHES;
	if (m_AllBranchType == AllBranchType::AllBranches)
		m_LogList.m_ShowMask|=CGit::LOG_INFO_ALL_BRANCH;
	else if (m_AllBranchType == AllBranchType::AllLocalBranches)
		m_LogList.m_ShowMask |= CGit::LOG_INFO_LOCAL_BRANCHES;
	else if (m_AllBranchType == AllBranchType::AllBasicRefs)
		m_LogList.m_ShowMask |= CGit::LOG_INFO_BASIC_REFS;
	else
		m_LogList.m_ShowMask&=~CGit::LOG_INFO_ALL_BRANCH;

	HandleShowLabels(m_bShowTags, LOGLIST_SHOWTAGS);
	HandleShowLabels(m_bShowLocalBranches, LOGLIST_SHOWLOCALBRANCHES);
	HandleShowLabels(m_bShowRemoteBranches, LOGLIST_SHOWREMOTEBRANCHES);

//	SetPromptParentWindow(m_hWnd);
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_AUTHOREMAIL)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_COMMITTEREMAIL)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_MERGEPOINT)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_PARENT1)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_PARENT2)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_TAG)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_TAG_FF)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_BRANCH)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_BRANCH_FF)));
	m_JumpType.AddString(CString(MAKEINTRESOURCE(IDS_PROC_SELECTION_HISTORY)));
	m_JumpType.SetCurSel(0);
	m_JumpUp.SetIcon((HICON)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_JUMPUP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
	m_JumpDown.SetIcon((HICON)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_JUMPDOWN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

	if (hWndExplorer)
		CenterWindow(CWnd::FromHandle(hWndExplorer));
	EnableSaveRestore(_T("LogDlg"));

	DWORD yPos1 = CRegDWORD(_T("Software\\TortoiseGit\\TortoiseProc\\ResizableState\\LogDlgSizer1"));
	DWORD yPos2 = CRegDWORD(_T("Software\\TortoiseGit\\TortoiseProc\\ResizableState\\LogDlgSizer2"));
	RECT rcDlg, rcLogList, rcChgMsg;
	GetClientRect(&rcDlg);
	m_LogList.GetWindowRect(&rcLogList);
	ScreenToClient(&rcLogList);
	m_ChangedFileListCtrl.GetWindowRect(&rcChgMsg);
	ScreenToClient(&rcChgMsg);
	if (yPos1 && ((LONG)yPos1 < rcDlg.bottom - 185))
	{
		RECT rectSplitter;
		m_wndSplitter1.GetWindowRect(&rectSplitter);
		ScreenToClient(&rectSplitter);
		int delta = yPos1 - rectSplitter.top;

		if ((rcLogList.bottom + delta > rcLogList.top)&&(rcLogList.bottom + delta < rcChgMsg.bottom - 30))
		{
			m_wndSplitter1.SetWindowPos(nullptr, rectSplitter.left, yPos1, 0, 0, SWP_NOSIZE);
			DoSizeV1(delta);
		}
	}
	if (yPos2 && ((LONG)yPos2 < rcDlg.bottom - 153))
	{
		RECT rectSplitter;
		m_wndSplitter2.GetWindowRect(&rectSplitter);
		ScreenToClient(&rectSplitter);
		int delta = yPos2 - rectSplitter.top;

		if ((rcChgMsg.top + delta < rcChgMsg.bottom)&&(rcChgMsg.top + delta > rcLogList.top + 30))
		{
			m_wndSplitter2.SetWindowPos(nullptr, rectSplitter.left, yPos2, 0, 0, SWP_NOSIZE);
			DoSizeV2(delta);
		}
	}

	SetSplitterRange();

	if (m_bSelect)
	{
		// the dialog is used to select revisions
		// enable the OK button if appropriate
		EnableOKButton();
	}
	else
	{
		// the dialog is used to just view log messages
		// hide the OK button and set text on Cancel button to OK
		GetDlgItemText(IDOK, temp);
		SetDlgItemText(IDCANCEL, temp);
		GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	}

	// first start a thread to obtain the log messages without
	// blocking the dialog
	//m_tTo = 0;
	//m_tFrom = (DWORD)-1;

	// scroll to user selected or current revision
	if (m_hightlightRevision.IsEmpty() || g_Git.GetHash(m_LogList.m_lastSelectedHash, m_hightlightRevision))
	{
		if (g_Git.GetHash(m_LogList.m_lastSelectedHash, _T("HEAD")))
			MessageBox(g_Git.GetGitLastErr(_T("Could not get HEAD hash.")), _T("TortoiseGit"), MB_ICONERROR);
	}

	if (g_Git.GetConfigValueBool(_T("tgit.logshowpatch")))
		TogglePatchView();

	m_LogList.FetchLogAsync(this);
	ShowGravatar();
	m_gravatar.Init();

	m_cFileFilter.SetCancelBitmaps(IDI_CANCELNORMAL, IDI_CANCELPRESSED);
	m_cFileFilter.SetInfoIcon(IDI_FILTEREDIT);
	temp.LoadString(IDS_FILEDIFF_FILTERCUE);
	temp = _T("   ") + temp;
	m_cFileFilter.SetCueBanner(temp);

	GetDlgItem(IDC_LOGLIST)->SetFocus();

	m_History.SetMaxHistoryItems((LONG)CRegDWORD(_T("Software\\TortoiseGit\\MaxRefHistoryItems"), 5));
	CString reg;
	reg.Format(_T("Software\\TortoiseGit\\History\\log-refs\\%s"), (LPCTSTR)g_Git.m_CurrentDir);
	reg.Replace(_T(':'),_T('_'));
	m_History.Load(reg, _T("ref"));

	reg.Format(_T("Software\\TortoiseGit\\History\\LogDlg_Limits\\%s\\FromDate"), (LPCTSTR)g_Git.m_CurrentDir);
	reg.Replace(_T(':'), _T('_'));
	m_regLastSelectedFromDate = CRegString(reg);

	ShowStartRef();
	return FALSE;
}

HBRUSH CLogDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);

	if ((m_LogList.m_Filter.m_NumberOfLogsScale >= CFilterData::SHOW_LAST_N_COMMITS || m_LogList.m_Filter.m_NumberOfLogsScale == CFilterData::SHOW_LAST_SEL_DATE && m_LogList.m_Filter.m_From != -1) && pWnd->GetDlgCtrlID() == IDC_FROMLABEL)
	{
		pDC->SetBkMode(TRANSPARENT);
		return m_Brush;
	}

	return hbr;
}

void CLogDlg::OnPaint()
{
	CPaintDC dc(this);

	if (!(m_LogList.m_Filter.m_NumberOfLogsScale >= CFilterData::SHOW_LAST_N_COMMITS || m_LogList.m_Filter.m_NumberOfLogsScale == CFilterData::SHOW_LAST_SEL_DATE && m_LogList.m_Filter.m_From != -1))
		return;
	
	CWnd* pWnd = GetDlgItem(IDC_FROMLABEL);
	if (!pWnd)
		return;

	CRect rect;
	pWnd->GetWindowRect(&rect);
	ScreenToClient(rect);
	rect.left -= 5;
	rect.top -= 3;
	rect.bottom += 3;

	dc.FillRect(rect, &m_Brush);
}

LRESULT CLogDlg::OnLogListLoading(WPARAM wParam, LPARAM /*lParam*/)
{
	int cur=(int)wParam;

	if( cur == GITLOG_START )
	{
		CString temp;
		temp.LoadString(IDS_PROGRESSWAIT);

		this->m_LogList.ShowText(temp, true);

		// We use a progress bar while getting the logs
		m_LogProgress.SetRange32(0, 100);
		m_LogProgress.SetPos(0);
		if (m_pTaskbarList)
		{
			m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NORMAL);
			m_pTaskbarList->SetProgressValue(m_hWnd, 0, 100);
		}

		GetDlgItem(IDC_WALKBEHAVIOUR)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_VIEW)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATBUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_PROGRESS)->ShowWindow(TRUE);

		DialogEnableWindow(IDC_WALKBEHAVIOUR, FALSE);
		DialogEnableWindow(IDC_STATBUTTON, FALSE);
		//DialogEnableWindow(IDC_REFRESH, FALSE);
		DialogEnableWindow(IDC_VIEW, FALSE);

	}
	else if( cur == GITLOG_END)
	{
		if(this->m_LogList.HasText())
		{
			this->m_LogList.ClearText();
		}
		UpdateLogInfoLabel();

		if (m_pTaskbarList)
			m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);

		DialogEnableWindow(IDC_WHOLE_PROJECT, !m_bFollowRenames && !m_path.IsEmpty());

		DialogEnableWindow(IDC_STATBUTTON, !(m_LogList.m_arShownList.empty() || m_LogList.m_arShownList.size() == 1 && m_LogList.m_bShowWC));
		DialogEnableWindow(IDC_REFRESH, TRUE);
		DialogEnableWindow(IDC_VIEW, TRUE);
		DialogEnableWindow(IDC_WALKBEHAVIOUR, TRUE);
//		PostMessage(WM_TIMER, LOGFILTER_TIMER);
		GetDlgItem(IDC_WALKBEHAVIOUR)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_VIEW)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATBUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_PROGRESS)->ShowWindow(FALSE);
		//CTime time=m_LogList.GetOldestTime();
		CTime begin,end;
		m_LogList.GetTimeRange(begin,end);

		if (m_LogList.m_Filter.m_From > 0 && m_LogList.m_Filter.m_NumberOfLogsScale >= CFilterData::SHOW_LAST_SEL_DATE)
			begin = m_LogList.m_Filter.m_From;
		m_DateFrom.SetTime(&begin);
		if (m_LogList.m_Filter.m_To != -1)
			end = m_LogList.m_Filter.m_To;
		m_DateTo.SetTime(&end);
		Invalidate();
	}
	else
	{
		if(this->m_LogList.HasText())
		{
			this->m_LogList.ClearText();
			this->m_LogList.Invalidate();
		}
		UpdateLogInfoLabel();
		m_LogProgress.SetPos(cur);
		if (m_pTaskbarList)
		{
			m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NORMAL);
			m_pTaskbarList->SetProgressValue(m_hWnd, cur, 100);
		}
	}
	return 0;
}
void CLogDlg::SetDlgTitle()
{
	if (m_sTitle.IsEmpty())
		GetWindowText(m_sTitle);

	if (m_LogList.m_Path.IsEmpty() || m_orgPath.GetWinPathString().IsEmpty())
		CAppUtils::SetWindowTitle(m_hWnd, g_Git.m_CurrentDir, m_sTitle);
	else
		CAppUtils::SetWindowTitle(m_hWnd, m_orgPath.GetWinPathString(), m_sTitle);
}

void CLogDlg::CheckRegexpTooltip()
{
	CWnd *pWnd = GetDlgItem(IDC_SEARCHEDIT);
	// Since tooltip describes regexp features, show it only if regexps are enabled.
	if (m_bFilterWithRegex)
		m_tooltips.AddTool(pWnd, IDS_LOG_FILTER_REGEX_TT);
	else
		m_tooltips.DelTool(pWnd);
}

void CLogDlg::EnableOKButton()
{
	if (m_bSelect)
	{
		// the dialog is used to select revisions
		if (m_bSelectionMustBeSingle)
		{
			// enable OK button if only a single revision is selected
			DialogEnableWindow(IDOK, (m_LogList.GetSelectedCount()==1));
		}
		else if (m_bSelectionMustBeContinuous)
			DialogEnableWindow(IDOK, (m_LogList.GetSelectedCount()!=0)&&(m_LogList.IsSelectionContinuous()));
		else
			DialogEnableWindow(IDOK, m_LogList.GetSelectedCount()!=0);
	}
	else
		DialogEnableWindow(IDOK, TRUE);
}

bool LookLikeGitHash(const CString& msg, int &pos)
{
	int c = 0;
	for (; pos < msg.GetLength(); ++pos)
	{
		if (msg[pos] >= '0' && msg[pos] <= '9' || msg[pos] >= 'a' && msg[pos] <= 'f')
			c++;
		else
			return c >= g_Git.GetShortHASHLength() && c <= GIT_HASH_SIZE * 2 && msg[pos] != '@';
	}
	return c >= g_Git.GetShortHASHLength() && c <= GIT_HASH_SIZE * 2;
}

std::vector<CHARRANGE> FindGitHashPositions(const CString& msg, int offset)
{
	std::vector<CHARRANGE> result;
	offset = offset < 0 ? 0 : offset;
	int old = offset;
	while (offset < msg.GetLength())
	{
		old = offset;
		TCHAR e = msg[offset];
		if (e == '\n')
		{
			++offset;
			if (msg.Mid(offset, 11) == _T("git-svn-id:")
				|| msg.Mid(offset, 14) == _T("Signed-off-by:")
				|| msg.Mid(offset, 10) == _T("Change-Id:")
			)
			{
				offset += 10;
				while (offset < msg.GetLength())
				{
					if (msg[++offset] == '\n')
						break;
				}
				continue;
			}
		}
		else if (e >= 'A' && e <= 'Z' || e >= 'h' && e <= 'z')
		{
			do
			{
				e = msg[++offset];
			} while (offset < msg.GetLength() && (e >= 'A' && e <= 'Z' || e >= 'a' && e <= 'z' || e >= '0' && e <= '9'));
		}
		else if (e >= 'a' && e <= 'g' || e >= '0' && e <= '9')
		{
			if (e == 'g')
			{
				++old;
				++offset;
			}
			if (LookLikeGitHash(msg, offset))
			{
				TCHAR d = offset < msg.GetLength() ? msg[offset] : '\0';
				if (!((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <= '9')))
				{
					CHARRANGE range = { old, offset };
					result.push_back(range);
				}
			}
			++offset;
		}
		else
			++offset;
	}

	return result;
}

BOOL FindGitHash(const CString& msg, int offset, CWnd *pWnd)
{
	std::vector<CHARRANGE> positions = FindGitHashPositions(msg, offset);
	CAppUtils::SetCharFormat(pWnd, CFM_LINK, CFE_LINK, positions);

	return positions.empty() ? FALSE : TRUE;
}

static int DescribeCommit(CGitHash& hash, CString& result)
{
	CAutoRepository repo(g_Git.GetGitRepository());
	if (!repo)
		return -1;
	CAutoObject commit;
	if (git_object_lookup(commit.GetPointer(), repo, (const git_oid *)hash.m_hash, GIT_OBJ_COMMIT))
		return -1;

	CAutoDescribeResult describe;
	git_describe_options describe_options = GIT_DESCRIBE_OPTIONS_INIT;
	describe_options.describe_strategy = CRegDWORD(_T("Software\\TortoiseGit\\DescribeStrategy"), GIT_DESCRIBE_DEFAULT);
	if (git_describe_commit(describe.GetPointer(), (git_object *)commit, &describe_options))
		return -1;

	CAutoBuf describe_buf;
	git_describe_format_options format_options = GIT_DESCRIBE_FORMAT_OPTIONS_INIT;
	format_options.abbreviated_size = CRegDWORD(_T("Software\\TortoiseGit\\DescribeAbbreviatedSize"), GIT_DESCRIBE_DEFAULT_ABBREVIATED_SIZE);
	format_options.always_use_long_format = CRegDWORD(_T("Software\\TortoiseGit\\DescribeAlwaysLong"));
	if (git_describe_format(describe_buf, describe, &format_options))
		return -1;

	result = CUnicodeUtils::GetUnicode(describe_buf->ptr);
	return 0;
}

void CLogDlg::FillLogMessageCtrl(bool bShow /* = true*/)
{
	// we fill here the log message rich edit control,
	// and also populate the changed files list control
	// according to the selected revision(s).

	CRichEditCtrl * pMsgView = (CRichEditCtrl*)GetDlgItem(IDC_MSGVIEW);
	// empty the log message view
	pMsgView->SetWindowText(_T(" "));
	FillPatchView(true);
	// empty the changed files list
	m_ChangedFileListCtrl.SetRedraw(FALSE);
//	InterlockedExchange(&m_bNoDispUpdates, TRUE);
	m_ChangedFileListCtrl.DeleteAllItems();

	// if we're not here to really show a selected revision, just
	// get out of here after clearing the views, which is what is intended
	// if that flag is not set.
	if (!bShow)
	{
		// force a redraw
		m_ChangedFileListCtrl.Invalidate();
//		InterlockedExchange(&m_bNoDispUpdates, FALSE);
		m_ChangedFileListCtrl.SetRedraw(TRUE);
		m_gravatar.LoadGravatar();
		return;
	}

	// depending on how many revisions are selected, we have to do different
	// tasks.
	int selCount = m_LogList.GetSelectedCount();
	if (selCount == 0)
	{
		// if nothing is selected, we have nothing more to do
//		InterlockedExchange(&m_bNoDispUpdates, FALSE);
		m_ChangedFileListCtrl.SetRedraw(TRUE);
		m_gravatar.LoadGravatar();
		return;
	}
	else if (selCount == 1)
	{
		// if one revision is selected, we have to fill the log message view
		// with the corresponding log message, and also fill the changed files
		// list fully.
		POSITION pos = m_LogList.GetFirstSelectedItemPosition();
		size_t selIndex = m_LogList.GetNextSelectedItem(pos);
		if (selIndex >= m_LogList.m_arShownList.size())
		{
//			InterlockedExchange(&m_bNoDispUpdates, FALSE);
			m_ChangedFileListCtrl.SetRedraw(TRUE);
			return;
		}
		GitRevLoglist* pLogEntry = m_LogList.m_arShownList.SafeGetAt(selIndex);

		{
			m_gravatar.LoadGravatar(pLogEntry->GetAuthorEmail());

			CString out_describe;
			if (m_bShowDescribe)
			{
				CString result;
				if (!DescribeCommit(pLogEntry->m_CommitHash, result))
					out_describe = _T("Describe: ") + result + _T("\r\n");
			}

			CString out_counter;
			if (m_bShowBranchRevNo && !pLogEntry->m_CommitHash.IsEmpty())
			{
				bool isFirstParentCommit = !pLogEntry->m_Lanes.empty() && Lanes::isActive(pLogEntry->m_Lanes[0]);

				if (isFirstParentCommit)
				{
					CString rev_counter, rev_err;
					if (g_Git.Run(_T("git.exe rev-list --count --first-parent " + pLogEntry->m_CommitHash.ToString()), &rev_counter, &rev_err, CP_UTF8))
						CMessageBox::Show(GetSafeHwnd(), L"Could not get rev count\n" + rev_counter + L"\n" + rev_err, _T("TortoiseGit"), MB_ICONERROR);
					else
						out_counter = _T(", ") + CString(MAKEINTRESOURCE(IDS_REV_COUNTER)) + _T(": ") + rev_counter.Trim();
				}
			}

			// set the log message text
			pMsgView->SetWindowText(CString(MAKEINTRESOURCE(IDS_HASH)) + _T(": ") + pLogEntry->m_CommitHash.ToString() + out_counter + _T("\r\n") + out_describe + _T("\r\n"));
			// turn bug ID's into links if the bugtraq: properties have been set
			// and we can find a match of those in the log message

			pMsgView->SetSel(-1,-1);
			CAppUtils::SetCharFormat(pMsgView, CFM_BOLD, CFE_BOLD);

			CString msg = m_bAsteriskLogPrefix ? _T("* ") : _T("");
			msg+=pLogEntry->GetSubject();
			pMsgView->ReplaceSel(msg);

			pMsgView->SetSel(-1,-1);
			CAppUtils::SetCharFormat(pMsgView, CFM_BOLD, 0);

			msg=_T("\n");
			msg+=pLogEntry->GetBody();

			if(!pLogEntry->m_Notes.IsEmpty())
			{
				msg+= _T("\n*") + CString(MAKEINTRESOURCE(IDS_NOTES)) + _T("* ");
				msg+= pLogEntry->m_Notes;
				msg+= _T("\n\n");
			}

			CString tagInfo = m_LogList.GetTagInfo(pLogEntry);
			if(!tagInfo.IsEmpty())
				tagInfo = _T("\n*") + CString(MAKEINTRESOURCE(IDS_PROC_LOG_TAGINFO)) + _T("*\n\n") + tagInfo;
			msg += tagInfo;

			HCURSOR old = GetCursor(); // hack to fix issue #2792
			pMsgView->ReplaceSel(msg);
			SetCursor(old);

			CString text;
			pMsgView->GetWindowText(text);
			// the rich edit control doesn't count the CR char!
			// to be exact: CRLF is treated as one char.
			text.Remove('\r');

			int findHashStart = text.Find('\n');
			if (!out_describe.IsEmpty())
				findHashStart = text.Find('\n', findHashStart + 1);
			FindGitHash(text, findHashStart, pMsgView);
			m_LogList.m_ProjectProperties.FindBugID(text, pMsgView);
			CAppUtils::StyleURLs(text, pMsgView);
			if (((DWORD)CRegStdDWORD(_T("Software\\TortoiseGit\\StyleCommitMessages"), TRUE)) == TRUE)
				CAppUtils::FormatTextInRichEditControl(pMsgView);

			CHARRANGE range;
			range.cpMin = 0;
			range.cpMax = 0;
			pMsgView->SendMessage(EM_EXSETSEL, NULL, (LPARAM)&range);

			if (!pLogEntry->m_IsDiffFiles)
			{
				m_ChangedFileListCtrl.SetBusyString(CString(MAKEINTRESOURCE(IDS_PROC_LOG_FETCHINGFILES)));
				m_ChangedFileListCtrl.SetBusy(TRUE);
				m_ChangedFileListCtrl.SetRedraw(TRUE);
				return;
			}

			CString matchpath=this->m_path.GetGitPathString();

			CTGitPathList& files = pLogEntry->GetFiles(&m_LogList);
			int count = files.GetCount();
			if (!m_bWholeProject && !matchpath.IsEmpty() && m_iHidePaths)
			{
				int matchPathLen = matchpath.GetLength();
				bool somethingHidden = false;
				for (int i = 0; i < count; ++i)
				{
					((CTGitPath&)files[i]).m_Action &= ~(CTGitPath::LOGACTIONS_HIDE | CTGitPath::LOGACTIONS_GRAY);

					if (files[i].GetGitPathString().Left(matchPathLen) != matchpath && ((files[i].m_Action & (CTGitPath::LOGACTIONS_REPLACED | CTGitPath::LOGACTIONS_COPY)) == 0 || files[i].GetGitOldPathString().Left(matchPathLen) != matchpath))
					{
						somethingHidden = true;
						if (m_iHidePaths == 1)
							((CTGitPath&)files[i]).m_Action |= CTGitPath::LOGACTIONS_HIDE;
						else if (m_iHidePaths == 2)
							((CTGitPath&)files[i]).m_Action |= CTGitPath::LOGACTIONS_GRAY;
					}
				}
				if (somethingHidden)
					pLogEntry->GetAction(&m_LogList) |= CTGitPath::LOGACTIONS_HIDE;
				else
					pLogEntry->GetAction(&m_LogList) &= ~CTGitPath::LOGACTIONS_HIDE;
			}
			else if (pLogEntry->GetAction(&m_LogList) & CTGitPath::LOGACTIONS_HIDE)
			{
				pLogEntry->GetAction(&m_LogList) &= ~CTGitPath::LOGACTIONS_HIDE;
				for (int i = 0 ; i < count; ++i)
					((CTGitPath&)files[i]).m_Action &= ~(CTGitPath::LOGACTIONS_HIDE | CTGitPath::LOGACTIONS_GRAY);
			}

			CString fileFilter;
			m_cFileFilter.GetWindowText(fileFilter);
			if (!fileFilter.IsEmpty())
			{
				fileFilter.MakeLower();
				bool somethingHidden = false;
				for (int i = 0; i < count; ++i)
				{
					CString sPath(files[i].GetGitPathString());
					sPath.MakeLower();
					if (sPath.Find(fileFilter) < 0)
					{
						((CTGitPath&)files[i]).m_Action |= CTGitPath::LOGACTIONS_HIDE;
						somethingHidden = true;
					}
				}
				if (somethingHidden)
					pLogEntry->GetAction(&m_LogList) |= CTGitPath::LOGACTIONS_HIDE;
			}

			m_ChangedFileListCtrl.UpdateWithGitPathList(files);
			m_ChangedFileListCtrl.m_CurrentVersion=pLogEntry->m_CommitHash;
			if (pLogEntry->m_CommitHash.IsEmpty() && m_bShowUnversioned)
			{
				m_ChangedFileListCtrl.UpdateUnRevFileList(pLogEntry->GetUnRevFiles());
				m_ChangedFileListCtrl.Show(GITSLC_SHOWVERSIONED | GITSLC_SHOWUNVERSIONED);
			}
			else
				m_ChangedFileListCtrl.Show(GITSLC_SHOWVERSIONED);

			m_ChangedFileListCtrl.SetBusy(FALSE);

			m_ChangedFileListCtrl.SetRedraw(TRUE);
			return;
		}

	}
	else
	{
		// more than one revision is selected:
		// the log message view must be emptied
		// the changed files list contains all the changed paths from all
		// selected revisions, with 'doubles' removed
		m_gravatar.LoadGravatar();
	}

	// redraw the views
//	InterlockedExchange(&m_bNoDispUpdates, FALSE);

	// sort according to the settings
	if (m_nSortColumnPathList > 0)
		SetSortArrow(&m_ChangedFileListCtrl, m_nSortColumnPathList, m_bAscendingPathList);
	else
		SetSortArrow(&m_ChangedFileListCtrl, -1, false);
	m_ChangedFileListCtrl.SetRedraw(TRUE);
}

void CLogDlg::FillPatchView(bool onlySetTimer)
{
	if (!::IsWindow(this->m_patchViewdlg.m_hWnd))
		return;

	KillTimer(LOG_FILLPATCHVTIMER);
	if (onlySetTimer)
	{
		SetTimer(LOG_FILLPATCHVTIMER, 100, nullptr);
		return;
	}

	POSITION posLogList = m_LogList.GetFirstSelectedItemPosition();
	if (posLogList == nullptr)
	{
		m_patchViewdlg.ClearView();
		return; // nothing is selected, get out of here
	}

	GitRev* pLogEntry = m_LogList.m_arShownList.SafeGetAt(m_LogList.GetNextSelectedItem(posLogList));
	if (pLogEntry == nullptr || m_LogList.GetNextSelectedItem(posLogList) != -1)
	{
		m_patchViewdlg.ClearView();
		return;
	}

	POSITION pos = m_ChangedFileListCtrl.GetFirstSelectedItemPosition();
	CString out;

	if (pos == nullptr)
	{
		int diffContext = g_Git.GetConfigValueInt32(_T("diff.context"), -1);
		CStringA outA;
		CString rev1 = pLogEntry->m_CommitHash.IsEmpty() ? _T("HEAD") : (pLogEntry->m_CommitHash.ToString() + _T("~1"));
		CString rev2 = pLogEntry->m_CommitHash.IsEmpty() ? GIT_REV_ZERO : pLogEntry->m_CommitHash.ToString();
		g_Git.GetUnifiedDiff(CTGitPath(), rev1, rev2, &outA, false, false, diffContext);
		out = CUnicodeUtils::GetUnicode(outA);
	}
	else
	{
		while (pos)
		{
			int nSelect = m_ChangedFileListCtrl.GetNextSelectedItem(pos);
			CTGitPath * p = (CTGitPath*)m_ChangedFileListCtrl.GetItemData(nSelect);
			if (p && !(p->m_Action&CTGitPath::LOGACTIONS_UNVER))
			{
				CString cmd;
				if (pLogEntry->m_CommitHash.IsEmpty())
					cmd.Format(_T("git.exe diff HEAD -- \"%s\""), (LPCTSTR)p->GetGitPathString());
				else
					cmd.Format(_T("git.exe diff %s^%d..%s -- \"%s\""), (LPCTSTR)pLogEntry->m_CommitHash.ToString(), p->m_ParentNo + 1, (LPCTSTR)pLogEntry->m_CommitHash.ToString(), (LPCTSTR)p->GetGitPathString());
				g_Git.Run(cmd, &out, CP_UTF8);
			}
		}
	}

	m_patchViewdlg.SetText(out);
}

void CLogDlg::TogglePatchView()
{
	m_patchViewdlg.m_ParentDlg = this;
	if (!IsWindow(m_patchViewdlg.m_hWnd))
	{
		if (g_Git.GetConfigValueBool(L"tgit.logshowpatch") == FALSE)
			g_Git.SetConfigValue(_T("tgit.logshowpatch"), _T("true"));
		m_patchViewdlg.Create(IDD_PATCH_VIEW, this);
		m_patchViewdlg.m_ctrlPatchView.Call(SCI_SETSCROLLWIDTHTRACKING, TRUE);
		CRect rect;
		this->GetWindowRect(&rect);

		m_patchViewdlg.ShowWindow(SW_SHOW);
		m_patchViewdlg.SetWindowPos(nullptr, rect.right, rect.top, rect.Width(), rect.Height(), SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

		FillPatchView();
	}
	else
	{
		g_Git.SetConfigValue(_T("tgit.logshowpatch"), _T("false"));
		m_patchViewdlg.ShowWindow(SW_HIDE);
		m_patchViewdlg.DestroyWindow();
	}
}

LRESULT CLogDlg::OnFileListCtrlItemChanged(WPARAM /*wparam*/, LPARAM /*lparam*/)
{
	FillPatchView(true);
	return 0;
}

void CLogDlg::OnMoving(UINT fwSide, LPRECT pRect)
{
	__super::OnMoving(fwSide, pRect);

	if (!::IsWindow(m_patchViewdlg.m_hWnd))
		return;

	RECT patchrect;
	m_patchViewdlg.GetWindowRect(&patchrect);
	if (!::IsWindow(m_hWnd))
		return;

	RECT thisrect;
	GetWindowRect(&thisrect);
	if (patchrect.left == thisrect.right)
	{
		m_patchViewdlg.SetWindowPos(nullptr, patchrect.left - (thisrect.left - pRect->left), patchrect.top - (thisrect.top - pRect->top), 0, 0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
	}
}

void CLogDlg::OnSizing(UINT fwSide, LPRECT pRect)
{
	__super::OnSizing(fwSide, pRect);

	if (!::IsWindow(this->m_patchViewdlg.m_hWnd))
		return;

	CRect thisrect, patchrect;
	this->GetWindowRect(thisrect);
	this->m_patchViewdlg.GetWindowRect(patchrect);
	if (thisrect.right != patchrect.left)
		return;

	patchrect.left -= (thisrect.right - pRect->right);
	patchrect.right -= (thisrect.right - pRect->right);

	if (patchrect.bottom == thisrect.bottom)
		patchrect.bottom -= (thisrect.bottom - pRect->bottom);
	if (patchrect.top == thisrect.top)
		patchrect.top -= thisrect.top - pRect->top;
	m_patchViewdlg.MoveWindow(patchrect);
}

void CLogDlg::OnSelectSearchField()
{
	m_cFilter.SetSel(0, -1, FALSE);
	m_cFilter.SetFocus();
}

void CLogDlg::GoBack()
{
	GoBackForward(false, false);
}

void CLogDlg::GoForward()
{
	GoBackForward(false, true);
}

void CLogDlg::GoBackAndSelect()
{
	GoBackForward(true, false);
}

void CLogDlg::GoForwardAndSelect()
{
	GoBackForward(true, true);
}

void CLogDlg::GoBackForward(bool select, bool bForward)
{
	m_LogList.m_highlight.Empty();
	CGitHash gotoHash;
	if (bForward ? m_LogList.m_selectionHistory.GoForward(gotoHash) : m_LogList.m_selectionHistory.GoBack(gotoHash))
	{
		int i;
		for (i = 0; i < (int)m_LogList.m_arShownList.size(); ++i)
		{
			GitRev* rev = m_LogList.m_arShownList.SafeGetAt(i);
			if (!rev) continue;
			if (rev->m_CommitHash == gotoHash)
			{
				m_LogList.EnsureVisible(i, FALSE);
				if (select)
				{
					m_LogList.m_highlight.Empty();
					m_LogList.SetItemState(m_LogList.GetSelectionMark(), 0, LVIS_SELECTED | LVIS_FOCUSED);
					POSITION pos = m_LogList.GetFirstSelectedItemPosition();
					while (pos)
					{
						int index = m_LogList.GetNextSelectedItem(pos);
						if (index >= 0)
							m_LogList.SetItemState(index, 0, LVIS_SELECTED);
					}
					m_bNavigatingWithSelect = true;
					m_LogList.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					m_LogList.SetSelectionMark(i);
					m_bNavigatingWithSelect = false;
				}
				else
					m_LogList.m_highlight = gotoHash;
				m_LogList.Invalidate();
				return;
			}
		}
		if (i == (int)m_LogList.m_arShownList.size())
		{
			CString msg;
			msg.Format(IDS_LOG_NOT_VISIBLE, (LPCTSTR)gotoHash.ToString());
			MessageBox(msg, _T("TortoiseGit"), MB_OK | MB_ICONINFORMATION);
			return;
		}
	}
	PlaySound((LPCTSTR)SND_ALIAS_SYSTEMASTERISK, nullptr, SND_ASYNC | SND_ALIAS_ID);
}

void CLogDlg::OnBnClickedRefresh()
{
	Refresh (true);
}

void CLogDlg::Refresh (bool clearfilter /*autoGoOnline*/)
{
	if (m_bSelect)
		DialogEnableWindow(IDOK, FALSE);
	m_LogList.Refresh(clearfilter);
	EnableOKButton();
	ShowStartRef();
	FillLogMessageCtrl(false);
}



BOOL CLogDlg::Cancel()
{
	return m_bCancelled;
}

void CLogDlg::SaveSplitterPos()
{
	if (!IsIconic())
	{
		CRegDWORD regPos1(_T("Software\\TortoiseGit\\TortoiseProc\\ResizableState\\LogDlgSizer1"));
		CRegDWORD regPos2(_T("Software\\TortoiseGit\\TortoiseProc\\ResizableState\\LogDlgSizer2"));
		RECT rectSplitter;
		m_wndSplitter1.GetWindowRect(&rectSplitter);
		ScreenToClient(&rectSplitter);
		regPos1 = rectSplitter.top;
		m_wndSplitter2.GetWindowRect(&rectSplitter);
		ScreenToClient(&rectSplitter);
		regPos2 = rectSplitter.top;
	}
}

void CLogDlg::OnCancel()
{
	this->ShowWindow(SW_HIDE);

	// canceling means stopping the working thread if it's still running.
	m_LogList.SafeTerminateAsyncDiffThread();
	if (this->IsThreadRunning())
	{
		m_LogList.SafeTerminateThread();
	}
	UpdateData();

	SaveSplitterPos();
	__super::OnCancel();
}

void CLogDlg::CopyChangedSelectionToClipBoard()
{
	POSITION posLogList = m_LogList.GetFirstSelectedItemPosition();
	if (posLogList == nullptr)
		return;	// nothing is selected, get out of here

	CString sPaths;

//	CGitRev* pLogEntry = reinterpret_cast<CGitRev* >(m_LogList.m_arShownList.SafeGetAt(m_LogList.GetNextSelectedItem(pos)));
//	if (posLogList)
	{
		POSITION pos = m_ChangedFileListCtrl.GetFirstSelectedItemPosition();
		while (pos)
		{
			int nItem = m_ChangedFileListCtrl.GetNextSelectedItem(pos);
			CTGitPath *path = (CTGitPath*)m_ChangedFileListCtrl.GetItemData(nItem);
			if(path)
				sPaths += path->GetGitPathString();
			sPaths += _T("\r\n");
		}
	}
#if 0
	else
	{
		// only one revision is selected in the log dialog top pane
		// but multiple items could be selected  in the changed items list
		POSITION pos = m_ChangedFileListCtrl.GetFirstSelectedItemPosition();
		while (pos)
		{
			int nItem = m_ChangedFileListCtrl.GetNextSelectedItem(pos);
			LogChangedPath * changedlogpath = pLogEntry->pArChangedPaths->SafeGetAt(nItem);

			if ((m_cHidePaths.GetState() & 0x0003)==BST_CHECKED)
			{
				// some items are hidden! So find out which item the user really selected
				INT_PTR selRealIndex = -1;
				for (INT_PTR hiddenindex=0; hiddenindex<pLogEntry->pArChangedPaths->GetCount(); ++hiddenindex)
				{
					if (pLogEntry->pArChangedPaths->SafeGetAt(hiddenindex)->sPath.Left(m_sRelativeRoot.GetLength()).Compare(m_sRelativeRoot)==0)
						++selRealIndex;
					if (selRealIndex == nItem)
					{
						changedlogpath = pLogEntry->pArChangedPaths->SafeGetAt(hiddenindex);
						break;
					}
				}
			}
			if (changedlogpath)
			{
				sPaths += changedlogpath->sPath;
				sPaths += _T("\r\n");
			}
		}
	}
#endif
	sPaths.Trim();
	CStringUtils::WriteAsciiStringToClipboard(sPaths, GetSafeHwnd());
}

void CLogDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
	// we have 4 separate context menus:
	// one for the branch label in the upper left
	// one for FROM date control
	// one shown on the log message list control,
	// one shown in the changed-files list control
	if (pWnd == GetDlgItem(IDC_STATIC_REF))
	{
		CIconMenu popup;
		if (!popup.CreatePopupMenu())
			return;

		int cnt = 0;
		popup.AppendMenuIcon(++cnt, IDS_MENUREFBROWSE);
		popup.SetDefaultItem(cnt);
		popup.AppendMenuIcon(++cnt, _T("HEAD"));
		CGitHash fetchHead;
		g_Git.GetHash(fetchHead, g_Git.FixBranchName(_T("FETCH_HEAD")));
		popup.AppendMenuIcon(++cnt, _T("FETCH_HEAD"));
		popup.EnableMenuItem(cnt, fetchHead.IsEmpty());
		popup.AppendMenuIcon(++cnt, IDS_ALL);
		popup.EnableMenuItem(cnt, m_bFollowRenames);
		popup.AppendMenuIcon(++cnt, IDS_PROC_LOG_SELECT_BASIC_REFS);
		popup.EnableMenuItem(cnt, m_bFollowRenames);
		popup.AppendMenuIcon(++cnt, IDS_PROC_LOG_SELECT_LOCAL_BRANCHES);
		popup.EnableMenuItem(cnt, m_bFollowRenames);
		int offset = ++cnt;
		if (m_History.GetCount() > 0)
		{
			popup.AppendMenu(MF_SEPARATOR, 0);
			for (size_t i = 0; i < m_History.GetCount(); ++i)
			{
				CString entry = m_History.GetEntry(i);
				if (entry.GetLength() > 150)
					entry = entry.Left(150) + _T("...");
				popup.AppendMenuIcon(cnt++, entry);
			}
		}

		int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
		if (cmd == 0)
			return;
		else if (cmd == 1)
		{
			OnBnClickedBrowseRef();
			return;
		}

		m_LogList.m_ShowMask &= ~(CGit::LOG_INFO_ALL_BRANCH | CGit::LOG_INFO_BASIC_REFS | CGit::LOG_INFO_LOCAL_BRANCHES);
		m_bAllBranch = BST_UNCHECKED;
		m_AllBranchType = AllBranchType::None;
		if (cmd == 2)
		{
			SetRange(g_Git.GetCurrentBranch(true));
		}
		else if (cmd == 3)
		{
			SetRange(fetchHead.ToString());
		}
		else if (cmd == 4)
		{
			m_bAllBranch = BST_CHECKED;
			m_AllBranchType = AllBranchType::AllBranches;
			m_LogList.m_ShowMask |= CGit::LOG_INFO_ALL_BRANCH;
		}
		else if (cmd == 5)
		{
			m_bAllBranch = BST_INDETERMINATE;
			m_AllBranchType = AllBranchType::AllBasicRefs;
			m_LogList.m_ShowMask |= CGit::LOG_INFO_BASIC_REFS;
		}
		else if (cmd == 6)
		{
			m_bAllBranch = BST_INDETERMINATE;
			m_AllBranchType = AllBranchType::AllLocalBranches;
			m_LogList.m_ShowMask |= CGit::LOG_INFO_LOCAL_BRANCHES;
		}
		else if (cmd >= offset)
		{
			SetRange(m_History.GetEntry(cmd - offset));
			m_History.AddEntry(m_LogList.m_sRange);
			m_History.Save();
		}

		UpdateData(FALSE);

		OnRefresh();
		FillLogMessageCtrl(false);

		return;
	}

	if (pWnd == GetDlgItem(IDC_DATEFROM))
	{
		CIconMenu popup;
		if (!popup.CreatePopupMenu())
			return;

		int cnt = 0;

		popup.AppendMenuIcon(++cnt, IDS_NO_LIMIT);

		DWORD scale = (DWORD)CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\NumberOfLogsScale"), CFilterData::SHOW_NO_LIMIT);
		CString strScale;
		switch (scale)
		{
		case CFilterData::SHOW_LAST_N_COMMITS:
			strScale.LoadString(IDS_LAST_N_COMMITS);
			break;
		case CFilterData::SHOW_LAST_N_YEARS:
			strScale.LoadString(IDS_LAST_N_YEARS);
			break;
		case CFilterData::SHOW_LAST_N_MONTHS:
			strScale.LoadString(IDS_LAST_N_MONTHS);
			break;
		case CFilterData::SHOW_LAST_N_WEEKS:
			strScale.LoadString(IDS_LAST_N_WEEKS);
			break;
		}
		if (!strScale.IsEmpty() && m_LogList.m_Filter.m_NumberOfLogs > 0)
		{
			popup.AppendMenu(MF_SEPARATOR, 0);
			CString number;
			number.Format(L"%ld", m_LogList.m_Filter.m_NumberOfLogs);
			CString item;
			item.Format(strScale, (LPCTSTR)number);
			popup.AppendMenuIcon(++cnt, item);
		}

		int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
		if (cmd <= 0)
			return;
		else if (cmd == 1)
		{
			m_LogList.m_Filter.m_NumberOfLogsScale = CFilterData::SHOW_NO_LIMIT;
			// reset last selected date
			m_regLastSelectedFromDate.removeValue();
		}
		else if (cmd == 2)
			m_LogList.m_Filter.m_NumberOfLogsScale = scale;

		UpdateData(FALSE);
		OnRefresh();
		FillLogMessageCtrl(false);

		return;
	}

	int selCount = m_LogList.GetSelectedCount();
	if ((selCount == 1)&&(pWnd == GetDlgItem(IDC_MSGVIEW)))
	{
		POSITION pos = m_LogList.GetFirstSelectedItemPosition();
		int selIndex = -1;
		if (pos)
			selIndex = m_LogList.GetNextSelectedItem(pos);

		GitRevLoglist* pRev = nullptr;
		if (selIndex >= 0)
			pRev = m_LogList.m_arShownList.SafeGetAt(selIndex);

		if ((point.x == -1) && (point.y == -1))
		{
			CRect rect;
			GetDlgItem(IDC_MSGVIEW)->GetClientRect(&rect);
			ClientToScreen(&rect);
			point = rect.CenterPoint();
		}
		CString sMenuItemText;
		CIconMenu popup;
		if (popup.CreatePopupMenu())
		{
			long start = -1, end = -1;
			auto pEdit = (CRichEditCtrl *)GetDlgItem(IDC_MSGVIEW);
			pEdit->GetSel(start, end);
			// add the 'default' entries
			popup.AppendMenuIcon(WM_COPY, IDS_SCIEDIT_COPY, IDI_COPYCLIP);
			if (start >= end)
				popup.EnableMenuItem(WM_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			popup.AppendMenu(MF_SEPARATOR);
			sMenuItemText.LoadString(IDS_STATUSLIST_CONTEXT_COPYEXT);
			popup.AppendMenuIcon(EM_SETSEL, sMenuItemText, IDI_COPYCLIP);
			if (pRev && !pRev->m_CommitHash.IsEmpty())
			{
				popup.AppendMenu(MF_SEPARATOR, NULL);
				sMenuItemText.LoadString(IDS_EDIT_NOTES);
				popup.AppendMenuIcon(CGitLogList::ID_EDITNOTE, sMenuItemText, IDI_EDIT);
			}

			int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
			switch (cmd)
			{
			case 0:
				break;	// no command selected
			case EM_SETSEL:
				{
					pEdit->SetRedraw(FALSE);
					int oldLine = pEdit->GetFirstVisibleLine();
					pEdit->SetSel(0, -1);
					pEdit->Copy();
					pEdit->SetSel(start, end);
					int newLine = pEdit->GetFirstVisibleLine();
					pEdit->LineScroll(oldLine - newLine);
					pEdit->SetRedraw(TRUE);
					pEdit->RedrawWindow();
				}
				break;
			case WM_COPY:
				::SendMessage(GetDlgItem(IDC_MSGVIEW)->GetSafeHwnd(), cmd, 0, -1);
				break;
			case CGitLogList::ID_EDITNOTE:
				CAppUtils::EditNote(pRev);
				this->FillLogMessageCtrl(true);
				break;
			}
		}
	}
}

void CLogDlg::OnOK()
{
	// since the log dialog is also used to select revisions for other
	// dialogs, we have to do some work before closing this dialog
	if (GetFocus() != GetDlgItem(IDOK))
		return;	// if the "OK" button doesn't have the focus, do nothing: this prevents closing the dialog when pressing enter

	m_LogList.SafeTerminateAsyncDiffThread();
	if (this->IsThreadRunning())
	{
		m_LogList.SafeTerminateThread();
	}
	UpdateData();
	// get the selected row(s)
	POSITION pos = m_LogList.GetFirstSelectedItemPosition();
	while (pos)
	{
		int selIndex = m_LogList.GetNextSelectedItem(pos);
		GitRev* pLogEntry = m_LogList.m_arShownList.SafeGetAt(selIndex);
		if (!pLogEntry)
			continue;
		m_sSelectedHash.push_back(pLogEntry->m_CommitHash);
	}
	UpdateData(FALSE);
	SaveSplitterPos();
	__super::OnOK();

	#if 0
	if (!GetDlgItem(IDOK)->IsWindowVisible() && GetFocus() != GetDlgItem(IDCANCEL))
		return; // the Cancel button works as the OK button. But if the cancel button has not the focus, do nothing.

	CString temp;
	CString buttontext;
	GetDlgItemText(IDOK, buttontext);
	temp.LoadString(IDS_MSGBOX_CANCEL);
	if (temp.Compare(buttontext) != 0)
		__super::OnOK();	// only exit if the button text matches, and that will match only if the thread isn't running anymore
	m_bCancelled = TRUE;
	m_selectedRevs.Clear();
	m_selectedRevsOneRange.Clear();
	if (m_pNotifyWindow)
	{
		int selIndex = m_LogList.GetSelectionMark();
		if (selIndex >= 0)
		{
			PLOGENTRYDATA pLogEntry = nullptr;
			POSITION pos = m_LogList.GetFirstSelectedItemPosition();
			pLogEntry = reinterpret_cast<PLOGENTRYDATA>(m_arShownList.SafeGetAt(m_LogList.GetNextSelectedItem(pos)));
			m_selectedRevs.AddRevision(pLogEntry->Rev);
			git_revnum_t lowerRev = pLogEntry->Rev;
			git_revnum_t higherRev = lowerRev;
			while (pos)
			{
				pLogEntry = reinterpret_cast<PLOGENTRYDATA>(m_arShownList.SafeGetAt(m_LogList.GetNextSelectedItem(pos)));
				git_revnum_t rev = pLogEntry->Rev;
				m_selectedRevs.AddRevision(pLogEntry->Rev);
				if (lowerRev > rev)
					lowerRev = rev;
				if (higherRev < rev)
					higherRev = rev;
			}
			if (m_sFilterText.IsEmpty() && m_nSortColumn == 0 && IsSelectionContinuous())
			{
				m_selectedRevsOneRange.AddRevRange(lowerRev, higherRev);
			}
			BOOL bSentMessage = FALSE;
			if (m_LogList.GetSelectedCount() == 1)
			{
				// if only one revision is selected, check if the path/url with which the dialog was started
				// was directly affected in that revision. If it was, then check if our path was copied from somewhere.
				// if it was copied, use the copy from revision as lowerRev
				if ((pLogEntry)&&(pLogEntry->pArChangedPaths)&&(lowerRev == higherRev))
				{
					CString sUrl = m_path.GetGitPathString();
					if (!m_path.IsUrl())
					{
						sUrl = GetURLFromPath(m_path);
					}
					sUrl = sUrl.Mid(m_sRepositoryRoot.GetLength());
					for (int cp = 0; cp < pLogEntry->pArChangedPaths->GetCount(); ++cp)
					{
						LogChangedPath * pData = pLogEntry->pArChangedPaths->SafeGetAt(cp);
						if (pData)
						{
							if (sUrl.Compare(pData->sPath) == 0)
							{
								if (!pData->sCopyFromPath.IsEmpty())
								{
									lowerRev = pData->lCopyFromRev;
									m_pNotifyWindow->SendMessage(WM_REVSELECTED, m_wParam & (MERGE_REVSELECTSTART), lowerRev);
									m_pNotifyWindow->SendMessage(WM_REVSELECTED, m_wParam & (MERGE_REVSELECTEND), higherRev);
									m_pNotifyWindow->SendMessage(WM_REVLIST, m_selectedRevs.GetCount(), (LPARAM)&m_selectedRevs);
									bSentMessage = TRUE;
								}
							}
						}
					}
				}
			}
			if ( !bSentMessage )
			{
				m_pNotifyWindow->SendMessage(WM_REVSELECTED, m_wParam & (MERGE_REVSELECTSTART | MERGE_REVSELECTMINUSONE), lowerRev);
				m_pNotifyWindow->SendMessage(WM_REVSELECTED, m_wParam & (MERGE_REVSELECTEND | MERGE_REVSELECTMINUSONE), higherRev);
				m_pNotifyWindow->SendMessage(WM_REVLIST, m_selectedRevs.GetCount(), (LPARAM)&m_selectedRevs);
				if (m_selectedRevsOneRange.GetCount())
					m_pNotifyWindow->SendMessage(WM_REVLISTONERANGE, 0, (LPARAM)&m_selectedRevsOneRange);
			}
		}
	}
	UpdateData();
	CRegDWORD reg(_T("Software\\TortoiseGit\\ShowAllEntry"));
	SaveSplitterPos();
#endif
}

void CLogDlg::OnPasteGitHash()
{
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	CClipboardHelper clipboardHelper;
	if (!clipboardHelper.Open(GetSafeHwnd()))
		return;

	HGLOBAL hClipboardData = GetClipboardData(CF_TEXT);
	if (!hClipboardData)
		return;

	char* pStr = (char*)GlobalLock(hClipboardData);
	CString str(pStr);
	GlobalUnlock(hClipboardData);
	if (str.IsEmpty())
		return;

	int pos = 0;
	if (LookLikeGitHash(str, pos))
		JumpToGitHash(str);
}

void CLogDlg::JumpToGitHash(CString& hash)
{
	for (int i = 0; i < (int)m_LogList.m_arShownList.size(); ++i)
	{
		GitRev* rev = m_LogList.m_arShownList.SafeGetAt(i);
		if (!rev) continue;
		if (rev->m_CommitHash.ToString().Left(hash.GetLength()) != hash)
			continue;

		m_LogList.SetItemState(m_LogList.GetSelectionMark(), 0, LVIS_SELECTED | LVIS_FOCUSED);
		POSITION pos = m_LogList.GetFirstSelectedItemPosition();
		while (pos)
		{
			int index = m_LogList.GetNextSelectedItem(pos);
			if (index >= 0)
				m_LogList.SetItemState(index, 0, LVIS_SELECTED);
		}
		m_LogList.EnsureVisible(i, FALSE);
		m_LogList.SetSelectionMark(i);
		m_LogList.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		// refresh of selection needs to be queued instead of done immediately, to ensure hyperlinks in target selection are created
		PostMessage(WM_TGIT_REFRESH_SELECTION, 0, 0);
		return;
	}

	CMessageBox::ShowCheck(GetSafeHwnd(), IDS_PROC_LOG_JUMPNOTFOUND, IDS_APPNAME, 1, IDI_INFORMATION, IDS_OKBUTTON, 0, 0, _T("NoJumpNotFoundWarning"), IDS_MSGBOX_DONOTSHOWAGAIN);
}

BOOL CLogDlg::PreTranslateMessage(MSG* pMsg)
{
	// Skip Ctrl-C when copying text out of the log message or search filter
	bool bSkipAccelerator = (pMsg->message == WM_KEYDOWN && (pMsg->wParam == 'C' || pMsg->wParam == VK_INSERT) && (GetFocus() == GetDlgItem(IDC_MSGVIEW) || GetFocus() == GetDlgItem(IDC_SEARCHEDIT) || GetFocus() == GetDlgItem(IDC_FILTER)) && GetKeyState(VK_CONTROL) & 0x8000);
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam=='\r')
	{
		if (GetFocus()==GetDlgItem(IDC_LOGLIST))
		{
			if (CRegDWORD(_T("Software\\TortoiseGit\\DiffByDoubleClickInLog"), FALSE))
			{
				m_LogList.DiffSelectedRevWithPrevious();
				return TRUE;
			}
		}
		if (GetFocus() == GetDlgItem(IDC_SEARCHEDIT))
		{
			KillTimer(LOGFILTER_TIMER);
			Refresh(false);
		}
		if (GetFocus() == GetDlgItem(IDC_FILTER))
		{
			KillTimer(FILEFILTER_TIMER);
			FillLogMessageCtrl();
			return TRUE;
		}
	}
	else if (pMsg->message == WM_KEYDOWN && ((pMsg->wParam == 'V' && GetKeyState(VK_CONTROL) < 0) || (pMsg->wParam == VK_INSERT && GetKeyState(VK_SHIFT) < 0 && GetKeyState(VK_CONTROL) >= 0)) && GetKeyState(VK_MENU) >= 0)
	{
		if (GetFocus() != GetDlgItem(IDC_SEARCHEDIT) && GetFocus() != GetDlgItem(IDC_FILTER))
		{
			OnPasteGitHash();
			return TRUE;
		}
	}
	else if (pMsg->message == WM_XBUTTONUP)
	{
		bool select = (pMsg->wParam & MK_SHIFT) == 0;
		if (HIWORD(pMsg->wParam) & XBUTTON1)
			GoBackForward(select, false);
		if (HIWORD(pMsg->wParam) & XBUTTON2)
			GoBackForward(select, true);
		if (HIWORD(pMsg->wParam) & (XBUTTON1 | XBUTTON2))
			return TRUE;
	}
	if (m_hAccel && !bSkipAccelerator)
	{
		int ret = TranslateAccelerator(m_hWnd, m_hAccel, pMsg);
		if (ret)
			return TRUE;
	}

	return __super::PreTranslateMessage(pMsg);
}


BOOL CLogDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	//if (this->IsThreadRunning())
	if(m_LogList.m_bNoDispUpdates)
	{
		// only show the wait cursor over the list control
		if ((pWnd)&&
			((pWnd == GetDlgItem(IDC_LOGLIST))||
			(pWnd == GetDlgItem(IDC_MSGVIEW))||
			(pWnd == GetDlgItem(IDC_LOGMSG))))
		{
			HCURSOR hCur = LoadCursor(nullptr, IDC_WAIT);
			SetCursor(hCur);
			return TRUE;
		}
	}
	if ((pWnd) && (pWnd == GetDlgItem(IDC_MSGVIEW)))
		return CResizableStandAloneDialog::OnSetCursor(pWnd, nHitTest, message);

	HCURSOR hCur = LoadCursor(nullptr, IDC_ARROW);
	SetCursor(hCur);
	return CResizableStandAloneDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CLogDlg::OnLvnItemchangedLoglist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	//if (this->IsThreadRunning())
	if(m_LogList.m_bNoDispUpdates)
		return;
	if (pNMLV->iItem >= 0)
	{
		if (!m_LogList.m_highlight.IsEmpty())
		{
			m_LogList.m_highlight.Empty();
			m_LogList.Invalidate();
		}
		this->m_LogList.m_nSearchIndex = pNMLV->iItem;
		GitRev* pLogEntry = m_LogList.m_arShownList.SafeGetAt(pNMLV->iItem);
		if (pLogEntry == nullptr)
			return;
		m_LogList.m_lastSelectedHash = pLogEntry->m_CommitHash;
		if (pNMLV->iSubItem != 0)
			return;
		if (pNMLV->iItem == (int)m_LogList.m_arShownList.size())
		{
			// remove the selected state
			if (pNMLV->uChanged & LVIF_STATE)
			{
				m_LogList.SetItemState(pNMLV->iItem, 0, LVIS_SELECTED);
				FillLogMessageCtrl();
				UpdateData(FALSE);
				UpdateLogInfoLabel();
			}
			return;
		}
		if (pNMLV->uChanged & LVIF_STATE)
		{
			if ((pNMLV->uNewState & LVIS_SELECTED) && !m_bNavigatingWithSelect)
			{
				m_LogList.m_selectionHistory.Add(m_LogList.m_lastSelectedHash);
				m_LogList.m_lastSelectedHash = pLogEntry->m_CommitHash;
			}
			FillLogMessageCtrl();
			UpdateData(FALSE);
		}
	}
	else
	{
		m_LogList.m_lastSelectedHash.Empty();
		FillLogMessageCtrl();
		UpdateData(FALSE);
	}
	EnableOKButton();
	if (pNMLV->iItem < 0)
		return;
	UpdateLogInfoLabel();
}

void CLogDlg::OnLvnItemchangedLogmsg(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	if (m_ChangedFileListCtrl.IsBusy())
		return;
	UpdateLogInfoLabel();
}

void CLogDlg::OnEnLinkMsgview(NMHDR *pNMHDR, LRESULT *pResult)
{
	ENLINK *pEnLink = reinterpret_cast<ENLINK *>(pNMHDR);
	if ((pEnLink->msg == WM_LBUTTONUP) || (pEnLink->msg == WM_SETCURSOR))
	{
		CString url, msg;
		GetDlgItemText(IDC_MSGVIEW, msg);
		msg.Replace(_T("\r\n"), _T("\n"));
		url = msg.Mid(pEnLink->chrg.cpMin, pEnLink->chrg.cpMax-pEnLink->chrg.cpMin);
		auto findResult = m_LogList.m_ProjectProperties.FindBugIDPositions(msg);
		if (std::find_if(findResult.cbegin(), findResult.cend(), 
			[=] (const CHARRANGE &cr) -> bool { return cr.cpMin == pEnLink->chrg.cpMin && cr.cpMax == pEnLink->chrg.cpMax; }
		) != findResult.cend())
		{
			url = m_LogList.m_ProjectProperties.GetBugIDUrl(url);
			url = GetAbsoluteUrlFromRelativeUrl(url);
		}
		if (::PathIsURL(url))
		{
			if (pEnLink->msg == WM_LBUTTONUP)
				ShellExecute(this->m_hWnd, _T("open"), url, nullptr, nullptr, SW_SHOWDEFAULT);
			else
			{
				static RECT prevRect = { 0 };
				CWnd * pMsgView = GetDlgItem(IDC_MSGVIEW);
				if (pMsgView)
				{
					RECT rc;
					POINTL pt;
					pMsgView->SendMessage(EM_POSFROMCHAR, (WPARAM)&pt, pEnLink->chrg.cpMin);
					rc.left = pt.x;
					rc.top = pt.y;
					pMsgView->SendMessage(EM_POSFROMCHAR, (WPARAM)&pt, pEnLink->chrg.cpMax);
					rc.right = pt.x;
					rc.bottom = pt.y + 12;
					if ((prevRect.left != rc.left) || (prevRect.top != rc.top))
					{
						m_tooltips.DelTool(pMsgView, 1);
						m_tooltips.AddTool(pMsgView, url, &rc, 1);
						prevRect = rc;
					}
				}
			}
		}
		else if(pEnLink->msg == WM_LBUTTONUP)
		{
			int pos = 0;
			if (LookLikeGitHash(url, pos))
				JumpToGitHash(url);
		}
	}
	*pResult = 0;
}

void CLogDlg::OnBnClickedStatbutton()
{
	if (this->IsThreadRunning())
		return;
	if (m_LogList.m_arShownList.empty() || m_LogList.m_arShownList.size() == 1 && m_LogList.m_bShowWC)
		return;		// nothing or just the working copy changes are shown, so no statistics.
	// the statistics dialog expects the log entries to be sorted by date
	SortByColumn(3, false);

	CStatGraphDlg dlg;
	m_LogList.RecalculateShownList(&dlg.m_ShowList);

	dlg.m_path = m_orgPath;
	dlg.DoModal();
	// restore the previous sorting
	SortByColumn(m_nSortColumn, m_bAscending);
	OnTimer(LOGFILTER_TIMER);
}

void CLogDlg::MoveToSameTop(CWnd *pWndRef, CWnd *pWndTarget)
{
	CRect rcWndPicAuthor, rcWndMsgView;
	pWndRef->GetWindowRect(rcWndMsgView);
	ScreenToClient(rcWndMsgView);
	pWndTarget->GetWindowRect(rcWndPicAuthor);
	ScreenToClient(rcWndPicAuthor);
	int diff = rcWndMsgView.top - rcWndPicAuthor.top;
	rcWndPicAuthor.top += diff;
	rcWndPicAuthor.bottom += diff;
	pWndTarget->MoveWindow(rcWndPicAuthor);
}

void CLogDlg::DoSizeV1(int delta)
{
	RemoveMainAnchors();

	// first, reduce the middle section to a minimum.
	// if that is not sufficient, minimize the lower section

	CRect changeListViewRect;
	m_ChangedFileListCtrl.GetClientRect(changeListViewRect);
	CRect messageViewRect;
	GetDlgItem(IDC_MSGVIEW)->GetClientRect(messageViewRect);

	int messageViewDelta = max(-delta, 20 - messageViewRect.Height());
	int changeFileListDelta = -delta - messageViewDelta;

	// set new sizes & positions
	CSplitterControl::ChangeHeight(&m_LogList, delta, CW_TOPALIGN);
	CSplitterControl::ChangeHeight(GetDlgItem(IDC_MSGVIEW), messageViewDelta, CW_TOPALIGN);
	CSplitterControl::ChangePos(GetDlgItem(IDC_MSGVIEW), 0, delta);
	MoveToSameTop(GetDlgItem(IDC_MSGVIEW), GetDlgItem(IDC_PIC_AUTHOR));
	CSplitterControl::ChangePos(GetDlgItem(IDC_SPLITTERBOTTOM), 0, -changeFileListDelta);
	CSplitterControl::ChangeHeight(&m_ChangedFileListCtrl, changeFileListDelta, CW_BOTTOMALIGN);

	AddMainAnchors();
	ArrangeLayout();
	AdjustMinSize();
	SetSplitterRange();
	m_LogList.Invalidate();
	m_ChangedFileListCtrl.Invalidate();
	GetDlgItem(IDC_MSGVIEW)->Invalidate();
	m_gravatar.Invalidate();
}

void CLogDlg::DoSizeV2(int delta)
{
	RemoveMainAnchors();

	// first, reduce the middle section to a minimum.
	// if that is not sufficient, minimize the top section

	CRect logViewRect;
	m_LogList.GetClientRect(logViewRect);
	CRect messageViewRect;
	GetDlgItem(IDC_MSGVIEW)->GetClientRect(messageViewRect);

	int messageViewDelta = max(delta, 20 - messageViewRect.Height());
	int logListDelta = delta - messageViewDelta;

	// set new sizes & positions
	CSplitterControl::ChangeHeight(&m_LogList, logListDelta, CW_TOPALIGN);
	CSplitterControl::ChangePos(GetDlgItem(IDC_SPLITTERTOP), 0, logListDelta);

	CSplitterControl::ChangeHeight(GetDlgItem(IDC_MSGVIEW), messageViewDelta, CW_TOPALIGN);
	CSplitterControl::ChangePos(GetDlgItem(IDC_MSGVIEW), 0, logListDelta);
	MoveToSameTop(GetDlgItem(IDC_MSGVIEW), GetDlgItem(IDC_PIC_AUTHOR));

	CSplitterControl::ChangeHeight(&m_ChangedFileListCtrl, -delta, CW_BOTTOMALIGN);

	AddMainAnchors();
	ArrangeLayout();
	AdjustMinSize();
	SetSplitterRange();
	GetDlgItem(IDC_MSGVIEW)->Invalidate();
	m_ChangedFileListCtrl.Invalidate();
	m_gravatar.Invalidate();
}

void CLogDlg::AddMainAnchors()
{
	AddAnchor(IDC_STATIC_REF, TOP_LEFT);
	AddAnchor(IDC_FROMLABEL, TOP_LEFT);
	AddAnchor(IDC_DATEFROM, TOP_LEFT);
	AddAnchor(IDC_TOLABEL, TOP_LEFT);
	AddAnchor(IDC_DATETO, TOP_LEFT);

	AddAnchor(IDC_SEARCHEDIT, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_LOG_JUMPTYPE, TOP_RIGHT);
	AddAnchor(IDC_LOG_JUMPUP, TOP_RIGHT);
	AddAnchor(IDC_LOG_JUMPDOWN, TOP_RIGHT);

	AddAnchor(IDC_LOGLIST, TOP_LEFT, MIDDLE_RIGHT);
	AddAnchor(IDC_SPLITTERTOP, MIDDLE_LEFT, MIDDLE_RIGHT);
	AddAnchor(IDC_MSGVIEW, MIDDLE_LEFT, MIDDLE_RIGHT); // keep in sync with ShowGravatar()
	AddAnchor(IDC_PIC_AUTHOR, MIDDLE_RIGHT);
	AddAnchor(IDC_SPLITTERBOTTOM, MIDDLE_LEFT, MIDDLE_RIGHT);
	AddAnchor(IDC_LOGMSG, MIDDLE_LEFT, BOTTOM_RIGHT);

	AddAnchor(IDC_LOGINFO, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_WALKBEHAVIOUR, BOTTOM_LEFT);
	AddAnchor(IDC_VIEW, BOTTOM_LEFT);
	AddAnchor(IDC_LOG_ALLBRANCH, BOTTOM_LEFT);
	AddAnchor(IDC_FILTER, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_WHOLE_PROJECT, BOTTOM_LEFT);
	AddAnchor(IDC_REFRESH, BOTTOM_LEFT);
	AddAnchor(IDC_STATBUTTON, BOTTOM_LEFT);
	AddAnchor(IDC_PROGRESS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDCANCEL, BOTTOM_RIGHT);
	AddAnchor(IDHELP, BOTTOM_RIGHT);
}

void CLogDlg::RemoveMainAnchors()
{
	RemoveAnchor(IDC_STATIC_REF);
	RemoveAnchor(IDC_FROMLABEL);
	RemoveAnchor(IDC_DATEFROM);
	RemoveAnchor(IDC_TOLABEL);
	RemoveAnchor(IDC_DATETO);

	RemoveAnchor(IDC_SEARCHEDIT);
	RemoveAnchor(IDC_LOG_JUMPTYPE);
	RemoveAnchor(IDC_LOG_JUMPUP);
	RemoveAnchor(IDC_LOG_JUMPDOWN);

	RemoveAnchor(IDC_LOGLIST);
	RemoveAnchor(IDC_SPLITTERTOP);
	RemoveAnchor(IDC_MSGVIEW);
	RemoveAnchor(IDC_PIC_AUTHOR);
	RemoveAnchor(IDC_SPLITTERBOTTOM);
	RemoveAnchor(IDC_LOGMSG);

	RemoveAnchor(IDC_LOGINFO);
	RemoveAnchor(IDC_WALKBEHAVIOUR);
	RemoveAnchor(IDC_VIEW);
	RemoveAnchor(IDC_LOG_ALLBRANCH);
	RemoveAnchor(IDC_FILTER);
	RemoveAnchor(IDC_WHOLE_PROJECT);
	RemoveAnchor(IDC_REFRESH);
	RemoveAnchor(IDC_STATBUTTON);
	RemoveAnchor(IDC_PROGRESS);
	RemoveAnchor(IDOK);
	RemoveAnchor(IDCANCEL);
	RemoveAnchor(IDHELP);
}

void CLogDlg::AdjustMinSize()
{
	// adjust the minimum size of the dialog to prevent the resizing from
	// moving the list control too far down.
	CRect rcChgListView;
	m_ChangedFileListCtrl.GetClientRect(rcChgListView);
	CRect rcLogList;
	m_LogList.GetClientRect(rcLogList);
	CRect rcLogMsg;
	GetDlgItem(IDC_MSGVIEW)->GetClientRect(rcLogMsg);

	SetMinTrackSize(CSize(m_DlgOrigRect.Width(),
		m_DlgOrigRect.Height()-m_ChgOrigRect.Height()-m_LogListOrigRect.Height()-m_MsgViewOrigRect.Height()
		+ rcLogMsg.Height() + abs(rcChgListView.Height() - rcLogList.Height()) + 60));
}

LRESULT CLogDlg::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_NOTIFY:
		if (wParam == IDC_SPLITTERTOP)
		{
			SPC_NMHDR* pHdr = (SPC_NMHDR*) lParam;
			DoSizeV1(pHdr->delta);
		}
		else if (wParam == IDC_SPLITTERBOTTOM)
		{
			SPC_NMHDR* pHdr = (SPC_NMHDR*) lParam;
			DoSizeV2(pHdr->delta);
		}
		break;
	}

	return CResizableDialog::DefWindowProc(message, wParam, lParam);
}

void CLogDlg::SetSplitterRange()
{
	if ((m_LogList)&&(m_ChangedFileListCtrl))
	{
		CRect rcTop;
		m_LogList.GetWindowRect(rcTop);
		ScreenToClient(rcTop);
		CRect rcBottom;
		m_ChangedFileListCtrl.GetWindowRect(rcBottom);
		ScreenToClient(rcBottom);
		m_wndSplitter1.SetRange(rcTop.top + 20, rcBottom.bottom - 50);
		m_wndSplitter2.SetRange(rcTop.top + 50, rcBottom.bottom - 20);
	}
}

void CLogDlg::OnEnscrollMsgview()
{
	m_tooltips.DelTool(GetDlgItem(IDC_MSGVIEW), 1);
}

LRESULT CLogDlg::OnClickedInfoIcon(WPARAM wParam, LPARAM lParam)
{
	if ((HWND)wParam == m_cFileFilter.GetSafeHwnd())
		return 0;

	// FIXME: x64 version would get this function called with unexpected parameters.
	if (!lParam)
		return 0;

	RECT * rect = (LPRECT)lParam;
	CPoint point;
	CString temp;
	point = CPoint(rect->left, rect->bottom);
#define LOGMENUFLAGS(x) (MF_STRING | MF_ENABLED | ((m_LogList.m_SelectedFilters & x) ? MF_CHECKED : MF_UNCHECKED))
	CMenu popup;
	if (popup.CreatePopupMenu())
	{
		temp.LoadString(IDS_LOG_FILTER_SUBJECT);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_SUBJECT), LOGFILTER_SUBJECT, temp);

		temp.LoadString(IDS_LOG_FILTER_MESSAGES);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_MESSAGES), LOGFILTER_MESSAGES, temp);

		temp.LoadString(IDS_LOG_FILTER_PATHS);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_PATHS), LOGFILTER_PATHS, temp);

		temp.LoadString(IDS_LOG_FILTER_AUTHORS);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_AUTHORS), LOGFILTER_AUTHORS, temp);

		temp.LoadString(IDS_LOG_FILTER_EMAILS);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_EMAILS), LOGFILTER_EMAILS, temp);

		temp.LoadString(IDS_LOG_FILTER_REVS);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_REVS), LOGFILTER_REVS, temp);

		temp.LoadString(IDS_LOG_FILTER_REFNAME);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_REFNAME), LOGFILTER_REFNAME, temp);

		temp.LoadString(IDS_PROC_LOG_TAGINFO);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_ANNOTATEDTAG), LOGFILTER_ANNOTATEDTAG, temp);

		temp.LoadString(IDS_NOTES);
		popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_NOTES), LOGFILTER_NOTES, temp);

		if (m_LogList.m_bShowBugtraqColumn == TRUE) {
			temp.LoadString(IDS_LOG_FILTER_BUGIDS);
			popup.AppendMenu(LOGMENUFLAGS(LOGFILTER_BUGID), LOGFILTER_BUGID, temp);
		}

		popup.AppendMenu(MF_SEPARATOR, NULL);

		temp.LoadString(IDS_LOG_FILTER_TOGGLE);
		popup.AppendMenu(MF_STRING | MF_ENABLED, LOGFILTER_TOGGLE, temp);

		temp.LoadString(IDS_ALL);
		popup.AppendMenu(MF_STRING | MF_ENABLED, LOGFILTER_ALL, temp);

		popup.AppendMenu(MF_SEPARATOR, NULL);

		temp.LoadString(IDS_LOG_FILTER_REGEX);
		popup.AppendMenu(MF_STRING | MF_ENABLED | (m_bFilterWithRegex ? MF_CHECKED : MF_UNCHECKED), LOGFILTER_REGEX, temp);

		temp.LoadString(IDS_LOG_FILTER_CASESENSITIVE);
		popup.AppendMenu(MF_STRING | MF_ENABLED | (m_bFilterCaseSensitively ? MF_CHECKED : MF_UNCHECKED), LOGFILTER_CASE, temp);

		m_tooltips.Pop();
		int selection = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
		if (selection != 0)
		{
			if (selection == LOGFILTER_REGEX)
			{
				m_bFilterWithRegex = !m_bFilterWithRegex;
				CRegDWORD b(_T("Software\\TortoiseGit\\UseRegexFilter"), FALSE);
				b = m_bFilterWithRegex;
				m_LogList.m_bFilterWithRegex = m_bFilterWithRegex;
				SetFilterCueText();
				CheckRegexpTooltip();
				m_cFilter.ValidateAndRedraw();
			}
			else if (selection == LOGFILTER_CASE)
			{
				m_bFilterCaseSensitively = !m_bFilterCaseSensitively;
				CRegDWORD b(_T("Software\\TortoiseGit\\FilterCaseSensitively"), FALSE);
				b = m_bFilterCaseSensitively;
				m_LogList.m_bFilterCaseSensitively = m_bFilterCaseSensitively;
			}
			else if (selection == LOGFILTER_TOGGLE)
			{
				m_LogList.m_SelectedFilters = (~m_LogList.m_SelectedFilters) & LOGFILTER_ALL;
				SetFilterCueText();
			}
			else if (selection == LOGFILTER_ALL)
			{
				m_LogList.m_SelectedFilters = selection;
				SetFilterCueText();
			}
			else
			{
				m_LogList.m_SelectedFilters ^= selection;
				SetFilterCueText();
			}
			CRegDWORD(_T("Software\\TortoiseGit\\SelectedLogFilters")) = m_LogList.m_SelectedFilters;
			// Reload only if a search text is entered
			if (m_LogList.HasFilterText())
				SetTimer(LOGFILTER_TIMER, 1000, nullptr);
		}
	}
	return 0L;
}

LRESULT CLogDlg::OnClickedCancelFilter(WPARAM wParam, LPARAM /*lParam*/)
{
	if ((HWND)wParam == m_cFileFilter.GetSafeHwnd())
	{
		KillTimer(FILEFILTER_TIMER);

		FillLogMessageCtrl();
		return 0L;
	}

	KillTimer(LOGFILTER_TIMER);

	m_LogList.m_sFilterText.Empty();
	UpdateData(FALSE);
	theApp.DoWaitCursor(1);
	FillLogMessageCtrl(false);

	m_LogList.RemoveFilter();

	Refresh();

	theApp.DoWaitCursor(-1);
	GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_SHOW);
	GetDlgItem(IDC_SEARCHEDIT)->SetFocus();
	UpdateLogInfoLabel();

	return 0L;
}


void CLogDlg::SetFilterCueText()
{
	CString temp(MAKEINTRESOURCE(IDS_LOG_FILTER_BY));
	temp += _T(" ");

	if (m_LogList.m_SelectedFilters & LOGFILTER_SUBJECT)
	{
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_SUBJECT));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_MESSAGES)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_MESSAGES));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_PATHS)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_PATHS));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_AUTHORS)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_AUTHORS));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_EMAILS)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_EMAILS));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_REVS)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_REVS));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_REFNAME)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_REFNAME));
	}

	if (m_LogList.m_SelectedFilters & LOGFILTER_NOTES)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_NOTES));
	}

	if (m_LogList.m_bShowBugtraqColumn && m_LogList.m_SelectedFilters & LOGFILTER_BUGID)
	{
		if (temp.ReverseFind(_T(' ')) != temp.GetLength() - 1)
			temp += _T(", ");
		temp += CString(MAKEINTRESOURCE(IDS_LOG_FILTER_BUGIDS));
	}

	// to make the cue banner text appear more to the right of the edit control
	temp = _T("   ")+temp;
	m_cFilter.SetCueBanner(temp.TrimRight());
}

bool CLogDlg::Validate(LPCTSTR string)
{
	if (!m_bFilterWithRegex)
		return true;
	std::tr1::wregex pat;
	return m_LogList.ValidateRegexp(string, pat, false);
}

void CLogDlg::OnEnChangeFileFilter()
{
	SetTimer(FILEFILTER_TIMER, 1000, nullptr);
}

void CLogDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == LOGFTIME_TIMER)
	{
		KillTimer(LOGFTIME_TIMER);
		Refresh(false);
	}
	else if (nIDEvent == LOG_FILLPATCHVTIMER)
		FillPatchView();
	else if (nIDEvent == LOGFILTER_TIMER)
	{
		KillTimer(LOGFILTER_TIMER);
		Refresh(false);

#if 0
		/* we will use git built-in grep to filter log */
		if (this->IsThreadRunning())
		{
			// thread still running! So just restart the timer.
			SetTimer(LOGFILTER_TIMER, 1000, nullptr);
			return;
		}
		CWnd * focusWnd = GetFocus();
		bool bSetFocusToFilterControl = ((focusWnd != GetDlgItem(IDC_DATEFROM))&&(focusWnd != GetDlgItem(IDC_DATETO))
			&& (focusWnd != GetDlgItem(IDC_LOGLIST)));
		if (m_LogList.m_sFilterText.IsEmpty())
		{
			DialogEnableWindow(IDC_STATBUTTON, !(((this->IsThreadRunning())||(m_LogList.m_arShownList.IsEmpty()))));
			// do not return here!
			// we also need to run the filter if the filter text is empty:
			// 1. to clear an existing filter
			// 2. to rebuild the m_arShownList after sorting
		}
		theApp.DoWaitCursor(1);
		CStoreSelection storeselection(this);
		KillTimer(LOGFILTER_TIMER);
		FillLogMessageCtrl(false);

		// now start filter the log list
		m_LogList.StartFilter();

		if ( m_LogList.GetItemCount()==1 )
		{
			m_LogList.SetSelectionMark(0);
			m_LogList.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
		}
		theApp.DoWaitCursor(-1);
		GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_SHOW);
		if (bSetFocusToFilterControl)
			GetDlgItem(IDC_SEARCHEDIT)->SetFocus();
		UpdateLogInfoLabel();
#endif
	} // if (nIDEvent == LOGFILTER_TIMER)
	else if (nIDEvent == LOG_HEADER_ORDER_TIMER)
	{
		KillTimer(LOG_HEADER_ORDER_TIMER);
		CLogOrdering orderDlg;
		if (orderDlg.DoModal() == IDOK)
			Refresh();
	}
	else if (nIDEvent == FILEFILTER_TIMER)
	{
		KillTimer(FILEFILTER_TIMER);
		FillLogMessageCtrl();
	}
	DialogEnableWindow(IDC_STATBUTTON, !(((this->IsThreadRunning())||(m_LogList.m_arShownList.empty() || m_LogList.m_arShownList.size() == 1 && m_LogList.m_bShowWC))));
	__super::OnTimer(nIDEvent);
}

void CLogDlg::OnDtnDatetimechangeDateto(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	try
	{
		CTime _time;
		m_DateTo.GetTime(_time);

		CTime fromTime;
		m_DateFrom.GetTime(fromTime);
		if (_time < fromTime)
		{
			_time = fromTime;
			m_DateTo.SetTime(&_time);
		}

		CTime time(_time.GetYear(), _time.GetMonth(), _time.GetDay(), 23, 59, 59);
		if (time.GetTime() != m_LogList.m_Filter.m_To)
		{
			m_LogList.m_Filter.m_To = (DWORD)time.GetTime();
			SetTimer(LOGFTIME_TIMER, 10, nullptr);
		}
	}
	catch (...)
	{
		CMessageBox::Show(GetSafeHwnd(), _T("Invalidate Parameter"), _T("TortoiseGit"), MB_OK | MB_ICONERROR);
	}

	*pResult = 0;
}

void CLogDlg::OnDtnDatetimechangeDatefrom(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	try
	{
		CTime _time;
		m_DateFrom.GetTime(_time);

		CTime toTime;
		m_DateTo.GetTime(toTime);
		if (_time > toTime)
		{
			_time = toTime;
			m_DateFrom.SetTime(&_time);
		}

		CTime time(_time.GetYear(), _time.GetMonth(), _time.GetDay(), 0, 0, 0);
		if (time.GetTime() != m_LogList.m_Filter.m_From)
		{
			m_LogList.m_Filter.m_From = (DWORD)time.GetTime();
			m_LogList.m_Filter.m_NumberOfLogsScale = CFilterData::SHOW_LAST_SEL_DATE;

			if (CFilterData::SHOW_LAST_SEL_DATE == (DWORD)CRegDWORD(_T("Software\\TortoiseGit\\LogDialog\\NumberOfLogsScale"), CFilterData::SHOW_NO_LIMIT))
				m_regLastSelectedFromDate = time.Format(L"%Y-%m-%d");

			SetTimer(LOGFTIME_TIMER, 10, nullptr);
		}
	}
	catch (...)
	{
		CMessageBox::Show(GetSafeHwnd(), _T("Invalidate Parameter"), _T("TortoiseGit"), MB_OK | MB_ICONERROR);
	}

	*pResult = 0;
}

void CLogDlg::OnCbnSelchangeJumpType()
{
	// reserved for future use
}

void CLogDlg::OnBnClickedJumpUp()
{
	int sel = m_JumpType.GetCurSel();
	if (sel < 0) return;
	JumpType jumpType = (JumpType)sel;

	if (jumpType == JumpType_History)
	{
		GoBack();
		return;
	}

	CString strValue;
	CGitHash hashValue;
	int index = -1;
	POSITION pos = m_LogList.GetFirstSelectedItemPosition();
	if (pos)
	{
		index = m_LogList.GetNextSelectedItem(pos);
		if (index == 0) return;

		GitRev* data = m_LogList.m_arShownList.SafeGetAt(index);
		if (jumpType == JumpType_AuthorEmail)
			strValue = data->GetAuthorEmail();
		else if (jumpType == JumpType_CommitterEmail)
			strValue = data->GetCommitterEmail();
		else if (jumpType == JumpType_Parent1)
			hashValue = data->m_CommitHash;
		else if (jumpType == JumpType_Parent2)
			hashValue = data->m_CommitHash;
		else if (jumpType == JumpType_TagFF)
			hashValue = data->m_CommitHash;
		else if (jumpType == JumpType_BranchFF)
			hashValue = data->m_CommitHash;

		m_LogList.SetItemState(index, 0, LVIS_SELECTED);
	}
	else
		return;

	while (pos)
	{
		index = m_LogList.GetNextSelectedItem(pos);
		m_LogList.SetItemState(index, 0, LVIS_SELECTED);
	}
	m_LogList.SetSelectionMark(-1);

	for (int i = index - 1; i >= 0; i--)
	{
		bool found = false;
		GitRev* data = m_LogList.m_arShownList.SafeGetAt(i);
		if (jumpType == JumpType_AuthorEmail)
			found = strValue == data->GetAuthorEmail();
		else if (jumpType == JumpType_CommitterEmail)
			found = strValue == data->GetCommitterEmail();
		else if (jumpType == JumpType_MergePoint)
			found = data->ParentsCount() > 1;
		else if (jumpType == JumpType_Parent1)
		{
			if (!data->m_ParentHash.empty())
				found = data->m_ParentHash[0] == hashValue;
		}
		else if (jumpType == JumpType_Parent2)
		{
			if (data->m_ParentHash.size() > 1)
				found = data->m_ParentHash[1] == hashValue;
		}
		else if (jumpType == JumpType_Tag || jumpType == JumpType_TagFF)
		{
			STRING_VECTOR refList = m_LogList.m_HashMap[data->m_CommitHash];
			for (size_t j = 0; j < refList.size(); ++j)
			{
				if (refList[j].Left(10) == _T("refs/tags/"))
				{
					found = true;
					break;
				}
			}

			if (found && jumpType == JumpType_TagFF)
				found = g_Git.IsFastForward(hashValue, data->m_CommitHash);
		}
		else if (jumpType == JumpType_Branch || jumpType == JumpType_BranchFF)
		{
			STRING_VECTOR refList = m_LogList.m_HashMap[data->m_CommitHash];
			for (size_t j = 0; j < refList.size(); ++j)
			{
				if (refList[j].Left(11) == _T("refs/heads/") || refList[j].Left(13) == _T("refs/remotes/"))
				{
					found = true;
					break;
				}
			}

			if (found && jumpType == JumpType_BranchFF)
				found = g_Git.IsFastForward(hashValue, data->m_CommitHash);
		}

		if (found)
		{
			m_LogList.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
			m_LogList.EnsureVisible(i, FALSE);
			m_LogList.SetSelectionMark(i);
			return;
		}
	}

	CMessageBox::ShowCheck(GetSafeHwnd(), IDS_PROC_LOG_JUMPNOTFOUND, IDS_APPNAME, 1, IDI_INFORMATION, IDS_OKBUTTON, 0, 0, _T("NoJumpNotFoundWarning"), IDS_MSGBOX_DONOTSHOWAGAIN);
}

void CLogDlg::OnBnClickedJumpDown()
{
	int jumpType = m_JumpType.GetCurSel();
	if (jumpType < 0) return;

	if (jumpType == JumpType_History)
	{
		GoForward();
		return;
	}

	CString strValue;
	CGitHash hashValue;
	int index = -1;
	POSITION pos = m_LogList.GetFirstSelectedItemPosition();
	if (pos)
	{
		index = m_LogList.GetNextSelectedItem(pos);
		if (index == 0) return;

		GitRev* data = m_LogList.m_arShownList.SafeGetAt(index);
		if (jumpType == JumpType_AuthorEmail)
			strValue = data->GetAuthorEmail();
		else if (jumpType == JumpType_CommitterEmail)
			strValue = data->GetCommitterEmail();
		else if (jumpType == JumpType_Parent1)
		{
			if (!data->m_ParentHash.empty())
				hashValue = data->m_ParentHash.at(0);
			else
				return;
		}
		else if (jumpType == JumpType_Parent2)
		{
			if (data->m_ParentHash.size() > 1)
				hashValue = data->m_ParentHash.at(1);
			else
				return;
		}
		else if (jumpType == JumpType_TagFF)
			hashValue = data->m_CommitHash;
		else if (jumpType == JumpType_BranchFF)
			hashValue = data->m_CommitHash;

		m_LogList.SetItemState(index, 0, LVIS_SELECTED);
	}
	else
		return;

	while (pos)
	{
		index = m_LogList.GetNextSelectedItem(pos);
		m_LogList.SetItemState(index, 0, LVIS_SELECTED);
	}
	m_LogList.SetSelectionMark(-1);

	for (int i = index + 1; i < m_LogList.GetItemCount(); ++i)
	{
		bool found = false;
		GitRev* data = m_LogList.m_arShownList.SafeGetAt(i);
		if (jumpType == JumpType_AuthorEmail)
			found = strValue == data->GetAuthorEmail();
		else if (jumpType == JumpType_CommitterEmail)
			found = strValue == data->GetCommitterEmail();
		else if (jumpType == JumpType_MergePoint)
			found = data->ParentsCount() > 1;
		else if (jumpType == JumpType_Parent1)
			found = data->m_CommitHash == hashValue;
		else if (jumpType == JumpType_Parent2)
			found = data->m_CommitHash == hashValue;
		else if (jumpType == JumpType_Tag || jumpType == JumpType_TagFF)
		{
			STRING_VECTOR refList = m_LogList.m_HashMap[data->m_CommitHash];
			for (size_t j = 0; j < refList.size(); ++j)
			{
				if (refList[j].Left(10) == _T("refs/tags/"))
				{
					found = true;
					break;
				}
			}

			if (found && jumpType == JumpType_TagFF)
				found = g_Git.IsFastForward(data->m_CommitHash, hashValue);
		}
		else if (jumpType == JumpType_Branch || jumpType == JumpType_BranchFF)
		{
			STRING_VECTOR refList = m_LogList.m_HashMap[data->m_CommitHash];
			for (size_t j = 0; j < refList.size(); ++j)
			{
				if (refList[j].Left(11) == _T("refs/heads/") || refList[j].Left(13) == _T("refs/remotes/"))
				{
					found = true;
					break;
				}
			}

			if (found && jumpType == JumpType_BranchFF)
				found = g_Git.IsFastForward(data->m_CommitHash, hashValue);
		}

		if (found)
		{
			m_LogList.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
			m_LogList.EnsureVisible(i, FALSE);
			m_LogList.SetSelectionMark(i);
			return;
		}
	}

	CMessageBox::ShowCheck(GetSafeHwnd(), IDS_PROC_LOG_JUMPNOTFOUND, IDS_APPNAME, 1, IDI_INFORMATION, IDS_OKBUTTON, 0, 0, _T("NoJumpNotFoundWarning"), IDS_MSGBOX_DONOTSHOWAGAIN);
}

void CLogDlg::SortByColumn(int /*nSortColumn*/, bool /*bAscending*/)
{
#if 0
	switch(nSortColumn)
	{
	case 0: // Revision
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscRevSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescRevSort());
		}
		break;
	case 1: // action
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscActionSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescActionSort());
		}
		break;
	case 2: // Author
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscAuthorSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescAuthorSort());
		}
		break;
	case 3: // Date
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscDateSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescDateSort());
		}
		break;
	case 4: // Message or bug id
		if (m_bShowBugtraqColumn)
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscBugIDSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescBugIDSort());
			break;
		}
		// fall through here
	case 5: // Message
		{
			if(bAscending)
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::AscMessageSort());
			else
				std::sort(m_logEntries.begin(), m_logEntries.end(), CLogDataVector::DescMessageSort());
		}
		break;
	default:
		ATLASSERT(0);
		break;
	}
#endif
}

void CLogDlg::OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (this->IsThreadRunning())
		return;		//no sorting while the arrays are filled
#if 0
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	const int nColumn = pNMLV->iSubItem;
	m_bAscending = nColumn == m_nSortColumn ? !m_bAscending : TRUE;
	m_nSortColumn = nColumn;
	SortByColumn(m_nSortColumn, m_bAscending);
	SetSortArrow(&m_LogList, m_nSortColumn, !!m_bAscending);
	SortShownListArray();
	m_LogList.Invalidate();
	UpdateLogInfoLabel();
#else
	UNREFERENCED_PARAMETER(pNMHDR);
#endif
	*pResult = 0;

	SetTimer(LOG_HEADER_ORDER_TIMER, 10, nullptr);
}

void CLogDlg::SortShownListArray()
{
	// make sure the shown list still matches the filter after sorting.
	OnTimer(LOGFILTER_TIMER);
	// clear the selection states
	POSITION pos = m_LogList.GetFirstSelectedItemPosition();
	while (pos)
	{
		m_LogList.SetItemState(m_LogList.GetNextSelectedItem(pos), 0, LVIS_SELECTED);
	}
	m_LogList.SetSelectionMark(-1);
}

void CLogDlg::SetSortArrow(CListCtrl * control, int nColumn, bool bAscending)
{
	if (!control)
		return;
	// set the sort arrow
	CHeaderCtrl * pHeader = control->GetHeaderCtrl();
	HDITEM HeaderItem = {0};
	HeaderItem.mask = HDI_FORMAT;
	for (int i=0; i<pHeader->GetItemCount(); ++i)
	{
		pHeader->GetItem(i, &HeaderItem);
		HeaderItem.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
		pHeader->SetItem(i, &HeaderItem);
	}
	if (nColumn >= 0)
	{
		pHeader->GetItem(nColumn, &HeaderItem);
		HeaderItem.fmt |= (bAscending ? HDF_SORTUP : HDF_SORTDOWN);
		pHeader->SetItem(nColumn, &HeaderItem);
	}
}

int CLogDlg::m_nSortColumnPathList = 0;
bool CLogDlg::m_bAscendingPathList = false;

void CLogDlg::OnBnClickedHidepaths()
{
	FillLogMessageCtrl();
	m_ChangedFileListCtrl.Invalidate();
}

void CLogDlg::UpdateLogInfoLabel()
{
	CGitHash rev1 ;
	CGitHash rev2 ;
	long selectedrevs = 0;
	long selectedfiles = 0;
	int count = (int)m_LogList.m_arShownList.size();
	int start = 0;
	if (count)
	{
		rev1 = m_LogList.m_arShownList.SafeGetAt(0)->m_CommitHash;
		if(this->m_LogList.m_bShowWC && rev1.IsEmpty()&&(count>1))
			start = 1;
		rev1 = m_LogList.m_arShownList.SafeGetAt(start)->m_CommitHash;
		//pLogEntry = reinterpret_cast<PLOGENTRYDATA>(m_arShownList.SafeGetAt(m_arShownList.GetCount()-1));
		rev2 = m_LogList.m_arShownList.SafeGetAt(count - 1)->m_CommitHash;
		selectedrevs = m_LogList.GetSelectedCount();
		if (selectedrevs)
			selectedfiles = m_ChangedFileListCtrl.GetSelectedCount();
	}
	CString sTemp;
	sTemp.Format(IDS_PROC_LOG_STATS,
		count - start,
		(LPCTSTR)rev2.ToString().Left(g_Git.GetShortHASHLength()), (LPCTSTR)rev1.ToString().Left(g_Git.GetShortHASHLength()), selectedrevs, selectedfiles);

	if(selectedrevs == 1)
	{
		CString str=m_ChangedFileListCtrl.GetStatisticsString(true);
		str.Replace(_T('\n'), _T(' '));
		sTemp += _T("\r\n") + str;
	}
	m_sLogInfo = sTemp;

	UpdateData(FALSE);
}

void CLogDlg::OnDtnDropdownDatefrom(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	// the date control should not show the "today" button
	CMonthCalCtrl * pCtrl = m_DateFrom.GetMonthCalCtrl();
	if (pCtrl)
		pCtrl->ModifyStyle(0, MCS_NOTODAY);
	*pResult = 0;
}

void CLogDlg::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	//set range
	SetSplitterRange();
}

void CLogDlg::OnRefresh()
{
	{
		this->m_LogProgress.SetPos(0);

		Refresh (false);
	}
}

void CLogDlg::OnFocusFilter()
{
	GetDlgItem(IDC_SEARCHEDIT)->SetFocus();
}

void CLogDlg::OnEditCopy()
{
	if (GetFocus() == &m_ChangedFileListCtrl)
		CopyChangedSelectionToClipBoard();
	else
		m_LogList.CopySelectionToClipBoard();
}

CString CLogDlg::GetAbsoluteUrlFromRelativeUrl(const CString& url)
{
	// is the URL a relative one?
	if (url.Left(2).Compare(_T("^/")) == 0)
	{
		// URL is relative to the repository root
		CString url1 = m_sRepositoryRoot + url.Mid(1);
		TCHAR buf[INTERNET_MAX_URL_LENGTH] = { 0 };
		DWORD len = url.GetLength();
		if (UrlCanonicalize((LPCTSTR)url1, buf, &len, 0) == S_OK)
			return CString(buf, len);
		return url1;
	}
	else if (url[0] == '/')
	{
		// URL is relative to the server's hostname
		CString sHost;
		// find the server's hostname
		int schemepos = m_sRepositoryRoot.Find(_T("//"));
		if (schemepos >= 0)
		{
			sHost = m_sRepositoryRoot.Left(m_sRepositoryRoot.Find('/', schemepos+3));
			CString url1 = sHost + url;
			TCHAR buf[INTERNET_MAX_URL_LENGTH] = { 0 };
			DWORD len = url.GetLength();
			if (UrlCanonicalize((LPCTSTR)url, buf, &len, 0) == S_OK)
				return CString(buf, len);
			return url1;
		}
	}
	return url;
}

void CLogDlg::ShowGravatar()
{
	m_gravatar.EnableGravatar(m_bShowGravatar);
	RemoveAnchor(IDC_MSGVIEW);
	if (m_gravatar.IsGravatarEnabled())
	{
		RECT rect, rect2;
		GetDlgItem(IDC_MSGVIEW)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		m_gravatar.GetWindowRect(&rect2);
		ScreenToClient(&rect2);
		rect.right = rect2.left;
		GetDlgItem(IDC_MSGVIEW)->MoveWindow(&rect);
		m_gravatar.ShowWindow(SW_SHOW);
	}
	else
	{
		RECT rect, rect2;
		GetDlgItem(IDC_MSGVIEW)->GetWindowRect(&rect);
		ScreenToClient(&rect);
		m_gravatar.GetWindowRect(&rect2);
		ScreenToClient(&rect2);
		rect.right = rect2.right;
		GetDlgItem(IDC_MSGVIEW)->MoveWindow(&rect);
		m_gravatar.ShowWindow(SW_HIDE);
	}
	AddAnchor(IDC_MSGVIEW, MIDDLE_LEFT, MIDDLE_RIGHT); // keep in sync with AddMainAnchors
}


void CLogDlg::OnEnChangeSearchedit()
{
	UpdateData();
	if (!m_LogList.HasFilterText())
	{
		// clear the filter, i.e. make all entries appear
		theApp.DoWaitCursor(1);
		KillTimer(LOGFILTER_TIMER);
		FillLogMessageCtrl(false);

		Refresh();
		//m_LogList.StartFilter();
#if 0
		InterlockedExchange(&m_bNoDispUpdates, TRUE);
		m_arShownList.RemoveAll();
		for (DWORD i=0; i<m_logEntries.size(); ++i)
		{
			if (IsEntryInDateRange(i))
				m_arShownList.Add(m_logEntries[i]);
		}
		InterlockedExchange(&m_bNoDispUpdates, FALSE);
		m_LogList.DeleteAllItems();
		m_LogList.SetItemCountEx(ShownCountWithStopped());
		m_LogList.RedrawItems(0, ShownCountWithStopped());
		m_LogList.SetRedraw(false);
		m_LogList.SetRedraw(true);
#endif
		theApp.DoWaitCursor(-1);
		GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_SEARCHEDIT)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SEARCHEDIT)->SetFocus();
		DialogEnableWindow(IDC_STATBUTTON, !(this->IsThreadRunning() || m_LogList.m_arShownList.empty()));
		return;
	}
	if (Validate(m_LogList.m_sFilterText))
		SetTimer(LOGFILTER_TIMER, 1000, nullptr);
	else
		KillTimer(LOGFILTER_TIMER);
}

void CLogDlg::OnBnClickedAllBranch()
{
	// m_bAllBranch is not auto-toggled by MFC, we have to handle it manually (in order to prevent the indeterminate state)

	m_LogList.m_ShowMask &=~ (CGit::LOG_INFO_LOCAL_BRANCHES | CGit::LOG_INFO_BASIC_REFS | CGit::LOG_INFO_ALL_BRANCH);

	if (m_bAllBranch)
	{
		m_bAllBranch = BST_UNCHECKED;
		m_AllBranchType = AllBranchType::None;
		m_ChangedFileListCtrl.m_sDisplayedBranch = m_LogList.GetRange();
	}
	else
	{
		m_bAllBranch = BST_CHECKED;
		m_AllBranchType = AllBranchType::AllBranches;
		m_LogList.m_ShowMask|=CGit::LOG_INFO_ALL_BRANCH;
		m_ChangedFileListCtrl.m_sDisplayedBranch.Empty();
	}

	// need to save value here, so that log dialogs started from now on also have AllBranch activated
	m_regbAllBranch = (DWORD)m_AllBranchType;

	UpdateData(FALSE);

	OnRefresh();

	FillLogMessageCtrl(false);
}

void CLogDlg::OnBnClickedFollowRenames()
{
	if(m_bFollowRenames)
	{
		m_LogList.m_ShowMask |= CGit::LOG_INFO_FOLLOW;
		m_LogList.m_ShowMask &=~ CGit::LOG_INFO_LOCAL_BRANCHES;
		m_LogList.m_ShowMask &= ~CGit::LOG_INFO_BASIC_REFS;
		if (m_bAllBranch)
		{
			m_bAllBranch = FALSE;
			m_AllBranchType = AllBranchType::None;
			m_LogList.m_ShowMask &=~ CGit::LOG_INFO_ALL_BRANCH;
		}
	}
	else
		m_LogList.m_ShowMask &= ~CGit::LOG_INFO_FOLLOW;

	DialogEnableWindow(IDC_LOG_ALLBRANCH, !m_bFollowRenames);
	DialogEnableWindow(IDC_WHOLE_PROJECT, !m_bFollowRenames && !m_path.IsEmpty());

	OnRefresh();

	FillLogMessageCtrl(false);
}

void CLogDlg::HandleShowLabels(bool var, int flag)
{
	if (var)
		m_LogList.m_ShowRefMask |= flag;
	else
		m_LogList.m_ShowRefMask &= ~flag;

	if ((m_LogList.m_ShowFilter & CGitLogListBase::FILTERSHOW_REFS) && !(m_LogList.m_ShowFilter & CGitLogListBase::FILTERSHOW_ANYCOMMIT))
	{
		// Remove commits where labels are not shown.
		OnRefresh();
		FillLogMessageCtrl(false);
	}
	else
	{
		// Just redraw
		m_LogList.Invalidate();
	}
}

void CLogDlg::OnBnClickedCompressedGraph()
{
	UpdateData();

	if (m_iCompressedGraph == 2)
		m_LogList.m_ShowFilter = CGitLogListBase::FILTERSHOW_REFS;
	else if (m_iCompressedGraph == 1)
		m_LogList.m_ShowFilter = static_cast<CGitLogListBase::FilterShow>(CGitLogListBase::FILTERSHOW_REFS | CGitLogListBase::FILTERSHOW_MERGEPOINTS);
	else
		m_LogList.m_ShowFilter = CGitLogListBase::FILTERSHOW_ALL;

	OnRefresh();
	FillLogMessageCtrl(false);
}

void CLogDlg::OnBnClickedBrowseRef()
{
	CString newRef = CBrowseRefsDlg::PickRef(false, m_LogList.GetRange(), gPickRef_All, true);
	if(newRef.IsEmpty())
		return;

	m_History.AddEntry(newRef);
	m_History.Save();

	SetRange(newRef);

	m_LogList.m_ShowMask &= ~(CGit::LOG_INFO_ALL_BRANCH | CGit::LOG_INFO_BASIC_REFS | CGit::LOG_INFO_LOCAL_BRANCHES);
	m_bAllBranch = BST_UNCHECKED;
	m_AllBranchType = AllBranchType::None;
	UpdateData(FALSE);

	OnRefresh();
	FillLogMessageCtrl(false);
}

void CLogDlg::ShowStartRef()
{
	//Show ref name on top
	if(!::IsWindow(m_hWnd))
		return;
	if (m_LogList.m_ShowMask & (CGit::LOG_INFO_ALL_BRANCH | CGit::LOG_INFO_BASIC_REFS | CGit::LOG_INFO_LOCAL_BRANCHES))
	{
		switch (m_LogList.m_ShowMask & (CGit::LOG_INFO_ALL_BRANCH | CGit::LOG_INFO_BASIC_REFS | CGit::LOG_INFO_LOCAL_BRANCHES))
		{
		case CGit::LOG_INFO_ALL_BRANCH:
			m_staticRef.SetWindowText(CString(MAKEINTRESOURCE(IDS_PROC_LOG_ALLBRANCHES)));
			break;

		case CGit::LOG_INFO_BASIC_REFS:
			m_staticRef.SetWindowText(CString(MAKEINTRESOURCE(IDS_PROC_LOG_BASIC_REFS)));
			break;

		case CGit::LOG_INFO_LOCAL_BRANCHES:
			m_staticRef.SetWindowText(CString(MAKEINTRESOURCE(IDS_PROC_LOG_LOCAL_BRANCHES)));
			break;
		}
		
		m_staticRef.Invalidate(TRUE);
		m_tooltips.DelTool(GetDlgItem(IDC_STATIC_REF));
		return;
	}

	CString showStartRef = m_LogList.GetRange();
	if (showStartRef.IsEmpty() || showStartRef == _T("HEAD"))
	{
		showStartRef.Empty();
		//Ref name is HEAD
		if (g_Git.Run(L"git.exe symbolic-ref HEAD", &showStartRef, nullptr, CP_UTF8))
			showStartRef.LoadString(IDS_PROC_LOG_NOBRANCH);
		showStartRef.Trim(L"\r\n\t ");
	}


	showStartRef = g_Git.StripRefName(showStartRef);

	m_staticRef.SetWindowText(showStartRef);
	CWnd *pWnd = GetDlgItem(IDC_STATIC_REF);
	m_tooltips.AddTool(pWnd, showStartRef);
	m_staticRef.Invalidate(TRUE);
}

void CLogDlg::SetRange(const CString& range)
{
	m_LogList.SetRange(range);
	m_ChangedFileListCtrl.m_sDisplayedBranch = range;

	ShowStartRef();
}

static void AppendMenuChecked(CMenu &menu, UINT nTextID, UINT_PTR nItemID, BOOL checked = FALSE, BOOL enabled = TRUE)
{
	CString text;
	text.LoadString(nTextID);
	menu.AppendMenu(MF_STRING | (enabled ? MF_ENABLED : MF_DISABLED) | (checked ? MF_CHECKED : MF_UNCHECKED), nItemID, text);
}

#define WALKBEHAVIOUR_FIRSTPARENT			1
#define WALKBEHAVIOUR_FOLLOWRENAMES			2
#define WALKBEHAVIOUR_COMPRESSEDGRAPH		3
#define WALKBEHAVIOUR_LABELEDCOMMITS		4
#define WALKBEHAVIOUR_NOMERGES				5

void CLogDlg::OnBnClickedWalkBehaviour()
{
	CMenu popup;
	if (popup.CreatePopupMenu())
	{
		AppendMenuChecked(popup, IDS_WALKBEHAVIOUR_FIRSTPARENT, WALKBEHAVIOUR_FIRSTPARENT, m_bFirstParent);
		AppendMenuChecked(popup, IDS_WALKBEHAVIOUR_NOMERGES, WALKBEHAVIOUR_NOMERGES, m_bNoMerges);
		AppendMenuChecked(popup, IDS_WALKBEHAVIOUR_FOLLOWRENAMES, WALKBEHAVIOUR_FOLLOWRENAMES, m_bFollowRenames, !(m_path.IsEmpty() || m_path.IsDirectory()));
		popup.AppendMenu(MF_SEPARATOR, NULL);
		AppendMenuChecked(popup, IDS_WALKBEHAVIOUR_COMPRESSED, WALKBEHAVIOUR_COMPRESSEDGRAPH, m_iCompressedGraph == 1);
		AppendMenuChecked(popup, IDS_WALKBEHAVIOUR_LABELEDCOMMITS, WALKBEHAVIOUR_LABELEDCOMMITS, m_iCompressedGraph == 2);

		m_tooltips.Pop();
		RECT rect;
		GetDlgItem(IDC_WALKBEHAVIOUR)->GetWindowRect(&rect);
		int selection = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, rect.left, rect.top, this, 0);
		switch (selection)
		{
		case WALKBEHAVIOUR_FIRSTPARENT:
			m_bFirstParent = !m_bFirstParent;
			OnBnClickedFirstParent();
			break;
		case WALKBEHAVIOUR_NOMERGES:
			m_bNoMerges = !m_bNoMerges;
			OnBnClickedFirstParent(); // OnBnClickedFirstParent handles both cases: m_bFirstParent and m_bNoMerges
			break;
		case WALKBEHAVIOUR_FOLLOWRENAMES:
			m_bFollowRenames = !m_bFollowRenames;
			OnBnClickedFollowRenames();
			break;
		case WALKBEHAVIOUR_COMPRESSEDGRAPH:
			m_iCompressedGraph = (m_iCompressedGraph == 1 ? 0 : 1);
			OnBnClickedCompressedGraph();
			break;
		case WALKBEHAVIOUR_LABELEDCOMMITS:
			m_iCompressedGraph = (m_iCompressedGraph == 2 ? 0 : 2);
			OnBnClickedCompressedGraph();
			break;
		default:
			break;
		}
		m_bWalkBehavior = (m_bFirstParent || m_bNoMerges || m_bFollowRenames || m_iCompressedGraph);
		UpdateData(FALSE);
	}
}

#define VIEW_HIDEPATHS				1
#define VIEW_GRAYPATHS				2
#define VIEW_SHOWTAGS				3
#define VIEW_SHOWLOCALBRANCHES		4
#define VIEW_SHOWREMOTEBRANCHES		5
#define VIEW_SHOWGRAVATAR			6
#define VIEW_SHOWPATCH				7

void CLogDlg::OnBnClickedView()
{
	CMenu popup;
	if (popup.CreatePopupMenu())
	{
		AppendMenuChecked(popup, IDS_SHOWFILES_HIDEPATHS, VIEW_HIDEPATHS, m_iHidePaths == 1);
		AppendMenuChecked(popup, IDS_SHOWFILES_GRAYPATHS, VIEW_GRAYPATHS, m_iHidePaths == 2);
		popup.AppendMenu(MF_SEPARATOR, NULL);
		CMenu showLabelsMenu;
		if (showLabelsMenu.CreatePopupMenu())
		{
			AppendMenuChecked(showLabelsMenu, IDS_VIEW_SHOWTAGLABELS, VIEW_SHOWTAGS, m_bShowTags);
			AppendMenuChecked(showLabelsMenu, IDS_VIEW_SHOWLOCALBRANCHLABELS, VIEW_SHOWLOCALBRANCHES, m_bShowLocalBranches);
			AppendMenuChecked(showLabelsMenu, IDS_VIEW_SHOWREMOTEBRANCHLABELS, VIEW_SHOWREMOTEBRANCHES, m_bShowRemoteBranches);
			popup.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)showLabelsMenu.m_hMenu, (CString)MAKEINTRESOURCE(IDS_VIEW_LABELS));
		}
		popup.AppendMenu(MF_SEPARATOR, NULL);
		AppendMenuChecked(popup, IDS_VIEW_SHOWGRAVATAR, VIEW_SHOWGRAVATAR, m_bShowGravatar);
		AppendMenuChecked(popup, IDS_MENU_VIEWPATCH, VIEW_SHOWPATCH, IsWindow(this->m_patchViewdlg.m_hWnd));

		m_tooltips.Pop();
		RECT rect;
		GetDlgItem(IDC_VIEW)->GetWindowRect(&rect);
		int selection = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, rect.left, rect.top, this, 0);
		switch (selection)
		{
		case VIEW_HIDEPATHS:
			if (m_iHidePaths == 1)
				m_iHidePaths = 0;
			else
				m_iHidePaths = 1;
			OnBnClickedHidepaths();
			break;
		case VIEW_GRAYPATHS:
			if (m_iHidePaths == 2)
				m_iHidePaths = 0;
			else
				m_iHidePaths = 2;
			OnBnClickedHidepaths();
			break;
		case VIEW_SHOWTAGS:
			m_bShowTags = !m_bShowTags;
			HandleShowLabels(m_bShowTags, LOGLIST_SHOWTAGS);
			break;
		case VIEW_SHOWLOCALBRANCHES:
			m_bShowLocalBranches = !m_bShowLocalBranches;
			HandleShowLabels(m_bShowLocalBranches, LOGLIST_SHOWLOCALBRANCHES);
			break;
		case VIEW_SHOWREMOTEBRANCHES:
			m_bShowRemoteBranches = !m_bShowRemoteBranches;
			HandleShowLabels(m_bShowRemoteBranches, LOGLIST_SHOWREMOTEBRANCHES);
			break;
		case VIEW_SHOWGRAVATAR:
			{
				m_bShowGravatar = !m_bShowGravatar;
				ShowGravatar();
				m_gravatar.Init();
				CString email;
				POSITION pos = m_LogList.GetFirstSelectedItemPosition();
				if (pos)
				{
					int selIndex = m_LogList.GetNextSelectedItem(pos);
					int moreSel = m_LogList.GetNextSelectedItem(pos);
					if (moreSel < 0)
					{
						GitRev* pLogEntry = m_LogList.m_arShownList.SafeGetAt(selIndex);
						if (pLogEntry)
							email = pLogEntry->GetAuthorEmail();
					}
				}
				m_gravatar.LoadGravatar(email);
				break;
			}
		case VIEW_SHOWPATCH:
			TogglePatchView();
			break;
		default:
			break;
		}
	}
}

void CLogDlg::OnBnClickedFirstParent()
{
	if(this->m_bFirstParent)
		m_LogList.m_ShowMask|=CGit::LOG_INFO_FIRST_PARENT;
	else
		m_LogList.m_ShowMask&=~CGit::LOG_INFO_FIRST_PARENT;

	if (m_bNoMerges)
		m_LogList.m_ShowMask |= CGit::LOG_INFO_NO_MERGE;
	else
		m_LogList.m_ShowMask &= ~CGit::LOG_INFO_NO_MERGE;

	OnRefresh();

	FillLogMessageCtrl(false);
}

void CLogDlg::OnBnClickShowWholeProject()
{
	this->UpdateData();

	if(this->m_bWholeProject)
	{
		m_LogList.m_Path.Reset();
		SetDlgTitle();
	}
	else
		m_LogList.m_Path=m_path;

	m_regShowWholeProject = m_bWholeProject;

	SetDlgTitle();

	OnRefresh();

	FillLogMessageCtrl(false);
}

LRESULT CLogDlg::OnRefLogChanged(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	ShowStartRef();
	return 0;
}

LRESULT CLogDlg::OnTaskbarBtnCreated(WPARAM wParam, LPARAM lParam)
{
	m_pTaskbarList.Release();
	m_pTaskbarList.CoCreateInstance(CLSID_TaskbarList);
	return __super::OnTaskbarButtonCreated(wParam, lParam);
}

LRESULT CLogDlg::OnRefreshSelection(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	// it's enough to deselect, then select again one item of the whole selection
	int selMark = m_LogList.GetSelectionMark();
	if (selMark >= 0)
	{
		m_LogList.SetSelectionMark(selMark);
		m_LogList.SetItemState(selMark, 0, LVIS_SELECTED);
		m_LogList.SetItemState(selMark, LVIS_SELECTED, LVIS_SELECTED);
	}
	return 0;
}

void CLogDlg::OnExitClearFilter()
{
	if (!m_LogList.m_sFilterText.IsEmpty())
	{
		OnClickedCancelFilter(NULL, NULL);
		return;
	}
	SendMessage(WM_CLOSE);
}
