#ifndef _I_WEB_SOCKET_MD_H_
#define _I_WEB_SOCKET_MD_H_
#include <string>

#if (defined(_WIN32) || defined(_WIN64))
#ifdef KF_WEBSOCKET_DLL_EXPORTS
#define WEBSOCKETDLLEXPORT		__declspec(dllexport)
#else
#define WEBSOCKETDLLEXPORT		__declspec(dllimport)
#endif
#else
#define WEBSOCKETDLLEXPORT
#endif


//< websocket回调通知类型
enum E_WSCBNotifyType
{
	EWSCBNotifyMessage = 0,			// 消息通知
	EWSCBNotifyCreateConnectFailed,	// 建立连接失败
	EWSCBNotifyConnectFailed,		// 连接失败
    EWSCBNotifyConnectSuccess,      // 连接成功
    EWSCBNotifyConnectInterrupt,    // 连接中断
	EWSCBNotifyClose,				// 连接关闭时
	EWSCBNotifyReceive,				// 接收数据
};

class IWebSocketMD
{
public:
	virtual void on_ws_notify(void* pUser, const char* pszData, E_WSCBNotifyType type) = 0;
};

class IWebSocketBase {
public:
	virtual int init() = 0;
	virtual bool connect() = 0;
	virtual void send(const std::string& stdJson) = 0;
};

/**********************************************************************
* 函数名称:	wb_init
* 功能描述:	初始化
* 输入参数:	IWebSocketMD * pWebSocket: 传入：用于回调处理
			const char* ur: 链接行情地址
* 输出参数:	NA
* 返 回 值:	返回NULL表示失败
* 其它说明:	NA
***********************************************************************/
extern "C" WEBSOCKETDLLEXPORT void* wb_init(IWebSocketMD * pWebSocket, const char* url);

/**********************************************************************
* 函数名称:	wb_close
* 功能描述:	释放
* 输入参数:	void* obj: 释放
* 输出参数:	NA
* 返 回 值:	无
* 其它说明:	NA
***********************************************************************/
extern "C" WEBSOCKETDLLEXPORT void wb_release(void* obj);

#endif