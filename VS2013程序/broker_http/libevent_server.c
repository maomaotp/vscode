#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <Winsock2.h>
#include <stdlib.h>
#include <stdio.h>

int init_win_socket()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return -1;
	}
	return 0;
}

void generic_handler(struct evhttp_request *req, void *arg)
{
	struct evbuffer *buf = evbuffer_new();
	if (!buf)
	{
		puts("failed to create response buffer \n");
		return;
	}

	//evbuffer_add_printf(buf, "Server Responsed. Requested: %s\n", evhttp_request_get_uri(req));
	evbuffer_add_printf(buf, "Server Responsed. Requested: %s\n", arg);
	evhttp_send_reply(req, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}

int main(int argc, char* argv[])
{
	init_win_socket();

	short http_port = 8888;
	char *http_addr = "127.0.0.1";

	struct event_base * base = event_base_new();

	struct evhttp * http_server = evhttp_new(base);
	if (!http_server)
	{
		return -1;
	}

	int ret = evhttp_bind_socket(http_server, http_addr, http_port);
	if (ret != 0)
	{
		return -1;
	}

	evhttp_set_gencb(http_server, generic_handler, "hello world");

	printf("http server start OK! \n");

	event_base_dispatch(base);

	evhttp_free(http_server);

	WSACleanup();
	return 0;
}