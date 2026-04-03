const onenet = require('../../utils/onenet.js');

Page({
  data: {
    deviceName: "康复监测臂环",
    deviceStatus: "获取中...",
    lastSyncTime: "-",
    uploadStatus: "等待数据流",
    onenetStatus: "校验中",
    loading: false,
    testValue: "123", // 默认下发测试值
    testLoading: false
  },

  onLoad() {
    this.checkDeviceState();
  },

  checkDeviceState() {
    if(this.data.loading) return;
    this.setData({
      deviceStatus: "获取中...",
      onenetStatus: "鉴权校验中",
      loading: true
    });
    
    onenet.checkDeviceState().then(res => {
      if(res.success) {
        const d = new Date();
        const timeStr = `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`;
        
        this.setData({
          deviceStatus: "在线",
          lastSyncTime: timeStr,
          uploadStatus: "数据流通畅",
          onenetStatus: res.msg,
          loading: false
        });
      } else {
        this.setData({
          deviceStatus: "离线",
          uploadStatus: "数据流中断",
          onenetStatus: res.msg,
          loading: false
        });
      }
    }).catch(err => {
      this.setData({
        deviceStatus: "连接失败",
        uploadStatus: "网络超时",
        onenetStatus: "未连接",
        loading: false
      });
    });
  },

  reconnect() {
    this.checkDeviceState();
  },

  onTestInput(e) {
    this.setData({ testValue: e.detail.value });
  },

  sendTest() {
    if(this.data.testLoading) return;
    
    // 1. 获取输入字符
    let valStr = this.data.testValue || "";
    // 2. 校验是否全由数字组成
    if (!/^\d+$/.test(valStr)) {
      wx.showToast({ title: '必须输入正整数', icon: 'none' });
      return;
    }
    
    const val = parseInt(valStr, 10);
    // 3. 校验范围 0-999
    if (val < 0 || val > 999) {
      wx.showToast({ title: '数值必须在0~999之间', icon: 'none' });
      return;
    }

    this.setData({ testLoading: true });
    onenet.sendTestValue(val).then(res => {
      if(res.success) {
        wx.showToast({ title: '下发成功！', icon: 'success' });
      } else {
        wx.showToast({ title: `失败: ${res.msg}`, icon: 'none', duration: 3000 });
      }
      this.setData({ testLoading: false });
    }).catch(err => {
      wx.showToast({ title: '网络请求超时或失败', icon: 'none' });
      this.setData({ testLoading: false });
    });
  }
});