#include "stdafx.h"
#include "Entry.h"

static TCHAR const* const delimiter = _T("\t");

void Entry::AddFolder(std::set<tstring>& folderPaths) {
	TCHAR folderPath[MAX_PATH];
	StringCbCopy(folderPath, sizeof(folderPath), path1);
	PathRemoveFileSpec(folderPath);
	folderPaths.insert(folderPath);
	StringCbCopy(folderPath, sizeof(folderPath), path2);
	PathRemoveFileSpec(folderPath);
	folderPaths.insert(folderPath);
}

bool Entry::CreateFromString(LPTSTR string) {
	LPTSTR context;
	LPCTSTR t1 = _tcstok_s(string, delimiter, &context);
	LPCTSTR t2 = _tcstok_s(nullptr, delimiter, &context);
	LPCTSTR t3 = _tcstok_s(nullptr, delimiter, &context);
	if(FAILED(StringCchCopy(path1, _countof(path1), t1)) || FAILED(StringCchCopy(path2, _countof(path2), t2)) || t3 == nullptr) {
		return false;
	}
	isTwoWay = *t3 != _T('0');
	return SetTimes();
}

void Entry::SaveToFile(FILE* fout) {
	_fputts(path1, fout);
	_fputts(delimiter, fout);
	_fputts(path2, fout);
	_fputts(delimiter, fout);
	_fputtc(isTwoWay ? _T('1') : _T('0'), fout);
	_fputtc(_T('\n'), fout);
}

static bool SelectFile(HWND window, LPTSTR filePath, bool isPrimary) {
	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = window;
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = isPrimary ? _T("Choose Main File") : _T("Choose Back-up File");
	ofn.Flags = OFN_DONTADDTORECENT | OFN_HIDEREADONLY;
	if(isPrimary) {
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	}
	return !!GetOpenFileName(&ofn);
}

bool Entry::CheckBackup() {
	CopyFile(path1, path2, true);
	return true;
}

bool Entry::SelectFromUser(HWND window) {
	return SelectFile(window, path1, true) && SelectFile(window, path2, false) && CheckBackup() && SetTimes();
}

void Entry::GetPath1(LPTSTR &p) {
	p = path1;
}

void Entry::GetPath2(LPTSTR &p) {
	p = path2;
}

static bool GetTime(LPCTSTR filePath, FILETIME& lastWriteTime) {
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if(GetFileAttributesEx(filePath, GetFileExInfoStandard, &fad)) {
		lastWriteTime = fad.ftLastWriteTime;
		return true;
	}
	return false;
}

void Entry::Synchronize() {
	FILETIME lastWriteTime;
	if(GetTime(path1, lastWriteTime)) {
		if(lastWriteTime1.dwLowDateTime != lastWriteTime.dwLowDateTime
			|| lastWriteTime1.dwHighDateTime != lastWriteTime.dwHighDateTime) {
			// The main file changed.  Copy it to the other file.
			CopyFile(path1, path2, FALSE);
			lastWriteTime1 = lastWriteTime2 = lastWriteTime;
		} else if(isTwoWay && GetTime(path2, lastWriteTime)) {
			if(lastWriteTime2.dwLowDateTime != lastWriteTime.dwLowDateTime
				|| lastWriteTime2.dwHighDateTime != lastWriteTime.dwHighDateTime) {
				// The other file changed.  Copy it to the main file.
				CopyFile(path2, path1, FALSE);
				lastWriteTime1 = lastWriteTime2 = lastWriteTime;
			}
		}
	}
}

bool Entry::SetTimes() {
	return GetTime(path1, lastWriteTime1) && GetTime(path2, lastWriteTime2);
}
