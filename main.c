#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#define TOKEN_MAX_SIZE 128

#define CREATE_URL(url) "https://invest-public-api.tinkoff.ru/rest/tinkoff.public.invest.api.contract.v1."#url


#define LOAD_BASE_HEADERS(list) \
do { \
    list = curl_slist_append( list, \
                              "accept: application/json"); \
	list = curl_slist_append( list,\
                              get_key_value_token("token.txt"));\
	list = curl_slist_append( list,\
                                    "Content-Type: application/json");\
} while (0);\


struct response_data {
	char*  data;
	size_t size;
};

struct sum_data {
	double units;	
	double nano;	
};

static char* get_token(const char* filename)
{
	FILE* fp;
	char* token;

	token = malloc(TOKEN_MAX_SIZE);
	fp = fopen(filename, "r");
	if (!fp)
		return NULL;

	fgets(token, TOKEN_MAX_SIZE, fp);

	return token;
}

static char* get_key_value_token (const char* filename)
{
	char* token = get_token(filename);
	char* key  = malloc(strlen("Authorization: Bearer ") +  TOKEN_MAX_SIZE);
	strcpy(key, "Authorization: Bearer ");	
	strcat(key, token);
	key[strlen(key) - 1] = '\0';

	return key;	
}

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

static struct response_data* http_request(const char*        url, 
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
		
	curl_easy_cleanup(curl);
	
	return result;
}


static char* get_shareName_by_figi(char* figiName)
{
		
	struct response_data* json_data;
	struct curl_slist*    header_list = NULL;
	cJSON* json;	
	char* body = malloc(256);
	
	char* share_name;

	LOAD_BASE_HEADERS(header_list);	
	
	snprintf(body, 256, "{\"%s\": \"%i\",\"%s\": \"%s\"}", "idType", 1, "id", figiName);

	json_data = http_request(CREATE_URL(InstrumentsService/ShareBy),
                             header_list,
                             body);		
	

	if (!json_data) return NULL;

	json = cJSON_Parse(json_data->data);	
	json = cJSON_GetObjectItemCaseSensitive(json, "instrument");
	json = cJSON_GetObjectItemCaseSensitive(json, "name");

	if (!json) return NULL;

	if (cJSON_IsString(json) && (json->valuestring != NULL))
		share_name = strdup(json->valuestring);	
	
	free(body);
	free(json_data->data);
	free(json_data);	
	
	return share_name;
}

static char* get_account_id(void)
{
	struct response_data* rdata;
	struct curl_slist*    headers;
	cJSON*                json;
	cJSON*                account_json;
	char*                 id;

	LOAD_BASE_HEADERS(headers);

	rdata = http_request(CREATE_URL(UsersService/GetAccounts), headers, "{}");

	if (!rdata) return NULL;

	json = cJSON_Parse(rdata->data);
	json = cJSON_GetObjectItemCaseSensitive(json, "accounts");

	cJSON_ArrayForEach(account_json, json) {
		cJSON* tmp_json = cJSON_GetObjectItemCaseSensitive(account_json, "id");	
		if (cJSON_IsString(tmp_json) && (tmp_json->valuestring != NULL)) 
			id = strdup(tmp_json->valuestring);	
	}	
			
	cJSON_Delete(json);
	free(rdata->data);
	free(rdata);

	return id; 
}



static struct sum_data* get_quotation(cJSON* json)
{
	cJSON*      tmp_json;
	struct sum_data* sptr = malloc(sizeof(struct sum_data));
	
	tmp_json    = cJSON_GetObjectItemCaseSensitive(json, "units");			
	sptr->units = atoi(tmp_json->valuestring);
	
	tmp_json   = cJSON_GetObjectItemCaseSensitive(json, "nano");
	sptr->nano = tmp_json->valuedouble;

	return sptr; 
}

static double get_money_value(cJSON* json)
{
	struct sum_data* sptr = get_quotation(json);
	
	double money = sptr->units;
	double nano  = sptr->nano;

	nano *= 0.000000001;

	free(sptr);	

	return money + nano;
}


static char* get_daily_results(cJSON* json)
{
	double money;
	char*  result_text = malloc(64);  

	if (!result_text)
		return NULL;

	if ((money = get_money_value(json)) < 0)
		snprintf(result_text, 64, "падение на <font color=FF0000>%.2f%</font>", money);	
	else if (money > 0)	
		snprintf(result_text, 64, "рост на <font color=008000>%.2f%</font>", money);	
	else
		snprintf(result_text, 64, "изменений нет", money);
	
	return result_text;	
}

static void write_data_of_shares(char* accountId, char* filename)
{
	struct curl_slist*    headers = NULL;
	struct response_data* rdata;	
	char*                 body = malloc(256);
	cJSON*                json;
	cJSON*                position;
	cJSON*                sum_shares_json;
	cJSON*                percent_json;
	FILE*                 fp;
	char*                 daily_results;
	double                daily_amount = 0;
	double                total_up     = 0;
	double                total_down   = 1;


	snprintf(body,
             256,
             "{\"%s\":\"%s\",\"currency\":\"RUB\"}", "accountId", accountId);

	LOAD_BASE_HEADERS(headers);

	rdata = http_request(CREATE_URL(OperationsService/GetPortfolio),
                         headers,
                         body);		
	free(body);	
	
	if (!rdata)	return;

	json = cJSON_Parse(rdata->data);
	
	sum_shares_json = cJSON_GetObjectItemCaseSensitive(json, "totalAmountShares");
	percent_json = cJSON_GetObjectItemCaseSensitive(json, "expectedYield");
	
	json = cJSON_GetObjectItemCaseSensitive(json, "positions");

	fp = fopen(filename, "w");

	fprintf(fp, "## Сумма портфеля: %.2f₽\n", get_money_value(sum_shares_json));
	fprintf(fp, "## Процент роста портфеля: %.2f\%\n", get_money_value(percent_json));

	cJSON_ArrayForEach(position, json) {
		cJSON* instrumentType = cJSON_GetObjectItemCaseSensitive(position, "instrumentType");		
		if (strcmp(instrumentType->valuestring, "share") == 0) {
			cJSON* figi = cJSON_GetObjectItemCaseSensitive(position, "figi"); 			
			cJSON* price = cJSON_GetObjectItemCaseSensitive(position, "currentPrice");	
			cJSON* quantity = cJSON_GetObjectItemCaseSensitive(position, "quantity");
			cJSON* daily_res = cJSON_GetObjectItemCaseSensitive(position, "dailyYield");
			cJSON* year_expectations  = cJSON_GetObjectItemCaseSensitive(position, "expectedYieldFifo");
				

			daily_amount = get_money_value(daily_res);

			if (daily_amount > 0)
				total_up += daily_amount;
			else
				total_down += daily_amount *= -1;

			fprintf(fp,
                    "# %s %.2f₽ (%.0f)(%.2f)\n> Дневной результат - %s\n> Годовое ожидание - %s\n",
                    get_shareName_by_figi(figi->valuestring),
                    get_money_value(price),
                    get_quotation(quantity)->units,
                    get_money_value(price) * get_quotation(quantity)->units,
                    get_daily_results(daily_res),
                    get_daily_results(year_expectations));	
				
		}
		
	}
	fprintf(fp, "## Прирост за день: <font color=008000>%.2f₽</font>\n",   total_up);
	fprintf(fp, "## Падение за день: <font color=FF0000>%.2f₽</font>\n", total_down);

	fclose(fp);	
}

int main()
{
	char* accountId;
	accountId = get_account_id();	
	write_data_of_shares(accountId, "test.txt");	
}
