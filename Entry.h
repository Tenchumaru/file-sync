#pragma once
#pragma warning(disable: 4351) /* new behavior: elements of array will be default initialized */

class Entry
{
private:
	TCHAR path1[MAX_PATH];
	TCHAR path2[MAX_PATH];
	FILETIME lastWriteTime1, lastWriteTime2;
	bool isTwoWay;

public:
	Entry() : path1(), path2(), lastWriteTime1(), lastWriteTime2(), isTwoWay(false) {}
	void AddFolder(std::set<tstring>& folderPaths);
	void Create(LPCTSTR path1, LPCTSTR path2);
	bool CreateFromString(LPTSTR string);
	void SaveToFile(FILE* fout);
	void Synchronize();
	bool SelectFromUser(HWND window);
	void GetPath1(LPTSTR& p);
	void GetPath2(LPTSTR& p);
	_declspec(property(get=get_IsTwoWay,put=put_IsTwoWay)) bool IsTwoWay;
	bool get_IsTwoWay() const { return isTwoWay; }
	void put_IsTwoWay(bool value) { isTwoWay= value; }

private:
	bool CheckBackup();
	bool SetTimes();
};
