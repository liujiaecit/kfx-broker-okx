#include "websocket_helper.h"
#include "websocket_manage.h"

namespace kungfu::wingchun::okx {
    CWebsocketHelper::CWebsocketHelper(CWebsocketManage* pWsManage, int connId)
        : ws_manage_(pWsManage)
        , connId_(connId)
    {

    }

    CWebsocketHelper::~CWebsocketHelper()
    {

    }

    bool CWebsocketHelper::init(const std::string& url) {
        reconnect_count_ = 0;
        dataServiceAddr_ = url;
        m_pWebSocketOkxMd = (IWebSocketBase*)wb_init(this, url.c_str());

        if (m_pWebSocketOkxMd) {
            int nRet = m_pWebSocketOkxMd->init();
            SPDLOG_INFO("m_pWebSocketOkxMd init: {}", nRet);

            bool bRet = m_pWebSocketOkxMd->connect();

            if (!bRet) {
                SPDLOG_ERROR("m_pWebSocketOkxMd connect: {}", bRet);
                b_is_connect = false;
                ws_manage_->update_state(BrokerState::DisConnected);
                return false;
            }
            else {
                b_is_connect = true;
                SPDLOG_INFO("m_pWebSocketOkxMd connect: {}", bRet);
            }

            return true;
        }

        return false;
    }

    void CWebsocketHelper::uninit() {
        if (m_pWebSocketOkxMd) {
            wb_release(m_pWebSocketOkxMd);
        }
    }

    void CWebsocketHelper::check_and_reconnect() {
        if (!b_is_connect) {
            reconnect_count_++;
            if (reconnect_count_ <= 3) {
                SPDLOG_INFO("reconnect reconnect_count_={}", reconnect_count_);
                reconnect_server();
            }
        }
    }

    void CWebsocketHelper::reconnect_server() {
        SPDLOG_INFO("reconnect_server begin");
        if (m_pWebSocketOkxMd) {
            wb_release(m_pWebSocketOkxMd);

            bool bRet = false;
            m_pWebSocketOkxMd = (IWebSocketBase*)wb_init(this, dataServiceAddr_.c_str());
            if (m_pWebSocketOkxMd) {
                int nRet = m_pWebSocketOkxMd->init();
                SPDLOG_INFO("reconnect_server m_pWebSocketOkxMd init: {}", nRet);
                bRet = m_pWebSocketOkxMd->connect();
            }

            if (!bRet) {
                SPDLOG_ERROR("reconnect_server connect: {}", bRet);
                b_is_connect = false;
                //ws_manage_->update_state(BrokerState::DisConnected);
            }
            else {
                b_is_connect = true;
                SPDLOG_INFO("reconnect_server pWebSocketOkxMd connect: {}", bRet);
            }
        }
        SPDLOG_INFO("reconnect_server end");
    }

    void CWebsocketHelper::on_ws_notify(void* pUser, const char* pszData, E_WSCBNotifyType type) {
        if (pszData) {
            //SPDLOG_DEBUG("on_ws_notify type = {}, pszData={}", int(type), pszData);
        }
        else {
            SPDLOG_DEBUG("on_ws_notify type = {}", int(type));
        }
        switch (type) {
        case EWSCBNotifyMessage: {
            if (strcmp(pszData, std::string("pong").c_str()) == 0) {
                SPDLOG_TRACE("on_ws_notify ------ pong");
                return;
            }
            break;
        }
        case EWSCBNotifyCreateConnectFailed: {
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyCreateConnectFailed");
            b_is_connect = false;
            break;
        }
        case EWSCBNotifyConnectFailed:
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyConnectFailed}");
            b_is_connect = false;
            break;
        case EWSCBNotifyConnectSuccess: {
            b_is_connect = true;
            break;
        }
        case EWSCBNotifyConnectInterrupt:
            SPDLOG_ERROR("on_ws_notify EWSCBNotifyConnectInterrupt}");
            b_is_connect = false;
            break;
        case EWSCBNotifyClose:
            SPDLOG_INFO("on_ws_notify EWSCBNotifyClose");
            b_is_connect = false;
            break;
        default:
            break;
        }

        if (ws_manage_) {
            ws_manage_->websocket_notify(pUser, pszData, type, connId_);
        }
    }

    void CWebsocketHelper::subscribe(const std::string& data, const std::set<uint32_t>& sub_sets) {
        if (m_pWebSocketOkxMd) {
            SPDLOG_DEBUG("subscribe data = {}, size={}", data, sub_sets.size());
            for (auto& iter : sub_sets) {
                if (map_sub_sets_.find(iter) == map_sub_sets_.end()) {
                    map_sub_sets_.insert(iter);
                }
            }

            m_pWebSocketOkxMd->send(data);
        }
    }

    void CWebsocketHelper::unsubscribe(const std::string& data, const std::set<uint32_t>& sub_sets) {
        if (m_pWebSocketOkxMd) {
            SPDLOG_DEBUG("unsubscribe data = {}, size={}", data, sub_sets.size());
            for (auto& iter : sub_sets) {
                auto iterSub = map_sub_sets_.find(iter);
                if (iterSub != map_sub_sets_.end()) {
                    map_sub_sets_.erase(iterSub);
                }
            }

            m_pWebSocketOkxMd->send(data);
        }
    }

    //< 发送ping 防止30s没有收到信息服务器会主动断开
    void CWebsocketHelper::send_ping() {
        if (m_pWebSocketOkxMd) {
            SPDLOG_DEBUG("send_ping");
            m_pWebSocketOkxMd->send("ping");
        }
    }

    int CWebsocketHelper::get_subscribe_count()
    {
        SPDLOG_DEBUG("get_subscribe_count size={}", map_sub_sets_.size());
        return map_sub_sets_.size();
    }
}