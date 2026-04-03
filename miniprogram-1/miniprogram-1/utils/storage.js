const SETTINGS_KEY = 'app_settings';
const PLAN_KEY = 'app_plan';

const defaultSettings = {
  nickName: "康复新人",
  avatarUrl: "/static/头像.jpg",
  age: 28,
  gender: "男",
  height: 175,
  weight: 70,
  rehabPart: "左肩膀",
  rehabPhase: "第三阶段恢复",
  target: "全面恢复活动度",
  hrLimit: 120,
  boLimit: 95,
  alertOn: true
};

const defaultPlan = {
  elbow_flex: 15,
  front_raise: 20,
  side_raise: 20,
  shoulder_raise: 15
};

module.exports = {
  getSettings() {
    return wx.getStorageSync(SETTINGS_KEY) || defaultSettings;
  },
  saveSettings(data) {
    wx.setStorageSync(SETTINGS_KEY, data);
  },
  getPlan() {
    return wx.getStorageSync(PLAN_KEY) || defaultPlan;
  },
  savePlan(data) {
    wx.setStorageSync(PLAN_KEY, data);
  }
};
