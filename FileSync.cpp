#include "stdafx.h"
#include "FileSync.h"
#include "Dialog.h"
#include "Entry.h"

static LPCTSTR const applicationDataFolderParts[] = { _T("Adrezdi"), _T("FileSync") };
static LPCTSTR const applicationDataPath = _T("Adrezdi\\FileSync\\Settings.txt");
static UINT const WM_CLIPBOARD_CHANGED = WM_USER;
static UINT const WM_STATUS_NOTIFY = WM_CLIPBOARD_CHANGED + 1;
static UINT const WM_SHOW_ICON = WM_STATUS_NOTIFY + 1;

static std::vector<Entry> entries;
static std::set<tstring> folderPaths;
static HINSTANCE instance;
static HANDLE signal, port, thread;
static UINT taskbarCreatedMessageId;
static bool enabled;

static void LoadSettings() {
	// Delete the current entries.
	entries.clear();

	// Create new entries from the roaming user profile application data folder.
	TCHAR path[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		if(PathAppend(path, applicationDataPath)) {
			FILE* fin;
			if(_tfopen_s(&fin, path, _T("rt")) == 0) {
				for(int i = 0; _fgetts(path, MAX_PATH, fin) != nullptr; ++i) {
					Entry entry;
					if(!entry.CreateFromString(path)) {
						break;
					}
					entry.AddFolder(folderPaths);
					entries.push_back(entry);
				}
				fclose(fin);
			}
		}
	}
}

static void SaveSettings() {
	TCHAR path[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		for(auto* folderPart : applicationDataFolderParts) {
			if(!PathAppend(path, folderPart)) {
				return;
			}
			if(_tmkdir(path) != 0 && errno != EEXIST) {
				return;
			}
		}
		if(PathAppend(path, _T("Settings.txt"))) {
			FILE* fout;
			if(_tfopen_s(&fout, path, _T("wt")) == 0) {
				folderPaths.clear();
				for(int i = 0; i < (int)entries.size(); ++i) {
					entries[i].SaveToFile(fout);
					entries[i].AddFolder(folderPaths);
				}
				fclose(fout);
			}
		}
	}
}

static DWORD WINAPI WatchForChanges(HWND /*window*/) {
	for(;;) {
		// Collect the signal and all folders.
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { signal };
		HANDLE* p = handles;
		for(auto& folderPath : folderPaths) {
			*++p = FindFirstChangeNotification(folderPath.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
		}

		// Wait for a signal or a folder change.
		DWORD result = WaitForMultipleObjects(p - handles + 1, handles, FALSE, INFINITE);

		// Close all folder change notification handles.
		while(p > handles) {
			FindCloseChangeNotification(*p--);
		}

		// Respond to what happened.
		if(result == WAIT_OBJECT_0) {
			// signal
			DWORD n, key;
			LPOVERLAPPED po;
			while(GetQueuedCompletionStatus(port, &n, &key, &po, 0)) {
				if(n != 0) {
					return PostThreadMessage(n, WM_QUIT, 0, 0);
				}
			}
		} else if(result != WAIT_FAILED && enabled) {
			// change
			std::for_each(entries.begin(), entries.end(), std::mem_fun_ref(&Entry::Synchronize));
		}
	}
}

static void AddStatusAreaIcon(HWND window) {
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = window;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_FILESYNC));
	nid.uCallbackMessage = WM_STATUS_NOTIFY;
	StringCbCopy(nid.szTip, sizeof(nid.szTip), _T("Enabled"));
	Shell_NotifyIcon(NIM_ADD, &nid);
}

static void UpdateStatusAreaIcon(HWND window) {
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = window;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_TIP;
	nid.hIcon = LoadIcon(instance, MAKEINTRESOURCE(enabled ? IDI_FILESYNC : IDI_NOFILESYNC));
	StringCbCopy(nid.szTip, sizeof(nid.szTip), enabled ? _T("Enabled") : _T("Disabled"));
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static void RemoveStatusAreaIcon(HWND window) {
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = window;
	nid.uID = 1;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

static void ShowContextMenu(HWND window) {
	POINT pt;
	::GetCursorPos(&pt);
	HMENU menu = CreatePopupMenu();
	if(menu) {
		AppendMenu(menu, MF_STRING | (enabled ? MF_CHECKED : MF_UNCHECKED), IDM_ENABLE, _T("Enabled"));
		AppendMenu(menu, MF_STRING, IDM_SELECT, _T("Select..."));
		AppendMenu(menu, MF_STRING, IDM_ABOUT, _T("About..."));
		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		AppendMenu(menu, MF_STRING, IDM_HIDE, _T("Hide"));
		AppendMenu(menu, MF_STRING, IDM_EXIT, _T("Exit"));
		SetForegroundWindow(window);
		TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, window, NULL);
		DestroyMenu(menu);
	}
}

static void HandleStatusAreaMessage(HWND window, UINT messageId, UINT /*iconId*/) {
	switch(messageId) {
	case WM_CONTEXTMENU:
	case WM_RBUTTONUP:
		ShowContextMenu(window);
		break;
	case WM_LBUTTONDBLCLK:
		PostMessage(window, WM_COMMAND, IDM_ENABLE, 0);
		break;
	}
}

static INT_PTR CALLBACK AboutProcedure(HWND dialog, UINT messageId, WPARAM wParam, LPARAM /*lParam*/) {
	switch(messageId) {
	case WM_COMMAND:
		if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(dialog, LOWORD(wParam));
		}
		return TRUE;
	case WM_INITDIALOG:
		// Allow the system to set the focus.
		return TRUE;
	case WM_GETICON:
		SetWindowLongPtr(dialog, DWL_MSGRESULT, (LONG_PTR)LoadIcon(instance, MAKEINTRESOURCE(wParam == ICON_BIG ? IDR_MAINFRAME : IDI_FILESYNC)));
		return TRUE;
	}
	return FALSE;
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT messageId, WPARAM wParam, LPARAM lParam) {
	switch(messageId) {
	case WM_CREATE:
		enabled = true;
		LoadSettings();
		if(entries.empty()) {
			if(Dialog::SelectFiles(window, entries)) {
				SaveSettings();
			} else {
				return -1;
			}
		}
		signal = CreateEvent(NULL, FALSE, FALSE, NULL);
		port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
		AddStatusAreaIcon(window);
		thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WatchForChanges, window, 0, NULL);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDM_HIDE:
			RemoveStatusAreaIcon(window);
			break;
		case IDM_SELECT:
			if(Dialog::SelectFiles(window, entries)) {
				SaveSettings();
				SetEvent(signal);
			}
			break;
		case IDM_ENABLE:
			enabled = !enabled;
			UpdateStatusAreaIcon(window);
			break;
		case IDM_ABOUT:
			DialogBox(instance, MAKEINTRESOURCE(IDD_ABOUTBOX), window, AboutProcedure);
			break;
		case IDM_EXIT:
			PostMessage(window, WM_CLOSE, 0, 0);
			break;
		}
		break;
	case WM_STATUS_NOTIFY:
		HandleStatusAreaMessage(window, lParam, wParam);
		break;
	case WM_SHOW_ICON:
		enabled = true;
		AddStatusAreaIcon(window);
		LoadSettings();
		break;
	case WM_DESTROY:
		if(thread != NULL) {
			RemoveStatusAreaIcon(window);
			PostQueuedCompletionStatus(port, GetCurrentThreadId(), 0, NULL);
			SetEvent(signal);
		} else {
			PostQuitMessage(0);
		}
		break;
	default:
		if(messageId == taskbarCreatedMessageId) {
			AddStatusAreaIcon(window);
		} else {
			return DefWindowProc(window, messageId, wParam, lParam);
		}
	}
	return 0;
}

int APIENTRY _tWinMain(HINSTANCE instance, HINSTANCE /*previousInstance*/, LPTSTR /*commandLine*/, int /*showCommand*/) {
	::instance = instance;
	taskbarCreatedMessageId = RegisterWindowMessage(_T("TaskbarCreated"));

	// Initialize the common controls to get the new theme.
	INITCOMMONCONTROLSEX iccex = {};
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&iccex);

	// If a current instance is already running, show it.
	HWND currentWindow = FindWindow(_T("Adrezdi:FileSync"), _T("File Synchronizer"));
	if(currentWindow != NULL) {
		PostMessage(currentWindow, WM_SHOW_ICON, 0, 0);
		return 0;
	}

	// Create the window class.
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WindowProcedure;
	wcex.hInstance = instance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = _T("Adrezdi:FileSync");
	ATOM classAtom = RegisterClassEx(&wcex);
	if(classAtom == 0) {
		return 0;
	}

	// Create the window.
	HWND window = CreateWindow(MAKEINTATOM(classAtom), _T("File Synchronizer"), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, NULL, instance, NULL);
	if(window == NULL) {
		return 0;
	}

	// Process messages.
	MSG messageId;
	while(GetMessage(&messageId, NULL, 0, 0)) {
		TranslateMessage(&messageId);
		DispatchMessage(&messageId);
	}

	CloseHandle(signal);
	CloseHandle(port);
	return (int)messageId.wParam;
}
