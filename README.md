# Build
```
yarn install
yarn --registry https://registry.npmmirror.com/
yarn build
```

# Install
```

# Run (dev)

## MD
```
yarn dev md -s okx
```

## TD
```
yarn dev td -s okx -a <account_id>
```
## 说明
```
1、不支持币币杠杆期权的交易、持仓。

2、交割和永续的交易模式：全仓交易模式cross。

3、现货交易模式：cash。现货交易需要指定现货交易币对比如要交易BTC，需要指定BTC-USD。手动交易时，在行情订阅列表先搜索添加；策略交易时，指定Instrument_id即可。

4、账户的现货资产，在kungfu的持仓中显示，且不可以直接交易，需要交易得通过交易币对进行，例如：BTC-USDT或者BTC-USDC。
持仓中展示的现货点击时不会在交易面板联动下单，深度行情也不会有。

5、订阅市场：现货（Exchange.OKX_SPOT）、U本位（Exchange.OKX_USD_FUTURE）、币本位（Exchange.OKX_COIN_FUTURE）。

6、底层不计算持仓浮动盈亏，使用服务器同步返回数据，当交易订单变动时会实时同步一次资金，同时也会定时60s一次同步一次服务器数据，

7、现货交易：现货交易信息显示交易币对，订单成交之后，持仓中持有相应的币会发生变化。比如买入ETH-BTC 1手，在持仓中也是ETH的数量会增加1手，BTC数量减少。
