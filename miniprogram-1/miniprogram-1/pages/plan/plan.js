const app = getApp();
const onenet = require('../../utils/onenet.js');

Page({
  data: {
    planConfig: {
      elbow_flex: 0,
      front_raise: 0,
      side_raise: 0,
      shoulder_raise: 0
    },
    actionMap: {
      elbow_flex: "曲腕",
      front_raise: "前平举",
      side_raise: "侧平举",
      shoulder_raise: "前屈上举"
    }
  },

  onLoad() {
    this.setData({
      planConfig: Object.assign({}, app.globalData.plan)
    });
  },

  changeCount(e) {
    const key = e.currentTarget.dataset.key;
    const delta = parseInt(e.currentTarget.dataset.delta);
    let newVal = this.data.planConfig[key] + delta;
    if (newVal < 0) newVal = 0;
    
    this.setData({
      [`planConfig.${key}`]: newVal
    });
  },

  inputCount(e) {
    const key = e.currentTarget.dataset.key;
    let val = parseInt(e.detail.value) || 0;
    if (val < 0) val = 0;
    
    this.setData({
      [`planConfig.${key}`]: val
    });
  },

  restoreDefault() {
    wx.showModal({
      title: '提示',
      content: '确定要恢复默认计划吗？',
      success: (res) => {
        if (res.confirm) {
          const storage = require('../../utils/storage.js');
          const dp = {
            elbow_flex: 15,
            front_raise: 20,
            side_raise: 20,
            shoulder_raise: 15
          };
          this.setData({ planConfig: dp });
        }
      }
    });
  },

  savePlan() {
    app.updateGlobalPlan(this.data.planConfig);
    
    wx.showLoading({ title: '正在同步设备' });
    
    // 如果硬件支持接收设定值，则将目标发送至云端传递给硬件
    onenet.sendPlanToDevice(this.data.planConfig).then(res => {
      wx.hideLoading();
      wx.showToast({ title: '保存并下发成功', icon: 'success' });
      setTimeout(() => { wx.navigateBack(); }, 1500);
    }).catch(err => {
      wx.hideLoading();
      // 哪怕云端下发失败，本地保存也成功了，给出轻提醒
      wx.showToast({ title: '已保存（设备同步超时）', icon: 'none' });
      setTimeout(() => { wx.navigateBack(); }, 1500);
    });
  }
});