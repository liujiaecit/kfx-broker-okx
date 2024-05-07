#ifndef KUNGFU_TYPE_CONVERT_MD_H
#define KUNGFU_TYPE_CONVERT_MD_H

//#include "trader_okx.h"
#include "buffer_data.h"
#include "url_define.h"
#include <kungfu/wingchun/timezone.h>
#include <kungfu/wingchun/encoding.h>

namespace kungfu::wingchun::okx {
    using namespace kungfu::longfist;
    using namespace kungfu::longfist::enums;
    using namespace kungfu::longfist::types;
    using namespace kungfu::yijinjing;
    using namespace kungfu::yijinjing::data;

#define TERMINALINFO "Kungfu_3.0"

#define ObtainValue(json, key, var)                                                                                    \
  if (json.contains(key)) {                                                                                            \
    var = json.at(key);                                                                                                \
  }

#define ObtainJsonValue(json, key, var) ObtainValue(json, key, var)

    // 判断字符是否存在
    inline bool isfind(const std::string& str, const std::string& strfind) {
        return (str.find(strfind) != std::string::npos);
    }

    inline longfist::enums::InstrumentType translate_instrument_type(const std::string& instType, const std::string& instId) {
        if (instType == SPOT_INST_TYPE) {
            return longfist::enums::InstrumentType::Crypto;
        }
        else if (instType == SWAP_INST_TYPE || instType == FUTURES_INST_TYPE) {
            if (isfind(instId, USD_FUTURE)) {
                return longfist::enums::InstrumentType::CryptoFuture;
            }
            else {
                return longfist::enums::InstrumentType::CryptoUFuture;
            }
        }
		else {
			SPDLOG_ERROR("translate_instrument_type not support instType ={}, instId={}", instType, instId);
			return longfist::enums::InstrumentType::Unknown;
        }
    }

    inline std::string translate_exchange_id(longfist::enums::InstrumentType instrument_type) {
        if (instrument_type == longfist::enums::InstrumentType::Crypto) {
            return EXCHANGE_OKX_SPOT;
        }
        else if (instrument_type == longfist::enums::InstrumentType::CryptoFuture) {
            return EXCHANGE_OKX_COIN_FUTURE;
        }
        else if (instrument_type == longfist::enums::InstrumentType::CryptoUFuture){
            return EXCHANGE_OKX_USD_FUTURE;
        }

        SPDLOG_ERROR("translate_exchange_id instrument_type ={}", (int)instrument_type);
        return "";
    }

    // 注意：在kungfu内，所有的数字货币都会做统一处理。订阅、下单的时候再还原
    // 现货：BTC-USDT
    // 永续：BTC-USD-SWAP  转成 BTC-USDT.SWAP
    // 期货：BTC-USD-240628 转成 BTC-USD-240628.FUTURE
    inline void from_okx(const nlohmann::json& jsonValue, Instrument& inst) {
        std::string instType = jsonValue["instType"].get<std::string>();
        std::string instId = jsonValue["instId"].get<std::string>();
        /*std::string last = jsonValue["last"].get<std::string>();
        std::string lastSz = jsonValue["lastSz"].get<std::string>();
        std::string askPx = jsonValue["askPx"].get<std::string>();
        std::string bidPx = jsonValue["bidPx"].get<std::string>();
        std::string bidSz = jsonValue["bidSz"].get<std::string>();
        std::string open24h = jsonValue["open24h"].get<std::string>();
        std::string high24h = jsonValue["high24h"].get<std::string>();
        std::string low24h = jsonValue["low24h"].get<std::string>();
        std::string volCcy24h = jsonValue["volCcy24h"].get<std::string>();
        std::string vol24h = jsonValue["vol24h"].get<std::string>();
        std::string ts = jsonValue["ts"].get<std::string>();
        std::string sodUtc0 = jsonValue["sodUtc0"].get<std::string>();
        std::string sodUtc8 = jsonValue["sodUtc8"].get<std::string>();*/

        inst.currency = Currency::USD;
        inst.instrument_type = translate_instrument_type(instType, instId);
        strncpy(inst.exchange_id, translate_exchange_id(inst.instrument_type).c_str(), EXCHANGE_ID_LEN);
        strncpy(inst.instrument_id, instId.c_str(), INSTRUMENT_ID_LEN);

        char instrument_name[PRODUCT_ID_LEN] = { 0 };
        strncpy(instrument_name, gbk2utf8(instId).c_str(), PRODUCT_ID_LEN);
        memcpy(inst.product_id, instrument_name, PRODUCT_ID_LEN); // 产品ID
    }

    // 解析基础信息
    inline void from_okx_public_instruments(const nlohmann::json& jsonValue, InstrumentInfo& info) {
		ObtainJsonValue(jsonValue, "ctMult", info.ctMult);
		ObtainJsonValue(jsonValue, "instId", info.instId);
		ObtainJsonValue(jsonValue, "instType", info.instType);
		ObtainJsonValue(jsonValue, "ctType", info.ctType);
		ObtainJsonValue(jsonValue, "listTime", info.listTime);
		ObtainJsonValue(jsonValue, "lotSz", info.lotSz);
		ObtainJsonValue(jsonValue, "ctVal", info.ctVal);
		ObtainJsonValue(jsonValue, "ctValCcy", info.ctValCcy);
		ObtainJsonValue(jsonValue, "instFamily", info.instFamily);
		ObtainJsonValue(jsonValue, "minSz", info.minSz);
		ObtainJsonValue(jsonValue, "settleCcy", info.settleCcy);
		ObtainJsonValue(jsonValue, "state", info.state);
		ObtainJsonValue(jsonValue, "lever", info.lever);
		ObtainJsonValue(jsonValue, "tickSz", info.tickSz);
		ObtainJsonValue(jsonValue, "uly", info.uly);
    }


    /*
        long：开平仓模式开多  short：开平仓模式开空
        net：买卖模式（交割/永续/期权：pos为正代表开多，pos为负代表开空。币币杠杆：posCcy为交易货币时，代表开多；posCcy为计价货币时，代表开空。）
    */
    inline Direction okx_side_to_kf_direction(const std::string& posSide, double volume) {
        if (posSide == "net") {
            if (volume < 0) {
                return Direction::Short;
            }
            else {
                return Direction::Long;
            }
        }
        else if (posSide == "short") {
            return Direction::Short;
        }

        return Direction::Long;
    }

    inline std::string kf_direction_to_okx_side(const OrderInput& order_input) {
        /* posSide 在买卖模式下，默认 net 在开平仓模式下必填，且仅可选择 long 或 short，仅适用于交割/永续*/
        if (order_input.instrument_type == InstrumentType::CryptoFuture ||
            order_input.instrument_type == InstrumentType::CryptoUFuture) {

        }
        else {

        }
        return "";
	}

	inline Offset okx_to_kf_offset(const std::string& posSide, const std::string& side) {
		if (posSide == "net") {
			if (side == "buy") {
				return Offset::Open;
			}
			else {
				return Offset::Close;
			}
		}
		else if (posSide == "long") {
			return Offset::Open;
		}
		else if (posSide == "short") {
			return Offset::Close;
		}
		else {
			SPDLOG_ERROR("okx_to kf offset error posSide={}, size={}", posSide, side);
		}

		return Offset::Open;
	}

    inline std::string okx_mode_to_kf(const std::string& tdMode) {
        return "";
    }

    // kf 买开->开多：开多 buy long  
    // kf 卖平->平多：平多头 sell long  
    // kf 卖开->开空：开空头 sell short  
    // kf 买平->平空：平空头 buy short 
    inline void kf_to_okx_side_and_pos_side(const OrderInput& order_input, std::string& side, std::string& posSide) {
        if (order_input.instrument_type == InstrumentType::CryptoFuture ||
            order_input.instrument_type == InstrumentType::CryptoUFuture) {
            if (order_input.side == Side::Buy) {
                side = "buy";
                if (order_input.offset == Offset::Open) {
                    posSide = "long";
                }
                else {
                    posSide = "short";
                }
            }
            else if (order_input.side == Side::Sell) {
                side = "sell";
                if (order_input.offset == Offset::Open) {
                    posSide = "short";
                }
                else {
                    posSide = "long";
                }
            }
        }
        else {
            side = order_input.side == Side::Buy ? "buy" : "sell";
            posSide = "";
        }
    }

    // okx 开多buy long ->买开：kf 买开 buy open  
    // okx 平多sell long->卖平：kf 卖平 sell close  
    // okx 开空sell short->卖开：kf 卖开 sell open  
    // okx 平空buy short->买平：kf 买平 buy short 
    inline void okx_to_kf_sid_and_offset(const std::string& posSide, const std::string& okxSide, enums::Side& side, enums::Offset& offset) {
        side = okxSide == "buy" ? Side::Buy : Side::Sell;

        if (posSide == "long") {
            if (okxSide == "buy") {
                offset = Offset::Open;
                return;
            }
            else if (okxSide == "sell") {
                offset = Offset::Close;
                return;
            }
        }
        else if (posSide == "short") {
            if (okxSide == "buy") {
                offset = Offset::Close;
                return;
            }
            else if (okxSide == "sell") {
                offset = Offset::Open;
                return;
            }
        }
        else {
            if (okxSide == "buy") {
                offset = Offset::Open;
                return;
            }
            else if (okxSide == "sell") {
                offset = Offset::Close;
                return;
            }
        }
        
        SPDLOG_ERROR("okx_to kf offset error posSide={}, okxSide={}", posSide, okxSide);
    }

    inline std::string kf_to_okx_mode(const OrderInput& order_input) {
        /*
        交易模式
        保证金模式 isolated：逐仓 cross：全仓
        非保证金模式 cash：现金
        spot_isolated：现货逐仓(仅适用于现货带单) ，现货带单时，tdMode 的值需要指定为spot_isolated
        */
        if (order_input.instrument_type == InstrumentType::Crypto)
        {
            return "cash";
        }

        return "cross";
    }

    inline std::string kf_to_okx_ccy(const std::string& instrument_id) {
        int nPos = instrument_id.find("-");

        if (nPos != std::string::npos)
        {
            return instrument_id.substr(0, nPos);
        }

		SPDLOG_ERROR("Unknown kf_to_okx_ccy instrument id: {}", instrument_id);
        return instrument_id;
    }

    // 订单类型
    inline PriceType okx_order_type_to_kf(const std::string& ordType) {
        if (ordType == "limit") {
            return PriceType::Limit;
        }
        else if (ordType == "market") {
            return PriceType::Any;
        }
        else {
            SPDLOG_ERROR("Unknown okx price_type: {}", ordType);
        }

        return PriceType::Unknown;
    }

    inline std::string kf_order_type_to_okx(const OrderInput& order_input) {
        /*订单类型
        market：市价单
        limit：限价单
        post_only：只做maker单
        fok：全部成交或立即取消
        ioc：立即成交并取消剩余
        optimal_limit_ioc：市价委托立即成交并取消剩余（仅适用交割、永续）
        mmp：做市商保护(仅适用于组合保证金账户模式下的期权订单)
        mmp_and_post_only：做市商保护且只做maker单(仅适用于组合保证金账户模式下的期权订单)
        */
        switch (order_input.price_type) {
        case PriceType::Limit:
            return "limit";
		case PriceType::Any:
			return "market";
        default:
            SPDLOG_ERROR("Unexpected price_type: {}", order_input.price_type);
            return "limit";
        }
    }
    
    /*
	* 订单状态
	canceled：撤单成功
	live：等待成交
	partially_filled：部分成交
	filled：完全成交
	mmp_canceled：做市商保护机制导致的自动撤单
    */
    inline enums::OrderStatus okx_order_state_to_kf(const std::string& state, double volume, double volume_left) {
        if (state == "canceled") {
            if (volume != volume_left) {
                return enums::OrderStatus::PartialFilledNotActive;
            }

            return enums::OrderStatus::Cancelled;
		}
		else if (state == "live") {
			return enums::OrderStatus::Submitted;
		}
		else if (state == "partially_filled") {
			return enums::OrderStatus::PartialFilledActive;
		}
		else if (state == "filled") {
			return enums::OrderStatus::Filled;
		}
		else if (state == "mmp_canceled") {
			return enums::OrderStatus::Cancelled;
		}
		else {
			SPDLOG_ERROR("Unknown okx state: {}", state);
        }

		return enums::OrderStatus::Unknown;
    }

    inline void from_okx(const nlohmann::json& jsonValue, Quote& quote) {
        try {
            std::string instType = jsonValue["instType"].get<std::string>();
            std::string instId = jsonValue["instId"].get<std::string>();
            std::string last = jsonValue["last"].get<std::string>();        // 最新成交价
            std::string lastSz = jsonValue["lastSz"].get<std::string>();       // 最新成交的数量
            std::string askPx = jsonValue["askPx"].get<std::string>();
            std::string askSz = jsonValue["askSz"].get<std::string>();
            std::string bidPx = jsonValue["bidPx"].get<std::string>();
            std::string bidSz = jsonValue["bidSz"].get<std::string>();
            std::string open24h = jsonValue["open24h"].get<std::string>();
            std::string high24h = jsonValue["high24h"].get<std::string>();
            std::string low24h = jsonValue["low24h"].get<std::string>();
            std::string volCcy24h = jsonValue["volCcy24h"].get<std::string>();  // 24小时成交量，以币为单位    如果是衍生品合约，数值为交易货币的数量。    如果是币币/币币杠杆，数值为计价货币的数量。
            std::string vol24h = jsonValue["vol24h"].get<std::string>();    // 24小时成交量，以张为单位
            std::string ts = jsonValue["ts"].get<std::string>();
            std::string sodUtc0 = jsonValue["sodUtc0"].get<std::string>();  // UTC+0 时开盘价
            std::string sodUtc8 = jsonValue["sodUtc8"].get<std::string>();  // UTC+8 时开盘价

            memset(&quote, 0, sizeof(Quote));
            quote.last_price = atof(last.c_str());
            quote.close_price = atof(last.c_str());
            quote.open_price = atof(open24h.c_str());
            quote.high_price = atof(high24h.c_str());
            quote.low_price = atof(low24h.c_str());
            quote.volume = atof(lastSz.c_str());
            quote.turnover = atof(volCcy24h.c_str());
            quote.pre_close_price = atof(sodUtc0.c_str());
            quote.bid_volume[0] = atof(bidSz.c_str());
            quote.ask_volume[0] = atof(askSz.c_str());
            quote.bid_price[0] = atof(bidPx.c_str());
            quote.ask_price[0] = atof(askPx.c_str());

            //SPDLOG_INFO("from_okx update_quote last={}, last_price={}", last, quote.last_price);
        }
        catch (...) {
        }
    }

    inline void from_okx(const nlohmann::json& jsonValue, Transaction& trans) {
        //std::string instId = jsonValue["instId"].get<std::string>();    // 产品ID
        std::string tradeId = jsonValue["tradeId"].get<std::string>();   // 聚合的多笔交易中最新一笔交易的成交ID
        std::string px = jsonValue["px"].get<std::string>();   // 成交价格
        std::string sz = jsonValue["sz"].get<std::string>();   // 成交数量
        std::string side = jsonValue["side"].get<std::string>();   // 成交方向 buy sell
        std::string ts = jsonValue["ts"].get<std::string>();   // 成交时间，Unix时间戳的毫秒数格式，如 1597026383085

        trans.price = atof(px.c_str());
        trans.volume = atof(sz.c_str());
        trans.side = side == "buy" ? Side::Buy : Side::Sell;
        trans.main_seq = atoll(tradeId.c_str());
        trans.data_time = atoll(ts.c_str());
    }

	/*inline void from_okx_account_cash_position(const nlohmann::json& json, okx_account& account_info) {
		ObtainJsonValue(json, "adjEq", account_info.adjEq);
		ObtainJsonValue(json, "borrowFroz", account_info.borrowFroz);

		for (auto detail : json["details"].items()) {
			okx_account_detail account_detail;
			ObtainJsonValue(detail.value(), "availBal", account_detail.availBal);
			ObtainJsonValue(detail.value(), "availEq", account_detail.availEq);
			ObtainJsonValue(detail.value(), "borrowFroz", account_detail.borrowFroz);
			ObtainJsonValue(detail.value(), "cashBal", account_detail.cashBal);
			ObtainJsonValue(detail.value(), "ccy", account_detail.ccy);
			ObtainJsonValue(detail.value(), "coinUsdPrice", account_detail.coinUsdPrice);
			ObtainJsonValue(detail.value(), "crossLiab", account_detail.crossLiab);
			ObtainJsonValue(detail.value(), "disEq", account_detail.disEq);
			ObtainJsonValue(detail.value(), "eq", account_detail.eq);
			ObtainJsonValue(detail.value(), "eqUsd", account_detail.eqUsd);
			ObtainJsonValue(detail.value(), "fixedBal", account_detail.fixedBal);
			ObtainJsonValue(detail.value(), "frozenBal", account_detail.frozenBal);
			ObtainJsonValue(detail.value(), "imr", account_detail.imr);
			ObtainJsonValue(detail.value(), "interest", account_detail.interest);
			ObtainJsonValue(detail.value(), "isoEq", account_detail.isoEq);
			ObtainJsonValue(detail.value(), "isoLiab", account_detail.isoLiab);
			ObtainJsonValue(detail.value(), "isoUpl", account_detail.isoUpl);
			ObtainJsonValue(detail.value(), "liab", account_detail.liab);
			ObtainJsonValue(detail.value(), "maxLoan", account_detail.maxLoan);
			ObtainJsonValue(detail.value(), "mgnRatio", account_detail.mgnRatio);
			ObtainJsonValue(detail.value(), "mmr", account_detail.mmr);
			ObtainJsonValue(detail.value(), "notionalLever", account_detail.notionalLever);
			ObtainJsonValue(detail.value(), "ordFrozen", account_detail.ordFrozen);
			ObtainJsonValue(detail.value(), "rewardBal", account_detail.rewardBal);
			ObtainJsonValue(detail.value(), "spotInUseAmt", account_detail.spotInUseAmt);
			ObtainJsonValue(detail.value(), "spotIsoBal", account_detail.spotIsoBal);
			ObtainJsonValue(detail.value(), "stgyEq", account_detail.stgyEq);
			ObtainJsonValue(detail.value(), "twap", account_detail.twap);
			ObtainJsonValue(detail.value(), "uTime", account_detail.uTime);
			ObtainJsonValue(detail.value(), "upl", account_detail.upl);
			ObtainJsonValue(detail.value(), "uplLiab", account_detail.uplLiab);

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
		ObtainJsonValue(json, "imr", account_info.imr);
		ObtainJsonValue(json, "isoEq", account_info.isoEq);
		ObtainJsonValue(json, "mgnRatio", account_info.mgnRatio);
		ObtainJsonValue(json, "mmr", account_info.mmr);
		ObtainJsonValue(json, "notionalUsd", account_info.notionalUsd);
		ObtainJsonValue(json, "ordFroz", account_info.ordFroz);
		ObtainJsonValue(json, "totalEq", account_info.totalEq);
		ObtainJsonValue(json, "uTime", account_info.uTime);
		ObtainJsonValue(json, "upl", account_info.upl);
	}*/

    //< 通过资金账号获取现货持仓
	inline void from_okx_account_cash_position(const nlohmann::json& json, okx_account_detail& account_detail) {
		ObtainJsonValue(json, "availBal", account_detail.availBal);
		ObtainJsonValue(json, "availEq", account_detail.availEq);
		ObtainJsonValue(json, "borrowFroz", account_detail.borrowFroz);
		ObtainJsonValue(json, "cashBal", account_detail.cashBal);
		ObtainJsonValue(json, "ccy", account_detail.ccy);
		ObtainJsonValue(json, "coinUsdPrice", account_detail.coinUsdPrice);
		ObtainJsonValue(json, "crossLiab", account_detail.crossLiab);
		ObtainJsonValue(json, "disEq", account_detail.disEq);
		ObtainJsonValue(json, "eq", account_detail.eq);
		ObtainJsonValue(json, "eqUsd", account_detail.eqUsd);
		ObtainJsonValue(json, "fixedBal", account_detail.fixedBal);
		ObtainJsonValue(json, "frozenBal", account_detail.frozenBal);
		ObtainJsonValue(json, "imr", account_detail.imr);
		ObtainJsonValue(json, "interest", account_detail.interest);
		ObtainJsonValue(json, "isoEq", account_detail.isoEq);
		ObtainJsonValue(json, "isoLiab", account_detail.isoLiab);
		ObtainJsonValue(json, "isoUpl", account_detail.isoUpl);
		ObtainJsonValue(json, "liab", account_detail.liab);
		ObtainJsonValue(json, "maxLoan", account_detail.maxLoan);
		ObtainJsonValue(json, "mgnRatio", account_detail.mgnRatio);
		ObtainJsonValue(json, "mmr", account_detail.mmr);
		ObtainJsonValue(json, "notionalLever", account_detail.notionalLever);
		ObtainJsonValue(json, "ordFrozen", account_detail.ordFrozen);
		ObtainJsonValue(json, "rewardBal", account_detail.rewardBal);
		ObtainJsonValue(json, "spotInUseAmt", account_detail.spotInUseAmt);
		ObtainJsonValue(json, "spotIsoBal", account_detail.spotIsoBal);
		ObtainJsonValue(json, "stgyEq", account_detail.stgyEq);
		ObtainJsonValue(json, "twap", account_detail.twap);
		ObtainJsonValue(json, "uTime", account_detail.uTime);
		ObtainJsonValue(json, "upl", account_detail.upl);
		ObtainJsonValue(json, "uplLiab", account_detail.uplLiab);
	}

	inline void from_cash_position_to_position(okx_account_detail account_detail, BufferOKXPositionField& pos) {
		/*std::string instrument_id;
		if (account_detail.ccy == "USDT") {
			instrument_id = "USDT-USDC";
		}
		else {
			instrument_id = account_detail.ccy + "-USDT";
		}*/

		strncpy(pos.instrument_id, account_detail.ccy.c_str(), INSTRUMENT_ID_LEN);
		pos.instrument_type = (int)InstrumentType::Crypto;
		strncpy(pos.exchange_id, EXCHANGE_OKX_SPOT, EXCHANGE_ID_LEN);
		strncpy(pos.mgnMode, "cash", INSTRUMENT_ID_LEN);
		strncpy(pos.posSide, "net", INSTRUMENT_ID_LEN);

		pos.volume = atof(account_detail.eq.c_str());
		pos.last_price = atof(account_detail.coinUsdPrice.c_str());
		pos.avg_open_price = atof(account_detail.coinUsdPrice.c_str());
		pos.unrealized_pnl = atof(account_detail.upl.c_str()); // 未实现收益（以标记价格计算）
		pos.realized_pnl = atof(account_detail.isoUpl.c_str()); // 已实现收益
		pos.position_pnl = atof(account_detail.isoUpl.c_str()); // 平仓订单累计收益额
		pos.close_price = atof(account_detail.coinUsdPrice.c_str());
		pos.frozen_total = atof(account_detail.frozenBal.c_str());

		pos.avail_volume = atof(account_detail.availBal.c_str());

    }

    // 下单、撤单回报
	inline void from_okx_operate_order(const nlohmann::json& json, BufferOKXHandleOrderField& order) {
		strncpy(order.ordId, json["ordId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.clOrdId, json["clOrdId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        if (json.contains("tag")) {
            strncpy(order.tag, json["tag"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        }
		strncpy(order.sCode, json["sCode"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.sMsg, json["sMsg"].get<std::string>().c_str(), ERROR_MSG_LEN);
    }

    inline void from_okx_pos(const nlohmann::json& json, BufferOKXPositionField& pos) {
        std::string instType = json["instType"].get<std::string>();
        std::string instId = json["instId"].get<std::string>();
        strncpy(pos.instrument_id, instId.c_str(), INSTRUMENT_ID_LEN);
        longfist::enums::InstrumentType inst_type = translate_instrument_type(instType, instId);
        pos.instrument_type = int(inst_type);
        strncpy(pos.exchange_id, translate_exchange_id(inst_type).c_str(), EXCHANGE_ID_LEN);

        strncpy(pos.mgnMode, json["mgnMode"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        strncpy(pos.posSide, json["posSide"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        pos.volume = atof(json["pos"].get<std::string>().c_str());
        pos.last_price = atof(json["last"].get<std::string>().c_str());
        pos.avg_open_price = atof(json["avgPx"].get<std::string>().c_str());
        pos.unrealized_pnl = atof(json["upl"].get<std::string>().c_str()); // 未实现收益（以标记价格计算）
        pos.realized_pnl = atof(json["realizedPnl"].get<std::string>().c_str()); // 已实现收益
        pos.position_pnl = atof(json["pnl"].get<std::string>().c_str()); // 平仓订单累计收益额
        pos.close_price = atof(json["last"].get<std::string>().c_str());
        
        //可平仓数量，适用于 币币杠杆, 交割 / 永续（开平仓模式），期权
        //对于杠杆仓位，平仓时，杠杆还清负债后，余下的部分会视为币币交易，如果想要减少币币交易的数量，可通过"获取最大可用数量"接口获取只减仓的可用数量。
        pos.avail_volume = atof(json["availPos"].get<std::string>().c_str());
        pos.frozen_total = pos.volume - pos.avail_volume;
        pos.frozen_total = pos.frozen_total > 0 ? pos.frozen_total : 0;

        /*SPDLOG_DEBUG("from_okx_pos instId={}, pos.volume={}, last_price={}, last={}, pos.avg_open_price={}, avgPx={}", \
            instId, pos.volume, pos.last_price, json["last"].get<std::string>(), pos.avg_open_price, \
            json["avgPx"].get<std::string>());*/
    }

	inline void from_okx_order(const nlohmann::json& json, BufferOKXOrderField& order) {
		strncpy(order.instrument_id, json["instId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		std::string instType = json["instType"].get<std::string>();
		longfist::enums::InstrumentType inst_type = translate_instrument_type(instType, order.instrument_id);
        order.instrument_type = int(inst_type);
		strncpy(order.exchange_id, translate_exchange_id(inst_type).c_str(), EXCHANGE_ID_LEN);

		strncpy(order.ordId, json["ordId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.clOrdId, json["clOrdId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.ordType, json["ordType"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        order.px = atof(json["px"].get<std::string>().c_str());
		order.sz = atof(json["sz"].get<std::string>().c_str());
		strncpy(order.side, json["side"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.posSide, json["posSide"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.tdMode, json["tdMode"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
        order.accFillSz = atof(json["accFillSz"].get<std::string>().c_str());
		order.avgPx = atof(json["avgPx"].get<std::string>().c_str());
		order.fillMarkPx = atof(json["fillMarkPx"].get<std::string>().c_str());
		strncpy(order.state, json["state"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.execType, json["execType"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.tradeId, json["tradeId"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		order.fillPx = atof(json["fillPx"].get<std::string>().c_str());
		order.fillSz = atof(json["fillSz"].get<std::string>().c_str());
		order.uTime = atoll(json["uTime"].get<std::string>().c_str()) * time_unit::NANOSECONDS_PER_MILLISECOND;
		order.cTime = atoll(json["cTime"].get<std::string>().c_str()) * time_unit::NANOSECONDS_PER_MILLISECOND;
		order.fillTime = atoll(json["fillTime"].get<std::string>().c_str()) * time_unit::NANOSECONDS_PER_MILLISECOND;
		strncpy(order.code, json["code"].get<std::string>().c_str(), INSTRUMENT_ID_LEN);
		strncpy(order.msg, json["msg"].get<std::string>().c_str(), ERROR_MSG_LEN);
    }

    // 系统外订单创建：
    inline void order_from_okx_create(const BufferOKXOrderField& okx_order, Order& order) {
        strncpy(order.instrument_id, okx_order.instrument_id, INSTRUMENT_ID_LEN);
        order.instrument_type = longfist::enums::InstrumentType(okx_order.instrument_type);
        strncpy(order.exchange_id, okx_order.exchange_id, EXCHANGE_ID_LEN);
        strncpy(order.external_order_id, okx_order.ordId, EXTERNAL_ID_LEN);

        okx_to_kf_sid_and_offset(okx_order.posSide, okx_order.side, order.side, order.offset);
        order.volume = okx_order.sz;
        order.volume_left = okx_order.sz - okx_order.accFillSz;

        order.status = okx_order_state_to_kf(okx_order.state, order.volume, order.volume_left);
        order.price_type = okx_order_type_to_kf(okx_order.ordType);

        //< 市价用成交价格
        if (order.price_type == PriceType::Any) {
            order.limit_price = okx_order.fillPx;
        }
        else {
            //< 现价用返回价格
            if (order.limit_price == 0.0) {
                order.limit_price = okx_order.px;
            }
        }
        /*SPDLOG_DEBUG("order_from_okx_create price_type={}, limit_price={}, fillPx={}, ordType={}", \
            int(order.price_type), order.limit_price, okx_order.fillPx, okx_order.ordType);*/

        order.frozen_price = order.limit_price;
        order.insert_time = okx_order.cTime;
        order.update_time = okx_order.uTime;
        order.error_id = atoi(okx_order.code);
        strncpy(order.error_msg, okx_order.msg, ERROR_MSG_LEN);
    }

    inline void order_from_okx(const BufferOKXOrderField& okx_order, Order& order) {
        order.volume = okx_order.sz;
        order.volume_left = okx_order.sz - okx_order.accFillSz;

        //< 市价用成交价格
        if (order.price_type == PriceType::Any) {
            if (okx_order.fillPx > 0) {
                order.limit_price = okx_order.fillPx;
            }
        }
        else {
            //< 现价用返回价格
            if (order.limit_price == 0.0) {
                if (okx_order.px == 0.0) {
                    order.limit_price = okx_order.fillPx;
                }
                else {
                    order.limit_price = okx_order.px;
                }
            }
        }
        order.frozen_price = order.limit_price;

        /*SPDLOG_DEBUG("order_from_okx price_type={}, limit_price={}, fillPx={}, ordType={}, px={}", \
            int(order.price_type), order.limit_price, okx_order.fillPx, okx_order.ordType, okx_order.px);*/

        order.status = okx_order_state_to_kf(okx_order.state, order.volume, order.volume_left);
		order.update_time = okx_order.uTime;
		order.error_id = atoi(okx_order.code);
		strncpy(order.error_msg, okx_order.msg, ERROR_MSG_LEN);
    }

    // 当是0.02 获取有效位，返回2， 1.e-10 返回10
    inline size_t get_sign_digit(double value) {
        if (value >= 1) {
            return 0;
        }

        std::ostringstream oss;
        oss << value;
        std::string str_value = oss.str();
        transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);

        size_t pos = str_value.find('e');
        if (pos == std::string::npos) {
            std::string str_value = std::to_string(value);
            size_t pos_zero = str_value.find_last_not_of('0');
            if (pos_zero != std::string::npos) {
                if (str_value[pos_zero] == '.') {
                    pos_zero = str_value.find_last_not_of('.');
                }
                str_value.erase(pos_zero + 1);
            }
            else {
                return 0;
            }

            size_t npos = str_value.find('.');
            return str_value.length() - npos - 1;
        }
        else {
            pos = str_value.find('-');
            if (pos != std::string::npos) {
                std::string strSub = str_value.substr(pos + 1, str_value.length() - pos - 1);
                return atoi(strSub.c_str());
            }
        }

        return 0;
    }

	inline double translate_value_by_price_tick(double price, double price_tick = 0.0001) {
        if (price_tick >= 1 || price_tick <= 0) {
            return price;
        }

        int digits = get_sign_digit(price_tick);
        uint64_t tick = pow(10, digits);
        int minus = 1;
        if (price < 0) {
            minus = -1;
        }

        uint64_t uPrice = (uint64_t)((std::abs(price) + price_tick * 0.5) * tick);
        return (double)uPrice / tick * minus;
	}

} // namespace kungfu::wingchun::okx

#endif