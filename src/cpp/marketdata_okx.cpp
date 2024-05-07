#include "marketdata_okx.h"
#include <fstream>
#include <iosfwd>
#include <sstream>
#include "url_define.h"
#include "websocket_manage.h"
#include "LibCurlHelper.h"

namespace kungfu::wingchun::okx {
    MarketDataOkx::MarketDataOkx(broker::BrokerVendor& vendor) : MarketData(vendor) { KUNGFU_SETUP_LOG(); }

    MarketDataOkx::~MarketDataOkx() {}

    void MarketDataOkx::on_start() {
        SPDLOG_INFO("okx start.");
        if (request_public_instruments_info(SPOT_INST_TYPE) &&
            request_public_instruments_info(SWAP_INST_TYPE) &&
            request_public_instruments_info(FUTURES_INST_TYPE) &&
            request_market_tickers_info(SPOT_INST_TYPE) &&
            request_market_tickers_info(SWAP_INST_TYPE) &&
            request_market_tickers_info(FUTURES_INST_TYPE)) {

            tm_ = std::shared_ptr<MarketDataOkx>(this);
            m_websocket_manage_ = std::make_shared<CWebsocketManage>(this);
            m_websocket_manage_->init();
        }
        else {
            update_broker_state(BrokerState::DisConnected);
            SPDLOG_ERROR("request_market_tickers info failed.");
        }
    }

    void MarketDataOkx::on_exit() {
        SPDLOG_INFO("okx exit.");
        client_exit_ = true;
        m_set_subscribe_instrument.clear();

        if (m_websocket_manage_) {
            m_websocket_manage_->uninit();
        }
    }

    bool MarketDataOkx::subscribe(const std::vector<InstrumentKey>& instrument_keys) {
        SPDLOG_INFO("subscribe instrument_keys size = {}", instrument_keys.size());
        std::vector<InstrumentKey> sub_instrument_keys;
        for (const auto& inst : instrument_keys) {
            SPDLOG_DEBUG("subscribe exchange_id = {}, instrument_id={}, instrument_type={}", inst.exchange_id, inst.instrument_id, int(inst.instrument_type));
            if (!(inst.instrument_type == InstrumentType::Crypto ||
                inst.instrument_type == InstrumentType::CryptoFuture ||
                inst.instrument_type == InstrumentType::CryptoUFuture)) {
                continue;
            }

            auto iter = m_set_subscribe_instrument.find(hash_instrument(inst.exchange_id, inst.instrument_id));
            if (iter != m_set_subscribe_instrument.end()) {
                SPDLOG_DEBUG("subscribe instrument_keys set_subscribe_instrument find instrument_id = {}", inst.instrument_id);
                continue;
            }

            if (strcmp(inst.instrument_id, "req_outside_position") == 0) {
                // 请求场外持仓时，会把instrument_id设置成"req_outside_position"，参考trader_okx
                continue;
            }

            std::string symbol = inst.instrument_id;
            SPDLOG_DEBUG("subscribe instrument_id = {}, exchange_id ={}", inst.instrument_id, inst.exchange_id);
            m_map_scribe_instruments.insert(std::make_pair(symbol, inst));
            m_set_subscribe_instrument.emplace(hash_instrument(inst.exchange_id, inst.instrument_id));
            sub_instrument_keys.emplace_back(inst);
        }

        if (sub_instrument_keys.size() > 0 && m_websocket_manage_) {
            m_websocket_manage_->subscribe(sub_instrument_keys);
        }

        return true;
    }

    bool MarketDataOkx::unsubscribe(const std::vector<longfist::types::InstrumentKey>& instrument_keys) {
        SPDLOG_DEBUG("unsubscribe instrument_keys size = {}", instrument_keys.size());
        nlohmann::json jSubscribe;
        nlohmann::json jSymbols = nlohmann::json::array();

        std::vector<InstrumentKey> unsub_instrument_keys;
        for (const auto& inst : instrument_keys) {
            auto iter = m_set_subscribe_instrument.find(hash_instrument(inst.exchange_id, inst.instrument_id));
            if (iter != m_set_subscribe_instrument.end()) {
                m_set_subscribe_instrument.erase(iter);
                unsub_instrument_keys.emplace_back(inst);
            }
        }

        if (unsub_instrument_keys.size() > 0 && m_websocket_manage_) {
            m_websocket_manage_->unsubscribe(unsub_instrument_keys);
        }

        return true;
    }

    void MarketDataOkx::websocket_notify(const char* pszData, E_WSCBNotifyType type) {
        //SPDLOG_DEBUG("websocket_notify type = {}, strData={}", int(type), pszData);
        int event_type = type;
        if (type == EWSCBNotifyMessage) {
            OnResponseMarketData(pszData);
            return;
        }

        auto& bf_okx_market = tm_->get_thread_writer()->open_custom_data<int>(kOKXMarketFieldType, tm_->now());
        memcpy(&bf_okx_market, &event_type, sizeof(int));
        tm_->get_thread_writer()->close_data();
    }

    bool MarketDataOkx::on_custom_event(const event_ptr& event) {
        SPDLOG_DEBUG("msg_type: {}", event->msg_type());
        switch (event->msg_type()) {
        case kOKXMarketFieldType:
            return custom_OnRtnMarketData(event);
        default:
            SPDLOG_ERROR("unrecognized msg_type: {}", event->msg_type());
            return false;
        }
        return true;
    }

    bool MarketDataOkx::custom_OnRtnMarketData(const event_ptr& event) {
        const auto& bf_okx_market = event->custom_data<int>();
        return custom_return_market_data(bf_okx_market);
    }

    bool MarketDataOkx::custom_return_market_data(const int& event_type) {
        SPDLOG_DEBUG("custom_return_market_data begin event_type={}", event_type);
        E_WSCBNotifyType type = (E_WSCBNotifyType)event_type;

        switch (type) {
        case EWSCBNotifyMessage: {
            SPDLOG_INFO("custom_return_market_data 连接成功之后");
            break;
        }
        case EWSCBNotifyCreateConnectFailed:
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyCreateConnectFailed");
            update_broker_state(BrokerState::LoginFailed);
            break;
        case EWSCBNotifyConnectFailed:
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyConnectFailed}");
            update_broker_state(BrokerState::LoginFailed);
            break;
        case EWSCBNotifyConnectSuccess: {
            SPDLOG_INFO("on_ws_notify EWSCBNotifyConnectSuccess");
            update_broker_state(BrokerState::Connected);
            update_broker_state(BrokerState::Ready);

            add_time_interval(time_unit::NANOSECONDS_PER_SECOND * 25, [&](auto e) { m_websocket_manage_->send_ping(); });
            break;
        }
        case EWSCBNotifyConnectInterrupt:
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyConnectInterrupt}");
            m_set_subscribe_instrument.clear();
            update_broker_state(BrokerState::DisConnected);
            //b_is_connect = false;
            //reconnect_count_ = 0;
            break;
        case EWSCBNotifyClose:
            SPDLOG_INFO("on_ws_notify EWSCBNotifyClose");
            m_set_subscribe_instrument.clear();
            if (!client_exit_) {
                update_broker_state(BrokerState::DisConnected);
                //b_is_connect = false;
                //reconnect_count_ = 0;
            }
            break;
        case EWSCBNotifyReceive:
            break;
        default:
            break;
        }

        SPDLOG_DEBUG("custom_return_market_data end");
        return true;
    }

    void MarketDataOkx::OnResponseMarketData(const std::string& strData) {
        //SPDLOG_DEBUG("OnResponseMarketData return data: {}", strData);
        nlohmann::json jsonQuote = nlohmann::json::parse(strData.c_str());

        if (!jsonQuote.is_object()) {
            SPDLOG_ERROR("OnResponseMarketData error string: {}", strData);
            return;
        }
        if (jsonQuote.contains("event") && jsonQuote["event"].get<std::string>() == "unsubscribe") {
            SPDLOG_ERROR("OnResponseMarketData error 取消订阅: {}", strData);
            // 取消订阅
            return;
        }
        if (jsonQuote.contains("arg")) {
            if (jsonQuote["arg"]["channel"].get<std::string>() == "tickers") {
                for (auto item : jsonQuote["data"].items()) {
                    parse_tickers_quote(item.value());                    
                }
            }
            else if (jsonQuote["arg"]["channel"].get<std::string>() == "books") {
                if (jsonQuote.contains("action")) {
                    std::string action = jsonQuote["action"].get<std::string>();

                    if (action == "snapshot") { // 全量
                        parse_books_all(jsonQuote);
                    }
                    else if (action == "update") {  // 增量
                        parse_books_update(jsonQuote);
                    }
                }
            }
            else if (jsonQuote["arg"]["channel"].get<std::string>() == "books5") {
                parse_books5(jsonQuote);
            }
            else if (jsonQuote["arg"]["channel"].get<std::string>() == "trades") {
                for (auto item : jsonQuote["data"].items()) {
                    parse_trades(item.value());
                }
            }
            else if (jsonQuote["arg"]["channel"].get<std::string>() == "trades-all") {
                for (auto item : jsonQuote["data"].items()) {
                    parse_trades_all(item.value());
                }
            }
        }
    }

    // 解析行情
    void MarketDataOkx::parse_tickers_quote(const nlohmann::json& jsonValue) {
        //SPDLOG_DEBUG("parse_tickers_quote return data: {}", jsonValue.dump());
        std::string instId = jsonValue["instId"].get<std::string>();
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            map_ticker_quote_json_[instId] = jsonValue.dump();
            update_quote(instId);
        }
    }

    // 解析深度行情：5档数据
    void MarketDataOkx::parse_books5(const nlohmann::json& jsonValue) {
        std::string instId = jsonValue["arg"]["instId"].get<std::string>();
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            for (auto item : jsonValue["data"].items()) {
                StruDepthsInfo struDepthsInfo;
                int count = 0;
                for (auto iter : item.value()["asks"]) {
                    DepthsInfo depthInfo;
                    depthInfo.price = iter[0].get<std::string>();
                    depthInfo.volume = iter[1].get<std::string>();
                    depthInfo.reserve = iter[2].get<std::string>();
                    depthInfo.order_volume = iter[3].get<std::string>();
                    struDepthsInfo.ask_price[count] = atof(depthInfo.price.c_str());
                    struDepthsInfo.ask_volume[count] = atof(depthInfo.volume.c_str());
                    count++;
                }

                count = 0;
                for (auto iter : item.value()["bids"]) {
                    DepthsInfo depthInfo;
                    depthInfo.price = iter[0].get<std::string>();
                    depthInfo.volume = iter[1].get<std::string>();
                    depthInfo.reserve = iter[2].get<std::string>();
                    depthInfo.order_volume = iter[3].get<std::string>();
                    struDepthsInfo.bid_price[count] = atof(depthInfo.price.c_str());
                    struDepthsInfo.bid_volume[count] = atof(depthInfo.volume.c_str());
                    count++;
                }

                map_depth_[instId] = struDepthsInfo;
            }

            update_quote(instId, true);
        }
    }

    // 解析深度行情：全量
    void MarketDataOkx::parse_books_all(const nlohmann::json& jsonValue) {
        std::string instId = jsonValue["arg"]["instId"].get<std::string>();
        SPDLOG_INFO("request_market_tickers parse_books_all instId ={}", instId);
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            for (auto item : jsonValue["data"].items()) {
                std::set<std::string> set_asks; // 卖是递增， 买是递减
                std::set<std::string> set_bids;
                std::unordered_map<std::string, DepthsInfo> mapAsksDepthsInfo;
                std::unordered_map<std::string, DepthsInfo> mapBidsDepthsInfo;

                StruDepthsInfo struDepthsInfo;
                int count = 0;
                for (auto iter : item.value()["asks"]) {
                    DepthsInfo depthInfo;
                    depthInfo.price = iter[0].get<std::string>();
                    depthInfo.volume = iter[1].get<std::string>();
                    depthInfo.reserve = iter[2].get<std::string>();
                    depthInfo.order_volume = iter[3].get<std::string>();

                    set_asks.insert(depthInfo.price);
                    mapAsksDepthsInfo.emplace(depthInfo.price, depthInfo);

                    if (count < 10) {
                        struDepthsInfo.ask_price[count] = atof(depthInfo.price.c_str());
                        struDepthsInfo.ask_volume[count] = atof(depthInfo.volume.c_str());
                    }

                    count++;
                    if (count == 50) {
                        break;
                    }
                }

                count = 0;
                for (auto iter : item.value()["bids"]) {
                    DepthsInfo depthInfo;
                    depthInfo.price = iter[0].get<std::string>();
                    depthInfo.volume = iter[1].get<std::string>();
                    depthInfo.reserve = iter[2].get<std::string>();
                    depthInfo.order_volume = iter[3].get<std::string>();

                    set_bids.insert(depthInfo.price);
                    mapBidsDepthsInfo.emplace(depthInfo.price, depthInfo);

                    if (count < 10) {
                        struDepthsInfo.bid_price[count] = atof(depthInfo.price.c_str());
                        struDepthsInfo.bid_volume[count] = atof(depthInfo.volume.c_str());
                    }

                    count++;
                    if (count == 50) {
                        break;
                    }
                }

                map_depth_.emplace(instId, struDepthsInfo);

                map_tickers_set_asks_depth_[instId] = set_asks;
                map_tickers_set_bids_depth_[instId] = set_bids;
                map_tickers_asks_depth_[instId] = mapAsksDepthsInfo;
                map_tickers_bids_depth_[instId] = mapBidsDepthsInfo;
            }

            update_quote(instId, true);
        }
        else {
            SPDLOG_ERROR("request_market_tickers parse_books_all not find instId ={}", instId);
        }
    }

    // 解析深度行情：增量
    void MarketDataOkx::parse_books_update(const nlohmann::json& jsonValue) {
        std::string instId = jsonValue["arg"]["instId"].get<std::string>();
        if (map_tickers_set_asks_depth_.find(instId) == map_tickers_set_asks_depth_.end() ||
            map_tickers_set_bids_depth_.find(instId) == map_tickers_set_bids_depth_.end()) {
            SPDLOG_ERROR("parse_books update not find, inst = {}", instId);
            return;
        }

        auto& set_asks = map_tickers_set_asks_depth_[instId];
        auto& set_bids = map_tickers_set_bids_depth_[instId];
        auto& mapAsksDepthsInfo = map_tickers_asks_depth_[instId];
        auto& mapBidsDepthsInfo = map_tickers_bids_depth_[instId];

        for (auto item : jsonValue["data"].items()) {
            for (auto iter : item.value()["asks"]) {
                DepthsInfo depthInfo;
                depthInfo.price = iter[0].get<std::string>();
                depthInfo.volume = iter[1].get<std::string>();
                depthInfo.reserve = iter[2].get<std::string>();
                depthInfo.order_volume = iter[3].get<std::string>();

                auto iterAsksSet = set_asks.find(depthInfo.price);
                if (iterAsksSet == set_asks.end()) {
                    //SPDLOG_DEBUG("parse_books update asks insert depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                    if (depthInfo.volume != "0") {
                        set_asks.insert(depthInfo.price);
                        mapAsksDepthsInfo.emplace(depthInfo.price, depthInfo);
                    }
                }
                else {
                    if (depthInfo.volume == "0") {
                        //SPDLOG_DEBUG("parse_books update asks delete depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                        // 删除数量为0的档位
                        set_asks.erase(iterAsksSet);
                        auto iterAsksDepth = mapAsksDepthsInfo.find(depthInfo.price);
                        if (iterAsksDepth != mapAsksDepthsInfo.end()) {
                            mapAsksDepthsInfo.erase(iterAsksDepth);
                        }
                    }
                    else {
                        // 更新数量
                        //SPDLOG_DEBUG("parse_books update asks update depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                        mapAsksDepthsInfo[depthInfo.price] = depthInfo;
                    }
                }
            }

            for (auto iter : item.value()["bids"]) {
                DepthsInfo depthInfo;
                depthInfo.price = iter[0].get<std::string>();
                depthInfo.volume = iter[1].get<std::string>();
                depthInfo.reserve = iter[2].get<std::string>();
                depthInfo.order_volume = iter[3].get<std::string>();

                auto iterAsksSet = set_bids.find(depthInfo.price);
                if (iterAsksSet == set_bids.end()) {
                    //SPDLOG_DEBUG("parse_books update bids insert depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                    if (depthInfo.volume != "0") {
                        set_bids.insert(depthInfo.price);
                        mapBidsDepthsInfo.emplace(depthInfo.price, depthInfo);
                    }
                }
                else {
                    if (depthInfo.volume == "0") {
                        //SPDLOG_DEBUG("parse_books update bids insert depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                        // 删除数量为0的档位
                        set_bids.erase(iterAsksSet);
                        auto iterBidsDepth = mapBidsDepthsInfo.find(depthInfo.price);
                        if (iterBidsDepth != mapBidsDepthsInfo.end()) {
                            mapBidsDepthsInfo.erase(iterBidsDepth);
                        }
                    }
                    else {
                        // 更新数量
                        //SPDLOG_DEBUG("parse_books update bids insert depthInfo.price = {}, volume = {}", depthInfo.price, depthInfo.volume);
                        mapBidsDepthsInfo[depthInfo.price] = depthInfo;
                    }
                }
            }
        }

        // 更新深度行情数据
        if (map_depth_.find(instId) != map_depth_.end()) {
            auto& struDepthsInfo = map_depth_[instId];
            int index = 0;
            for (auto it : map_tickers_set_asks_depth_[instId]) {
                if (map_tickers_asks_depth_.find(instId) != map_tickers_asks_depth_.end()) {
                    if (map_tickers_asks_depth_[instId].find(it) != map_tickers_asks_depth_[it].end()) {
                        struDepthsInfo.ask_price[index] = atof(map_tickers_asks_depth_[instId][it].price.c_str());
                        struDepthsInfo.ask_volume[index] = atof(map_tickers_asks_depth_[instId][it].volume.c_str());
                        index++;
                        if (index >= 10) {
                            break;
                        }
                    }
                }
            }

            /*SPDLOG_DEBUG("parse_books update bids map_tickers_set_bids_depth_ size = {}, map_tickers_bids_depth_ size = {}",
                map_tickers_set_bids_depth_[instId].size(), map_tickers_bids_depth_[instId].size());*/
            //// 倒序遍历 买入，因为set是升序排列
            index = 0;
            std::vector<std::string> descVector(map_tickers_set_bids_depth_[instId].begin(), map_tickers_set_bids_depth_[instId].end());
            // 使用reverse算法将vector反转  
            std::reverse(descVector.begin(), descVector.end());

            // 遍历反转后的vector  
            for (const auto& it : descVector) {
                if (map_tickers_bids_depth_.find(instId) != map_tickers_bids_depth_.end()) {
                    if (map_tickers_bids_depth_[instId].find(it) != map_tickers_bids_depth_[it].end()) {
                        struDepthsInfo.bid_price[index] = atof(map_tickers_bids_depth_[instId][it].price.c_str());
                        struDepthsInfo.bid_volume[index] = atof(map_tickers_bids_depth_[instId][it].volume.c_str());
                        index++;
                        if (index >= 10) {
                            break;
                        }
                    }
                }
            }
        }

        update_quote(instId, true);
    }

    void MarketDataOkx::update_quote(const std::string& instId, bool bDepth) {
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            auto instQuote = map_ticker_quote_json_.find(instId);
			if (instQuote != map_ticker_quote_json_.end()) {

                Quote& quote = get_public_writer()->open_data<Quote>(0);
                from_okx(nlohmann::json::parse(map_ticker_quote_json_[instId]), quote);

                if (map_depth_.find(instId) != map_depth_.end()) {
                    //SPDLOG_INFO("request_market_tickers map_depth_ find instId={}", instId);
                    for (int i = 0; i < 10; i++) {
                        quote.ask_price[i] = map_depth_[instId].ask_price[i];
                        quote.ask_volume[i] = map_depth_[instId].ask_volume[i];
                        quote.bid_price[i] = map_depth_[instId].bid_price[i];
                        quote.bid_volume[i] = map_depth_[instId].bid_volume[i];
                    }
                }

                quote.instrument_id = inst->second.instrument_id;
                quote.exchange_id = inst->second.exchange_id;
				quote.instrument_type = inst->second.instrument_type;
				/*SPDLOG_DEBUG("update_quote instId: {}, instrument_id={}, exchange_id={},instrument_type={}", 
                    instId, quote.instrument_id, quote.exchange_id, (int)quote.instrument_type);*/

                get_public_writer()->close_data();
            }
        }
    }

    // 解析 交易频道
    void MarketDataOkx::parse_trades(const nlohmann::json& jsonValue) {
        std::string instId = jsonValue["instId"].get<std::string>();
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            Transaction& transaction = get_public_writer()->open_data<Transaction>(0);
            from_okx(jsonValue, transaction);
            transaction.instrument_id = inst->second.instrument_id;
            transaction.exchange_id = inst->second.exchange_id;
            transaction.instrument_type = inst->second.instrument_type;
            get_public_writer()->close_data();

        }
    }

    // 解析 交易频道
    void MarketDataOkx::parse_trades_all(const nlohmann::json& jsonValue) {
        std::string instId = jsonValue["instId"].get<std::string>();
        auto inst = m_map_scribe_instruments.find(instId);
        if (inst != m_map_scribe_instruments.end()) {
            Transaction& transaction = get_public_writer()->open_data<Transaction>(0);
            from_okx(jsonValue, transaction);
            transaction.instrument_id = inst->second.instrument_id;
            transaction.exchange_id = inst->second.exchange_id;
            transaction.instrument_type = inst->second.instrument_type;
            get_public_writer()->close_data();
        }
    }

    bool MarketDataOkx::request_market_tickers_info(const std::string& instType)
    {
        SPDLOG_INFO("request_market_tickers info begin.");
        int nRet = -1;
        std::string url = SPLICE(OKX_MARKET_TICKERS_URL);
        url += "?instType=";
        url += instType;
        SPDLOG_INFO("request_market_tickers info url:{}", url);
        std::string resData = CLibCurlHelper::getInstance().GetJsonFile(url.c_str(), GET, nullptr, nRet);
        //SPDLOG_DEBUG("request_market_tickers info resData len={}", resData.length());
        //SPDLOG_DEBUG("request_market_tickers info resData ={}", resData);
        
        //https://www.okx.com/api/v5/public/instruments?instType=SPOT
        if (!resData.empty())
        {
            nlohmann::json jsonQuote = nlohmann::json::parse(resData.c_str());

            if (!jsonQuote.is_object()) {
                SPDLOG_ERROR("request_market_tickers info error string: {}", resData);
                return false;
            }
            std::string code = jsonQuote["code"].get<std::string>();

            if (code != "0") {
                SPDLOG_ERROR("request_market_tickers info error code: {}", code);
                return false;
            }

            for (auto item : jsonQuote["data"].items()) {
                Instrument& inst = get_public_writer()->open_data<Instrument>(time::now_in_nano());
				from_okx(item.value(), inst);

                auto iter = map_instrument_infos_.find(inst.instrument_id);
                if (iter != map_instrument_infos_.end()) {
                    if (!iter->second.ctMult.empty()) {
						inst.contract_multiplier = atoi(iter->second.ctMult.c_str());
                    }
					else {
						inst.contract_multiplier = 1;
                    }
					inst.price_tick = atof(iter->second.tickSz.c_str());
					inst.quantity_unit = atof(iter->second.minSz.c_str());
                }

                get_public_writer()->close_data();
            }
        }
        else
        {
            SPDLOG_ERROR("request_market_tickers info error: {}", nRet);
            return false;
        }
        SPDLOG_INFO("request_market_tickers info success end.");
        return true;
    }

	bool MarketDataOkx::request_public_instruments_info(const std::string& instType)
	{
		SPDLOG_INFO("request_public_instruments info begin.");
		int nRet = -1;
		std::string url = SPLICE(OKX_PUBLIC_INSTRUMENTS_URL);
		url += "?instType=";
		url += instType;
		SPDLOG_INFO("request_market_tickers info url:{}", url);
		std::string resData = CLibCurlHelper::getInstance().GetJsonFile(url.c_str(), GET, nullptr, nRet);

		if (!resData.empty())
		{
			nlohmann::json jsonQuote = nlohmann::json::parse(resData.c_str());

			if (!jsonQuote.is_object()) {
				SPDLOG_ERROR("request_public_instruments info error string: {}", resData);
				return false;
			}
			std::string code = jsonQuote["code"].get<std::string>();

			if (code != "0") {
				SPDLOG_ERROR("request_public_instruments info error code: {}", code);
				return false;
			}

			for (auto item : jsonQuote["data"].items()) {
                InstrumentInfo info;
                from_okx_public_instruments(item.value(), info);
				map_instrument_infos_.emplace(info.instId, info);
			}
		}
		else
		{
			SPDLOG_ERROR("request_public_instruments info error: {}", nRet);
			return false;
		}

		SPDLOG_INFO("request_public_instruments info success end.");
		return true;
	}

} // namespace kungfu::wingchun::okx