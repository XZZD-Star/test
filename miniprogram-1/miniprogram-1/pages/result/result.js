Page({
  data: {
    finishedCount: 0,
    targetCount: 0,
    percent: 0,
    hr: 0,
    bo: 0,
    duration: "10:30", // mock
    suggestion: "训练达标，状态良好，继续保持！"
  },

  onLoad(options) {
    const fn = parseInt(options.finished || "0");
    const tar = parseInt(options.target || "60");
    let pc = (fn / tar * 100).toFixed(0);
    if(pc > 100) pc = 100;
    
    const h = parseInt(options.hr || "0");
    const b = parseInt(options.bo || "0");
    
    let sugg = "今日训练已达标，继续保持！";
    if (pc < 80) sugg = "今日训练尚未达标，下次加油！";
    if (h > 120) sugg = "心率偏高，请注意适度休息。";
    if (b < 95 && b > 0) sugg = "血氧偏低，下次训练避免过强强度。";

    this.setData({
      finishedCount: fn,
      targetCount: tar,
      percent: pc,
      hr: h,
      bo: b,
      suggestion: sugg
    });
  },

  goHome() {
    wx.switchTab({
      url: '/pages/home/home'
    });
  },

  goDetail() {
    wx.navigateTo({
      url: '/pages/data/data'
    });
  }
});