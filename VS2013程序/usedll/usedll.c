#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "loadManageClient.h"

#pragma comment(lib,"client_dll.lib")

#define DEBUG
mrcpInfo status;
/*
int main(int argc, char* argv[])

{
	int i = 0;
	int j = 0;

	int threadId = init_client("127.0.0.1", 8888);
	printf("threadId = %d\n",threadId);

	if (argc < 2) {
		printf("argc <2 error!\n");
		return;
	}
	Sleep(1000);
	_snprintf_s(status.IPaddr, sizeof(status.IPaddr), 20, "192.168.0.%d", i);
	_snprintf_s(status.mrcpId, sizeof(status.mrcpId), 20, "mrcpId%d", i);
	status.current = i;
	status.max = 100;
	updateStatus(&status);
	Sleep(1000);

	return 0;
}
*/
int main(int argc, char* argv[])
{
	mrcpInfo loadStatus;
	int threadId;
	int i = 22;
	if ((threadId = initBroker("127.0.0.1", 8888)) < 0){
		printf("init error");
		return -1;
	}
	Sleep(1000);
#if 0
	logInfo logMessage;
	logMessage.beginTime = "20140915";
	logMessage.callSeq = "1001";
	logMessage.endTime = "20140915";
	logMessage.ip = "192.168.1.1";
	logMessage.spare1 = "001";
	logMessage.spare2 = "001";
	logMessage.spendTime = "20";
	logMessage.synthesisResult = "002";
	logMessage.ttsReqSerialNumber = "200";
	logMessage.ttsTxt = "testTEST";
	logMessage.ttsVoiceFileName = "file1";

	postLog(&logMessage);
	postLog(&logMessage);
	Sleep(100000);
#endif

#if 1
	while (1) {
		if (200 == i) {
			i = 0;
		}
		_snprintf_s(loadStatus.IPaddr, sizeof(loadStatus.IPaddr), 20, "192.168.0.%d", i++);
		_snprintf_s(loadStatus.mrcpId, sizeof(loadStatus.mrcpId), 20, "mrcpId%d", i);
		loadStatus.current = 1000 - i;
		loadStatus.max = 100;

		updateStatus(&loadStatus);

		//for (int j = 0; j < 10; j++) {
		//	printf("i am working and no blocked\n");
		//}
		Sleep(1000);
	}
#endif
	return 0;
}