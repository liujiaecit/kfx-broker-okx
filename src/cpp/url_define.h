#ifndef _URL_DEFINE_H_
#define _URL_DEFINE_H_

#define BASE_MD_URL_PUBLIC "wss://ws.okx.com:8443/ws/v5/public"
#define BASE_MD_URL_BUSINESS "wss://ws.okx.com:8443/ws/v5/business"

#define BASE_URL_PRIVATE "wss://ws.okx.com:8443/ws/v5/private"
#define BASE_TD_URL_PRIVATE "wss://wspap.okx.com:8443/ws/v5/private?brokerId=9999"

#define HOST "www.okx.com"
#define SPLICE(x) "https://" HOST x
#define OKX_MARKET_TICKERS_URL			"/api/v5/market/tickers"
#define OKX_PUBLIC_INSTRUMENTS_URL			"/api/v5/public/instruments"	// 获取交易产品基础信息
#define OKX_TRADE_ORDERS_HISTORY_URL	"/api/v5/trade/orders-history"		// 获取历史订单记录
#define SPOT_INST_TYPE	"SPOT"			// 币币
#define SWAP_INST_TYPE	"SWAP"			// 永续合约
#define FUTURES_INST_TYPE	"FUTURES"	// 交割合约
#define MARGIN_INST_TYPE	"MARGIN"	// 币币杠杆
#define OPTION_INST_TYPE	"OPTION"	// 期权
#define USDT_FUTURE	"-USDT-"		// U本位
#define USD_FUTURE	"-USD-"			// 币本位


#endif // _URL_DEFINE_H_