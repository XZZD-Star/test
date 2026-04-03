const app = getApp();
const onenet = require('../../utils/onenet.js');

Page({
  data: {
    userName: "训练达人",
    currentDate: "",
    trainStatus: "未开始",
    
    planName: "今日康复计划",
    totalTarget: 70,
    finishedCount: 0,
    remainCount: 70,
    progressPercent: 0,
    
    deviceStatus: "获取中...",
    lastSyncTime: "-",
    
    heartRate: 0,
    bloodOxygen: 0,
    healthStatus: "正常",
    
    loading: true
  },

  onLoad() {
    this.initDate();
  },
  
  onShow() {
    this.syncInitData();
    this.startAutoSync();
  },
  
  syncInitData() {
    let name = "康复达人";
    if (app.globalData.settings) {
      name = app.globalData.settings.nickName;
    }
    
    let tt = 0;
    if (app.globalData.plan) {
      const p = app.globalData.plan;
      tt = p.elbow_flex + p.front_raise + p.side_raise + p.shoulder_raise;
    }

    this.setData({
      userName: name,
      totalTarget: tt,
      remainCount: Math.max(tt - this.data.finishedCount, 0)
    });
    this.updateProgress(this.data.finishedCount);
  },

  onHide() { this.stopAutoSync(); },
  onUnload() { this.stopAutoSync(); },

  initDate() {
    const d = new Date();
    const dateStr = `${d.getFullYear()}-${(d.getMonth()+1).toString().padStart(2, '0')}-${d.getDate().toString().padStart(2, '0')}`;
    this.setData({ currentDate: dateStr });
  },

  startAutoSync() {
    if(!this.syncTimer) {
      this.fetchData();
      // 首页轮询比较缓和，5秒同步一次概览即可
      this.syncTimer = setInterval(() => { this.fetchData(); }, 5000);
    }
  },
  stopAutoSync() {
    if(this.syncTimer) { clearInterval(this.syncTimer); this.syncTimer = null; }
  },

  updateProgress(totalFinished) {
    let remain = Math.max(this.data.totalTarget - totalFinished, 0);
    let percent = this.data.totalTarget > 0 ? (totalFinished / this.data.totalTarget) * 100 : 0;
    if (percent > 100) percent = 100;

    let tStatus = totalFinished > 0 ? "进行中" : "未开始";
    if (totalFinished >= this.data.totalTarget && this.data.totalTarget > 0) tStatus = "已完成";

    this.setData({
      finishedCount: totalFinished,
      remainCount: remain,
      progressPercent: percent,
      trainStatus: tStatus
    });
  },

  fetchData() {
    onenet.getDeviceData().then(res => {
      if(res.success) {
        let obj = res.data;
        let st = app.globalData.settings;
        let hrLim = st ? st.hrLimit : 120;
        let boLim = st ? st.boLimit : 95;
        let alertOn = st ? st.alertOn : true;
        
        let hStatus = "正常";
        if (alertOn) {
          if (obj.hr > hrLim || obj.hr < 50) hStatus = "异常";
          if (obj.bo > 0 && obj.bo < boLim) hStatus = "警告";
        }
        
        let totalFinished = obj.elbow_flex + obj.front_raise + obj.side_raise + obj.shoulder_raise;
        this.updateProgress(totalFinished);

        const d = new Date();
        const timeStr = `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;

        this.setData({
          deviceStatus: "在线",
          lastSyncTime: timeStr,
          heartRate: obj.hr,
          bloodOxygen: obj.bo,
          healthStatus: hStatus,
          loading: false
        });
      } else {
        this.setData({ deviceStatus: "离线", loading: false });
      }
    }).catch(err => {
      this.setData({ deviceStatus: "连接失败", loading: false });
    });
  },

  goToTrain() { wx.switchTab({ url: '/pages/train/train' }); },
  goToPage(e) { wx.navigateTo({ url: e.currentTarget.dataset.url }); }
});