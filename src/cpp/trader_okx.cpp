#include "trader_okx.h"
#include "type_convert_okx.h"
#include <cstdlib>
#include <iostream>
#include <kungfu/yijinjing/util/os.h>
#include <regex>
#include <sstream>
#include "common/Base64Util.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <iostream>  
#include <chrono>  
#include <ctime>  
#include <iomanip>  
#include "url_define.h"
#include "LibCurlHelper.h"

namespace kungfu::wingchun::okx {
    using namespace kungfu::longfist;
    using namespace kungfu::longfist::enums;
    using namespace kungfu::longfist::types;
    using namespace kungfu::yijinjing;
    using namespace kungfu::yijinjing::data;

#define NOT_SPACE(x) ((x) != 0x20 && ((x) < 0 || 0x1d < (x)))

    template <typename CharType> void StringTrimT(std::basic_string<CharType>& output) {
        if (output.empty())
            return;
        size_t bound1 = 0;
        size_t bound2 = output.length();
        const CharType* src = output.data();

        for (; bound2 > 0; bound2--)
            if (NOT_SPACE(src[bound2 - 1]))
                break;

        for (; bound1 < bound2; bound1++)
            if (NOT_SPACE(src[bound1]))
                break;

        if (bound1 < bound2) {
            memmove((void*)src, src + bound1, sizeof(CharType) * (bound2 - bound1));
        }

        output.resize(bound2 - bound1);
    }

    std::string& StringTrim(std::string& input) /* both left and right */
    {
        StringTrimT<char>(input);
        return input;
    }

    uint64_t okx_string_to_int(const std::string& ordId) {
        uint64_t order_id = 0;
        std::string str_order_id = ordId;
        str_order_id = StringTrim(str_order_id);
        int len = str_order_id.length();
        if (!str_order_id.empty()) {
            std::istringstream stream(str_order_id);
            stream >> order_id;
        }

        return order_id;
    }

    std::string double_to_string(double value) {
        std::ostringstream oss;
        oss << value;
        std::string str_value = oss.str();
        transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);

        size_t pos = str_value.find('e');
        if (pos == std::string::npos) {
            return std::to_string(value);
        }
        else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(15) << value;
            std::string str_value = oss.str();
            size_t pos = str_value.find_last_not_of('0');
            if (pos != std::string::npos) {
                if (str_value[pos] == '.') {
                    pos = str_value.find_last_not_of('.');
                }
                str_value.erase(pos + 1);
            }
            else {
                str_value = "0";
            }
            return str_value;
        }

    }

    TraderOkx::TraderOkx(broker::BrokerVendor& vendor) : Trader(vendor) {
        KUNGFU_SETUP_LOG();
    }

    TraderOkx::~TraderOkx() {}

    void TraderOkx::pre_start() {
        auto config = nlohmann::json::parse(get_config());
        account_id_ = config.value("account_id", "");
        password_ = config.value("password", "");
        apiKey_ = config.value("api_key", "");
        secretkey_ = config.value("secret_key", "");
        sync_external_order_ = config.value<bool>("sync_external_order", false);
        virtual_trader_ = config.value<bool>("virtual_trader", false);
        SPDLOG_DEBUG("pre_start account_id_={}, password_={}, apiKey_={}, secretkey_={}, sync_external_order={}", 
            account_id_, password_, apiKey_, secretkey_, sync_external_order_);
    }

    // 获取utc时间，因为请求http短连接时间戳必须转换成utc iso标准时间才行
    std::string TraderOkx::get_utc_time() {
        auto now = std::chrono::system_clock::now();
        // 转换为从1970-01-01 00:00:00 UTC开始的秒数  
        auto duration = now.time_since_epoch();
        // 转换为秒  
        std::time_t seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        // 转换为tm结构以获取年、月、日筿 
        std::tm* timeinfo = std::gmtime(&seconds);

        std::stringstream ss;
        ss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%SZ"); // 格式化时间为字符丿
        SPDLOG_DEBUG("on_start strTime={}", ss.str());
        return ss.str();
    }

    void TraderOkx::on_start() {
        SPDLOG_INFO("on_start begin.");
        if (virtual_trader_) {
            dataServiceAddr_ = BASE_TD_URL_PRIVATE;
        }
        else {
            dataServiceAddr_ = BASE_URL_PRIVATE;
        }
        reconnect_count_ = 0;
        td_ = std::shared_ptr<TraderOkx>(this);
        websocket_okx_td_ = (IWebSocketBase*)wb_init(this, dataServiceAddr_.c_str());
        run();

        SPDLOG_INFO("on_start begin 1");
        request_public_instruments_info(SPOT_INST_TYPE);
        request_public_instruments_info(SWAP_INST_TYPE);
        request_public_instruments_info(FUTURES_INST_TYPE);

        SPDLOG_INFO("on_start begin 2");
        SPDLOG_INFO("on_start end.");
    }

    void TraderOkx::on_exit() {
        SPDLOG_INFO("on_exit");
        client_exit_ = true;

        if (websocket_okx_td_) {
            wb_release(websocket_okx_td_);
        }
    }

    int TraderOkx::run() {
        if (websocket_okx_td_) {
            int nRet = websocket_okx_td_->init();
            SPDLOG_INFO("websocket_okx_td_ init: {}", nRet);
            bool bRet = websocket_okx_td_->connect();

            if (!bRet) {
                SPDLOG_ERROR("websocket_okx_td_ connect: {}", bRet);
                b_is_connect = false;
                update_broker_state(BrokerState::DisConnected);
            }
            else {
                b_is_connect = true;
                SPDLOG_INFO("websocket_okx_td_ connect: {}", bRet);
            }
        }

        return 0;
    }

    bool TraderOkx::insert_order(const event_ptr& event) {
        const auto& input = event->data<OrderInput>();
        SPDLOG_INFO("SendOrder begin");

        if (!has_writer(event->source())) {
            SPDLOG_ERROR("insert_order: not find id:{}", event->source());
            return false;
        }
        // 先写入本地订单
        uint64_t nano = time::now_in_nano();
        auto writer = get_writer(event->source());
        auto& order = writer->open_data<Order>(event->gen_time());
        order_from_input(input, order);
        order.status = OrderStatus::Pending;
        order.insert_time = nano;
        order.update_time = nano;
        SPDLOG_INFO("insert_order order: {}", order.to_string());
        writer->close_data();

        SPDLOG_DEBUG("insert_order OrderInput = {}, nano={}", input.to_string(), nano);
        nlohmann::json jSendValue;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        std::string side, posSide;
        kf_to_okx_side_and_pos_side(input, side, posSide);
        jValue["side"] = side;
        if (!posSide.empty())
            jValue["posSide"] = posSide;
        jValue["instId"] = input.instrument_id;
        jValue["tdMode"] = kf_to_okx_mode(input);
        jValue["ordType"] = kf_order_type_to_okx(input);
        jValue["sz"] = double_to_string(input.volume);
        if (input.price_type != PriceType::Any) {
            jValue["px"] = double_to_string(input.limit_price);
        }
        jValue["ccy"] = kf_to_okx_ccy(input.instrument_id);

        if (input.instrument_type == InstrumentType::Crypto && input.price_type == PriceType::Any) {
            // 币币市价单委托数量sz的单位 base_ccy: 交易货币 ；quote_ccy：计价货币 仅适用于币币市价订单
            // 默认买单为quote_ccy，卖单为base_ccy
            jValue["tgtCcy"] = "base_ccy";
        }
        //"tgtCcy": "base_ccy",
        std::string clOrdId = "KF" + std::to_string(input.order_id);
        jValue["clOrdId"] = clOrdId; // 由用户设置的订单ID 字母（区分大小写）与数字的组合，可以是纯字母、纯数字且长度要圿 - 32位之间〿
        jArgs.emplace_back(jValue);

        jSendValue["id"] = std::to_string(input.order_id);
        jSendValue["op"] = "order";
        jSendValue["args"] = jArgs;
        std::string strData = jSendValue.dump();
        SPDLOG_INFO("insert_order jSendValue dump = {}", strData);

        map_kf_local_insert_order_.emplace(clOrdId, input.order_id);
        if (websocket_okx_td_) {
            websocket_okx_td_->send(strData);
        }

        SPDLOG_INFO("SendOrder end");
        return true;
    }

    bool TraderOkx::cancel_order(const event_ptr& event) {
        SPDLOG_INFO("cancel_order begin");
        const auto& action = event->data<OrderAction>();
        auto& order_state = get_order(action.order_id); // orders_.at(action.order_id);
        SPDLOG_DEBUG("map_kf_orderid_to_okx_ordId_ 取消订单 action.order_id={}, action.order_action_id={}", action.order_id, action.order_action_id);

        auto ordIdIter = map_kf_orderid_to_okx_ordId_.find(action.order_id);
        if (ordIdIter == map_kf_orderid_to_okx_ordId_.end()) {
            SPDLOG_ERROR("cancel_order map_kf_orderid_to_okx_ordId can not find order_id ={}", action.order_id);
            return false;
        }

        nlohmann::json jSendValue;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        // todo marsjliu
        jValue["instId"] = order_state.data.instrument_id;
        jValue["ordId"] = ordIdIter->second;
        jArgs.emplace_back(jValue);

        jSendValue["id"] = std::to_string(action.order_action_id);
        jSendValue["op"] = "cancel-order";
        jSendValue["args"] = jArgs;
        std::string strData = jSendValue.dump();
        SPDLOG_DEBUG("ws_cancel_order jSendValue dump = {}", strData);

        map_okx_cancel_order_.emplace(action.order_action_id, action.order_id);

        if (websocket_okx_td_) {
            websocket_okx_td_->send(strData);
        }

        SPDLOG_INFO("cancel_order end");
        return true;
    }
    bool TraderOkx::req_position() {
        SPDLOG_DEBUG("req position begin.");
        ws_position();
        ws_orders();
        // 底层1分钟轮训一次
        sync_account_position();
        SPDLOG_DEBUG("req position end.");
        return true;
    }

    bool TraderOkx::req_account() {
        SPDLOG_DEBUG("req account begin.");
        ws_account();
        SPDLOG_DEBUG("req account end.");

        return true;
    }

    void TraderOkx::on_recover() {
        SPDLOG_INFO("on_recover begin orders_ size={}, trades_ size ={} ", get_orders().size(), get_trades().size());
        for (auto& pair : get_orders()) {
            const std::string str_external_order_id = pair.second.data.external_order_id.to_string();
            if (not str_external_order_id.empty()) {
                uint64_t order_id = pair.first;
                SPDLOG_INFO("on_recover str_external_order_id={}, order_id={}", str_external_order_id, order_id);
                map_kf_orderid_to_okx_ordId_.emplace(order_id, str_external_order_id);
                map_okx_ordId_to_kf_orderid_.emplace(str_external_order_id, order_id);

                // 加入撤单列表
                uint64_t id = pair.second.data.order_id;
                map_okx_cancel_order_.emplace(order_id, id);
            }
        }

        for (const auto& trade_pair : get_trades()) {
            const Trade& trade = trade_pair.second.data;
            SPDLOG_DEBUG("on_recover trade trade_id={}, order_id ={}, external_order_id={}, external_trade_id={} ",
                trade.trade_id, trade.order_id, trade.external_order_id, trade.external_trade_id);
            map_ExchangeID_OrderSysID_to_TradeIDs_
                .try_emplace(make_ExchangeID_OrderSysID(trade.exchange_id.value, trade.external_order_id.value))
                .first->second.emplace(trade.trade_id);
        }

        SPDLOG_INFO("req recover end.");
    }

    void TraderOkx::check_and_reconnect() {
        if (!b_is_connect) {
            reconnect_count_++;
            if (reconnect_count_ <= 3) {
                SPDLOG_INFO("reconnect reconnect_count_={}", reconnect_count_);
                reconnect_server();
            }
        }
    }

    void TraderOkx::reconnect_server() {
        SPDLOG_INFO("reconnect_server begin");
        if (websocket_okx_td_) {
            wb_release(websocket_okx_td_);

            bool bRet = false;
            websocket_okx_td_ = (IWebSocketBase*)wb_init(this, dataServiceAddr_.c_str());
            run();
        }
        SPDLOG_INFO("reconnect_server end");
    }

    // 查询委托订单
    bool TraderOkx::req_history_order() {
        // /api/v5/trade/orders-history
        SPDLOG_DEBUG("req_history order begin");
        std::string instType = SPOT_INST_TYPE;
        std::string url = OKX_TRADE_ORDERS_HISTORY_URL;
        url += "?instType=";
        url += instType;
        std::string strData = request_http_path(url);

        SPDLOG_DEBUG("req_history order ={}", strData);
        if (!strData.empty())
        {
            nlohmann::json jsonValue = nlohmann::json::parse(strData.c_str());

            if (jsonValue.is_object()) {
                try {
                    //on_return_account_position(jsonValue);
                }
                catch (...) {
                    SPDLOG_ERROR("parse history order error. strData ={}", strData);
                }
            }
        }

        SPDLOG_DEBUG("req_history order end");

        req_history_order_over_ = true;
        return true;
    }

    bool TraderOkx::req_history_trade() {
        SPDLOG_DEBUG("req_history trade begin");

        req_history_trade_over_ = true;
        try_ready();
        SPDLOG_DEBUG("req_history trade end");
        return true;
    }

    void TraderOkx::try_ready() {
        SPDLOG_INFO("try_ready begin");

        if (BrokerState::Ready == get_state()) {
            SPDLOG_WARN("try_ready is ready.");
            return;
        }

        SPDLOG_DEBUG("req_history_order_over_: {}, req_history_trade_over_={}", req_history_order_over_, req_history_trade_over_);
        SPDLOG_DEBUG("histroy_order_count_: {}, histroy_trade_count_: {}, disable_recover_={}", histroy_order_count_, histroy_trade_count_, disable_recover_);
        if (disable_recover_ or (req_history_order_over_ and req_history_trade_over_ and histroy_order_count_ == 0 and
            histroy_trade_count_ == 0)) {
            update_broker_state(BrokerState::Ready);
            sync_account_position();
            SPDLOG_INFO("try_ready login in successful.");

            // 连接成功之后，开始检测重连
            add_time_interval(time_unit::NANOSECONDS_PER_SECOND * 30, [&](auto e) { check_and_reconnect(); });
        }
        SPDLOG_INFO("try_ready end");
    }


    void TraderOkx::on_ws_notify(void* pUser, const char* pszData, E_WSCBNotifyType type) {
        SPDLOG_DEBUG("on_ws_notify type = {}", int(type));
        if (EWSCBNotifyMessage == type && strcmp(pszData, std::string("pong").c_str()) == 0) {
            //SPDLOG_DEBUG("on_ws_notify ------ pong");
            return;
        }

        BufferTraderField buffer;
        buffer.type = int(type);

        if (pszData) {
            strncpy(buffer.data, pszData, DATA_SIZE);
            OnResponseTraderData(pszData);
            if (EWSCBNotifyMessage == type) {
                return;
            }
        }

        auto& bf_okx_trader = td_->get_thread_writer()->open_custom_data<int>(kOKXTraderFieldType, td_->now());
        memcpy(&bf_okx_trader, &buffer, sizeof(buffer));
        td_->get_thread_writer()->close_data();
    }

    bool TraderOkx::on_custom_event(const event_ptr& event) {
        SPDLOG_TRACE("msg_type: {}", event->msg_type());

        switch (event->msg_type()) {
        case kOKXTraderFieldType:
            return custom_OnRtnTraderData(event);
        case kOKXAssetFieldType:
            return custom_OnRtnAssetData(event);
        case kOKXPositionFieldType:
            return custom_OnRtnPositionData(event);
        case kOKXPositionEndFieldType:
            return custom_OnRtnPositionEndData(event);
        case kOKXOrderFieldType:
            return custom_OnRtnOrderData(event);
        case kOKXInsertOrderFieldType:
            return custom_OnRtnInsertOrderData(event);
        case kOKXCancelOrderFieldType:
            return custom_OnRtnCancelOrderData(event);
        default:
            SPDLOG_ERROR("unrecognized msg_type: {}", event->msg_type());
            return false;
        }
    }

    bool TraderOkx::custom_OnRtnTraderData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferTraderField>();
        return custom_return_trader_data(bf_data);
    }

    bool TraderOkx::custom_return_trader_data(const BufferTraderField& okx_trader) {
        SPDLOG_DEBUG("custom_return_trader_data begin event_type={}", okx_trader.type);
        E_WSCBNotifyType type = (E_WSCBNotifyType)okx_trader.type;

        switch (type) {
        case EWSCBNotifyMessage: {
            OnResponseTraderData(okx_trader.data);
            break;
        }
        case EWSCBNotifyCreateConnectFailed:
            SPDLOG_ERROR("custom_return_trader_data EWSCBNotifyCreateConnectFailed");
            update_broker_state(BrokerState::LoginFailed);
            break;
        case EWSCBNotifyConnectFailed:
            SPDLOG_ERROR("custom_return_trader_data EWSCBNotifyConnectFailed}");
            update_broker_state(BrokerState::LoginFailed);
            break;
        case EWSCBNotifyConnectSuccess: {
            SPDLOG_INFO("custom_return_trader_data EWSCBNotifyConnectSuccess");
            update_broker_state(BrokerState::Connected);
            login();
            add_time_interval(time_unit::NANOSECONDS_PER_SECOND * 25, [&](auto e) { send_ping(); });
            //add_time_interval(time_unit::NANOSECONDS_PER_SECOND * 30, [&](auto e) { sync_account_position(); });
            break;
        }
        case EWSCBNotifyConnectInterrupt:
            SPDLOG_ERROR("custom_return_trader_data EWSCBNotifyConnectInterrupt}");
            update_broker_state(BrokerState::DisConnected);
            b_is_connect = false;
            reconnect_count_ = 0;
            break;
        case EWSCBNotifyClose:
            SPDLOG_INFO("custom_return_trader_data EWSCBNotifyClose");
            if (!client_exit_) {
                update_broker_state(BrokerState::DisConnected);
                b_is_connect = false;
                reconnect_count_ = 0;
            }
            break;
        case EWSCBNotifyReceive:
            break;
        default:
            break;
        }

        SPDLOG_DEBUG("custom_return_trader_data end");
        return true;
    }

    bool TraderOkx::custom_OnRtnAssetData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferOKXAssetField>();
        return custom_return_asset(bf_data);
    }

    bool TraderOkx::custom_return_asset(const BufferOKXAssetField& okx_asset) {
        SPDLOG_DEBUG("custom_return asset begin");
        auto writer = get_asset_writer();
        int64_t nano = time::now_in_nano();
        Asset& asset = writer->open_data<Asset>(nano);
        asset.avail = okx_asset.availBal_total;
        asset.avail_margin = okx_asset.availEq_total;
        asset.margin = okx_asset.frozenBal_total;
        asset.unrealized_pnl = okx_asset.isoUpl_total;
        asset.market_value = okx_asset.isoEq_total;
        asset.total_asset = okx_asset.total_asset;

        asset.holder_uid = get_home()->uid;
        asset.update_time = nano;// okx_asset.uTime;
        writer->close_data();
        enable_asset_sync();

        SPDLOG_DEBUG("custom_return asset end");
        return true;
    }

    bool TraderOkx::custom_OnRtnPositionEndData(const event_ptr& event) {
        auto writer = get_position_writer();
        PositionEnd& end = writer->open_data<PositionEnd>(location::SYNC);
        end.holder_uid = get_home()->uid;
        writer->close_data();
        enable_positions_sync();
        return true;
    }

    bool TraderOkx::custom_OnRtnPositionData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferOKXPositionField>();
        return custom_return_position_data(bf_data);
    }

    bool TraderOkx::custom_return_position_data(const BufferOKXPositionField& okx_position) {
        SPDLOG_DEBUG("custom_return position data begin");
        int64_t nano = time::now_in_nano();
        auto writer = get_position_writer();

        // 只更新的时候，写入到Public
        if (okx_position.is_update) {
            writer = get_public_writer();
        }

        Position& pos = writer->open_data<Position>(location::SYNC);
        strncpy(pos.instrument_id, okx_position.instrument_id, INSTRUMENT_ID_LEN);
        strncpy(pos.exchange_id, okx_position.exchange_id, EXCHANGE_ID_LEN);
        pos.instrument_type = enums::InstrumentType(okx_position.instrument_type);
        pos.ledger_category = LedgerCategory::Account;

        double price_tick = 0.0001;
        double quantity_unit = 0.0001;
        auto iter = map_instrument_infos_.find(pos.instrument_id);
        if (iter != map_instrument_infos_.end()) {
            price_tick = atof(iter->second.tickSz.c_str());
            quantity_unit = atof(iter->second.minSz.c_str());
        }

        pos.direction = okx_side_to_kf_direction(okx_position.posSide, okx_position.volume); // 持仓方向
        pos.volume = translate_value_by_price_tick(abs(okx_position.volume), quantity_unit);
        pos.static_yesterday = pos.volume;
        //pos.yesterday_volume = translate_value_by_price_tick(pos.volume, quantity_unit);
        pos.yesterday_volume = 0;
        pos.frozen_yesterday = 0;
        //pos.open_volume = pos.volume;
        pos.frozen_total = translate_value_by_price_tick(okx_position.frozen_total, quantity_unit); // 冻结数量
        pos.last_price = translate_value_by_price_tick(okx_position.last_price, price_tick);                     // 最新价
        pos.avg_open_price = translate_value_by_price_tick(okx_position.avg_open_price, price_tick);             // 总开仓均仿
        //pos.position_cost_price = okx_position.position_cost_price;   // 持仓成本  持仓均价
        pos.close_price = translate_value_by_price_tick(okx_position.close_price, price_tick);                   // 收盘价
        //pos.pre_close_price = okx_position.pre_close_price;           // 昨收盘价

        pos.position_pnl = translate_value_by_price_tick(okx_position.position_pnl, price_tick);     // 持仓盈亏
        pos.close_pnl = translate_value_by_price_tick(okx_position.close_pnl, price_tick);           // 平仓盈亏
        pos.realized_pnl = translate_value_by_price_tick(okx_position.realized_pnl, price_tick);     // 已实现盈亿
        pos.unrealized_pnl = translate_value_by_price_tick(okx_position.unrealized_pnl, price_tick); // 未实现盈亿
        pos.holder_uid = get_home()->uid;
        pos.source_id = get_home()->uid;

        pos.update_time = nano; // 更新时间
        writer->close_data();
        SPDLOG_DEBUG("custom_return position data Position: {}", pos.to_string());

        if (okx_position.is_last) {
            SPDLOG_DEBUG("custom_return position data PositionEnd begin.");
            PositionEnd& end = writer->open_data<PositionEnd>(location::SYNC);
            end.holder_uid = get_home()->uid;
            writer->close_data();
            enable_positions_sync();
            SPDLOG_DEBUG("custom_return position data PositionEnd end.");
        }

        SPDLOG_DEBUG("custom_return position data end");
        return true;
    }

    bool TraderOkx::custom_OnRtnInsertOrderData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferOKXHandleOrderField>();
        return custom_return_insert_order(bf_data);
    }

    bool TraderOkx::custom_return_insert_order(const BufferOKXHandleOrderField& okx_order) {
        SPDLOG_DEBUG("custom_return_handle order begin");
        std::string clOrdId = okx_order.clOrdId;
        std::string ordId = okx_order.ordId;
        std::string tag = okx_order.tag;
        std::string sCode = okx_order.sCode;
        std::string sMsg = okx_order.sMsg;

        uint64_t order_id = okx_string_to_int(okx_order.id);
        SPDLOG_DEBUG("custom_return_handle  order ordId={}, tag={}, sCode={}, sMsg={}, clOrdId={}", ordId, tag, sCode, sMsg, clOrdId);

        auto& orders = get_orders();

        if (order_id == 0 || orders.find(order_id) == orders.end()) {
            SPDLOG_WARN("custom_return_handle order outside ------- order_id={}, ordId={}", order_id, ordId);
            return true;
        }
        else {
            SPDLOG_DEBUG("custom_return_handle order find order_id={}", order_id);
            auto& order_state = get_order(order_id);
            // 状态不是终态
            if (sCode == "0" && !is_final_status(order_state.data.status)) {
                order_state.data.status = OrderStatus::Submitted;
                order_state.data.error_id = 0;

                strncpy(order_state.data.external_order_id, ordId.c_str(), EXTERNAL_ID_LEN);
                // 下单成功，okx返回订单id
                map_kf_orderid_to_okx_ordId_.emplace(order_id, ordId);
                map_okx_ordId_to_kf_orderid_.emplace(ordId, order_id);
                SPDLOG_DEBUG("map_kf_orderid_to_okx_ordId_ --=== 下单成功插入 order_id={}, ordId={}", order_id, ordId);
            }
            else {
                order_state.data.status = OrderStatus::Error;
                if (!sCode.empty()) {
                    order_state.data.error_id = okx_string_to_int(sCode);
                }
                else {
                    order_state.data.error_id = -1;
                }
            }

            if (order_state.data.error_id != 0) {
                strncpy(order_state.data.error_msg, sMsg.c_str(), ERROR_MSG_LEN);
            }

            order_state.data.update_time = okx_order.outTime;
            //order_state.data.update_time = time::now_in_nano();
            try_write_to(order_state.data, order_state.dest);
            SPDLOG_DEBUG("custom_return_handle order: {}", order_state.data.to_string());
        }
        SPDLOG_DEBUG("custom_return_handle order end");
        return true;
    }

    bool TraderOkx::custom_OnRtnCancelOrderData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferOKXHandleOrderField>();
        return custom_return_cancel_order(bf_data);
    }

    bool TraderOkx::custom_return_cancel_order(const BufferOKXHandleOrderField& okx_order) {
        SPDLOG_DEBUG("custom_return_cancel order begin");
        std::string clOrdId = okx_order.clOrdId;
        std::string ordId = okx_order.ordId;
        std::string sCode = okx_order.sCode;
        std::string sMsg = okx_order.sMsg;
        std::string code = okx_order.code;
        uint64_t order_action_id = okx_string_to_int(okx_order.id);
        SPDLOG_DEBUG("on_return_cancel order ordId={}, sCode={}, sMsg={}, clOrdId={}, order_action_id={}, code={}, msg={}", \
            ordId, sCode, sMsg, clOrdId, order_action_id, code, okx_order.msg);

        auto nano = time::now_in_nano();
        auto& orders = get_orders();
        if (order_action_id == 0 || map_okx_cancel_order_.find(order_action_id) == map_okx_cancel_order_.end()) {
            SPDLOG_ERROR("custom_return_cancel order outside cancel order, map_okx_cancel_order can not find. id = {}", okx_order.id);
            return false;
        }
        else {
            uint64_t order_id = map_okx_cancel_order_[order_action_id];
            if (orders.find(order_id) == orders.end()) {
                SPDLOG_ERROR("orders can not find. order_id = {}", order_id);
                return false;
            }

            SPDLOG_DEBUG("on_return_cancel order find order_id={}", order_id);
            auto& order_state = get_order(order_id);
            if (sCode == "0" && code == "0") {
                if (order_state.data.volume != order_state.data.volume_left) {
                    order_state.data.status = OrderStatus::PartialFilledNotActive;
                }
                else {
                    order_state.data.status = OrderStatus::Cancelled;
                }
            }
            else {
                if (has_writer(order_state.dest)) {
                    auto writer = get_writer(order_state.dest);
                    auto& error = writer->open_data<OrderActionError>(nano);
                    error.error_id = okx_string_to_int(sCode);
                    error.order_id = order_id;
                    error.order_action_id = order_action_id;
                    error.insert_time = time::now_in_nano();
                    strncpy(error.error_msg, sMsg.c_str(), ERROR_MSG_LEN);
                    writer->close_data();
                    SPDLOG_WARN("on_return_cancel OrderActionError: {}", error.to_string());
                }
                else {
                    SPDLOG_WARN("on_return_cancel has no writer of dest: {}", order_state.dest);
                }

                SPDLOG_DEBUG("on_return_cancel order end");
                return true;
            }

            //order_state.data.update_time = nano;
            order_state.data.update_time = okx_order.outTime;
            try_write_to(order_state.data, order_state.dest);
            SPDLOG_DEBUG("custom_return_cancel_order order: {}", order_state.data.to_string());
        }

        SPDLOG_DEBUG("custom_return_cancel order end");
        return true;
    }

    bool TraderOkx::custom_OnRtnOrderData(const event_ptr& event) {
        const auto& bf_data = event->custom_data<BufferOKXOrderField>();
        return custom_return_order_data(bf_data);
    }

    bool TraderOkx::custom_return_order_data(const BufferOKXOrderField& okx_order) {
        SPDLOG_DEBUG("custom_return_order data begin");
        uint64_t order_id = 0;
        SPDLOG_DEBUG("custom_return_order data clOrdId={}, ordId={}", okx_order.clOrdId, okx_order.ordId);
        // orders 推送会早于下单回报
        std::string clOrdId = okx_order.clOrdId;
        if (!clOrdId.empty()) {
            auto localIter = map_kf_local_insert_order_.find(clOrdId);
            if (localIter != map_kf_local_insert_order_.end()) {
                order_id = localIter->second;
            }
        }

        SPDLOG_DEBUG("custom_return_order data order_id={}", order_id);
        if (order_id == 0) {
            if (!sync_external_order_) {
                SPDLOG_WARN("custom_return_order no sync external order ordId={}", okx_order.ordId);
                return true;
            }

            SPDLOG_INFO("sync external order ordId={}", okx_order.ordId);
            if (map_okx_ordId_to_kf_orderid_.find(okx_order.ordId) == map_okx_ordId_to_kf_orderid_.end()) {
                SPDLOG_DEBUG("custom_return_order 系统外订单ordId={}", okx_order.ordId);
                Order& order = get_public_writer()->open_data<Order>(time::now_in_nano());
                if (order_id == 0) {
                    order_id = get_public_writer()->current_frame_uid();
                }

                order_from_okx_create(okx_order, order);
                order.order_id = order_id;
                strncpy(order.external_order_id, okx_order.ordId, EXTERNAL_ID_LEN);
                get_public_writer()->close_data();

                map_kf_orderid_to_okx_ordId_.emplace(order_id, okx_order.ordId);
                map_okx_ordId_to_kf_orderid_.emplace(okx_order.ordId, order_id);
                SPDLOG_DEBUG("map_kf_orderid_to_okx_ordId_ --=== 系统外订单返回order_id={}, ordId={}", order_id, okx_order.ordId);
            }
            else {
                order_id = map_okx_ordId_to_kf_orderid_[okx_order.ordId];
            }
        }

        if (order_id == 0)
        {
            SPDLOG_ERROR("custom_return_order data clOrdId={}, ordId={}", okx_order.clOrdId, okx_order.ordId);
            return false;
        }

        // orders 推送会早于下单order回报
        if (map_kf_orderid_to_okx_ordId_.find(order_id) == map_kf_orderid_to_okx_ordId_.end()) {
            map_kf_orderid_to_okx_ordId_.emplace(order_id, okx_order.ordId);
            map_okx_ordId_to_kf_orderid_.emplace(okx_order.ordId, order_id);
        }

        auto& order_state = get_order(order_id);
        order_from_okx(okx_order, order_state.data);
        try_write_to(order_state.data, order_state.dest);
        SPDLOG_DEBUG("custom_return_trade order: {}", order_state.data.to_string());

        uint64_t trade_id = okx_string_to_int(okx_order.tradeId);
        // 有订单成交
        if (trade_id != 0) {
            if (set_trade_wth_id_keys_.find(okx_order.tradeId) != set_trade_wth_id_keys_.end()) {
                SPDLOG_WARN("custom_return_order data the exist order, tradeId={}", okx_order.tradeId);
                return true;
            }

            set_trade_wth_id_keys_.emplace(okx_order.tradeId);

            auto writer = get_writer(order_state.dest);
            if (writer == nullptr) {
                SPDLOG_ERROR("custom_return_trade writer is null.");
                return false;
            }

            Trade& trade = writer->open_data<Trade>(time::now_in_nano());
            if (trade_id == 0) {
                trade_id = writer->current_frame_uid();
            }
            trade.trade_id = trade_id;
            trade.order_id = order_state.data.order_id;                          // 订单ID
            strcpy(trade.external_order_id, order_state.data.external_order_id); // 柜台订单id, 字符型则hash转换
            trade.trade_time = okx_order.fillTime;                                             // 成交时间
            strcpy(trade.instrument_id, order_state.data.instrument_id); // 合约ID
            strcpy(trade.exchange_id, order_state.data.exchange_id);     // 交易所ID
            trade.instrument_type = order_state.data.instrument_type;    // 合约类型
            trade.side = order_state.data.side;                          // 买卖方向
            trade.offset = order_state.data.offset;                      // 开平方吿
            trade.hedge_flag = order_state.data.hedge_flag;              // 投机套保标识
            trade.price = okx_order.fillPx;
            trade.volume = okx_order.fillSz;
            writer->close_data();

            //add_TradeID(trade.exchange_id, trade.external_order_id, trade.trade_id);
            SPDLOG_INFO("custom_return_trade Trade: {}", trade.to_string());
        }

        SPDLOG_DEBUG("custom_return_order data end");
        return true;
    }

    bool TraderOkx::login() {
        // https://www.okx.com/docs-v5/zh/?shell#overview-websocket-login
        SPDLOG_DEBUG("login begin");
        int64_t timestamp = time::now_in_nano() / time_unit::NANOSECONDS_PER_SECOND;

        std::string ca_data = std::to_string(timestamp) + "GET";
        ca_data += "/users/self/verify";
        SPDLOG_DEBUG("login timestamp = {}, ca_data = {}", timestamp, ca_data);

        unsigned char digest[EVP_MAX_MD_SIZE] = { '\0' };
        unsigned int digest_len = 0;
        HMAC(EVP_sha256(), secretkey_.c_str(), secretkey_.length(), (unsigned char*)ca_data.c_str(), ca_data.length(), digest, &digest_len);
        std::string strSignature;
        Util::Base64Encode(digest, digest_len, strSignature);

        nlohmann::json jLogin;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        jValue["apiKey"] = apiKey_;
        jValue["passphrase"] = password_;
        jValue["timestamp"] = std::to_string(timestamp);
        jValue["sign"] = strSignature;
        jArgs.emplace_back(jValue);

        jLogin["op"] = "login";
        jLogin["args"] = jArgs;
        std::string strLogin = jLogin.dump();
        SPDLOG_DEBUG("login dump = {}", strLogin);

        if (websocket_okx_td_) {
            websocket_okx_td_->send(strLogin);
        }

        return true;
    }

    std::string TraderOkx::request_http_path(const std::string& url_path) {
        SPDLOG_DEBUG("request_http_path begin");
        std::string strTime = get_utc_time();
        // UTC时间＿020-12-08T09:08:57.715Z
        std::string ca_data = strTime + "GET";
        ca_data += url_path;
        SPDLOG_DEBUG("loginHttp timestamp = {}, ca_data = {}", strTime, ca_data);

        unsigned char digest[EVP_MAX_MD_SIZE] = { '\0' };
        unsigned int digest_len = 0;
        HMAC(EVP_sha256(), secretkey_.c_str(), secretkey_.length(), (unsigned char*)ca_data.c_str(), ca_data.length(), digest, &digest_len);
        std::string strSignature;
        Util::Base64Encode(digest, digest_len, strSignature);

        std::vector<std::string> vectHeader;
        vectHeader.emplace_back("OK-ACCESS-KEY:" + apiKey_);
        vectHeader.emplace_back("OK-ACCESS-SIGN:" + strSignature);
        vectHeader.emplace_back("OK-ACCESS-TIMESTAMP:" + strTime);
        vectHeader.emplace_back("OK-ACCESS-PASSPHRASE:" + password_);
        if (virtual_trader_) {
            vectHeader.emplace_back("x-simulated-trading:1");
        }

        int nRet = -1;
        std::string url = "https://" HOST;
        url += url_path;
        SPDLOG_DEBUG("request_http_path info url:{}", url);
        std::string resData = CLibCurlHelper::getInstance().HttpGet(url.c_str(), vectHeader, nRet);
        SPDLOG_DEBUG("request_http_path nRet={}, resData={}", nRet, resData);

        return resData;
    }

    // 轮询请求持仓和资亿
    void TraderOkx::sync_account_position() {
        SPDLOG_DEBUG("sync_account position begin");
        std::string strData = request_http_path("/api/v5/account/balance");

        SPDLOG_DEBUG("sync_account position account ={}", strData);
        if (!strData.empty())
        {
            nlohmann::json jsonValue = nlohmann::json::parse(strData.c_str());

            if (jsonValue.is_object()) {
                try {
                    on_query_account_position(jsonValue);
                }
                catch (...) {
                    SPDLOG_ERROR("parse account balance error. strData ={}", strData);
                }
            }
        }

        std::string strPositionData = request_http_path("/api/v5/account/positions");
        SPDLOG_DEBUG("sync_account position positions ={}", strPositionData);
        if (!strPositionData.empty())
        {
            nlohmann::json jsonValue = nlohmann::json::parse(strPositionData.c_str());

            if (jsonValue.is_object()) {
                try {
                    on_query_position(jsonValue);
                }
                catch (...) {
                    SPDLOG_ERROR("parse account positions error. strData ={}", strPositionData);
                }
            }
        }
        SPDLOG_DEBUG("sync_account position end");
    }

    void TraderOkx::send_ping() {
        if (websocket_okx_td_ && b_is_connect) {
            websocket_okx_td_->send("ping");
        }
    }

    // 账户频道
    void TraderOkx::ws_account() {
        /*
        {
        "op": "subscribe",
        "args": [{
            "channel": "account",
            "ccy": "BTC"
        }]
        }
        */

        if (b_subscribe_asset) {
            SPDLOG_DEBUG("ws_account 已经订阅");
            return;
        }
        SPDLOG_INFO("ws_account begin");
        nlohmann::json jSendValue;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        jValue["channel"] = "account";
        //jValue["ccy"] = "BTC";
        jValue["extraParams"] = "{\"updateInterval\": \"0\"}";
        jArgs.emplace_back(jValue);

        jSendValue["op"] = "subscribe";
        jSendValue["args"] = jArgs;
        std::string strData = jSendValue.dump();
        SPDLOG_DEBUG("ws_account jSendValue dump = {}", strData);

        if (websocket_okx_td_) {
            websocket_okx_td_->send(strData);
        }
        SPDLOG_INFO("ws_account end");
    }

    // 持仓频道
    void TraderOkx::ws_position() {
        /*
        {
        "op": "subscribe",
        "args": [
            {
                "channel": "positions",
                "instType": "ANY",
                "extraParams": "
                    {
                        \"updateInterval\": \"0\"
                    }
                "
            }
        ]
        }
        */
        if (b_subscribe_position) {
            SPDLOG_DEBUG("ws_position 已经订阅");
            return;
        }
        SPDLOG_INFO("ws_position begin");
        nlohmann::json jSendValue;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        jValue["channel"] = "positions";
        jValue["instType"] = "ANY";
        jValue["extraParams"] = "{\"updateInterval\": \"0\"}";
        jArgs.emplace_back(jValue);

        jSendValue["op"] = "subscribe";
        jSendValue["args"] = jArgs;
        std::string strData = jSendValue.dump();
        SPDLOG_INFO("ws_position jSendValue dump = {}", strData);

        if (websocket_okx_td_) {
            websocket_okx_td_->send(strData);
        }

        SPDLOG_INFO("ws_position end");
    }

    // 订单频道
    void TraderOkx::ws_orders() {
        if (b_subscribe_orders) {
            return;
        }

        SPDLOG_INFO("ws_orders begin");
        nlohmann::json jSendValue;
        nlohmann::json jArgs = nlohmann::json::array();
        nlohmann::json jValue;
        jValue["channel"] = "orders";
        jValue["instType"] = "ANY";
        jArgs.emplace_back(jValue);

        jSendValue["op"] = "subscribe";
        jSendValue["args"] = jArgs;
        std::string strData = jSendValue.dump();
        SPDLOG_INFO("ws_orders jSendValue dump = {}", strData);

        if (websocket_okx_td_) {
            websocket_okx_td_->send(strData);
        }

        SPDLOG_INFO("ws_orders end");
    }

    void TraderOkx::OnResponseTraderData(const std::string& strData) {
        //SPDLOG_DEBUG("OnResponseTraderData return data: {}", strData);
        nlohmann::json jsonValue = nlohmann::json::parse(strData.c_str());

        if (!jsonValue.is_object()) {
            SPDLOG_ERROR("OnResponseTraderData error string: {}", strData);
            return;
        }

        if (jsonValue.contains("event")) {
            SPDLOG_DEBUG("OnResponseTraderData return data: {}", strData);
            std::string strEvent = jsonValue["event"].get<std::string>();
            if (strEvent == "error") {
                SPDLOG_ERROR("OnResponseTraderData error string: {}", strData);
                update_broker_state(BrokerState::LoginFailed);
                return;
            }
            else if (strEvent == "login") {
                on_return_login(jsonValue);
                return;
            }
            else if (strEvent == "subscribe" || strEvent == "unsubscribe") {
                SPDLOG_DEBUG("OnResponseTraderData return data: {}", strData);
                if (strEvent == "subscribe" && jsonValue.contains("arg") && jsonValue["arg"].contains("channel")) {
                    if (jsonValue["arg"]["channel"].get<std::string>() == "account") {
                        b_subscribe_asset = true;
                    }
                    else if (strEvent == "subscribe" && jsonValue["arg"]["channel"].get<std::string>() == "positions") {
                        b_subscribe_position = true;
                    }
                    else if (strEvent == "subscribe" && jsonValue["arg"]["channel"].get<std::string>() == "orders") {
                        b_subscribe_orders = true;
                    }
                }
                return;
            }
        }
        else if (jsonValue.contains("arg") && jsonValue["arg"].contains("channel")) {
            if (jsonValue["arg"]["channel"].get<std::string>() == "account") {
                on_return_account(jsonValue);
            }
            else if (jsonValue["arg"]["channel"].get<std::string>() == "positions") {
                on_return_position(jsonValue);
            }
            else if (jsonValue["arg"]["channel"].get<std::string>() == "orders") {
                SPDLOG_INFO("OnResponseTraderData return data: {}", strData);
                on_return_orders(jsonValue);
            }
            return;
        }
        else if (jsonValue.contains("op")) {
            SPDLOG_INFO("OnResponseTraderData return data: {}", strData);
            if (jsonValue["op"].get<std::string>() == "order") {
                on_return_insert_order(jsonValue);
            }
            else if (jsonValue["op"].get<std::string>() == "cancel-order") {
                on_return_cancel_order(jsonValue);
            }
            else if (jsonValue["op"].get<std::string>() == "subscribe") {
                SPDLOG_DEBUG("OnResponseTraderData op = subscribe");
            }
            return;
        }
    }

    void TraderOkx::on_return_login(const nlohmann::json& jsonValue) {
        std::string code = jsonValue["code"].get<std::string>();
        if (code != "0") {
            SPDLOG_ERROR("on_return_login error jsonValue: {}", jsonValue.dump());
            update_broker_state(BrokerState::LoginFailed);
        }
        else {
            update_broker_state(BrokerState::LoggedIn);
			#if 1
            update_broker_state(BrokerState::Ready);
            sync_account_position();
            SPDLOG_INFO("on_return_login login in successful.");
            // 连接成功之后，开始检测重连〿
            add_time_interval(time_unit::NANOSECONDS_PER_SECOND * 30, [&](auto e) { check_and_reconnect(); });
			#else
			
            SPDLOG_INFO("recover_history_order begin");
            req_history_order();
            req_history_trade();
            SPDLOG_INFO("recover_history_order end");
			#endif
        }
    }

    void TraderOkx::on_return_account(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_return_account begin");
        if (jsonValue.contains("data")) {
            for (auto item : jsonValue["data"].items()) {
                okx_account account_info;
                ObtainJsonValue(item.value(), "adjEq", account_info.adjEq);
                ObtainJsonValue(item.value(), "borrowFroz", account_info.borrowFroz);

                int nIndex = 0;
                int count = item.value()["details"].size();
                for (auto detail : item.value()["details"].items()) {
                    okx_account_detail account_detail;
                    from_okx_account_cash_position(detail.value(), account_detail);

                    // 更新持仓
                    BufferOKXPositionField bf_position;
                    from_cash_position_to_position(account_detail, bf_position);
                    bf_position.is_update = true;
                    auto& bf_okx_pos_field =
                        td_->get_thread_writer()->open_custom_data<BufferOKXPositionField>(kOKXPositionFieldType, td_->now());
                    memcpy(&bf_okx_pos_field, &bf_position, sizeof(BufferOKXPositionField));
                    td_->get_thread_writer()->close_data();

                    double coinUsdPrice = atof(account_detail.coinUsdPrice.c_str());
                    account_info.isoUpl_total += atof(account_detail.isoUpl.c_str()) * coinUsdPrice;
                    account_info.availBal_total += atof(account_detail.availBal.c_str()) * coinUsdPrice;
                    account_info.availEq_total += atof(account_detail.availEq.c_str()) * coinUsdPrice;
                    account_info.isoEq_total += atof(account_detail.isoEq.c_str()) * coinUsdPrice;
                    account_info.frozenBal_total += atof(account_detail.frozenBal.c_str()) * coinUsdPrice;
                    account_info.ordFrozen_total += atof(account_detail.ordFrozen.c_str());
                    account_info.eqUsd_total += atof(account_detail.eqUsd.c_str());

                    account_info.vect_details.emplace_back(account_detail);
                }

                ObtainJsonValue(item.value(), "imr", account_info.imr);
                ObtainJsonValue(item.value(), "isoEq", account_info.isoEq);
                ObtainJsonValue(item.value(), "mgnRatio", account_info.mgnRatio);
                ObtainJsonValue(item.value(), "mmr", account_info.mmr);
                ObtainJsonValue(item.value(), "notionalUsd", account_info.notionalUsd);
                ObtainJsonValue(item.value(), "ordFroz", account_info.ordFroz);
                ObtainJsonValue(item.value(), "totalEq", account_info.totalEq);
                ObtainJsonValue(item.value(), "uTime", account_info.uTime);
                ObtainJsonValue(item.value(), "upl", account_info.upl);

                BufferOKXAssetField bf_asset;
                bf_asset.isoUpl_total = account_info.isoUpl_total;
                bf_asset.availBal_total = account_info.availBal_total;
                bf_asset.availEq_total = account_info.availEq_total;
                bf_asset.eqUsd_total = account_info.eqUsd_total;
                bf_asset.isoEq_total = account_info.isoEq_total;
                bf_asset.frozenBal_total = account_info.frozenBal_total;
                bf_asset.ordFrozen_total = account_info.ordFrozen_total;

                bf_asset.total_asset = atof(account_info.totalEq.c_str());
                bf_asset.upl = atof(account_info.upl.c_str());
                bf_asset.uTime = atoll(account_info.uTime.c_str());

                auto& bf_okx_field =
                    td_->get_thread_writer()->open_custom_data<BufferOKXAssetField>(kOKXAssetFieldType, td_->now());
                memcpy(&bf_okx_field, &bf_asset, sizeof(BufferOKXAssetField));
                td_->get_thread_writer()->close_data();
            }
        }
        else {
            SPDLOG_ERROR("on_return_account error string: {}", jsonValue.dump());
        }

        SPDLOG_DEBUG("on_return_account end");
    }

    void TraderOkx::on_query_account_position(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_query_account position begin");
        if (jsonValue.contains("data")) {
            for (auto item : jsonValue["data"].items()) {
                for (auto detail : item.value()["details"].items()) {
                    okx_account_detail account_detail;
                    from_okx_account_cash_position(detail.value(), account_detail);

                    BufferOKXPositionField bf_position;
                    from_cash_position_to_position(account_detail, bf_position);
                    auto& bf_okx_pos_field =
                        td_->get_thread_writer()->open_custom_data<BufferOKXPositionField>(kOKXPositionFieldType, td_->now());
                    memcpy(&bf_okx_pos_field, &bf_position, sizeof(BufferOKXPositionField));
                    td_->get_thread_writer()->close_data();
                }
            }
        }
        else {
            SPDLOG_ERROR("on_query_account position error string: {}", jsonValue.dump());
        }

        SPDLOG_DEBUG("on_query_account position end");
    }

    // 长连接推送
    void TraderOkx::on_return_position(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_return position begin");
        if (jsonValue.contains("data")) {
            for (auto item : jsonValue["data"].items()) {
                BufferOKXPositionField bf_position;
                from_okx_pos(item.value(), bf_position);

                if (std::string(bf_position.mgnMode) == "isolated" ||
                    bf_position.instrument_type == (int)InstrumentType::Unknown) {
                    SPDLOG_ERROR("on_return position 该类型持仓不支持= {}, instrument_id={}, mgnMode={}", \
                        bf_position.instrument_type, bf_position.instrument_id, bf_position.mgnMode);
                    continue;
                }

                bf_position.is_update = true;

                auto& bf_okx_field =
                    td_->get_thread_writer()->open_custom_data<BufferOKXPositionField>(kOKXPositionFieldType, td_->now());
                memcpy(&bf_okx_field, &bf_position, sizeof(BufferOKXPositionField));
                td_->get_thread_writer()->close_data();
            }
        }

        SPDLOG_DEBUG("on_return position end");
    }

    // 短连接请求
    void TraderOkx::on_query_position(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_query position begin");
        if (jsonValue.contains("data")) {
            std::vector<BufferOKXPositionField> vectPositionBuff;

            for (auto item : jsonValue["data"].items()) {
                BufferOKXPositionField bf_position;
                from_okx_pos(item.value(), bf_position);

                if (std::string(bf_position.mgnMode) == "isolated" ||
                    bf_position.instrument_type == (int)InstrumentType::Unknown) {
                    SPDLOG_ERROR("on_query position 该类型持仓不支持= {}, instrument_id={}, mgnMode={}", \
                        bf_position.instrument_type, bf_position.instrument_id, bf_position.mgnMode);
                    continue;
                }

                vectPositionBuff.emplace_back(bf_position);
            }

            int nSize = vectPositionBuff.size();
            if (nSize > 0) {
                for (size_t i = 0; i < nSize; i++) {
                    if (nSize == i + 1) {
                        vectPositionBuff[i].is_last = true;
                    }

                    auto& bf_okx_field =
                        td_->get_thread_writer()->open_custom_data<BufferOKXPositionField>(kOKXPositionFieldType, td_->now());
                    memcpy(&bf_okx_field, &vectPositionBuff[i], sizeof(BufferOKXPositionField));
                    td_->get_thread_writer()->close_data();
                }
            }
            else {
                // 当平仓完的时候需要清空
                int position_end = 1;
                auto& bf_okx_field =
                    td_->get_thread_writer()->open_custom_data<int>(kOKXPositionEndFieldType, td_->now());
                memcpy(&bf_okx_field, &position_end, sizeof(int));
                td_->get_thread_writer()->close_data();
            }
        }

        SPDLOG_DEBUG("on_query position end");
    }

    void TraderOkx::on_return_orders(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_return orders begin");

        if (jsonValue.contains("data")) {
            for (auto item : jsonValue["data"].items()) {
                BufferOKXOrderField bf_order;
                from_okx_order(item.value(), bf_order);

                auto& bf_okx_field =
                    td_->get_thread_writer()->open_custom_data<BufferOKXOrderField>(kOKXOrderFieldType, td_->now());
                memcpy(&bf_okx_field, &bf_order, sizeof(BufferOKXOrderField));
                td_->get_thread_writer()->close_data();
            }
        }

        SPDLOG_DEBUG("on_return orders end");
    }

    void TraderOkx::on_return_insert_order(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_return_insert order begin");
        std::string code = jsonValue["code"].get<std::string>();
        std::string msg = jsonValue["msg"].get<std::string>();
        std::string id = jsonValue["id"].get<std::string>();
        std::string inTime = jsonValue["inTime"].get<std::string>();
        std::string outTime = jsonValue["outTime"].get<std::string>();

        if (code != "0") {
            SPDLOG_ERROR("on_return_insert order error code: {}, msg: {}", code, msg);
        }

        try
        {
            if (jsonValue.contains("op") && jsonValue.contains("data")) {
                if (jsonValue["data"].size() > 0) {
                    for (auto item : jsonValue["data"].items()) {
                        BufferOKXHandleOrderField handle_order;
                        from_okx_operate_order(item.value(), handle_order);
                        strncpy(handle_order.id, id.c_str(), INSTRUMENT_ID_LEN);
                        handle_order.inTime = atoll(inTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;
                        handle_order.inTime = atoll(outTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;

                        auto& bf_okx_field =
                            td_->get_thread_writer()->open_custom_data<BufferOKXHandleOrderField>(kOKXInsertOrderFieldType, td_->now());
                        memcpy(&bf_okx_field, &handle_order, sizeof(BufferOKXHandleOrderField));
                        td_->get_thread_writer()->close_data();
                    }
                }
                else {
                    BufferOKXHandleOrderField handle_order;
                    strncpy(handle_order.sCode, code.c_str(), INSTRUMENT_ID_LEN);
                    strncpy(handle_order.sMsg, msg.c_str(), ERROR_MSG_LEN);
                    strncpy(handle_order.id, id.c_str(), INSTRUMENT_ID_LEN);
                    handle_order.inTime = atoll(inTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;
                    handle_order.inTime = atoll(outTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;

                    auto& bf_okx_field =
                        td_->get_thread_writer()->open_custom_data<BufferOKXHandleOrderField>(kOKXInsertOrderFieldType, td_->now());
                    memcpy(&bf_okx_field, &handle_order, sizeof(BufferOKXHandleOrderField));
                    td_->get_thread_writer()->close_data();
                }
            }
        }
        catch (...)
        {
            SPDLOG_ERROR("on_return_insert order dump={}", jsonValue.dump());
        }

        SPDLOG_DEBUG("on_return_insert order end");
    }

    void TraderOkx::on_return_cancel_order(const nlohmann::json& jsonValue) {
        SPDLOG_DEBUG("on_return_cancel order begin");
        std::string code = jsonValue["code"].get<std::string>();
        std::string id = jsonValue["id"].get<std::string>();
        std::string inTime = jsonValue["inTime"].get<std::string>();
        std::string outTime = jsonValue["outTime"].get<std::string>();
        std::string msg = jsonValue["msg"].get<std::string>();
        if (code != "0") {
            SPDLOG_ERROR("on_return_cancel order error code: {}, msg: {}", code, msg);
            return;
        }

        try
        {
            if (jsonValue.contains("op")) {
                for (auto item : jsonValue["data"].items()) {
                    BufferOKXHandleOrderField handle_order;
                    from_okx_operate_order(item.value(), handle_order);
                    strncpy(handle_order.id, id.c_str(), INSTRUMENT_ID_LEN);
                    handle_order.inTime = atoll(inTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;
                    handle_order.inTime = atoll(outTime.c_str()) * time_unit::MILLISECONDS_PER_SECOND;
                    strncpy(handle_order.code, code.c_str(), INSTRUMENT_ID_LEN);
                    strncpy(handle_order.msg, msg.c_str(), ERROR_MSG_LEN);

                    auto& bf_okx_field =
                        td_->get_thread_writer()->open_custom_data<BufferOKXHandleOrderField>(kOKXCancelOrderFieldType, td_->now());
                    memcpy(&bf_okx_field, &handle_order, sizeof(BufferOKXHandleOrderField));
                    td_->get_thread_writer()->close_data();
                }
            }
        }
        catch (...)
        {
            SPDLOG_ERROR("on_return_cancel order dump={}", jsonValue.dump());
        }

        SPDLOG_DEBUG("on_return_cancel order end");
    }

    bool TraderOkx::request_public_instruments_info(const std::string& instType)
    {
        SPDLOG_DEBUG("request_public_instruments info begin.");
        int nRet = -1;
        std::string url = SPLICE(OKX_PUBLIC_INSTRUMENTS_URL);
        url += "?instType=";
        url += instType;
        SPDLOG_DEBUG("request_market_tickers info url:{}", url);
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

        SPDLOG_DEBUG("request_public_instruments info success end.");
        return true;
    }
} // namespace kungfu::wingchun::okx
