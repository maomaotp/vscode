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

#define LOCAL_IP "127.0.0.1"
#define SOCKET_SERVER_PORT 8888

mrcpList head;
int linkLength = 0;

struct event_base* base;
int i = 0;

int init_win_socket()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return -1;
	}
	return 0;
}

int init_server(int* fd)
{
	struct sockaddr_in servaddr;

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

	parse_xml(buf, sizeof(buf));
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

void generic_handler(struct evhttp_request *req, void *arg)
{
	char resBuf[1024];
	struct evbuffer *buf = evbuffer_new();
	if (NULL == buf)
	{
		puts("failed to create response buffer \n");
		return;
	}
	cat_xml(resBuf);
	printf("response:\n%s", resBuf);

	//evbuffer_expand(buf, sizeof(resBuf));
	evhttp_add_header(req->output_headers, "Server", "my_http");
	evhttp_add_header(req->output_headers, "Content-Type", "text/plain; charset=UTF-8");
	evhttp_add_header(req->output_headers, "Connection", "close");

	evbuffer_add_printf(buf, "Server Responsed. Requested: %s\n", "hello world");
	//evbuffer_add_printf(buf, "Server Responsed: %s\n", resBuf);
	evhttp_send_reply(req, HTTP_OK, "OK", buf);

	evbuffer_free(buf);
}


int event_http(void)
{
	short http_port = 8889;
	char *http_addr = "127.0.0.1";

	struct evhttp * http_server = evhttp_new(base);
	if (NULL == http_server){
		printf("evhttp_new error\n");
		return -1;
	}

	if (evhttp_bind_socket(http_server, http_addr, http_port) != 0){
		printf("evhttp_bind_socket error\n");
		return -1;
	}

	evhttp_set_gencb(http_server, generic_handler, NULL);
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

	event_http();
	/*socket*/
	int fd;
	struct event evListen;

	init_server(&fd);

	head = addNode();

	event_set(&evListen, fd, EV_READ | EV_PERSIST, onAccept, NULL);
	event_base_set(base, &evListen);
	event_add(&evListen, NULL);
	event_base_dispatch(base);

	WSACleanup();
	return 0;
}
