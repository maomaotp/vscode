#ifndef _HEAD_CLIENT_H
#define _HEAD_CLIENT_H

typedef struct _mrcpNode
{
	char mrcpId[20];
	char IPaddr[20];
	int current;
	int max;
}mrcpInfo;

typedef struct _logInfo
{
	char *callSeq;
	char *ttsReqSerialNumber;
	char *ip;
	char *ttsVoiceFileName;
	char *ttsTxt;
	char *beginTime;
	char *endTime;
	char *spendTime;
	char *synthesisResult;
	char *spare1;
	char *spare2;
}logInfo;

extern __declspec(dllexport) int initBroker(const char* ip, const short port);
extern __declspec(dllexport) void updateStatus(void *status);
extern __declspec(dllexport) void postLog(void *logMessage);

#endif