# 简介
  

本插件为数字交易货币欧易（okx）的交易插件，可搭载在“功夫量化”软件上使用并进行量化交易。
  

实盘交易、模拟交易均支持。
  

# 安装
  

1. 需要下载好 <a href="https://releases.kungfu-trader.com" title="超链接title">功夫客户端</a> （稳定版最新版即可）以及本链接中的okx插件包
  

2. 运行功夫客户端，进入后点击左上角菜单栏“文件-打开功夫安装目录”
  

3. 复制本链接的okx插件包，即文件夹“okx”，进入{功夫安装目录} --> app -->
kungfu-extensions 。粘贴okx文件夹到本路径
  

4. 粘贴okx文件夹后，请重启客户端
![](https://markdown.liuchengtu.com/work/uploads/upload_54fb5a2d6053cc5855cdde6ba4a52117.png)
  

# 使用
## 创建API
在软件中进行量化交易，必须通过API访问，需要先在欧易（okx） <a
href="https://www.okx.com/zh-hans/okx-api" title="超链接title">申请欧易API</a>
  

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
![](https://markdown.liuchengtu.com/work/uploads/upload_a60cbe2c04d52543b87823bfac94b0e3.png)
    对应okx的web页面的资产tab：
    ![](https://markdown.liuchengtu.com/work/uploads/upload_7362804669c513aad14839396b025be2.png)

  

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
  

## 一. windows环境下编译
### 1. 环境配置
###    (1) . 打包需要安装 :
​     visual studio community 2022 版本 (安装 visual studio 会带有 cmake )    
####    (2) . visual studio community 2022 下载地址
```
https://visualstudio.microsoft.com/zh-hans/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022&source=VSLandingPage&cid=2030&passive=false
```
#### (3) . 编译必须使用visual studio自带的64位shell
​     1) . 点击电脑左下角的开始菜单
![开始菜单](https://raw.githubusercontent.com/Pythonzhai/picture_24/main/img/%E5%BC%80%E5%A7%8B%E8%8F%9C%E5%8D%95.png)
​     2) . 向下查找 visual studio 2022
![vs2022](https://raw.githubusercontent.com/Pythonzhai/picture_24/main/img/vs2022.png)
  

​     3) . 选择 x64 Native Tools Command Prompt VS 2022
![x64native](https://raw.githubusercontent.com/Pythonzhai/picture_24/main/img/x64native.png)
  

## 二. 编译okx插件
### 1.下载kungfu-trader安装包
​     1) . [下载kungfu Windows安装包](https://www.kungfu-trader.com/)
  
​     2) . 安装kungfu-trader，例如：D:\Kungfu-trader
### 2.下载kfx-broker-okx代码
​     从github中把代码下载到本地，例如：D:\kfx-broker-okx
### 3. 编译kfx-broker-okx
​     操作如下：
  

​    1）启动vs2022命令行界面
  

​    2）进入kfx-broker-okx代码目录
  

​    3）输入编译命令行：D:\Kungfu-trader\resources\kfc\kfs.exe extension build
  

​    4）执行编译，成功编译后会在dist目录中生成okx文件夹
![](https://markdown.liuchengtu.com/work/uploads/upload_7ff890019440588c8f677bc78adf8b9d.png)

## 三. 拷贝编译成功的okx插件到kungfu-trader中
​     操作如下：
​     1）从dist目录中把okx文件拷贝到 D:\Kungfu-trader\resources\app\kungfu-extensions 文件夹中
  

![](https://markdown.liuchengtu.com/work/uploads/upload_5ec4eb0950d2619d526e0fef39ccbcd7.png)
  

![](https://markdown.liuchengtu.com/work/uploads/upload_a384a71d282f927c330f24787e8c112a.png)
  

## 四. 在kungfu客户端中添加okx插件
​     1) .添加交易柜台
![](https://markdown.liuchengtu.com/work/uploads/upload_a80de2031a96e62b0545bc1127a05d55.png)
  

​     2) .添加okx行情柜台
![](https://markdown.liuchengtu.com/work/uploads/upload_486a618c14f4c42ca865b188023f5a4a.png)
​     3) .运行柜台
![](https://markdown.liuchengtu.com/work/uploads/upload_f9a1625d6eba1ca68851fcd872071d29.png)
  
## 四. 策略demo用例
​     1）添加策略
![](https://markdown.liuchengtu.com/work/uploads/upload_4257c774b4551e0af81444d189705e4f.png)
![](https://markdown.liuchengtu.com/work/uploads/upload_67dff0f9b26fe6faaa9517b8436a80de.png)

​     2）运行策略
![](https://markdown.liuchengtu.com/work/uploads/upload_10fa0a32982b7297c73b8026a1b83881.png)
​     3）查看策略运行日志
![](https://markdown.liuchengtu.com/work/uploads/upload_f95592e9a4917b3f24ed46c1f2d96b6f.png)

  

```