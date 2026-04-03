Page({
  data: {
    content: ''
  },
  onInput(e) {
    this.setData({
      content: e.detail.value
    });
  },
  submitFeedback() {
    if(!this.data.content.trim()) {
      wx.showToast({
        title: '请输入反馈内容',
        icon: 'none'
      });
      return;
    }
    wx.showToast({
      title: '感谢您的反馈',
      icon: 'success'
    });
    setTimeout(() => {
      wx.navigateBack();
    }, 1500);
  }
});