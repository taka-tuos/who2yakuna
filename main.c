#include <curl/curl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // memmove
#include <ctype.h>  // isspace
#include <locale.h> // setlocale

char *streaming_json = NULL;

#define URI "api/v1/streaming/public"

void (*streaming_recieved_handler)(void);
void (*stream_event_handler)(struct json_object *);

char access_token[256];
char domain_string[256];

int prev_yakuna = -1;

char *create_uri_string(char *api)
{
	char *s = malloc(256);
	sprintf(s, "https://%s/%s", domain_string, api);
	return s;
}

int read_json_fom_path(struct json_object *obj, char *path, struct json_object **dst)
{
	char *dup = strdup(path);
	struct json_object *dir = obj;
	int exist = 1;
	char *next_key;
	char last_key[256];
	
	char *tok = dup;
	
	while(exist) {
		next_key = strtok(tok, "/");
		tok = NULL;
		if(!next_key) break;
		strcpy(last_key, next_key);
		struct json_object *next;
		exist = json_object_object_get_ex(dir, next_key, &next);
		if(exist) {
			dir = next;
		}
	}
	
	free(dup);
	
	*dst = dir;
	
	return exist;
}

size_t streaming_callback(void* ptr, size_t size, size_t nmemb, void* data) {
	if (size * nmemb == 0)
		return 0;
	
	char **json = ((char **)data);
	
	size_t realsize = size * nmemb;
	
	size_t length = realsize + 1;
	char *str = *json;
	str = realloc(str, (str ? strlen(str) : 0) + length);
	if(*((char **)data) == NULL) strcpy(str, "");
	
	*json = str;
	
	if (str != NULL) {
		strncat(str, ptr, realsize);
		if(str[strlen(str)-1] == 0x0a) {
			if(*str == ':') {
				free(str);
				*json = NULL;
			} else {
				streaming_recieved_handler();
			}
		}
	}

	return realsize;
}

void stream_event_update(struct json_object *jobj_from_string)
{
	struct json_object *content, *screen_name, *display_name, *reblog, *id;
	//struct json_object *jobj_from_string = json_tokener_parse(json);
	if(!jobj_from_string) return;
	read_json_fom_path(jobj_from_string, "content", &content);
	read_json_fom_path(jobj_from_string, "account/acct", &screen_name);
	read_json_fom_path(jobj_from_string, "account/display_name", &display_name);
	read_json_fom_path(jobj_from_string, "reblog", &reblog);
	read_json_fom_path(jobj_from_string, "id", &id);
	
	enum json_type type;
	
	type = json_object_get_type(reblog);
	
	if(type != json_type_null) return;
	
	char *src = json_object_get_string(content);
	char *src2 = (char *)malloc(strlen(src));
	
	strcpy(src2, "");
	
	int ltgt = 0;
	while(*src) {
		if(*src == '<') ltgt = 1;
		if(ltgt && strncmp(src, "<br", 3) == 0) strcat(src2, "\n");
		if(!ltgt) {
			if(*src == '&') {
				if(strncmp(src, "&amp;", 5) == 0) {
					strcat(src2, "&");
					src += 4;
				}
				else if(strncmp(src, "&lt;", 4) == 0) {
					strcat(src2, "<");
					src += 3;
				}
				else if(strncmp(src, "&gt;", 4) == 0) {
					strcat(src2, ">");
					src += 3;
				}
				else if(strncmp(src, "&quot;", 6) == 0) {
					strcat(src2, "\"");
					src += 5;
				}
				else if(strncmp(src, "&apos;", 6) == 0) {
					strcat(src2, "\'");
					src += 5;
				}
			} else {
				strncat(src2, src, 1);
			}
		}
		if(*src == '>') ltgt = 0;
		src++;
	}
	
	FILE *fp = fopen(".yakunalist", "rt");
	FILE *fp2 = fopen(".replylist", "rt");
	
	char *replylist[256];
	
	if(!fp || !fp2) return;
	
	int i = 0;
	
	do {
		replylist[i] = (char *)malloc(512);
		fgets(replylist[i], 512, fp2);
		char *p = replylist[i];
		while(*p) {
			if(*p == '\r' || *p == '\n') *p = 0;
			p++;
		}
		if(replylist[i][0] == 0) break;
		i++;
	} while(!feof(fp2));
	
	fclose(fp2);
	
	if(prev_yakuna < 0) prev_yakuna = (rand() % (i + 1)) - 1;
	
	do {
		char s2[512];
		fgets(s2, 512, fp);
		char *p = s2;
		while(*p) {
			if(*p == '\r' || *p == '\n') *p = 0;
			p++;
		}
		if(s2[0] == 0) break;	
		char *adr = strstr(src2, s2);
		if(adr) {
			char s[256];
			time_t timer;
			struct tm *timeptr;

			timer = time(NULL);
			timeptr = localtime(&timer);
			strftime(s, 256, "%m/%d %Y %a %H:%M:%S", timeptr);
			printf("Burned by %s(%s) at %s\n", json_object_get_string(screen_name), json_object_get_string(display_name), s);
			
			char *id_s = json_object_get_string(id);
			int yakuna = (rand() % (i + 1)) - 1;
			while(1) {
				yakuna = (rand() % (i + 1)) - 1;
				if(yakuna != prev_yakuna) {
					prev_yakuna = yakuna;
					break;
				}
			}
			char s3[512];
			sprintf(s3, "@%s\n%s",json_object_get_string(screen_name), replylist[yakuna]);
			do_favfav(id_s);
			do_toot(s3, id_s);
			break;
		}
	} while(!feof(fp));
	
	fclose(fp);
	
	for(int j = 0; j < i; j++) free(replylist[j]);
}

char **json_recieved = NULL;
int json_recieved_len = 0;

void streaming_recieved(void)
{
	json_recieved = realloc(json_recieved, (json_recieved_len + 1) * sizeof(char *));
	json_recieved[json_recieved_len] = strdup(streaming_json);
	
	if(strncmp(streaming_json, "event", 5) == 0) {
		char *type = strdup(streaming_json + 7);
		if(strncmp(type, "update", 6) == 0) stream_event_handler = stream_event_update;
		else stream_event_handler = NULL;
		
		char *top = type;
		while(*type != '\n') type++;
		type++;
		if(*type != 0) {
			free(streaming_json);
			streaming_json = strdup(type);
		}
		free(top);
	}
	if(strncmp(streaming_json, "data", 4) == 0) {
		if(stream_event_handler) {
			struct json_object *jobj_from_string = json_tokener_parse(streaming_json + 6);
			stream_event_handler(jobj_from_string);
			json_object_put(jobj_from_string);
			stream_event_handler = NULL;
		}
	}
	
	free(streaming_json);
	streaming_json = NULL;
}

void *stream_thread_func(void *param)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	char *uri = create_uri_string(URI);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 0);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&streaming_json);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, streaming_callback);
	
	streaming_recieved_handler = streaming_recieved;
	stream_event_handler = NULL;
	
	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
	
	return NULL;
}

void do_create_client(char *domain)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	
	char json_name[256], *uri;
	
	sprintf(json_name, "%s.ckcs", domain);
	
	uri = create_uri_string("api/v1/apps");
	
	FILE *f = fopen(json_name, "wb");

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_name",
				CURLFORM_COPYCONTENTS, "who2yakuna",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "redirect_uris",
				CURLFORM_COPYCONTENTS, "urn:ietf:wg:oauth:2.0:oob",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "scopes",
				CURLFORM_COPYCONTENTS, "read write follow",
				CURLFORM_END);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);
	
	ret = curl_easy_perform(hnd);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
}

void do_oauth(char *code, char *ck, char *cs)
{
	char fields[512];
	sprintf(fields, "client_id=%s&client_secret=%s&grant_type=authorization_code&code=%s&scope=read%20write%20follow", ck, cs, code);
	
	FILE *f = fopen(".who2yakuna", "wb");
	
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "grant_type",
				CURLFORM_COPYCONTENTS, "authorization_code",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "redirect_uri",
				CURLFORM_COPYCONTENTS, "urn:ietf:wg:oauth:2.0:oob",
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_id",
				CURLFORM_COPYCONTENTS, ck,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "client_secret",
				CURLFORM_COPYCONTENTS, cs,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "code",
				CURLFORM_COPYCONTENTS, code,
				CURLFORM_END);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, create_uri_string("oauth/token"));
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);
	
	ret = curl_easy_perform(hnd);
	
	fclose(f);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
}

void do_toot(char *s, char *id)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	struct curl_slist *slist1;
	
	FILE *f = fopen("/dev/null", "wb");
	
	char *uri = create_uri_string("api/v1/statuses");

	post1 = NULL;
	postend = NULL;
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "status",
				CURLFORM_COPYCONTENTS, s,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "in_reply_to_id",
				CURLFORM_COPYCONTENTS, id,
				CURLFORM_END);
	curl_formadd(&post1, &postend,
				CURLFORM_COPYNAME, "visibility",
				CURLFORM_COPYCONTENTS, "public",
				CURLFORM_END);
	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);

	ret = curl_easy_perform(hnd);
	
	/*long response_code;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &response_code);
	printf("%d\n", response_code);*/

	fclose(f);
	
	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

void do_favfav(char *id)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_httppost *post1;
	struct curl_httppost *postend;
	struct curl_slist *slist1;
	
	FILE *f = fopen("/dev/null", "wb");
	
	char api[512];
	sprintf(api,"api/v1/statuses/%s/favourite", id);
	
	char *uri = create_uri_string(api);

	post1 = NULL;
	postend = NULL;
	slist1 = NULL;
	slist1 = curl_slist_append(slist1, access_token);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, uri);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, f);

	ret = curl_easy_perform(hnd);
	
	/*long response_code;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &response_code);
	printf("%d\n", response_code);*/

	fclose(f);
	
	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_formfree(post1);
	post1 = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
}

int main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));
	
	FILE *fp = fopen(".who2yakuna", "rb");
	if(fp) {
		fclose(fp);
		struct json_object *token;
		struct json_object *jobj_from_file = json_object_from_file(".who2yakuna");
		read_json_fom_path(jobj_from_file, "access_token", &token);
		sprintf(access_token, "Authorization: Bearer %s", json_object_get_string(token));
		FILE *f2 = fopen(".current_domain", "rb");
		fscanf(f2, "%255s", domain_string);
		fclose(f2);
	} else {
		char domain[256];
		char key[256];
		char *ck;
		char *cs;
		printf("はじめまして！ようこそwho2yakunaへ!\n");
		printf("最初に、");
retry1:
		printf("あなたのいるインスタンスを教えてね。\n(https://[ここを入れてね]/)\n");
		printf(">");
		scanf("%255s", domain);
		printf("\n");
		
		FILE *f2 = fopen(".current_domain", "wb");
		fprintf(f2, "%s", domain);
		fclose(f2);
		
		char json_name[256];
		sprintf(json_name, "%s.ckcs", domain);
		strcpy(domain_string, domain);
		FILE *ckcs = fopen(json_name, "rb");
		if(!ckcs) {
			do_create_client(domain);
		} else {
			fclose(ckcs);
		}
		
		struct json_object *cko, *cso;
		struct json_object *jobj_from_file = json_object_from_file(json_name);
		int r1 = read_json_fom_path(jobj_from_file, "client_id", &cko);
		int r2 = read_json_fom_path(jobj_from_file, "client_secret", &cso);
		if(!r1 || !r2) {
			printf("何かがおかしいみたいだよ。\nもう一度やり直すね。");
			remove(json_name);
			remove(".current_domain");
			goto retry1;
		}
		ck = strdup(json_object_get_string(cko));
		cs = strdup(json_object_get_string(cso));
		
		char code[256];
		
		printf("次に、アプリケーションの認証をするよ。\n");
		printf("下に表示されるURLにアクセスして承認をしたら表示されるコードを入力してね。\n");
		printf("https://%s/oauth/authorize?client_id=%s&response_type=code&redirect_uri=urn:ietf:wg:oauth:2.0:oob&scope=read%%20write%%20follow\n", domain, ck);
		printf(">");
		scanf("%255s", code);
		printf("\n");
		do_oauth(code, ck, cs);
		struct json_object *token;
		jobj_from_file = json_object_from_file(".who2yakuna");
		int r3 = read_json_fom_path(jobj_from_file, "access_token", &token);
		if(!r3) {
			printf("何かがおかしいみたいだよ。\n入力したコードはあっているかな？\nもう一度やり直すね。");
			remove(json_name);
			remove(".current_domain");
			remove(".who2yakuna");
			goto retry1;
		}
		sprintf(access_token, "Authorization: Bearer %s", json_object_get_string(token));
		printf("これでおしまい!\nwho2yakunaライフを楽しんでね!\n");
	}
	
	printf("who2yakunaサービスを開始しました\n");
	
	stream_thread_func(NULL);

	return 0;
}
