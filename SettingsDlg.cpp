//  This file is part of Par-N-Rar
//  http://www.milow.net/site/projects/parnrar.html
//
//  Copyright (c) Gil Milow
//
//  Par-N-Rar is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  Par-N-Rar is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 
//  This code may not be used to develop a RAR (WinRAR) compatible archiver.
//

// SettingsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"		// main symbols
#include "SettingsDlg.h"
#include <afxhtml.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog dialog
CSettingsDialog::CSettingsDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSettingsDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSettingsDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pInfo.RemoveAll();	// Empty the page info
	m_csTitle = _T("");
	m_pParent = pParent;	// Parent wnd pointer, must be null for modal dialog
}

/////////////////////////////////////////////////////////////////////////////
//
CSettingsDialog::~CSettingsDialog()
{
	// Clean up garbage
	for (int i=0; i<m_pInfo.GetSize(); i++)
	{
		PAGE_INFO *pInfo = (PAGE_INFO *)m_pInfo.GetAt(i);
		if (pInfo) 
		{
			delete pInfo;
		}
	}
	m_pInfo.RemoveAll();
}

void CSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSettingsDialog)
	DDX_Control(pDX, IDC_PAGE_FRAME, m_PageFrame);
	DDX_Control(pDX, IDC_TREE_CTRL, m_TreeCtrl);
	DDX_Control(pDX, IDC_CAPTION_BAR, m_CaptionBarCtrl);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSettingsDialog, CDialog)
	//{{AFX_MSG_MAP(CSettingsDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_CTRL, OnTreeSelChanged)
	//ON_BN_CLICKED(IDC_PREFHELP, OnPreferenceHelp)
	ON_BN_CLICKED(IDAPPLY, OnApply)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog message handlers
//
BOOL CSettingsDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_PageFrame.GetWindowRect(m_FrameRect);
	ScreenToClient(m_FrameRect);
	m_FrameRect.DeflateRect(2,2);

	SetWindowText(m_csTitle);

	// Set some styles for indicator bar
	m_CaptionBarCtrl.m_textClr     = ::GetSysColor(COLOR_3DFACE);
	m_CaptionBarCtrl.m_fontWeight  = FW_BOLD;
	m_CaptionBarCtrl.m_fontSize    = 14;
	m_CaptionBarCtrl.m_csFontName  = "Verdana";
	m_CaptionBarCtrl.SetConstantText(m_csLogoText);

	InitTreeCtrl();		// Initialize the tree control with options
	ExpandTree();
	if (m_pInfo.GetAt(0)) ShowPage(m_pInfo.GetAt(0));
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////
//
void CSettingsDialog::InitTreeCtrl()
{
	// Fill the tree. we'll create the pages as we need them
	for (int i=0; i < m_pInfo.GetSize(); i++)
	{
		PAGE_INFO *pInfo = (PAGE_INFO *)m_pInfo.GetAt(i);
		if (!pInfo) continue;
		TV_INSERTSTRUCT tvi;

		if (pInfo->pWndParent) tvi.hParent = FindItem(pInfo->pWndParent);
		else tvi.hParent = FindItem(pInfo->csParentCaption);
		tvi.hInsertAfter = TVI_LAST;
		tvi.item.cchTextMax = 0;

		
		tvi.item.pszText = new char[pInfo->csCaption.GetLength() + 1];
		strncpy(tvi.item.pszText, pInfo->csCaption.GetBuffer(0), pInfo->csCaption.GetLength());
		tvi.item.pszText[pInfo->csCaption.GetLength()] = NULL;

		tvi.item.lParam = (long)pInfo;
		tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
		HTREEITEM hTree = m_TreeCtrl.InsertItem(&tvi);

		// Keep track of the pages we've added (for parent selection)
		if (hTree && pInfo->pPropPage)
		{
			DWORD dwTree = (DWORD)hTree;
			m_wndMap.SetAt(pInfo->pPropPage, dwTree);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::AddPage(...)
//
// Description: Add a page specified by CRuntimeClass. 
//		If pWndClass = NULL, the corresponding tree item has no page
//		The page identified by pCaption on the tree control will be inserted
//		under the parent tree item identified by pWndParent.
//		If pWndParent is NULL, them the page becomes a root item on the
//		tree control
// Return value: A CWnd type pointer to the page created if successful,
//		otherwise return NULL
//
CWnd* CSettingsDialog::AddPage(CRuntimeClass  *pWndClass, 
							   const char *pCaption, UINT nID /* = 0 */, 
							   CWnd *pWndParent /* = NULL */)
{
	CPropertyPage* pPropPage = NULL;
	if (m_hWnd)
	{
		// Can't add once the window has been created
		ASSERT(0);
		return pPropPage;
	}
	
	// Create the specified page object
	if (pWndClass) pPropPage = (CPropertyPage*) pWndClass->CreateObject();	

	PAGE_INFO *pInfo = new PAGE_INFO;
	pInfo->bViewClass = TRUE;			// Default to generic CWnd or CView class
	if (pPropPage)							// If it is a dialog or dialog-derived class
	{									// Set bViewClass = FALSE
		if (pPropPage->IsKindOf(RUNTIME_CLASS(CDialog)))	pInfo->bViewClass = FALSE;
	}

	pInfo->nID = nID;					// ID of the page
	pInfo->pPropPage = pPropPage;					// Pointer to the page
	pInfo->csCaption = pCaption;		// Caption of page in the tree control
	pInfo->pWndParent = pWndParent;		// Parent of the page if it has one
	pInfo->csParentCaption.Empty();		// Parent caption ignored
	m_pInfo.Add(pInfo);					// Add to the page list

	return pPropPage;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::AddPage(...)
//
// Description: Add a page specified by CRuntimeClass. 
//		If pWndClass = NULL, the corresponding tree item has no page
//		The page identified by pCaption on the tree control will be inserted
//		under the parent tree item identified by pParentCaption.
//		If pParentCaption is empty, them the page becomes a root item on the
//		tree control
// Return value: A CWnd type pointer to the page created if successful,
//		otherwise return NULL
//
CWnd* CSettingsDialog::AddPage(CRuntimeClass *pWndClass, const char *pCaption, 
							   UINT nID, const char *pParentCaption)
{
	CPropertyPage* pPropPage = NULL;
	if (m_hWnd)
	{
		// Can't add once the window has been created
		ASSERT(0);
		return pPropPage;
	}

	// Create the specified page object
	if (pWndClass) pPropPage = (CPropertyPage*) pWndClass->CreateObject();
	
	PAGE_INFO *pInfo = new PAGE_INFO;
	pInfo->bViewClass = TRUE;			// Default to generic CWnd or CView class
	if (pPropPage)							// If it is a dialog or dialog-derived class
	{									// Set bViewClass = FALSE
		if (pPropPage->IsKindOf(RUNTIME_CLASS(CDialog)))	pInfo->bViewClass = FALSE;
	}
	pInfo->nID = nID;					// ID of the page
	pInfo->pPropPage = pPropPage;					// Point to the page
	pInfo->csCaption = pCaption;		// Caption of page in the tree control
	pInfo->pWndParent = NULL;			// Parent page is not specified yet
	pInfo->csParentCaption = pParentCaption;	// Parent caption to be inserted under
	m_pInfo.Add(pInfo);					// Add to to page list

	return pPropPage;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::FindItem(CWnd *pWnd)
//
// Description: Find the tree item indexed by the page pointer pWnd. 
//	 
// Return value: An HTREEITEM is returned. If pWnd = NULL, TVI_ROOT returned
//	
HTREEITEM CSettingsDialog::FindItem(CWnd *pWnd)
{
	// If you didn't specify a parent in AddPage(...) , the
	// dialog becomes a root-level entry
	if (pWnd == NULL) return TVI_ROOT;
	else
	{
		DWORD dwHTree;
		if (m_wndMap.Lookup(pWnd, dwHTree)) return (HTREEITEM)dwHTree;
		else
		{
         // Specified a parent that has not 
         // been added to the tree - can't do that.
			ASSERT(FALSE);
			return (TVI_ROOT);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::FindItem(const CString &csCaption)
//
// Description: Find the tree item indexed by tree item text csCaption. 
//	 
// Return value: An HTREEITEM is returned. 
//				TVI_ROOT is returned 
//				if (1) csCaption = Empty, TVI_ROOT returned
//				If (2) the is no match of csCaption on the tree control,
//	
HTREEITEM CSettingsDialog::FindItem(const CString &csCaption)
{
	if(m_TreeCtrl.GetCount() == 0) return TVI_ROOT; // If the tree is empty
	if(csCaption.IsEmpty()) return TVI_ROOT;		// If no item text specified

	HTREEITEM hCurrent  = m_TreeCtrl.GetRootItem();
	while(hCurrent)
	{
		CString strText = m_TreeCtrl.GetItemText(hCurrent);
		if(!strText.CompareNoCase(csCaption))	return hCurrent;
		hCurrent = GetNextItemCOrS(hCurrent);
	}
	return TVI_ROOT;

}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::GetNextItemCOrS(HTREEITEM hItem)
//
// Description: Get handle of the child or sibling tree item 
//				indexed by current tree item handle. 
//	 
// Return value: An HTREEITEM is returned. 
//	
HTREEITEM CSettingsDialog::GetNextItemCOrS(HTREEITEM hItem)
{
	HTREEITEM	hti;
	if( m_TreeCtrl.ItemHasChildren( hItem ) )
		return m_TreeCtrl.GetChildItem( hItem );		// return first child
	else
	{
		// Return next sibling item
		// Go up the tree to find a parent's sibling if needed.
		while( (hti = m_TreeCtrl.GetNextSiblingItem( hItem )) == NULL )
		{
			if( (hItem = m_TreeCtrl.GetParentItem( hItem ) ) == NULL )
				return NULL;
		}
	}
	return hti;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::ShowPage(const PAGE_INFO *pInfo, UINT nShow /* = SW_SHOW */)
// Description: Display or hide the page if the associated tree item is selected.
//				nShow determines whether to display or hide the page.
//				pInfo identifies the page to be displayed.
//				Note, if the window of the page has not yet ben created
//				it will create the page window before the page gets displayed
// Return value: NULL
//
void CSettingsDialog::ShowPage(const PAGE_INFO *pInfo, UINT nShow /* = SW_SHOW */)
{
	if (!pInfo) return;
	m_CaptionBarCtrl.SetWindowText("");		// Clear the caption bar

	if (pInfo->pPropPage)						// If the page is valid
	{
		if (!::IsWindow(pInfo->pPropPage->m_hWnd))
		{									// Window has not been created, create it
			CreatePage(pInfo);
			pInfo->pPropPage->SetWindowPos (&m_TreeCtrl,0,0,0,0,0);
			pInfo->pPropPage->MoveWindow(m_FrameRect.left, m_FrameRect.top, m_FrameRect.Width(), m_FrameRect.Height());
			if( pInfo->pPropPage->IsKindOf( RUNTIME_CLASS(CView) ) )
				((CView*)pInfo->pPropPage)->OnInitialUpdate();
		}
		
		pInfo->pPropPage->ShowWindow(nShow);		// Show or hide the window
		if (nShow == SW_SHOW)				// Change the tree selection
		{
			pInfo->pPropPage->SetFocus();
			HTREEITEM hItem = FindItem(pInfo->pPropPage);
			if (hItem) m_TreeCtrl.SelectItem(hItem);
		}
	}
	if (nShow == SW_SHOW) 					// Update caption bar
		m_CaptionBarCtrl.SetWindowText(pInfo->csCaption);
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::CreatePage(const PAGE_INFO *pInfo)
// Description: Create the Window of the page specified by the page info.
// Return value: TRUE if successful, or FALSE if failed
// 
BOOL CSettingsDialog::CreatePage(const PAGE_INFO *pInfo)
{
	BOOL bCode = FALSE;

	if (!pInfo || !pInfo->pPropPage) return(FALSE);	// If no page is specified, return NULL
	if (!::IsWindow(pInfo->pPropPage->m_hWnd))		// If the window has not yet been created,
	{
		if (pInfo->pPropPage->IsKindOf(RUNTIME_CLASS(CDialog)))	// If the page indow is kind of dialog window
		{
			CDialog *pDlg = (CDialog*)pInfo->pPropPage;
			bCode = pDlg->Create(pInfo->nID, this);
			pDlg->ModifyStyle(WS_CAPTION, 0);
		}
		else if (pInfo->pPropPage->IsKindOf(RUNTIME_CLASS(CWnd)))	// generic CWnd derived Window
		{
			CWnd *pWnd = (CWnd*)pInfo->pPropPage;
			bCode = CreateWnd(pInfo->pPropPage);						// Create Window 
			pWnd->ModifyStyle(WS_BORDER|WS_THICKFRAME, 0);		// Remoce border and thick frame styles
		}
	}
	return(bCode);
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::CreateWnd(CWnd *pWnd, CCreateContext *pContext /* = NULL */)
// Description:	Create generic CWnd based Window of a page
// Return value: TRUE if successful, NULL if failed
//
BOOL CSettingsDialog::CreateWnd(CWnd *pWnd, CCreateContext *pContext /* = NULL */)
{
	CCreateContext context;	
	if (pContext == NULL)	
	{
		// If no context specified, generate one from the currently selected
		//  client if possible
		context.m_pCurrentFrame = (CFrameWnd*) this;
		context.m_pCurrentDoc = NULL;
		context.m_pNewViewClass = NULL;
		pContext = &context;

	}
	
    ASSERT_KINDOF(CWnd, pWnd);
	ASSERT(pWnd->m_hWnd == NULL);       // Not yet created
	
	DWORD dwStyle = AFX_WS_DEFAULT_VIEW;
	CRect rect;
	// Create with the right size and position
	if (!pWnd->Create(NULL, NULL, dwStyle, rect, this, 0, pContext))
	{
		TRACE0("Warning: couldn't create wnd in CSettingsDialog.\n");
		// pWnd will be cleaned up by PostNcDestroy
		return FALSE;
	}
	return(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult) 
// Description: Notification Handler for selection change of the tree control
// Return value: NULL
//
void CSettingsDialog::OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	if (pNMTreeView->itemNew.lParam)
	{									// Get previous selected page
		PAGE_INFO *pInfo = (PAGE_INFO *) pNMTreeView->itemOld.lParam;
		ShowPage(pInfo, SW_HIDE);		// Hide the page
										// Get current selected page
		pInfo = (PAGE_INFO *) pNMTreeView->itemNew.lParam;
		ShowPage(pInfo, SW_SHOW);		// Show the page
	}
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::OnCancel() 
// Description: Handler of the Cancel button of the dialog
//
void CSettingsDialog::OnCancel() 
{
	// TODO: Add extra cleanup here
	if ( m_pParent != NULL )	// Modaless dialog case		
	{
		// Inform the parent, the modaless case
		m_pParent->PostMessage( WM_SETTINGSDIALOG_CLOSE, IDCANCEL, 0 );
	}
	else						// Modal dialog case
	{
		DestroyPages();			// Destroy the Windows of the setting pages
		CDialog::OnCancel();
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::OnApply() 
// Description: Handler of the Apply button. When the Apply botton is click, the dialog
//		is still displayed, but the IDAPPLY message is sent to its parent window
//		to update the settings of the project.
//
void CSettingsDialog::OnApply() 
{
	// TODO: Add your control notification handler code here
	if (RefreshData() == FALSE)
		return;

	if (m_pParent)				// Modaless dialog case		
	{
		// Inform the parent, the modaless case
		m_pParent->PostMessage( WM_SETTINGSDIALOG_CLOSE, IDAPPLY, 0 );
	}
}


/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::OnOK() 
// Description: Handler of the OK button
//
void CSettingsDialog::OnOK() 
{
	// TODO: Add extra validation here
	if (RefreshData() == FALSE)
		return;
	if ( m_pParent != NULL )	// Modaless dialog case		
	{
		// Inform the parent, the modaless case
		m_pParent->PostMessage( WM_SETTINGSDIALOG_CLOSE, IDOK, 0 );
	}
	else						// Modal dialog case
	{
		DestroyPages();			// First destroy all the pages
		CDialog::OnOK();	
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::RefreshData()
// Description: Clean up function if OK and Apply buttons are clicked
// Return value: TRUE if successful, or FALSE if failed
//
BOOL CSettingsDialog::RefreshData()
{
	PAGE_INFO *pInfoErr = NULL, *pInfo;

	//First check data
	for (int i = 0; i < m_pInfo.GetSize(); i++)
	{
		pInfo = (PAGE_INFO *)m_pInfo.GetAt(i);
		if (pInfo && pInfo->pPropPage)
		{
			if (::IsWindow(pInfo->pPropPage->m_hWnd))
				if (pInfo->pPropPage->OnWizardFinish() == FALSE)
					return FALSE;
		}
	}

	//Do actual updates
	for (int i = 0; i < m_pInfo.GetSize(); i++)
	{
		pInfo = (PAGE_INFO *)m_pInfo.GetAt(i);
		if (pInfo && pInfo->pPropPage)
		{
			if (::IsWindow(pInfo->pPropPage->m_hWnd))
				pInfo->pPropPage->OnApply();
		}
	}
				
	return (TRUE);		
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::DestroyPages()
// Destoy all the page Windows if the dialog is dismissed
// Return value: TRUE if successful, or FALSE if failed
//
BOOL CSettingsDialog::DestroyPages()
{
    for (int i=0; i<m_pInfo.GetSize(); i++)
    {
        PAGE_INFO *pInfo = (PAGE_INFO *)m_pInfo.GetAt(i);
        if (pInfo && pInfo->pPropPage)
        {
            if (::IsWindow(pInfo->pPropPage->m_hWnd))
            {
                pInfo->pPropPage->DestroyWindow();
                if (!(pInfo->bViewClass))  
                {
                    delete(pInfo->pPropPage);
                    pInfo->pPropPage=NULL;
                }

            }
            else
            {
                delete(pInfo->pPropPage);    
                pInfo->pPropPage=NULL;
            }
        }
        delete pInfo;//delete
        pInfo=NULL;
    }

    m_pInfo.RemoveAll();
    return(true);
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::OnPreferenceHelp() 
// Description: Handler for Help button
//
void CSettingsDialog::OnPreferenceHelp() 
{
	// TODO: Add your control notification handler code here
	HTREEITEM hItem = m_TreeCtrl.GetSelectedItem();
	if (!hItem) return;
	
	PAGE_INFO *pInfo = (PAGE_INFO *) m_TreeCtrl.GetItemData(hItem);
	if (!pInfo || !pInfo->pPropPage) return;
	if (::IsWindow(pInfo->pPropPage->m_hWnd))
	{
		// Help!
		NMHDR nm;
		nm.code = PSN_HELP;
		nm.hwndFrom = m_hWnd;
		nm.idFrom = CSettingsDialog::IDD;
		pInfo->pPropPage->SendMessage(WM_NOTIFY, 0, (long)&nm);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
BOOL CSettingsDialog::PreTranslateMessage(MSG* pMsg) 
{
	// TODO: Add your specialized code here and/or call the base class
	ASSERT(pMsg != NULL);
	ASSERT_VALID(this);
	ASSERT(m_hWnd != NULL);

	// Don't let CDialog process the Escape key.
	if ((pMsg->message == WM_KEYDOWN) && (pMsg->wParam == VK_ESCAPE))
		return TRUE;

	if (CWnd::PreTranslateMessage(pMsg)) return TRUE;

   // Don't translate dialog messages when 
   // application is in help mode
	CFrameWnd* pFrameWnd = GetTopLevelFrame();
	if (pFrameWnd != NULL && pFrameWnd->m_bHelpMode) return FALSE;

	// Ensure the dialog messages will not
	// eat frame accelerators
	pFrameWnd = GetParentFrame();
	while (pFrameWnd != NULL)
	{
		if (pFrameWnd->PreTranslateMessage(pMsg)) return TRUE;
		pFrameWnd = pFrameWnd->GetParentFrame();
	}

	return PreTranslateInput(pMsg);	
//	return CDialog::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::SetTitle(CString sTitle)
// Description: Change the title of the dialog
void CSettingsDialog::SetTitle(CString sTitle)
{
	m_csTitle = sTitle;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsDialog::SetLogoText(CString sText)
// Description: Set the logo text in caption window
//
void CSettingsDialog::SetLogoText(CString sText)
{
	m_csLogoText= sText;
}

/////////////////////////////////////////////////////////////////////////////
//
void CSettingsDialog::ExpandTree()
{
	HTREEITEM hti = m_TreeCtrl.GetRootItem();
	while (hti)
	{
		ExpandBranch(hti);
		hti = m_TreeCtrl.GetNextSiblingItem(hti);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
void CSettingsDialog::ExpandBranch(HTREEITEM hti)
{
	if( m_TreeCtrl.ItemHasChildren( hti ) )
	{
		m_TreeCtrl.Expand( hti, TVE_EXPAND );
		hti = m_TreeCtrl.GetChildItem( hti );
		do
		{
			ExpandBranch( hti );
		} while( (hti = m_TreeCtrl.GetNextSiblingItem( hti )) != NULL );
	}
	m_TreeCtrl.EnsureVisible( m_TreeCtrl.GetSelectedItem() );
}

/////////////////////////////////////////////////////////////////////////////
//
BOOL CSettingsDialog::Create()
{
	return CDialog::Create(CSettingsDialog::IDD);
}

