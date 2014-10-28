#ifndef GREATDING_CTRIP_WORKDIRINITIALIZE_H
#define GREATDING_CTRIP_WORKDIRINITIALIZE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <stdlib.h>
#include <direct.h>
#include <iostream>

//�鿴��ֵ�ĺ���
int GetContent(HKEY ReRootKey,
	char *ReSubKey,
	char *ReValueName,
	__inout unsigned char * content) {
	HKEY hKey;
	int i = 0; //���������0==succeed
	DWORD dwType=REG_SZ; //�����ȡ��������
	DWORD dwLength=1024;
	if (RegOpenKeyEx(ReRootKey,ReSubKey,0,KEY_READ,&hKey)==ERROR_SUCCESS) {
		if (RegQueryValueEx(hKey,ReValueName,NULL,&dwType,(unsigned char *)
			content,&dwLength)!=ERROR_SUCCESS) {
				i=1;
		}
		RegCloseKey(hKey);
	}
	else {
		i = 1;
	}
	return i;
} 

//HKEY_LOCAL_MACHINE\SOFTWARE\Unimrcp
int WorkDirInitialize() {
	HKEY RootKey = HKEY_LOCAL_MACHINE; //ע�����������
	char SubKey[] = "SOFTWARE\\Unimrcp"; //����ע���ֵ�ĵ�ַ
	char ValueName[] ="UnimrcpPath"; //������ֵ������
	unsigned char content[2048] = {0};

	if (GetContent(RootKey,SubKey,ValueName,content)!=0) {
		//cout <<"Get failed"<<endl;
		return -1;
	}
	else {
		//	cout <<"WorkDir:"<<content<<endl;
        char binpath[2048]={0};
        sprintf(binpath,"%s\\bin");
		return(_chdir((char *)binpath));
	}
	
}
#endif