#ifndef TRADER_OKX_H
#define TRADER_OKX_H

#include <kungfu/common.h>
#include <kungfu/wingchun/broker/trader.h>
#include <kungfu/wingchun/encoding.h>
#include <kungfu/yijinjing/common.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "buffer_data.h"
#include "iwebsocketMd.h"

namespace kungfu::wingchun::okx {
    using namespace kungfu::longfist::types;
    using namespace kungfu::longfist::enums;

    class TraderOkx : public broker::Trader, public IWebSocketMD {
    public:
        explicit TraderOkx(broker::BrokerVendor& vendor);
        virtual ~TraderOkx();

    public:
        bool insert_order(const event_ptr& event) override;
        bool cancel_order(const event_ptr& event) override;
        bool req_position() override;
        bool req_account() override;
        void on_recover() override;

        void reconnect_server();
        void check_and_reconnect();
        bool on_custom_event(const event_ptr& event) override;


        [[nodiscard]] longfist::enums::AccountType get_account_type() const override {
            return longfist::enums::AccountType::Credit;
        }
        virtual void on_ws_notify(void* pUser, const char* pszData, E_WSCBNotifyType type) override;

    public:
        bool req_history_order();
        bool req_history_trade();

    protected:
        void pre_start() override;
        void on_start() override;
        void on_exit() override;

    private:
        int run();
        // 轮询请求持仓和资产
        void sync_account_position();
        void send_ping();
        // 账户频道
        void ws_account();
        // 持仓频道
        void ws_position();
        // 订单频道
        void ws_orders();

        void on_return_login(const nlohmann::json& jsonValue);
        void on_return_account(const nlohmann::json& jsonValue);
        void on_query_account_position(const nlohmann::json& jsonValue);
        void on_return_position(const nlohmann::json& jsonValue);
        void on_query_position(const nlohmann::json& jsonValue);
        void on_return_orders(const nlohmann::json& jsonValue);
        void on_return_insert_order(const nlohmann::json& jsonValue);
        void on_return_cancel_order(const nlohmann::json& jsonValue);
        
    private:
        // 获取当前时间的utc格式
        std::string get_utc_time();
        bool login();
        void try_ready();
        // 获取所有可交易产品的信息列表
        bool request_public_instruments_info(const std::string& instType);
        std::string request_http_path(const std::string& url_path);
        bool custom_OnRtnTraderData(const event_ptr& event);
        bool custom_OnRtnAssetData(const event_ptr& event);
        bool custom_OnRtnPositionData(const event_ptr& event);
        bool custom_OnRtnPositionEndData(const event_ptr& event);
		bool custom_OnRtnOrderData(const event_ptr& event);
		bool custom_OnRtnInsertOrderData(const event_ptr& event);
		bool custom_OnRtnCancelOrderData(const event_ptr& event);

        bool custom_return_trader_data(const BufferTraderField& okx_trader);
        bool custom_return_asset(const BufferOKXAssetField& okx_asset);
		bool custom_return_position_data(const BufferOKXPositionField& okx_position);
		bool custom_return_order_data(const BufferOKXOrderField& okx_order);
		bool custom_return_insert_order(const BufferOKXHandleOrderField& okx_order);
		bool custom_return_cancel_order(const BufferOKXHandleOrderField& okx_order);

        void OnResponseTraderData(const std::string& strData);

        static inline std::string make_ExchangeID_OrderSysID(const char* ExchangeID, const char* OrderSysID) {
            return fmt::format("{}:{}", ExchangeID, OrderSysID);
        }

    private:
        bool b_is_connect = false;
        IWebSocketBase* websocket_okx_td_ = nullptr;
        bool client_exit_ = false;
        std::string dataServiceAddr_;
        int reconnect_count_ = 0;

        std::string account_id_;    
        std::string apiKey_;
        std::string password_;
        std::string secretkey_;
        bool virtual_trader_ = false;

        bool sync_external_order_ = false; // 是否同步外部订单
        bool recover_order_trade_ = false; // 是否恢复订单

        inline static std::shared_ptr<TraderOkx> td_; // 用于给回调函数中static函数访问对象内成员

        std::unordered_map< std::string, InstrumentInfo> map_instrument_infos_;	// 合约基本信息
		std::unordered_map<std::string, uint64_t> map_kf_local_insert_order_; // 本地下单记录订单id
		std::unordered_map<uint64_t, std::string> map_kf_orderid_to_okx_ordId_; // 下单后okx返回成功的订单id
		std::unordered_map<std::string, uint64_t> map_okx_ordId_to_kf_orderid_; // 下单后okx返回成功的订单id
		std::unordered_map<uint64_t, uint64_t> map_okx_cancel_order_;          //< 撤单列表 取消id，订单id
        std::unordered_map<std::string, std::string> map_instrument_margin_query_; //< 请求保证金率map表
        std::unordered_map<std::string, std::unordered_map<std::string, double>> map_currency_rate_;

        std::unordered_set<std::string> set_trade_wth_id_keys_;        //< 成交推送id

        bool connected_ = false;
        bool login_ = false;
        bool req_history_order_over_ = false;
        bool req_history_trade_over_ = false;
        int histroy_order_count_ = 0;
        int histroy_trade_count_ = 0;

		bool b_subscribe_asset = false;
		bool b_subscribe_position = false;
		bool b_subscribe_orders = false;

        std::unordered_map<std::string, std::unordered_set<uint64_t>>
            map_ExchangeID_OrderSysID_to_TradeIDs_{}; // <交易所:订单号, set<已经处理过的成交编号>>
        bool b_account_ready_ = false;
        bool b_position_ready_ = false;

    };
} // namespace kungfu::wingchun::okx

#endif
