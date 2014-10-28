#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

#include <event.h>
#include <buffer.h>
#include <event_struct.h>
#include <http_struct.h>
#include <http.h>

#include <WinSock2.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "broker.h"

#define DEBUG

#define LOCAL_IP		"127.0.0.1"
#define SOCKET_PORT		8888
#define HTTP_PORT		8889

static mrcpList head;
static struct event_base *base;

int init_win_socket()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return -1;
	}
	return 0;
}

int initServer(int* fd)
{
	struct sockaddr_in servaddr;

	if ((*fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(LOCAL_IP);
	servaddr.sin_port = htons(SOCKET_PORT);

	bind(*fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(*fd, 5);

	return 0;
}

mrcpList addNode(void)
{
	mrcpList newNode = (mrcpList)malloc(sizeof(mrcpNode));
	newNode->next = NULL;
	if (newNode == NULL) {
		printf("malloc error\n");
		return NULL;
	}
	newNode->next = NULL;

	return newNode;
}

void insertLink(mrcpList newNode)
{
	if (head->next == NULL){
		newNode->next = NULL;
		head->next = newNode;
		return;
	}

	mrcpList curNode = head;
	while (curNode->next) {
		if ( !strncmp(curNode->next->mrcpId, newNode->mrcpId, sizeof(newNode->mrcpId)) ) {
			mrcpList tempNode;
			tempNode = curNode->next;
			curNode->next = tempNode->next;
			free(tempNode);
			tempNode = NULL;
			continue;
		}
		curNode = curNode->next;
	}

	curNode = head;
	while(curNode->next) {
		if (newNode->current < curNode->next->current) {
			newNode->next = curNode->next;
			curNode->next = newNode;
			return;
		}
		curNode = curNode->next;
	}
	curNode->next = newNode;
}

void printLink()
{
	mrcpList cur = head;
	int len = 1;

	while (cur->next) {
		printf("Node:%d\n>>IPaddr=%s, mrcpId=%s, max=%d, current=%d\n", len++, cur->next->IPaddr, cur->next->mrcpId, cur->next->max, cur->next->current);
		cur = cur->next;
	}
	printf("*********\n\n");
}

void parseXml(const char* buf, const int bufLen)
{
	xmlDocPtr doc;
	xmlNodePtr curNode;
	xmlNodePtr childNode;

	xmlKeepBlanksDefault(0);
	doc = xmlParseMemory(buf, bufLen);
	if(doc == NULL) {
		printf("doc is NULL\n");
		return;
	}

	curNode = xmlDocGetRootElement(doc);
	childNode = curNode->children;
	if (childNode == NULL || curNode == NULL) {
		printf("parse xml error!!!\n");
		xmlFreeDoc(doc);
		return;
	}
	mrcpList newNode = addNode();

	strcpy_s( newNode->mrcpId, 20, xmlGetProp(curNode, BAD_CAST "ID") );
	strcpy_s( newNode->IPaddr, 24, xmlGetProp(curNode, BAD_CAST "IP") );
	newNode->max = atoi( xmlGetProp(childNode, BAD_CAST "max") );
	newNode->current = atoi( xmlGetProp(childNode, BAD_CAST "current") );

	insertLink(newNode);
#ifdef DEBUG
	printLink();
#endif
	xmlFreeDoc(doc);
}

void createXml(char* resBuf)
{
	xmlChar* xmlbuf;
	int bufsize;
	//mrcpList tempNode = head->next;

	xmlDocPtr doc = xmlNewDoc(BAD_CAST"1.0");

	xmlNodePtr firstnode = xmlNewNode(NULL, BAD_CAST"monitorInfo");
	xmlDocSetRootElement(doc, firstnode);

	xmlNodePtr secondnode = xmlNewNode(NULL, BAD_CAST "ttsResources0001");
	xmlNodePtr ipnode = xmlNewNode(NULL, BAD_CAST"ttsIp");
	xmlNodePtr usernode = xmlNewNode(NULL, BAD_CAST"userNumber");
	xmlNodePtr freenode = xmlNewNode(NULL, BAD_CAST"freeNumber");
	xmlNodePtr restartnode = xmlNewNode(NULL, BAD_CAST"resartNumber");
	
	xmlAddChild(firstnode, secondnode);

	xmlAddChild(secondnode, ipnode);
	xmlAddChild(secondnode, usernode);
	xmlAddChild(secondnode, freenode);
	xmlAddChild(secondnode, restartnode);
	//set content
	xmlAddChild(ipnode, xmlNewText(BAD_CAST "192.168.2.2"));
	xmlAddChild(usernode, xmlNewText(BAD_CAST "5"));
	xmlAddChild(freenode, xmlNewText(BAD_CAST "25"));
	xmlAddChild(restartnode, xmlNewText(BAD_CAST "0"));

	xmlDocDumpFormatMemory(doc, &xmlbuf, &bufsize, 1);
	strcpy_s(resBuf, bufsize+1, xmlbuf);
	resBuf[bufsize+1] = '\0';

	xmlFree(xmlbuf);
	xmlFreeDoc(doc);

	return;
}

void onRead(int fd, short iEvent, void* arg)
{
	int iLen;
	//int cmdLen;
	char buf[1024];

	memset(&buf, 0, sizeof(buf));
	iLen = recv(fd, buf, sizeof(buf), 0);
	if(iLen <= 0) {
		printf("client close!\n");
		struct event* pEvRead = (struct event*)arg;
		event_del(pEvRead);
		free(pEvRead);
		return;
	}
#ifdef DEBUG
	printf("recv from client buf:%s\n\n", buf);
#endif

	parseXml(buf, sizeof(buf));
	return;
}

void onAccept(int fd, short iEvent, void* arg)
{
	int clientfd;
	struct sockaddr_in caddr;

	size_t clen = sizeof(caddr);
	clientfd = accept(fd, (struct sockaddr*)&caddr, &clen);

	struct event* pEvRead = NULL;
	pEvRead = (struct event*)malloc(sizeof(struct event));
	if(pEvRead == NULL) {
		printf("malloc error\n");
		return;
	}
	event_set(pEvRead, clientfd, EV_READ|EV_PERSIST, onRead, pEvRead);
	event_base_set(base, pEvRead);
	event_add(pEvRead, NULL);
}

void genericHandler(struct evhttp_request *req, void *arg)
{
	char resBuf[1024];
	struct evbuffer *evBuf = evbuffer_new();

	if (NULL == evBuf){
		puts("failed to create response buffer \n");
		return;
	}
	createXml(resBuf);
#ifdef DEBUG
	char *url;
	printf("the host is: %s \n", req->remote_host);
	url = req->uri;
	printf("requestUrl:<%s>\n\n%s\n", url, resBuf);
#endif 

	//evbuffer_expand(buf, sizeof(resBuf));
	evhttp_add_header(req->output_headers, "Server", "broker");
	evhttp_add_header(req->output_headers, "Content-Type", "text/xml; charset=UTF-8");
	evhttp_add_header(req->output_headers, "Connection", "close");

	evbuffer_add_printf(evBuf, "%s\n", resBuf);
	evhttp_send_reply(req, HTTP_OK, "OK", evBuf);

	evbuffer_free(evBuf);
}

int eventHttp(void)
{
	struct evhttp * httpServer = evhttp_new(base);
	if (NULL == httpServer){
		printf("evhttp_new error\n");
		return -1;
	}

	if (evhttp_bind_socket(httpServer, LOCAL_IP, HTTP_PORT) != 0){
		printf("evhttp_bind_socket error\n");
		return -1;
	}

	evhttp_set_gencb(httpServer, genericHandler, NULL);
	printf("http server start OK! \n");
	return 0;
}

int main()
{
	init_win_socket();

	base = event_base_new();
	if (NULL == base) {
		printf("event_base_new error\n");
		return -1;
	}

	eventHttp();

	int fd;
	struct event evListen;

	initServer(&fd);

	head = addNode();

	event_set(&evListen, fd, EV_READ | EV_PERSIST, onAccept, NULL);
	event_base_set(base, &evListen);
	event_add(&evListen, NULL);
	event_base_dispatch(base);

	WSACleanup();
	return 0;
}
