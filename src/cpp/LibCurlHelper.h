#pragma once
#include <string>
#include <map>
#include <functional>

enum NetRequestType
{
	GET,
	POST
};

class CLibCurlHelper
{
public:
	//单例模式
	static CLibCurlHelper& getInstance()
	{
		static CLibCurlHelper m_winHttp;
		return m_winHttp;
	}

	~CLibCurlHelper();

	//< 同步：请求接口
	std::string GetJsonFile(const char* url, NetRequestType type, const char* postData, int& nErrorCode);
	//< 异步：请求接口，传callback进行回调
	void asynGetJsonFile(const char* url, NetRequestType type, const char* postData, std::function<void(std::string)> callBack);
	// get请求
	std::string HttpGet(const std::string& url, const std::vector<std::string>& vect_header, int& nErrorCode);
private:
	CLibCurlHelper();
	// 请求数据
	std::string RequestDataInfo(const char* url, NetRequestType type, const char* postData, int& nErrorCode);
	//异步请求线程
	void asynRequestThread(const char* url, NetRequestType type, const char* postData);
	// post请求
	std::string HttpPost(const std::string& url, const std::string& postData, int& nErrorCode);
	// get请求
	std::string HttpGet(const std::string& url, int& nErrorCode);

private:
	std::function<void(std::string)> callBack = nullptr;
};

