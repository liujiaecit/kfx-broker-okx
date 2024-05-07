#pragma once
#include <string>
#include <set>
#include "iwebsocketMd.h"

namespace kungfu::wingchun::okx {
	class CWebsocketManage;
	class CWebsocketHelper : public IWebSocketMD
	{
	public:
		CWebsocketHelper(CWebsocketManage* pMd, int connId);
		~CWebsocketHelper();

		bool init(const std::string& url);
		void uninit();
		//< 发送ping 防止30s没有收到信息服务器会主动断开
		void send_ping();
		void subscribe(const std::string& data, const std::set<uint32_t>& sub_sets);
		void unsubscribe(const std::string& data, const std::set<uint32_t>& sub_sets);
		// 获取当前连接订阅了多少标的
		int get_subscribe_count();
		// 获取当前链接是否连接成功
		bool is_connected() { return b_is_connect; }
		// 返回当前链接id
		int get_connId() { return connId_; }

		virtual void on_ws_notify(void* pUser, const char* pszData, E_WSCBNotifyType type) override;

		void reconnect_server();
		void check_and_reconnect();
	private:

	private:
		bool b_is_connect = false;
		int reconnect_count_ = 0;
		std::string dataServiceAddr_;

		std::set<uint32_t> map_sub_sets_;	// < 订阅列表
		IWebSocketBase* m_pWebSocketOkxMd = nullptr;
		CWebsocketManage* ws_manage_;
		int subscribe_count_ = 0;
		int connId_ = 0;
	};
}