const app = getApp();

Page({
  data: {
    todaySummary: {
      duration: "10:30",
      count: 60,
      hr: 90,
      bo: 98,
      percent: 100
    },
    actionStats: {
      cepingju: 125,
      qubi: 45,
      qianqu: 60
    },
    historyList: [
      { date: "今天", duration: "10:30", percent: 100, status: "normal" },
      { date: "昨天", duration: "12:15", percent: 100, status: "normal" },
      { date: "03-26", duration: "05:10", percent: 40, status: "warning" },
      { date: "03-25", duration: "11:20", percent: 100, status: "normal" }
    ]
  },

  onLoad() {
    // 模拟数据加载
  },

  goDetail(e) {
    const item = e.currentTarget.dataset.item;
    const count = item.count || 0;
    const hr = item.hr || 0;
    const bo = item.bo || 0;
    // 页面跳转，带参
    wx.navigateTo({
      url: `/pages/record-detail/record-detail?date=${item.date}&duration=${item.duration}&percent=${item.percent}&count=${count}&hr=${hr}&bo=${bo}`
    });
  }
});