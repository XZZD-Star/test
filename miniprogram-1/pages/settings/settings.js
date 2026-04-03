const app = getApp();

Page({
  data: {
    isEditMode: false,
    settings: {}, // copy of app settings for editing
    phases: ["第一阶段恢复", "第二阶段恢复", "第三阶段恢复", "能力强化阶段"]
  },

  onLoad() {
    this.initData();
  },

  initData() {
    // load latest from globalData. It should be initialized via app.js
    let st = app.globalData.settings;
    if(!st) {
      const storage = require('../../utils/storage.js');
      st = storage.getSettings();
    }
    this.setData({ settings: Object.assign({}, st) });
  },

  toggleEdit() {
    this.setData({ isEditMode: !this.data.isEditMode });
    if (!this.data.isEditMode) {
      // Revert changes if cancel editing without save
      this.initData();
    }
  },

  inputChange(e) {
    const field = e.currentTarget.dataset.field;
    this.setData({
      [`settings.${field}`]: e.detail.value
    });
  },

  phaseChange(e) {
    const index = e.detail.value;
    this.setData({
      'settings.rehabPhase': this.data.phases[index]
    });
  },

  saveSettings() {
    app.updateGlobalSettings(this.data.settings);
    this.setData({ isEditMode: false });
    wx.showToast({
      title: '保存成功',
      icon: 'success'
    });
  }
});