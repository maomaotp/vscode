#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <WinSock2.h>
#include <windows.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <curl/curl.h>

#include "loadManageClient.h"

//#define DEBUG

#define RETURN_OK			0
#define RETURN_ERROR		-1

#define LOAD_STATUS			WM_USER+100
#define POST_LOG			WM_USER+200
#define POSTURL				"http://localhost:8888"

static int sockfd;
static int threadId;

char serverIp[20];
int serverPort;

void initAddrAndPort(const char* ip, const short port)
{
	memset(serverIp, 0, sizeof(serverIp));

	strcpy_s(serverIp, sizeof(serverIp), ip);
	serverPort = port;
}

int init_win_socket()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return -1;
	}
	return 0;
}

void createXml(const mrcpInfo* statusInfo, char* resBuf)
{
	xmlChar* xmlbuf;
	int bufsize;
	char temp[10];

	xmlDocPtr doc = xmlNewDoc(BAD_CAST"1.0");

	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST"load_balance");
	xmlDocSetRootElement(doc, node);
	xmlNewProp(node, BAD_CAST"IP", BAD_CAST statusInfo->IPaddr);
	xmlNewProp(node, BAD_CAST"ID", BAD_CAST statusInfo->mrcpId);

	xmlNodePtr grandnode = xmlNewNode(NULL, BAD_CAST "channels");
	xmlAddChild(node, grandnode);
	_itoa_s(statusInfo->current, temp, 10, 10);
	xmlNewProp(grandnode, BAD_CAST"current", BAD_CAST temp);

	_itoa_s(statusInfo->max, temp, 10, 10);
	xmlNewProp(grandnode, BAD_CAST"max", BAD_CAST temp);

	xmlDocDumpFormatMemory(doc, &xmlbuf, &bufsize, 1);
	strcpy_s(resBuf, bufsize + 1, xmlbuf);
	resBuf[bufsize + 1] = '\0';

	xmlFree(xmlbuf);
	xmlFreeDoc(doc);

	return;
}

int connectToBroker()
{
	struct sockaddr_in servaddr;

	if ( init_win_socket() < 0 ) {
		printf("init_win_socket error\n");
		return RETURN_ERROR;
	}

	if ((NULL == serverIp) || (0 == serverPort)) {
		printf("server IP is NULL and port=0\n");
		return RETURN_ERROR;
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket:error", GetLastError());
		return GetLastError();
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(serverPort);
	servaddr.sin_addr.s_addr = inet_addr(serverIp);
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		printf("connect to %s:%d    error:%d\n", serverIp, serverPort, GetLastError());
		return RETURN_ERROR;
	}
#ifdef DEBUG
	else printf("connect successed\n");
#endif
	return RETURN_OK;
}

//子线程函数
unsigned int __stdcall ThreadFun(void *param)
{
	MSG msg;
	char buf[1024];
	mrcpInfo *statusInfo;
	logInfo *logMessage;
	CURL *curl;
	CURLcode res;

	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	if (connectToBroker() < 0){
		printf("connect to broker error\n");
	}

	Sleep(1000);
	printf("thread func start:threadId = %d\n", GetCurrentThreadId());

	while (1) {
		if (GetMessage(&msg, 0, 0, 0)) {
			switch (msg.message) {
			case LOAD_STATUS:
#ifdef DEBUG
				printf("get msg.LOAD_STATUS(%d) from message queue\n", LOAD_STATUS);
#endif
				statusInfo = (mrcpInfo *)msg.wParam;
				createXml(statusInfo, buf);
				if (send(sockfd, buf, sizeof(buf), 0) <= 0) {
					connectToBroker();
					printf("send status error, and one more connect to broker\n");
				}
				break;
			case POST_LOG:
				logMessage = (logInfo *)msg.wParam;

				curl_global_init(CURL_GLOBAL_ALL);

				struct curl_slist *headerlist = curl_slist_append(NULL, "Content-Type:text/xml;charset=UTF-8");
				char postfields[1024];
				_snprintf_s(postfields, sizeof(postfields), sizeof(postfields), "?callSeq=%s&ttsReqSerialNumber=%s&ip=%s&ttsVoiceFileName=%s&ttsTxt=%s&beginTime=%s&endTime=%s&spendTime=%s&synthesisResult=%s&spare1=%s&spare2=%s", 
					logMessage->callSeq, logMessage->ttsReqSerialNumber, logMessage->ip, logMessage->ttsVoiceFileName, logMessage->ttsTxt, 
					logMessage->beginTime, logMessage->endTime, logMessage->spendTime, logMessage->synthesisResult, logMessage->spare1, logMessage->spare2);

#ifdef DEBUG
				printf("get msg.POST_LOG(%d) from message queue\n\n", POST_LOG);
				printf("postfields==%s\n", postfields);
#endif

				curl = curl_easy_init();
				if (curl) {
					curl_easy_setopt(curl, CURLOPT_URL, POSTURL);
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
					curl_easy_setopt(curl, CURLOPT_POST, 1);
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
					//curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
					//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
					//curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
					//curl_easy_setopt(curl, CURLOPT_WRITEDATA, ResContent);

					/* Perform the request */
					res = curl_easy_perform(curl);
					if (res != CURLE_OK) {
						fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
					}
					/*
					long retcode = 0;
					res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
					if ((res == CURLE_OK) && retcode == 200){
						printf("ResContent==%s\n", ResContent);
					}
					*/

					curl_easy_cleanup(curl);
					curl_slist_free_all(headerlist);
				}
				break;

			}
		}
	}
}

int initBroker(const char* ip, const short port)
{
	HANDLE handle;
	DWORD  ExitNum;

	if ((ip == NULL) || (port == 0)) {
		printf("init client error:serverIp is NULL or port =0\n");
		return RETURN_ERROR;
	}

	initAddrAndPort(ip, port);
	handle = (HANDLE)_beginthreadex(NULL, 0, &ThreadFun, NULL, 0, &threadId);
	if (handle == NULL) {
		printf("start thread failed,errno:%d\n", GetLastError());
		return GetLastError();
	}
	
	GetExitCodeThread(handle, &ExitNum);
	if (ExitNum == 0){
		return RETURN_ERROR;
	}
	
	//WaitForSingleObject(handle, INFINITE);
	return threadId;
}

void updateStatus(void *status)
{
	if (status == NULL) {
		printf("load_status pointer is NULL\n");
		return;
	}

	if (!PostThreadMessage(threadId, LOAD_STATUS, (WPARAM)status, 0)) {
		printf("post to threadID:%d error:%d\n", threadId, GetLastError());
	}
}

void postLog(void *logMessage)
{
	if (logMessage == NULL) {
		printf("load_status pointer is NULL\n");
		return;
	}
	if (!PostThreadMessage(threadId, POST_LOG, (WPARAM)logMessage, 0)) {
		printf("post to threadID:%d error:%d\n", threadId, GetLastError());
	}
}

/*
int main(int argc, char* argv[])
{
	mrcpInfo status;
	int threadId;
	int i = 22;
	if ( (threadId = init_client("127.0.0.1", 8888)) < 0){
		printf("init error");
		return -1;
	}
	Sleep(1000);

	while (1) {
		if (200 == i) {
			i = 0;
		}
		_snprintf_s(status.IPaddr, sizeof(status.IPaddr), 20, "192.168.0.%d", i++);
		_snprintf_s(status.mrcpId, sizeof(status.mrcpId), 20, "mrcpId%d", i);
		status.current = 1000 - i;
		status.max = 100;

		updateStatus(&status);

		//for (int j = 0; j < 10; j++) {
		//	printf("i am working and no blocked\n");
		//}
		Sleep(1000);
	}
	return 0;
}
*/