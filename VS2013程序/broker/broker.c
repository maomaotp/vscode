#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <event.h>
#include <event_struct.h>
#include <WinSock2.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "broker.h"

#define DEBUG

#define LOCAL_IP "127.0.0.1"
#define SOCKET_SERVER_PORT 8888

mrcpList head;
int linkLength = 0;

struct event_base* base;
int i = 0;

int init_server(int* fd)
{
	struct sockaddr_in servaddr;
	WORD request;
	WSADATA ws;

	request = MAKEWORD(1, 1);
	int err = WSAStartup(request, &ws);

	if ((*fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(LOCAL_IP);
	servaddr.sin_port = htons(SOCKET_SERVER_PORT);

	bind(*fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(*fd, 5);
	printf("wait the client:\n");

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

void insert_queue(mrcpList newNode)
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

void print_link()
{
	mrcpList cur = head;
	int len = 1;

	while (cur->next) {
		printf("Node:%d\n>>IPaddr=%s, mrcpId=%s, max=%d, current=%d\n", len++, cur->next->IPaddr, cur->next->mrcpId, cur->next->max, cur->next->current);
		cur = cur->next;
	}
	printf("*********\n\n");
}

void parse_xml(const char* buf, const int bufLen)
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

	insert_queue(newNode);
#ifdef DEBUG
	print_link();
#endif
	xmlFreeDoc(doc);
}

void cat_xml(char* resBuf)
{
	xmlChar* xmlbuf;
	int bufsize;
	char temp[10];
	mrcpList tempNode = head->next;

	xmlDocPtr doc = xmlNewDoc(BAD_CAST"1.0");

	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST"load_balance");
	xmlDocSetRootElement(doc, node);
	xmlNewProp(node, BAD_CAST"IP", BAD_CAST tempNode->IPaddr);
	xmlNewProp(node, BAD_CAST"ID", BAD_CAST tempNode->mrcpId);

	xmlNodePtr grandnode = xmlNewNode(NULL, BAD_CAST "channels");
	xmlAddChild(node, grandnode);
	_itoa_s(tempNode->current, temp, 10, 10);
	xmlNewProp(grandnode, BAD_CAST"current", BAD_CAST temp);

	_itoa_s(tempNode->max, temp, 10, 10);
	xmlNewProp( grandnode, BAD_CAST"max", BAD_CAST temp );

	xmlDocDumpFormatMemory(doc, &xmlbuf, &bufsize, 1);
	strcpy_s(resBuf, bufsize, xmlbuf+1);
	resBuf[bufsize] = '\0';

	xmlFree(xmlbuf);
	xmlFreeDoc(doc);

	return;
}

void onRead(int fd, short iEvent, void* arg)
{
	int iLen;
	//int cmdLen;
	char buf[1024];
	char getBuf[10] = "get";

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
	//printf("recv from client buf:%s\n\n", buf);
#endif

	if ( !strncmp(buf, "get", 3) ) {	
		char resBuf[1024];
	
		cat_xml(resBuf);
#ifdef DEBUG
	printf("response_xml:\n%s\n", resBuf);
#endif    
		send(fd, resBuf, sizeof(resBuf), 0);
	}
	else {
		parse_xml(buf, sizeof(buf));
	}
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

int main()
{
	int fd;
	struct event evListen;

	int ret = init_server(&fd);
	if (ret < 0) {
		printf("client init error\n");
		return;
	}
	head = addNode();

	base = event_base_new();

	event_set(&evListen, fd, EV_READ|EV_PERSIST, onAccept, NULL);
	event_base_set(base, &evListen);
	event_add(&evListen, NULL);
	event_base_dispatch(base);

	return 0;
}
