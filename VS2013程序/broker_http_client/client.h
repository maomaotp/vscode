#ifndef _HEAD_BROKET_H
#define _HEAD_BROKET_H

typedef struct _mrcpNode
{
	char mrcpId[20];
	char IPaddr[24];
	int current;
	int max;
	struct _mrcpNode* next;
}mrcpNode, *mrcpList;

int ns__add( int num1, int num2, int* sum );
void onAccept(int fd, short iEvent, void* arg);
void onRead(int fd, short iEvent, void* arg);
void cat_xml(char* resBuf);
void parse_xml(const char* buf, const int bufLen);
void print_link();
void insert_queue(mrcpList newNode);
mrcpList addNode(void);
int init_server(int* fd);

#endif