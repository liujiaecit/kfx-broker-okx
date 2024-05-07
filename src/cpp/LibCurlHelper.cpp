#include "LibCurlHelper.h"
#include <kungfu/wingchun/broker/marketdata.h>

#ifdef WIN32
#include "curl/curl.h"
#else
#include <curl/curl.h>
#endif
#include <thread>

#define KUNGFU_APP_USER_AGENT "kungfu-Browser/v1.0"
#define HTTP_TIME_OUT	30000

// 请求http回调
size_t GetDataCallback(void* contents, size_t size, size_t nmemb, void* stream)
{
	std::string* str = (std::string*)stream;
	(*str).append((char*)contents, size * nmemb);
	return size * nmemb;
}

CLibCurlHelper::CLibCurlHelper()
{

}

CLibCurlHelper::~CLibCurlHelper()
{

}

// post请求
std::string CLibCurlHelper::HttpPost(const std::string& url, const std::string& postData, int& nErrorCode)
{
	std::string response;
	CURL* curl_handle = curl_easy_init();
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");
	//headers = curl_slist_append(headers, "X-MBX-APIKEY: h4jQlABA974QWsaaUVuwwGJRya70HLPC470BwqMDlxe45xYq1fIlEfdyfbjSbBs4");

	if (curl_handle == NULL)
	{
		return "";
	}

	// 设置请求地址
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	// 设置请求头信
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	// 不显示接收头信息
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, 0);
	// 设置请求超时时间
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, HTTP_TIME_OUT);
	// 设置请求
	if (!postData.empty())
	{
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postData.c_str());
	}
	// 设置接收函数
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, GetDataCallback);
	// 设置接收内容
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&response);

	nErrorCode = curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	return response;
}

// get请求
std::string CLibCurlHelper::HttpGet(const std::string& url, const std::vector<std::string>& vect_header, int& nErrorCode) {
	SPDLOG_DEBUG("HttpGet:{}", url);
	std::string response;
	CURL* curl_handle = curl_easy_init();
	struct curl_slist* headers = NULL;

	headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");

	for (auto iter : vect_header) {
		headers = curl_slist_append(headers, iter.c_str());
	}

	if (curl_handle == NULL)
	{
		return "";
	}

	// 设置请求地址
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	// 设置请求头信
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	//curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, KUNGFU_APP_USER_AGENT);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	// 不显示接收头信息
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, 0);
	// 设置请求超时时间
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, HTTP_TIME_OUT);
	// 设置接收函数
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, GetDataCallback);
	// 设置接收内容
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&response);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
	//CURLcode i5 = curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

	nErrorCode = (int)curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	return response;
}

// get请求
std::string CLibCurlHelper::HttpGet(const std::string& url, int& nErrorCode)
{
	SPDLOG_DEBUG("HttpGet:{}", url);
	std::string response;
	CURL* curl_handle = curl_easy_init();
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");

	if (curl_handle == NULL)
	{
		return "";
	}

	// 设置请求地址
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	// 设置请求头信
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	//curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, KUNGFU_APP_USER_AGENT);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	// 不显示接收头信息
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, 0);
	// 设置请求超时时间
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, HTTP_TIME_OUT);
	// 设置接收函数
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, GetDataCallback);
	// 设置接收内容
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&response);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
	//CURLcode i5 = curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

	nErrorCode = (int)curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	return response;
}

// 请求数据
std::string CLibCurlHelper::RequestDataInfo(const char* url, NetRequestType type, const char* postData, int& nErrorCode)
{
	curl_global_init(CURL_GLOBAL_ALL);
	std::string retData;

	switch (type)
	{
	case GET:
		retData = HttpGet(url, nErrorCode);
		break;
	case POST:
		if (postData)
		{
			retData = HttpPost(url, postData, nErrorCode);
		}
		else
		{
			retData = HttpPost(url, "", nErrorCode);
		}
		break;
	default:
		break;
	}

	curl_global_cleanup();
	return retData;
}

//异步请求线程
void CLibCurlHelper::asynRequestThread(const char* url, NetRequestType type, const char* postData)
{
	int nErrorCode;
	std::string jsonStr = RequestDataInfo(url, type, postData, nErrorCode);

	if (callBack != NULL)
	{
		callBack(jsonStr);
	}
}

std::string CLibCurlHelper::GetJsonFile(const char* url, NetRequestType type, const char* postData, int& nErrorCode)
{
	return RequestDataInfo(url, type, postData, nErrorCode);
}

void CLibCurlHelper::asynGetJsonFile(const char* url, NetRequestType type, const char* postData, std::function<void(std::string)> callBack)
{
	this->callBack = callBack;
	std::thread t(&CLibCurlHelper::asynRequestThread, this, url, type, postData);
	t.detach();
}