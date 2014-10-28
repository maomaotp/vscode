#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>


#define POSTURL "localhost:8889"
#define POSTFIELDS "email=myemail@163.com&password=mypassword&autologin=1&submit=�� ¼&type="
#define FILENAME "curlposttest.log"


size_t write_data(void* buffer, size_t size, size_t nmemb, void *stream)
{
	FILE *fptr = (FILE*)stream;
	fwrite(buffer, size, nmemb, fptr);
	return size*nmemb;
}


int main(int argc, char *argv[])
{
	CURL *curl;
	CURLcode res;
	FILE* fptr;
	struct curl_slist *http_header = NULL;


	fopen_s(&fptr, FILENAME, "w");

	curl = curl_easy_init();
	if (!curl)
	{
		fprintf(stderr, "curl init failed\n");
		return -1;
	}


	curl_easy_setopt(curl, CURLOPT_URL, POSTURL); //url��ַ
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, POSTFIELDS); //post����
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data); //�Է��ص����ݽ��в����ĺ�����ַ
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fptr); //����write_data�ĵ��ĸ�����ֵ
	curl_easy_setopt(curl, CURLOPT_POST, 1); //�����ʷ�0��ʾ���β���Ϊpost
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); //��ӡ������Ϣ
	curl_easy_setopt(curl, CURLOPT_HEADER, 1); //����Ӧͷ��Ϣ����Ӧ��һ�𴫸�write_data
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); //����Ϊ��0,��Ӧͷ��Ϣlocation
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "/Users/zhu/CProjects/curlposttest.cookie");


	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
	{
		switch (res)
		{
		case CURLE_UNSUPPORTED_PROTOCOL:
			fprintf(stderr, "��֧�ֵ�Э��,��URL��ͷ��ָ��\n");
		case CURLE_COULDNT_CONNECT:
			fprintf(stderr, "�������ӵ�remote�������ߴ���\n");
		case CURLE_HTTP_RETURNED_ERROR:
			fprintf(stderr, "http���ش���\n");
		case CURLE_READ_ERROR:
			fprintf(stderr, "�������ļ�����\n");
		default:
			fprintf(stderr, "����ֵ:%d\n", res);
		}
		return -1;
	}


	curl_easy_cleanup(curl);


}
