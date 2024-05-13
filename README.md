# 简介

本插件为数字交易货币欧易（okx）的交易插件，可搭载在“功夫量化”软件上使用并进行量化交易。

实盘交易、模拟交易均支持。

# 安装

1. 需要下载好 <a href="https://releases.kungfu-trader.com" title="超链接title">功夫客户端</a> （稳定版最新版即可）以及本链接中的okx插件包

2. 运行功夫客户端，进入后点击左上角菜单栏“文件-打开功夫安装目录”

3. 复制本链接的okx插件包，即文件夹“okx”，进入{功夫安装目录} --> app --> kungfu-extensions 。粘贴okx文件夹到本路径

4. 粘贴okx文件夹后，请重启客户端
![](https://markdown.liuchengtu.com/work/uploads/upload_54fb5a2d6053cc5855cdde6ba4a52117.png)


# 使用

## 创建API

在软件中进行量化交易，必须通过API访问，需要先在欧易（okx） <a href="https://www.okx.com/zh-hans/okx-api" title="超链接title">申请欧易API</a> 

1. 点击“创建API”

2. 选择要交易的账户，用途选择“API交易”，权限请添加“交易”

3. 自定义密码（passphrase），用于查看已申请的API、登录验证。请务必牢记。

4. 点击左侧按钮“提交全部”，申请完成

5. 申请完毕后，需要点击“查看”，获取已申请的API及密钥

## 登录

交易前需要登录“行情源”及“交易账户”

1. 打开功夫客户端

2. 登录行情源：找到右上角“交易账户”面板，点击“添加”，选择“okx-数字货币”。
 
3. 登录交易账户：找到左上角“交易账户”面板，点击“添加”，选择“okx-数字货币”
    - 账户：备注，随意填写即可。
    - 密码：填写申请API时的密码（passphrase）
    - api_key ：填写“查看”获得的“API”
    - 密钥：填写“查看”获得的“密钥”
    - 是否同步外部订单：若需要，请打开。（打开之后，在其他软件委托的订单都能同步显示）
    - 模拟交易：若需要进行模拟交易、测试，请打开

4. 点击“行情源”、“交易账户” 进程的“打开”，状态变为“就绪”时，即可进行交易。

登录行情及账号可参考功夫登录教程（https://docs.kungfu-trader.com/latest/04-guide.html#id2）

# 功能支持

1. 不支持币币杠杆+期权的交易、持仓查询。只支持现货、永续、合约。

2. 交割和永续的交易模式：只支持全仓交易模式cross，不支持倍数交易。

3. 现货交易模式：只支持cash。不支持倍数交易。现货交易需要指定现货交易币对.

    如:交易BTC，需要指定BTC-USD。

    - 手动交易时，在行情订阅面板先搜索代码，添加到订阅列表才能进行交易。如搜索“BCH-BTC"

    - 策略交易时，也需要先订阅行情，如context.subscribe(source, ["BCH-BTC"])

4. 现货资产可通过“持仓”面板查看。

5. 通过策略订阅行情时，柜台Exchange如何填写？
    - 现货（Exchange.OKX_SPOT）
    
    - U本位（Exchange.OKX_USD_FUTURE）
    
    - 币本位（Exchange.OKX_COIN_FUTURE）。

6. 账户浮动盈亏及资金均使用服务器返回的数据，目前同步机制为2种
    - 定时同步：60s同步一次
    - 订单变动同步：交易订单变动时实时同步一次资金、持仓数据


7. 现货交易：现货交易以交易币对形式展示，订单成交后，持仓会同步变动。
    如：买入ETH-BTC 1手，持仓中ETH+1，BTC-对应数量。


# Build
```
yarn install
yarn --registry https://registry.npmmirror.com/
yarn build

编译成功之后，会在./dist 目录下生成okx文件夹，把okx文件夹拷贝到 kungfu安装目录下 ./resources/app/kungfu-extensions 文件夹中。

yarn dev

使用 yarn dev 命令可以在本地调试启动。
```