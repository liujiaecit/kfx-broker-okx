import kungfu.yijinjing.time as kft
from kungfu.wingchun.constants import *
import os, signal, time
import json
import time
from pykungfu import wingchun as wc

source = "okx"
account = "test1"
tickers = ["BTC-USDT", "LTC-USDT"]  # 现货
tickers_COINF = ["BTC-USD-240628"]  # 币本位-USD-
tickers_UF = ["LTC-USDT-SWAP"]      # U本位-USDT-

# 订阅标的所属市场： 
# 现货：Exchange.OKX_SPOT
# U本位市场：Exchange.OKX_USD_FUTURE，何种合约属于U本位市场，symbol中含有-USDT-的标的，例如：BTC-USD-240628
# 币本位市场：Exchange.OKX_COIN_FUTURE，何种合约属于币本位市场，symbol中含有-USD-的标的，例如：LTC-USDT-SWAP

EXCHANGE_SPOT = Exchange.OKX_SPOT
EXCHANGE_UF = Exchange.OKX_USD_FUTURE
EXCHANGE_CoinF = Exchange.OKX_COIN_FUTURE

counter = 0

def print_position(context):
    book = context.get_account_book(source, account)
    position = book.get_long_position(
                    source,
                    account,
                    EXCHANGE_UF,
                    "LTC-USDT-SWAP",
                )
    context.log.debug(
        f"context volume_total LTC-USDT-SWAP : {position.volume}"
    )

# 启动前回调，添加交易账户，订阅行情，策略初始化计算等
def pre_start(context):
    context.log.info("pre_start {}".format(context.now()))

    # 添加交易账户：source 交易账户柜台名，account 交易账户名
    context.add_account(source, account)
    # 添加行情柜台：参数1： 行情柜台名， 参数2： 订阅标的列表， 参数3：标的所属市场
    context.subscribe(source, tickers, EXCHANGE_SPOT)
    context.subscribe(source, tickers_UF, EXCHANGE_UF)
    context.subscribe(source, tickers_COINF, EXCHANGE_CoinF)
    context.hold_book()
    context.hold_positions()

# 启动准备工作完成后回调，策略只能在本函数回调以后才能进行获取持仓和报单
def post_start(context):
    context.log.info("post_start {}".format(context.now()))
    print_position(context)

# 收到快照行情时回调，行情信息通过quote对象获取
def on_quote(context, quote, location, dest):
    if quote.instrument_id in tickers_UF:
        # 计数器加1
        global counter
        counter += 1

        if counter == 1:
            order_id = context.insert_order(quote.instrument_id, EXCHANGE_UF, source, account, quote.last_price, 1,
                                        PriceType.Any, Side.Buy, Offset.Open)
            context.log.info("ticker insert order instrument_id: {},order_id: {}".format(quote.instrument_id, order_id))

def on_order(context, order, source_location, dest):
    context.log.info(
        f"on_order order_id: {order.order_id}, status: {int(order.status)}, "
        f"volume: {order.volume}, volume_left: {order.volume_left}"
    )
    print_position(context)

def on_trade(context, trade, source_location, dest):
    context.log.debug(f"on_trade order_id: {trade.order_id}, trade_id: {trade.trade_id}")
    print_position(context)