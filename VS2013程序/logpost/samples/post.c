#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

int main(int argc, char *argv[])
{
	CURL *curl;
	CURLcode res;
	
	if (argc < 3){
		printf("<fm_server IP operaName>");
		return -1;
	}

	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	struct curl_slist *plist = curl_slist_append(NULL, "Content-Type:text/xml;charset=UTF-8");
	//char *postfields = "clientId=iosClient&timestamp=20140101120000&cityName=jilin&radioId=35&provinceSpell=jilin&fmName=AsiaFM";
	char *postfields = "clientId=iosClient&timestamp=20140101120000&radioId=35&provinceSpell=jilin&fmName=AsiaFM";

	char uri[40];
	_snprintf_s(uri, sizeof(uri), 41, "%s:8888/?opName=%s", argv[1], argv[2]);
	printf("uri=%s\n", uri);

	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, uri);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, plist);
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
		
		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		curl_easy_cleanup(curl);
		
		/* then cleanup the formpost chain */
		curl_formfree(formpost);
		/* free slist */
		curl_slist_free_all (headerlist);
	}
	return 0;
}