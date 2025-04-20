#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

struct response_data {
	char*  data;
	size_t size;
};

static size_t write_function(char*  data,
                             size_t size,
                             size_t nmemb,
                             void*  clientp)
{
	size_t realsize = size * nmemb;
	struct response_data* r_data = (struct response_data*) clientp;
	
	char* ptr = strdup(data);
	if (!ptr)
		return 0;

	r_data->data = ptr;
	r_data->size = realsize;
	
	return realsize;
}

static struct response_data* http_request(char*              url, 
                                          struct curl_slist* headers,
                                          char*              body)
{
	//--------------------
	// Simple http_request
	//--------------------

	CURL*                 curl;
	CURLcode              res;
	struct response_data* result = malloc(sizeof(struct response_data));

	if (!url) return NULL;

	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)result);

	if (headers)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);		

	if (body)
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);	
		
	res = curl_easy_perform(curl);	
	
	printf("%i\n", res);
	
	curl_easy_cleanup(curl);
	
	return result;
}

void example(void)
{
	struct response_data* ptr;	
	struct curl_slist* headers = NULL;	
	
	headers = curl_slist_append(headers, "accept: application/json");	
	headers = curl_slist_append(headers, "Authorization: Bearer t.cIFSYC4geLKv5e1QMIIszpAoNA27oBccfAf-L8Kxju_vaquY3pniqce7dwEV7GLEk8tdEdRU3Qo-HigTN6rK-w");	
	headers = curl_slist_append(headers, "Content-Type: application/json");	

   	ptr = http_request("https://invest-public-api.tinkoff.ru/rest/tinkoff.public.invest.api.contract.v1.OperationsService/GetPositions", headers, "{\"accountId\":\"2227151115\"}");
	
	printf("%s\n", ptr->data);
	free(ptr->data);
	free(ptr);
}

int main()
{
	example();	
}
