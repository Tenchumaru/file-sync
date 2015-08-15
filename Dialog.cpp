#include "stdafx.h"
#include "FileSync.h"
#include "Dialog.h"

static HINSTANCE instance;

struct LAYOUT
{
	UINT id;
	enum { Stay, Move, Size } xMode, yMode;
	RECT rect;
};

static LAYOUT* s_layout;
static LAYOUT* s_pDialogLayout;

static void InitializeLayout(HWND dialog, LAYOUT layout[])
{
	int i;
	for(i= 0; layout[i].id != 0; ++i)
	{
		HWND control= ::GetDlgItem(dialog, layout[i].id);
		::GetWindowRect(control, &layout[i].rect);
		::MapWindowRect(NULL, dialog, &layout[i].rect);
	}
	s_pDialogLayout= (s_layout= layout) + i;
	::GetClientRect(dialog, &s_pDialogLayout->rect);
}

static void UpdateLayout(HWND dialog, int cx, int cy)
{
	LONG xDiff= cx - s_pDialogLayout->rect.right;
	LONG yDiff= cy - s_pDialogLayout->rect.bottom;
	s_pDialogLayout->rect.right= cx;
	s_pDialogLayout->rect.bottom= cy;
	for(LAYOUT* p= s_layout; p != s_pDialogLayout; ++p)
	{
		HWND control= ::GetDlgItem(dialog, p->id);
		RECT& rect= p->rect;
		switch(p->xMode)
		{
		case LAYOUT::Move:
			::OffsetRect(&rect, xDiff, 0);
			break;
		case LAYOUT::Size:
			rect.right += xDiff;
			break;
		}
		switch(p->yMode)
		{
		case LAYOUT::Move:
			::OffsetRect(&rect, 0, yDiff);
			break;
		case LAYOUT::Size:
			rect.bottom += yDiff;
			break;
		}
		::MoveWindow(control, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
}

///////////////////////////////////////////////////////////////////////////////

static bool CreateListViewColumns(HWND hWndListView)
{
	LPTSTR headers[]= { _T("Two-way"), _T("Main File"), _T("Back-up File") };
	LVCOLUMN lvc;
	lvc.mask= LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt= LVCFMT_LEFT;
	for(int i= 0; i < _countof(headers); ++i)
	{
		lvc.iSubItem= i;
		lvc.cx= i == 0 ? 60 : 235;
		lvc.pszText= headers[i];
		if(ListView_InsertColumn(hWndListView, i, &lvc) == -1)
			return false;
	}
	return true;
}

static bool AppendItem(HWND listView, Entry& entry)
{
	// Save the two-way state since the LVN_ITEMCHANGED handler below will set
	// it to false when I add the item here.
	bool isTwoWay= entry.IsTwoWay;
	int index= ListView_GetItemCount(listView);
	LVITEM listItem;
	listItem.mask= LVIF_PARAM;
	listItem.iItem= index;
	listItem.iSubItem= 0;
	listItem.lParam= (LPARAM)&entry;
	if(ListView_InsertItem(listView, &listItem) == -1)
		return false;
	listItem.mask= LVIF_TEXT;
	listItem.iSubItem= 1;
	entry.GetPath1(listItem.pszText);
	if(!ListView_SetItem(listView, &listItem))
		return false;
	listItem.mask= LVIF_TEXT;
	listItem.iSubItem= 2;
	entry.GetPath2(listItem.pszText);
	if(!ListView_SetItem(listView, &listItem))
		return false;
	ListView_SetCheckState(listView, index, isTwoWay);
	return true;
}

static bool AppendItems(HWND listView, std::vector<Entry>& entries)
{
	for(std::vector<Entry>::iterator i= entries.begin(); i != entries.end(); ++i)
	{
		if(!AppendItem(listView, *i))
			return false;
	}
	return true;
}

static bool AddEntry(HWND dialog, std::vector<Entry>& entries)
{
	Entry entry;
	if(entry.SelectFromUser(dialog))
	{
		entries.push_back(entry);
		AppendItem(GetDlgItem(dialog, IDC_LIST), entry);
		return true;
	}
	return false;
}

static void DeleteItem(int i, HWND listView, std::vector<Entry>& entries)
{
	ListView_DeleteItem(listView, i);
	entries[i]= entries.back();
	entries.pop_back();
}

static BOOL OnInitDialog(HWND dialog, HWND /*focusWindow*/, LPARAM /*lParam*/)
{
	// Initialize the layout logic with the current state of the window and
	// the controls involved in the dynamic layout.
	static LAYOUT layout[]= {
		{ IDC_LIST, LAYOUT::Size, LAYOUT::Size },
		{ IDM_SELECT, LAYOUT::Move, LAYOUT::Move },
		{ IDOK, LAYOUT::Move, LAYOUT::Move },
		{} // null terminator
	};
	InitializeLayout(dialog, layout);

	return FALSE;
}

static void OnSize(HWND dialog, UINT state, int cx, int cy)
{
	if(state != SIZE_MINIMIZED)
		UpdateLayout(dialog, cx, cy);
}

static INT_PTR CALLBACK SelectProcedure(HWND dialog, UINT messageId, WPARAM wParam, LPARAM lParam)
{
	static std::vector<Entry>* entries;
	static HWND listView;
	switch(messageId)
	{
		HANDLE_MSG(dialog, WM_SIZE, OnSize);
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(dialog, LOWORD(wParam));
			return TRUE;
		case IDM_SELECT:
			AddEntry(dialog, *entries);
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if(ListView_GetItemCount(listView) > 0)
		{
			LPNMITEMACTIVATE lpnmitem= (LPNMITEMACTIVATE)lParam;
			int i= lpnmitem->iItem;
			if(lpnmitem->hdr.code == LVN_ITEMCHANGED)
			{
				UINT checkState= ListView_GetCheckState(listView, i);
				entries->at(i).IsTwoWay= checkState != 0;
			}
			else if(lpnmitem->hdr.code == NM_RCLICK && i >= 0)
			{
				RECT rect;
				GetWindowRect(listView, &rect);
				POINT pt= { lpnmitem->ptAction.x + rect.left, lpnmitem->ptAction.y + rect.top };
				HMENU menu= CreatePopupMenu();
				if(menu)
				{
					UINT flags= MF_BYPOSITION | MF_STRING;
					InsertMenu(menu, (UINT)-1, flags, IDM_HIDE, _T("Delete"));
					InsertMenu(menu, (UINT)-1, flags, IDM_EXIT, _T("Cancel"));
					flags= TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_RIGHTBUTTON;
					bool choseDelete= TrackPopupMenu(menu, flags, pt.x, pt.y, 0, dialog, NULL) == IDM_HIDE;
					DestroyMenu(menu);
					if(choseDelete)
						DeleteItem(i, listView, *entries);
				}
			}
			else if(lpnmitem->hdr.code == LVN_KEYDOWN)
			{
				LPNMLVKEYDOWN lpkd= (LPNMLVKEYDOWN)lpnmitem;
				if(lpkd->wVKey == VK_DELETE)
				{
					i= ListView_GetSelectionMark(listView);
					if(i >= 0)
						DeleteItem(i, listView, *entries);
				}
			}
		}
		break;
	case WM_INITDIALOG:
		entries= (std::vector<Entry>*)lParam;
		listView= GetDlgItem(dialog, IDC_LIST);
		ListView_SetExtendedListViewStyle(listView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
		CreateListViewColumns(listView);
		AppendItems(listView, *entries);
		return OnInitDialog(dialog, NULL, lParam);
	case WM_GETICON:
		SetWindowLongPtr(dialog, DWL_MSGRESULT,
			(LONG_PTR)LoadIcon(instance, MAKEINTRESOURCE(wParam == ICON_BIG ? IDR_MAINFRAME : IDI_FILESYNC)));
		return TRUE;
	}
	return FALSE;
}

bool Dialog::SelectFiles(HWND window, std::vector<Entry>& entries)
{
	instance= GetModuleHandle(NULL);
	DialogBoxParam(instance, MAKEINTRESOURCE(IDD_FILESYNC_DIALOG), window, SelectProcedure, (LPARAM)&entries);
	return true;
}
