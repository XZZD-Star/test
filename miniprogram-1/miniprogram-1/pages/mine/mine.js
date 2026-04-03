const app = getApp();

Page({
  data: {
    userInfo: {
      nickName: "设置中...",
      avatarUrl: "/static/头像.jpg"
    },
    target: "获取中...",
    continueDays: 14 // 模拟数据
  },

  onShow() {
    this.initData();
  },

  initData() {
    const st = app.globalData.settings;
    if(st) {
      this.setData({
        'userInfo.nickName': st.nickName || '未命名',
        'userInfo.avatarUrl': st.avatarUrl || '/static/头像.jpg',
        target: st.target || '未设置目标'
      });
    }
  },

  goToPage(e) {
    const url = e.currentTarget.dataset.url;
    if(url) {
      wx.navigateTo({ url });
    } else {
      wx.showToast({
        title: '敬请期待',
        icon: 'none'
      });
    }
  }
});