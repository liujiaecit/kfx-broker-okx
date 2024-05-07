/*****************************************************************************
*  @Copyright (c) 2024, Marsjliu
*  @All rights reserved

*  @file     : marketdata_okx.h

*  @date     : 2024/03/08 10:58
*  @brief    :
*****************************************************************************/

#ifndef KUNGFU_OKX_EXT_MARKET_DATA_H
#define KUNGFU_OKX_EXT_MARKET_DATA_H

#include <set>
#include <vector>

#ifdef _WINDOWS
#undef strncpy
#endif

#include "buffer_data.h"
#include "iwebsocketMd.h"
#include "type_convert_okx.h"
#include <kungfu/wingchun/broker/marketdata.h>

namespace kungfu::wingchun::okx {
class CWebsocketManage;

class MarketDataOkx : public broker::MarketData {
public:
  explicit MarketDataOkx(broker::BrokerVendor &vendor);

  ~MarketDataOkx() override;

public:
  bool subscribe(const std::vector<longfist::types::InstrumentKey> &instrument_keys) override;
  bool unsubscribe(const std::vector<longfist::types::InstrumentKey> &instrument_keys) override;
  bool subscribe_all() override { return true; }

  void websocket_notify(const char *pszData, E_WSCBNotifyType type);

  //void reconnect_server();
  //void check_and_reconnect();
  bool on_custom_event(const event_ptr &event) override;
private:
  //int run();
  void OnResponseMarketData(const std::string &strData);
  bool custom_OnRtnMarketData(const event_ptr &event);
  bool custom_return_market_data(const int &event_type);
  // instType: 产品类型，目前只需要支持 SPOT：币币、SWAP：永续合约、FUTURES：交割合约
  bool request_market_tickers_info(const std::string& instType);
  // 获取所有可交易产品的信息列表
  bool request_public_instruments_info(const std::string& instType);

  // 解析行情
  void parse_tickers_quote(const nlohmann::json& jsonValue);
  // 解析深度行情：5档数据
  void parse_books5(const nlohmann::json& jsonValue);
  // 解析深度行情：全量
  void parse_books_all(const nlohmann::json& jsonValue);
  // 解析深度行情：增量
  void parse_books_update(const nlohmann::json& jsonValue);
  // 解析 交易频道
  void parse_trades(const nlohmann::json& jsonValue);
  // 解析 交易频道,按逐步
  void parse_trades_all(const nlohmann::json& jsonValue);

  // 更新行情, bDepth是否是深度行情数据
  void update_quote(const std::string& instId, bool bDepth = false);

protected:
  void on_start() override;
  void on_exit() override;

private:
  inline static std::shared_ptr<MarketDataOkx> tm_;
  std::set<uint32_t> m_set_subscribe_instrument;                                            // 标的订阅hash表
  std::unordered_map<std::string, longfist::types::InstrumentKey> m_map_scribe_instruments; //< 订阅标的列表
  bool client_exit_ = false;

  std::unordered_map< std::string, InstrumentInfo> map_instrument_infos_;	// 合约基本信息

  std::unordered_map< std::string, std::set<std::string>> map_tickers_set_asks_depth_;
  std::unordered_map< std::string, std::set<std::string>> map_tickers_set_bids_depth_;

  std::unordered_map< std::string, std::unordered_map<std::string, DepthsInfo>> map_tickers_asks_depth_;
  std::unordered_map< std::string, std::unordered_map<std::string, DepthsInfo>> map_tickers_bids_depth_;

  std::unordered_map< std::string, StruDepthsInfo> map_depth_;

  //std::unordered_map<std::string, nlohmann::json> map_ticker_quote_json_;
  std::unordered_map<std::string, std::string> map_ticker_quote_json_;
  std::shared_ptr<CWebsocketManage> m_websocket_manage_;

};
} // namespace kungfu::wingchun::okx

#endif // KUNGFU_OKX_EXT_MARKET_DATA_H