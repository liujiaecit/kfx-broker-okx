#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "iwebsocketMd.h"
#include "type_convert_okx.h"

namespace kungfu::wingchun::okx {
	using namespace kungfu::longfist::types;
	class MarketDataOkx;
	class CWebsocketHelper;

	class CWebsocketManage
	{
	public:
		CWebsocketManage(MarketDataOkx* pMd);
		~CWebsocketManage();

	public:
		void init();
		void uninit();
		void send_ping();
		void subscribe(const std::vector<InstrumentKey>& instrument_keys);
		void unsubscribe(const std::vector<InstrumentKey>& instrument_keys);

		void websocket_notify(void* pUser, const char* pszData, E_WSCBNotifyType type, int connId);
		void update_state(BrokerState state);
	private:
		//< 获取可用websocket对象
		std::shared_ptr<CWebsocketHelper> get_public_ws();
		std::shared_ptr<CWebsocketHelper> get_business_ws();
		// 在新链接创建成功之后执行
		void do_subscribe();

	private:
		std::unordered_map<std::string, std::shared_ptr<CWebsocketHelper>> map_public_websockets_;
		std::unordered_map<std::string, std::shared_ptr<CWebsocketHelper>> map_business_websockets_;
		MarketDataOkx* md_;
		int websocket_id_ = 0;
		std::vector<InstrumentKey> subscribe_instruments_cache_;	//< 待订阅标的列表
		bool ws_public_connected_ = false;
		bool ws_business_connected_ = false;
	};
}