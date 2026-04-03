const app = getApp();
const onenet = require('../../utils/onenet.js');

Page({
  data: {
    status: "ready", // ready, running, paused
    planName: "今日康复计划",
    deviceStatus: "获取中...",
    
    planList: [],
    currentPlanIndex: 0,
    currentPlan: {},
    totalTarget: 0,
    totalFinished: 0,
    
    baselines: null,
    lastRawObj: null,

    targetCount: 0, 
    finishedCount: 0,
    remainCount: 0,
    progressPercent: 0,
    
    recognizeAction: "等待动作",
    confidence: "0%",
    promptText: "请保持正确的姿势开始",
    promptType: "normal",
    
    heartRate: 0,
    bloodOxygen: 0,
    healthStatus: "正常",
    animClass: ""
  },

  onShow() {
    this.syncInitData();
    if(this.data.status === "running") {
      this.startAutoSync();
    } else if (this.data.status === "ready") {
       this.fetchData(); // pull once to have valid baseline
    }
  },

  syncInitData() {
    const p = app.globalData.plan || { elbow_flex: 0, front_raise: 0, side_raise: 0, shoulder_raise: 0 };
    let list = [];
    const names = { elbow_flex: '曲腕', front_raise: '前平举', side_raise: '侧平举', shoulder_raise: '前屈上举' };
    let tt = 0;
    
    for (let k of ['elbow_flex', 'front_raise', 'side_raise', 'shoulder_raise']) {
      if (p[k] > 0) {
        list.push({ key: k, name: names[k], target: p[k], current: 0 });
        tt += p[k];
      }
    }
    
    if (list.length === 0) {
      list.push({ key: 'free', name: '自由探索', target: 0, current: 0 });
    }

    let p0 = list[0];
    this.setData({
      planList: list,
      currentPlanIndex: 0,
      currentPlan: p0,
      totalTarget: tt,
      totalFinished: 0,
      baselines: null,
      lastRawObj: null,
      
      targetCount: p0.target,
      finishedCount: p0.current,
      remainCount: p0.target,
      progressPercent: 0
    });
  },

  onHide() { this.stopAutoSync(); },
  onUnload() { this.stopAutoSync(); },

  startAutoSync() {
    if(!this.syncTimer) {
      this.fetchData();
      // 训练页面需要高频抓取，以保证动效和安全监测无延迟（1.5秒~2秒频次）
      this.syncTimer = setInterval(() => { this.fetchData(); }, 1500);
    }
  },
  stopAutoSync() {
    if(this.syncTimer) { clearInterval(this.syncTimer); this.syncTimer = null; }
  },

  fetchData() {
    onenet.getDeviceData().then(res => {
      if(res.success) {
        let obj = res.data;
        let st = app.globalData.settings;
        let hrLim = st ? st.hrLimit : 120;
        let boLim = st ? st.boLimit : 95;
        let hStatus = "正常";
        
        if(st && st.alertOn) {
          if (obj.hr > hrLim || obj.hr < 50) hStatus = "异常";
          if (obj.bo > 0 && obj.bo < boLim) hStatus = "警告";
        }
        
        // 第一次抓取时，记录全部基线，防止历史累积次干扰当次训练
        if (!this.data.baselines) {
           this.setData({ 
             baselines: { ...obj }, 
             lastRawObj: { ...obj },
             deviceStatus: "在线",
             heartRate: obj.hr,
             bloodOxygen: obj.bo,
             healthStatus: hStatus
           });
           return;
        }

        let curIdx = this.data.currentPlanIndex;
        let planList = this.data.planList;
        let p = planList[curIdx];
        let lastObj = this.data.lastRawObj;

        // 利用增量差值判断当前这一秒半内设备执行了什么动作
        let diffs = {
           elbow_flex: Math.max(obj.elbow_flex - lastObj.elbow_flex, 0),
           front_raise: Math.max(obj.front_raise - lastObj.front_raise, 0),
           side_raise: Math.max(obj.side_raise - lastObj.side_raise, 0),
           shoulder_raise: Math.max(obj.shoulder_raise - lastObj.shoulder_raise, 0),
        };
        // 推进下一次比较用的基线
        this.setData({ lastRawObj: { ...obj } });

        let maxDiffKey = null; let maxDiff = 0;
        for(let k in diffs) { 
           if(diffs[k] > maxDiff) { maxDiff = diffs[k]; maxDiffKey = k; } 
        }
        
        let pText = this.data.promptText;
        let pType = this.data.promptType;
        let anim = "";
        let rAction = "未识别/静止";
        
        const names = { elbow_flex: '曲腕', front_raise: '前平举', side_raise: '侧平举', shoulder_raise: '前屈上举' };
        if (maxDiffKey && names[maxDiffKey]) rAction = names[maxDiffKey];

        // 核心顺序训练逻辑
        let isFinishTriggered = false;
        if (this.data.status === 'running' && p.key !== 'free') {
           if (maxDiffKey === p.key && maxDiff > 0) {
              // 动作有效且匹配当前计划
              p.current += maxDiff;
              if (p.current > p.target) p.current = p.target; // 防超限
              
              this.data.totalFinished += maxDiff;
              if (this.data.totalFinished > this.data.totalTarget) this.data.totalFinished = this.data.totalTarget;

              anim = "pop-anim";
              pText = "动作标准！继续完成" + p.name;
              pType = "success";
              setTimeout(() => { this.setData({ animClass: "" }); }, 500);

              // 检查本动作是否已做完
              if (p.current >= p.target) {
                 curIdx++;
                 if (curIdx < planList.length) {
                    p = planList[curIdx];
                    wx.showToast({ title: '换动作：' + p.name, icon: 'none', duration: 2000 });
                    pText = "进入下一套动作：" + p.name;
                 } else {
                    // 全部计划闯关完毕！
                    isFinishTriggered = true;
                 }
              }
           } else if (maxDiffKey && maxDiffKey !== p.key) {
              // 检测到多余的大动作但不属于当前计划（拦截）
              pText = "动作不对！当前应做：" + p.name;
              pType = "warning";
           } else {
              // 无增量，平稳阶段
              pText = "匀速发力，按照节奏继续...";
              pType = "normal";
           }
        }
        
        let totalPercent = this.data.totalTarget > 0 ? (this.data.totalFinished / this.data.totalTarget)*100 : 0;
        let curRemain = Math.max(p.target - p.current, 0);

        this.setData({
          planList: planList,
          currentPlanIndex: curIdx,
          currentPlan: p,
          totalFinished: this.data.totalFinished,
          progressPercent: Math.floor(totalPercent),
          finishedCount: p.current,
          targetCount: p.target,
          remainCount: curRemain,

          deviceStatus: "在线",
          heartRate: obj.hr,
          bloodOxygen: obj.bo,
          healthStatus: hStatus,
          animClass: anim || this.data.animClass,
          promptText: pText,
          promptType: pType,
          recognizeAction: rAction,
          confidence: maxDiff > 0 ? "98%" : (maxDiffKey ? "60%" : "0%")
        });
        
        if(hStatus === "异常" && this.data.status === 'running') {
          this.handlePause();
          wx.showModal({ title: '健康警告', content: '心率异常，请立即停止训练并休息！', showCancel: false });
        }
        
        // 如果上面判断闯关完成触发了 flag，则真正开始结束逻辑
        if(isFinishTriggered) {
          this.handleFinish(true);
        }
      } else {
        this.setData({ deviceStatus: "离线" });
      }
    }).catch(err => {
      this.setData({ deviceStatus: "连接失败" });
    });
  },

  handleStart() { this.setData({ status: "running", promptText: "训练正式开始！先做：" + this.data.currentPlan.name, promptType: "normal" }); this.startAutoSync(); },
  handlePause() { this.setData({ status: "paused", promptText: "训练已暂停", promptType: "warning" }); this.stopAutoSync(); },
  handleResume() { this.setData({ status: "running", promptText: "继续训练", promptType: "normal" }); this.startAutoSync(); },

  handleFinish(autoTrigger = false) {
    this.stopAutoSync();
    if(autoTrigger === true) {
        this.setData({ status: "ended" });
        wx.redirectTo({
          url: `/pages/result/result?finished=${this.data.totalFinished}&target=${this.data.totalTarget}&hr=${this.data.heartRate}&bo=${this.data.bloodOxygen}`
        });
    } else {
        wx.showModal({
          title: '确认结束',
          content: '确定要提前结束本次训练计划吗？',
          success: (res) => {
            if(res.confirm) {
              this.setData({ status: "ended" });
              wx.redirectTo({
                url: `/pages/result/result?finished=${this.data.totalFinished}&target=${this.data.totalTarget}&hr=${this.data.heartRate}&bo=${this.data.bloodOxygen}`
              });
            } else {
              if (this.data.status !== 'paused') this.startAutoSync();
            }
          }
        });
    }
  }
});