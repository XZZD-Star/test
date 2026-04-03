# 康复辅助助手 (Smart-Bracelet) 项目架构与配置完全指南

这不仅是一份对于当前成果状态的技术文档，更是为您（即使是前端小白）编写的一份“查字典说明书”。一旦你想改点什么，打开这份文档，就能精准定位对应的文件。

---

## 一、当前项目真实目录结构地图

目前您的项目已经做了非常清晰的代码解耦和抽象，告别了初期散落乱造的现象。以下是目前的文件指引结构图：

```text
根目录 (d:\嵌入式Start\微信小程序\miniprogram-1\miniprogram-1)
│
├── app.js               # 🧠 [核心逻辑] 整个小程序的心脏，存储您的 OneNET 全局鉴权 Token 
├── app.json             # ⚙️ [全局配置] 指定这小程序有多少个页面、标题栏颜色、底部有哪些 Tab
├── app.wxss             # 🎨 [全局样式] 为整个框架铺上兜底的灰色背景，并设定基础字号
│
├── styles/              # 📦 【公共 UI 组件库】(你的项目很简洁是因为我抽离了它)
│   ├── theme.wxss       # 颜色、参数设计令牌。要改主题色调，只需改这一个文件！
│   └── common.wxss      # 排版工具包。比如包含 .card, .container, .flex 等高复用结构配置
│
├── utils/               # 🛠️ 【通信与工具库】(底层逻辑剥离)
│   ├── storage.js       # 本地缓存管家：所有用户的初始信息、设置的阈值、历史数据，靠它存取
│   └── onenet.js        # 网络通信基站：所有读取硬件信息、心率的函数，都被高度封装成了 API 统一在这里
│
└── pages/               # 📱 【页面代码层】(核心业务，分为底座区和二级功能区)
    │
    ├── [三大底座 Tab 页面：常驻底部不销毁]
    ├── home/            # 首页展示（今日训练看板呈现，当前动作进度预览）
    ├── train/           # 训练核心页（识别反馈、血氧心率、开始训练/下发状态的控制器）
    ├── mine/            # 个人中心页（显示账号信息，负责向下分发功能跳转）
    │
    ├── [二级功能扩展区：由前三个主页面点入]
    ├── health/          # 健康档案（汇总用户的康复期次、智能健康建议评估）
    ├── settings/        # 参数配置（提供修改身体数据、设置预警安全下限的入口）
    ├── plan/            # 今日计划（设置侧平举、上举等四个独立动作的目标次数）
    ├── device/          # 设备监测（反映与 OneNET 云端及通信模块连接状态）
    ├── data/            # 数据记录（按日期归档的历史完成打卡记录）
    ├── result/          # 检测报告（单次训练完后的多维成绩统计大汇总）
    ├── about/           # 关于信息（版本号，技术支持）
    └── feedback/        # 用户反馈（功能留言表单收集）
```

---

## 二、当前项目的小程序核心配置情况

### 1. [app.json](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.json) 全局网关与路由
这是小程序的命脉，目前它进行了如下严格设置：
- **页面路由注册**：11 个合法页面，所有 `wx.navigateTo` 必须指向这里的名单；
- **Tab 栏配对**：硬编码指向了三大主页面，同时配置了高亮的 `#2f54eb` 主题蓝文字及其对应的 `static/` 中配对的点击图标；
- **页面底部防御涂装**：开启了 `backgroundColor: "#f2f4f8"` 兜底窗口防穿透保护。
- **UI 引擎**：开启了 `style: "v2"` 的先进触控引擎接口模式。

### 2. 多重嵌套背景防闪穿配置
这是之前我们重点解决视觉异常遗留的产物，原理非常精妙：
- **最浅底层(Native)**：所有 11 个孤立的 [.json](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.json) 以及 [app.json](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.json) 全都被附加上了 `"backgroundColor": "#f2f4f8"`。
- **画布层(App.wxss)**：根标签 `page` 被我改成了强制的 Flex 排版并加锁 `min-height: 100vh` 锁定满屏边界。
- **渲染层(common.wxss)**：二级页面的根入口 `<view class="container">` 增加 `flex: 1;` 使得它具备随风拉长的撑包能力，彻底防止旧画面泄露。
- **交互层(common.wxss)**：新增拦截滑入动画 `@keyframes pageContentShow` 及 `.page-enter`，新页面的内容只在该转场稳定落地（300ms 后）才浮现，杜绝底部按钮乱窜！

---

## 三、如果是前端小白，应该如何读懂和修改它？

如果您将来需要接手或是自己做点小修改，只要秉持小程序的**“四文件切片理论”**即可。假设你要修该 `train` 页面的内容，它的目录下有这四兄弟：

### 1️⃣ 如果想修改文字和界面结构？看 [.wxml](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/data/data.wxml)
WXML 是界面骨骼，找到对应的页面后：
- 你会看到这里没有 `<div>`，只有 `<view>`（盒子）和 `<text>`（纯文字）。
- 如果你想让一个盒子呈现白底阴影效果，你根本不需要重写 CSS，只需让 `<view class="card">` 挂上公共的卡片类，立马变得专业。
- 如果你要控制某项逻辑条件是否渲染，找类似于 `wx:if="{{status === 'ready'}}"` 这种条件声明。

### 2️⃣ 如果想修该某一处局部的颜色或尺寸？看 [.wxss](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.wxss) 和公用库
- **局部修改**：如果要调整某个特有标签的字体高度，修改当前目录下的 [.wxss](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.wxss) 文件。
- **最优雅的修改（全局统一）**：如果您老板要求：我们要把这个**医疗蓝**产品改成**康复绿**。
  - 不要傻傻地去挨个页面改色！请直奔 [styles/theme.wxss](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/styles/theme.wxss)！
  - 找到并修改 `--primary-color: #2f54eb;` 为 `#1890ff`（或其他蓝色）。您的 11 个页面的按钮、高亮、圆环颜色将全部在 1 秒钟之内自动切换！这是现代前端开发的核心思想（CSS变量）。

### 3️⃣ 如果想修改按钮逻辑或后端通信？看 [.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.js)
- 里面包含小程序的生命周期：[onLoad()](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/settings/settings.js#10-13)（页面启动就去获取云端数据）和 [onShow()](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/mine/mine.js#13-16)（从别的界面弹回依然刷新数据）。
- UI 显示的数据并非生来就有，而是存储在 `data: { ... }` 之中。
- 如果你想改下发给单片机的数据格式（OneNET对接），打开 [utils/onenet.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/utils/onenet.js) 这个公共车间。如果你要改心跳上限判断，直接去 [health.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/health/health.js) 或 [settings.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/settings/settings.js) 中寻找。

### 总结建议：
由于该套代码经过了高纯度的瘦身与解耦：
**如果你想改外观排版，通常不需要修改 [.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/app.js)。**
**如果你想增删表单记录数据项，请务必连同 [wxml](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/pages/data/data.wxml) 数据节点 和 [storage.js](file:///d:/%E5%B5%8C%E5%85%A5%E5%BC%8FStart/%E5%BE%AE%E4%BF%A1%E5%B0%8F%E7%A8%8B%E5%BA%8F/miniprogram-1/miniprogram-1/utils/storage.js) 的存储库一起添加。** 这是保证信息正常显示且重启后不消失的关键！
