#ifndef OKX_BUFFER_DATA_H
#define OKX_BUFFER_DATA_H

#include <kungfu/longfist/enums.h>
#include <kungfu/longfist/longfist.h>
#include <nlohmann/json.hpp>
#include <string>
#include <set>
#include <vector>
#include <map>
#include <unordered_map>

namespace kungfu::wingchun::okx {
using namespace kungfu::longfist::types;
using namespace kungfu::longfist::enums;

const int DATA_SIZE = 4096;
static constexpr int32_t kOKXOrderFieldType = 30150001;
static constexpr int32_t kOKXInsertOrderFieldType = 30150002;
static constexpr int32_t kOKXCancelOrderFieldType = 30150003;
static constexpr int32_t kOKXAssetFieldType = 30150004;
static constexpr int32_t kOKXPositionFieldType = 30150005;
static constexpr int32_t kOKXPositionEndFieldType = 30150006;
static constexpr int32_t kOKXMarketFieldType = 30170001;    // 行情
static constexpr int32_t kOKXTraderFieldType = 30170002;    // 交易

//< 交易websocket返回内容
struct BufferTraderField {
    char data[DATA_SIZE] = {0};
    int type = 0;
};
// 深度数据
struct DepthsInfo
{
    std::string price;
    std::string volume;
    std::string reserve;
    std::string order_volume;
};

struct StruDepthsInfo
{
    double bid_price[10] = {0.0};
    double ask_price[10] = { 0.0 };
    double bid_volume[10] = { 0.0 };
    double ask_volume[10] = { 0.0 };
};

// 行情数据
struct quote_item {
    char instType[EXCHANGE_ID_LEN] = { 0 };
    char instId[INSTRUMENT_ID_LEN] = { 0 };
    char last[EXTERNAL_ID_LEN] = { 0 };
    char lastSz[EXTERNAL_ID_LEN] = { 0 };
    char askPx[EXTERNAL_ID_LEN] = { 0 };
    char askSz[EXTERNAL_ID_LEN] = { 0 };
    char bidPx[EXTERNAL_ID_LEN] = { 0 };
    char bidSz[EXTERNAL_ID_LEN] = { 0 };
    char open24h[EXTERNAL_ID_LEN] = { 0 };
    char high24h[EXTERNAL_ID_LEN] = { 0 };
    char low24h[EXTERNAL_ID_LEN] = { 0 };
    char volCcy24h[EXTERNAL_ID_LEN] = { 0 };
    char vol24h[EXTERNAL_ID_LEN] = { 0 };
    char ts[EXTERNAL_ID_LEN] = { 0 };
    char sodUtc0[EXTERNAL_ID_LEN] = { 0 };
    char sodUtc8[EXTERNAL_ID_LEN] = { 0 };
};

// 基础信息
struct InstrumentInfo
{
	std::string ctMult; // 合约乘数，仅适用于交割/永续/期权
	std::string instId; // 
	std::string instType; // 
	std::string ctType; // 合约类型 linear：正向合约	inverse：反向合约		仅适用于交割 / 永续
	std::string listTime; // 
	std::string lotSz; // 下单数量精度。单位为Spread数量单位szCcy
	std::string ctVal; // 合约面值	仅支持FUTURES / SWAP
	std::string ctValCcy; // 
	std::string instFamily; // 
    std::string minSz; // 最小下单数量。单位为Spread数量单位szCcy。
    std::string settleCcy; // 盈亏结算和保证金币种
	std::string state; // 产品状态 	live：交易中 		suspend：暂停中		preopen：预上线 test：测试中（测试产品，不可交易）
    std::string lever; // 该instId支持的最大杠杆倍数，不适用于币币、期权
	std::string tickSz; // 下单价格精度，如 0.0001 	对于期权来说，是梯度中的最小下单价格精度，如果想要获取期权价格梯度，请使用"获取期权价格梯度"接口
    std::string uly; // 标的指数
}; 

// account 详情
struct okx_account_detail {
    std::string availBal;
    std::string availEq;
    std::string borrowFroz;
    std::string cashBal;
    std::string ccy;
    std::string coinUsdPrice;
    std::string crossLiab;
    std::string disEq;
    std::string eq;
    std::string eqUsd;
    std::string fixedBal;
    std::string frozenBal;
    std::string imr;
    std::string interest;
    std::string isoEq;
    std::string isoLiab;
    std::string isoUpl;
    std::string liab;
    std::string maxLoan;
    std::string mgnRatio;
    std::string mmr;
    std::string notionalLever;
    std::string ordFrozen;
    std::string rewardBal;
    std::string spotInUseAmt;
    std::string spotIsoBal;
    std::string stgyEq;
    std::string twap;
    std::string uTime;
    std::string upl;
    std::string uplLiab;
};

struct okx_account {
    std::string adjEq;
    std::string borrowFroz;
    std::vector<okx_account_detail> vect_details;
    std::string imr;
    std::string isoEq;
    std::string mgnRatio;
    std::string mmr;
    std::string notionalUsd;
    std::string ordFroz;
    std::string totalEq;
    std::string uTime;
    std::string upl;

    double isoUpl_total = 0.0;  // 逐仓未实现盈亏：浮动盈亏
    double availBal_total = 0.0;// 可用余额：可以资金
    double availEq_total = 0.0;// 可用保证金：保证金余额
    double eqUsd_total = 0.0;   // 币种权益美金价值:总资金
    double isoEq_total = 0.0;   // 美金层面逐仓仓位权益 占用
    double frozenBal_total = 0.0;   // 币种占用金额：相当于市值，占用保证金
    double ordFrozen_total = 0.0;   // 挂单冻结数量
};


// 资金查询
struct BufferOKXAssetField {
	double isoUpl_total = 0.0;  // 逐仓未实现盈亏：浮动盈亏
	double availBal_total = 0.0;// 可用余额：可以资金
	double availEq_total = 0.0;// 可用保证金：保证金余额
	double isoEq_total = 0.0;   // 美金层面逐仓仓位权益 占用
	double frozenBal_total = 0.0;   // 币种占用金额：相当于市值，占用保证金
	double ordFrozen_total = 0.0;   // 挂单冻结数量
    double eqUsd_total = 0.0;   // 币种总权益美金价值:总资金
    double total_asset = 0.0;
    double upl = 0.0;
    uint64_t uTime = 0;
};

// 操作订单：下单、撤单
struct BufferOKXHandleOrderField {
	char id[INSTRUMENT_ID_LEN] = { 0 };
    uint64_t inTime = 0;
	uint64_t outTime = 0;
	char ordId[INSTRUMENT_ID_LEN] = { 0 };
	char clOrdId[INSTRUMENT_ID_LEN] = { 0 };
	char tag[INSTRUMENT_ID_LEN] = { 0 };
	char sCode[INSTRUMENT_ID_LEN] = { 0 };
	char sMsg[ERROR_MSG_LEN] = { 0 };
	char code[INSTRUMENT_ID_LEN] = { 0 };
	char msg[ERROR_MSG_LEN] = { 0 };
};

// 订单推送
struct BufferOKXOrderField {
    char instrument_id[INSTRUMENT_ID_LEN] = { 0 };
	int instrument_type = 0;
	char exchange_id[EXCHANGE_ID_LEN] = { 0 };
	char ordId[INSTRUMENT_ID_LEN] = { 0 };
	char clOrdId[INSTRUMENT_ID_LEN] = { 0 };
	char ordType[INSTRUMENT_ID_LEN] = { 0 };
    double px = 0.0;    // 代表按价格下单，单位为币 (请求参数 px 的数值单位是BTC或ETH)
	double sz = 0.0;// 原始委托数量，币币/币币杠杆，以币为单位；交割/永续/期权 ，以张为单位
	char side[INSTRUMENT_ID_LEN] = { 0 };   // 订单方向，buy sell
	char posSide[INSTRUMENT_ID_LEN] = { 0 }; //posSide	持仓方向 long：开平仓模式开多 short：开平仓模式开空 net：买卖模式
	char tdMode[INSTRUMENT_ID_LEN] = { 0 }; //交易模式	保证金模式 isolated：逐仓 cross：全仓、非保证金模式 cash：现金
	double accFillSz = 0.0;// 累计成交数量
	double avgPx = 0.0;// 成交均价，如果成交数量为0，该字段也为0
	double fillMarkPx = 0.0;// 成交时的标记价格，仅适用于 交割/永续/期权
	char state[INSTRUMENT_ID_LEN] = { 0 };// 订单状态 canceled：撤单成功 live：等待成交 partially_filled：部分成交 filled：完全成交
	char execType[INSTRUMENT_ID_LEN] = { 0 };// 最新一笔成交的流动性方向 T：taker M：maker
	char tradeId[INSTRUMENT_ID_LEN] = { 0 };// 最新成交ID
	double fillPx = 0.0; // 最新成交价格
	double fillSz = 0.0;// 最新成交数量
	uint64_t uTime = 0;
	uint64_t cTime = 0;
	uint64_t fillTime = 0;// 最新成交时间
	char code[INSTRUMENT_ID_LEN] = { 0 };
	char msg[ERROR_MSG_LEN] = { 0 };
};

// 持仓推送
struct BufferOKXPositionField {
    char instrument_id[INSTRUMENT_ID_LEN] = { 0 };
    int instrument_type = 0;
    char exchange_id[EXCHANGE_ID_LEN] = { 0 };
    char mgnMode[INSTRUMENT_ID_LEN] = {};
    char posSide[INSTRUMENT_ID_LEN] = {};
    double volume = 0.0;
    double avail_volume = 0.0;
    double last_price = 0.0;
    double avg_open_price = 0.0;
    double position_pnl = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double close_pnl = 0.0;
    double close_price = 0.0;
    double pre_close_price = 0.0;
    double frozen_total = 0.0;

    bool is_update = false;   // 是否更新,只更新的时候就写到public
    bool is_last = false;
};

inline void to_json(nlohmann::json& j, const BufferOKXPositionField& ori) {
    j["instrument_id"] = std::string(ori.instrument_id);
    j["instrument_type"] = ori.instrument_type;
    j["exchange_id"] = std::string(ori.exchange_id);
    j["mgnMode"] = ori.mgnMode;
    j["posSide"] = ori.posSide;
    j["volume"] = ori.volume;
    j["avail_volume"] = ori.avail_volume;
    j["close_price"] = ori.close_price;
    j["pre_close_price"] = ori.pre_close_price;
    j["position_pnl"] = ori.position_pnl;
    j["realized_pnl"] = ori.realized_pnl;
    j["unrealized_pnl"] = ori.unrealized_pnl;
    j["last_price"] = ori.last_price;
    j["avg_open_price"] = ori.avg_open_price;
    j["close_pnl"] = ori.close_pnl;
}

template <typename T> std::string to_string(const T &ori) {
  nlohmann::json j;
  to_json(j, ori);
  return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
}
} // namespace kungfu::wingchun::okx

#endif // OKX_BUFFER_DATA_H
