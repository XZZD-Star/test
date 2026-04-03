import storage from './utils/storage';

App({
  globalData: {
    settings: null,
    plan: null,
    // OneNET config
    apiBaseUrl: "https://iot-api.heclouds.com",
    productId: "S9U9FY9ZdS",
    deviceName: "Bracelet",
    authInfo: "version=2018-10-31&res=products%2FS9U9FY9ZdS%2Fdevices%2FBracelet&et=1805863774&method=md5&sign=h%2F8qRCOICNVzmUC0cTq5Bg%3D%3D"
  },
  onLaunch() {
    // load global data from storage on launch
    this.globalData.settings = storage.getSettings();
    this.globalData.plan = storage.getPlan();
  },
  updateGlobalSettings(newSettings) {
    this.globalData.settings = newSettings;
    storage.saveSettings(newSettings);
  },
  updateGlobalPlan(newPlan) {
    this.globalData.plan = newPlan;
    storage.savePlan(newPlan);
  }
});
