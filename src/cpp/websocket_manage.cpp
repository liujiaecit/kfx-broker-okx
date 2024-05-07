#include "websocket_manage.h"
#include "websocket_helper.h"
#include "marketdata_okx.h"

namespace kungfu::wingchun::okx {
	CWebsocketManage::CWebsocketManage(MarketDataOkx* pMd)
		: md_(pMd)
	{

	}

	CWebsocketManage::~CWebsocketManage()
	{

	}

    void CWebsocketManage::init() {
        get_public_ws();
        get_business_ws();
    }

    void CWebsocketManage::uninit() {
        /*if (m_pWebSocketOkxMd) {
            wb_release(m_pWebSocketOkxMd);
        }*/
    }

    void CWebsocketManage::send_ping() {
        for (auto& iter : map_public_websockets_) {
            if (iter.second && iter.second->is_connected()) {
                iter.second->send_ping();
            }
        }

        for (auto& iter : map_business_websockets_) {
            if (iter.second && iter.second->is_connected()) {
                iter.second->send_ping();
            }
        }
    }

	void CWebsocketManage::subscribe(const std::vector<InstrumentKey>& instrument_keys) {
        SPDLOG_INFO("subscribe instrument_keys size = {}", instrument_keys.size());

        size_t size = instrument_keys.size();
        std::shared_ptr<CWebsocketHelper> public_ws = get_public_ws();
        if (!public_ws) {
            return;
        }

        std::shared_ptr<CWebsocketHelper> business_ws = get_business_ws();
        if (!business_ws) {
            return;
        }

        for (const auto& inst : instrument_keys) {
            subscribe_instruments_cache_.emplace_back(inst);
        }

        if (public_ws->get_subscribe_count() == 0 && public_ws->get_connId() != 1) {
            return;
        }

        do_subscribe();
	}

    void CWebsocketManage::do_subscribe() {
        if (subscribe_instruments_cache_.size() == 0) {
            return;
        }

        SPDLOG_INFO("sub subscribe_instruments_cache_ = {}", subscribe_instruments_cache_.size());

        std::shared_ptr<CWebsocketHelper> public_ws = get_public_ws();
        if (!public_ws) {
            return;
        }

        std::shared_ptr<CWebsocketHelper> business_ws = get_business_ws();
        if (!business_ws) {
            return;
        }
        int can_sub_count = 30 - public_ws->get_subscribe_count();
        int cache_size = subscribe_instruments_cache_.size();
        can_sub_count = (can_sub_count > cache_size) ? cache_size : can_sub_count;

        SPDLOG_INFO("sub subscribe_instruments_cache_  can_sub_count= {}, cache_size={}", can_sub_count, cache_size);

        nlohmann::json jSubscribe;
        nlohmann::json jSymbols = nlohmann::json::array();

        nlohmann::json jSubBusiness;
        nlohmann::json jSubBusinessSymbols = nlohmann::json::array();
        int count = 0;

        std::set<uint32_t> sub_sets;
        for (auto inst = subscribe_instruments_cache_.begin(); inst != subscribe_instruments_cache_.end(); ) {
            count++;
            SPDLOG_DEBUG("subscribe exchange_id = {}, instrument_id={}", inst->exchange_id, inst->instrument_id);
            std::string symbol = inst->instrument_id;
            
            // tickers
            nlohmann::json jchannelTickers;
            jchannelTickers["channel"] = "tickers";
            jchannelTickers["instId"] = symbol;
            // books
            nlohmann::json jchannelBooks;
            jchannelBooks["channel"] = "books";
            jchannelBooks["instId"] = symbol;

            jSymbols.emplace_back(jchannelTickers);
            jSymbols.emplace_back(jchannelBooks);

            // trades
            nlohmann::json jchannelTrades;
            //jchannelTrades["channel"] = "trades";
            jchannelTrades["channel"] = "trades-all";
            jchannelTrades["instId"] = symbol;
            jSubBusinessSymbols.emplace_back(jchannelTrades);

            sub_sets.emplace(hash_instrument(inst->exchange_id, inst->instrument_id));

            // 删除一条
            inst = subscribe_instruments_cache_.erase(inst);

            if (can_sub_count == count) {
                if (public_ws) {
                    jSubscribe["op"] = "subscribe";
                    jSubscribe["args"] = jSymbols;
                    std::string strSub = jSubscribe.dump();
                    SPDLOG_DEBUG("MarketDataOkx::subscribe public = {}", strSub);
                    public_ws->subscribe(strSub, sub_sets);
                }

                if (business_ws) {
                    jSubBusiness["op"] = "subscribe";
                    jSubBusiness["args"] = jSubBusinessSymbols;
                    std::string strSub = jSubBusiness.dump();
                    SPDLOG_DEBUG("MarketDataOkx::subscribe business = {}", strSub);
                    business_ws->subscribe(strSub, sub_sets);
                }

                break;
            }
        }

        if (subscribe_instruments_cache_.size() > 0) {
            SPDLOG_DEBUG("subscribe 创建新链接，继续订阅 size = {}", subscribe_instruments_cache_.size());
            subscribe(subscribe_instruments_cache_);
        }
    }

	void CWebsocketManage::unsubscribe(const std::vector<InstrumentKey>& instrument_keys) {
        SPDLOG_DEBUG("unsubscribe instrument_keys size = {}", instrument_keys.size());
        //nlohmann::json jSubscribe;
        //nlohmann::json jSymbols = nlohmann::json::array();

        //for (const auto& inst : instrument_keys) {
        //    auto iter = m_set_subscribe_instrument.find(hash_instrument(inst.exchange_id, inst.instrument_id));
        //    if (iter != m_set_subscribe_instrument.end()) {
        //        // tickers
        //        nlohmann::json jchannelTickers;
        //        jchannelTickers["channel"] = "tickers";
        //        jchannelTickers["instId"] = inst.instrument_id;
        //        // books
        //        nlohmann::json jchannelBooks;
        //        jchannelBooks["channel"] = "books";
        //        jchannelBooks["instId"] = inst.instrument_id;

        //        jSymbols.emplace_back(jchannelTickers);
        //        //jSymbols.emplace_back(jchannelBooks);
        //        m_set_subscribe_instrument.erase(iter);
        //    }
        //}

        //if (jSymbols.size() > 0) {
        //    jSubscribe["op"] = "unsubscribe";
        //    jSubscribe["args"] = jSymbols;

        //    if (m_pWebSocketOkxMd) {
        //        std::string strSub = jSubscribe.dump();
        //        SPDLOG_DEBUG("MarketDataOtc::unsubscribe = {}", strSub);
        //        m_pWebSocketOkxMd->send(strSub);
        //    }
        //}
	}

    //< 获取可用websocket对象
    std::shared_ptr<CWebsocketHelper> CWebsocketManage::get_public_ws() {
        SPDLOG_DEBUG("get_public ws begin. size={}", map_public_websockets_.size());

        for (auto& iter : map_public_websockets_) {
            if (iter.second && iter.second->get_subscribe_count() < 30) {
                SPDLOG_DEBUG("get_public ws find. id={}", iter.first);
                return iter.second;
            }
        }

        // < 第一次订阅，需要先创建一个public websocket连接和一个business 连接
        websocket_id_++;
        SPDLOG_DEBUG("get_public ws new. websocket_id_={}", websocket_id_);

        if (websocket_id_ % 2 == 0) {
            SPDLOG_ERROR("get_public ws new. error websocket_id_={}", websocket_id_);
        }

        std::shared_ptr<CWebsocketHelper> pws_public = std::make_shared<CWebsocketHelper>(this, websocket_id_);
        pws_public->init(BASE_MD_URL_PUBLIC);
        ws_public_connected_ = false;
        map_public_websockets_.insert(std::make_pair(std::to_string(websocket_id_), pws_public));
        return pws_public;
    }

    std::shared_ptr<CWebsocketHelper> CWebsocketManage::get_business_ws() {
        SPDLOG_DEBUG("get_business ws begin. size={}", map_business_websockets_.size());
        for (auto& iter : map_business_websockets_) {
            if (iter.second && iter.second->get_subscribe_count() < 30) {
                SPDLOG_DEBUG("get_business ws find. id={}", iter.first);
                return iter.second;
            }
        }

        websocket_id_++;
        SPDLOG_DEBUG("get_business ws new. websocket_id_={}", websocket_id_);

        if (websocket_id_ % 2 != 0) {
            SPDLOG_ERROR("get_business ws new. error websocket_id_={}", websocket_id_);
        }

        std::shared_ptr<CWebsocketHelper> pws_business = std::make_shared<CWebsocketHelper>(this, websocket_id_);
        pws_business->init(BASE_MD_URL_BUSINESS);
        ws_business_connected_ = false;
        map_business_websockets_.insert(std::make_pair(std::to_string(websocket_id_), pws_business));
        return pws_business;
    }

    void CWebsocketManage::update_state(BrokerState state) {
        if (md_) {
            md_->update_broker_state(state);
        }
    }

    void CWebsocketManage::websocket_notify(void* pUser, const char* pszData, E_WSCBNotifyType type, int connId) {
        //SPDLOG_DEBUG("on_ws_notify type = {}, pszData={}", int(type), pszData);
        bool error = false;
        switch (type) {
        case EWSCBNotifyCreateConnectFailed:
        case EWSCBNotifyConnectFailed:
        case EWSCBNotifyConnectInterrupt:
        case EWSCBNotifyClose: {
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyCreateConnectFailed connId={}", connId);
            error = true;
            break;
        }
        case EWSCBNotifyConnectSuccess: {
            SPDLOG_INFO("on_ws_notify EWSCBNotifyConnectSuccess connId = {}", connId);
            if (connId % 2 == 0) {
                ws_business_connected_ = true;
            }
            else {
                ws_public_connected_ = true;                
            }

            if (ws_public_connected_ && ws_business_connected_) {
                // 创建新链接成功之后，开始执行订阅
                SPDLOG_INFO("do_subscribe 创建ws 连接成功，执行订阅。");
                do_subscribe();
            }
            break;
        }
        default:
            break;
        }

        if (error) {
            auto iterPublicWs = map_public_websockets_.find(std::to_string(connId));
            if (iterPublicWs != map_public_websockets_.end()) {
                map_public_websockets_.erase(iterPublicWs);
            }

            auto iterws = map_business_websockets_.find(std::to_string(connId));
            if (iterws != map_business_websockets_.end()) {
                map_business_websockets_.erase(iterws);
            }

            if (map_public_websockets_.size() != 0 || map_business_websockets_.size() != 0) {
                return;
            }
        }

        if (md_) {
            md_->websocket_notify(pszData, type);
        }
    }
}