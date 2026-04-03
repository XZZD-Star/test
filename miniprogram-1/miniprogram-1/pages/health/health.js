const app = getApp();
const aiHelper = require('../../utils/ai-helper.js');

Page({
  data: {
    st: {},
    bmi: 0,
    bmiStatus: "正常",
    suggestionText: "",
    showAiModal: false,
    llmReport: null,
    isAiLoading: false
  },

  onLoad() {
    this.initData();
  },
  
  onShow() {
    this.initData();
  },

  initData() {
    const st = app.globalData.settings;
    if(!st) return;

    // Calc BMI = weight(kg) / (height(m) * height(m))
    let bmiVal = 0;
    let bStatus = "正常";
    if (st.height > 0 && st.weight > 0) {
      let hM = st.height / 100;
      bmiVal = (st.weight / (hM * hM)).toFixed(1);
      if (bmiVal < 18.5) bStatus = "偏轻";
      else if (bmiVal >= 24) bStatus = "偏重";
    }

    let suggest = `目前处于【${st.rehabPhase}】，主要康复部位为【${st.rehabPart}】。\n\n`;
    if (st.rehabPhase === '第一阶段恢复') {
      suggest += `建议以被动活动和轻度等长收缩为主，恢复基本活动度。\n注意控制训练幅度，避免过度拉扯。`;
    } else if (st.rehabPhase === '第二阶段恢复') {
      suggest += `建议增加主动助力训练，逐步恢复全关节活动范围。\n可优先完成前平举与侧平举基础动作。`;
    } else {
      suggest += `建议加强抗阻力训练，全面提升肌肉力量。\n目标达成为：“${st.target}”。\n若训练中出现明显不适或${st.rehabPart}疼痛加剧，请及时暂停训练。`;
    }

    this.setData({
      st: st,
      bmi: bmiVal,
      bmiStatus: bStatus,
      suggestionText: suggest
    });
  },

  openAiModal() {
    this.setData({ showAiModal: true });
  },

  closeAiModal() {
    this.setData({ showAiModal: false });
  },

  catchTouch() {
    // 阻止底层滑动穿透
  },

  requestAiReport() {
    if(this.data.isAiLoading) return;
    
    const d = new Date();
    const todayStr = `${d.getFullYear()}-${d.getMonth()+1}-${d.getDate()}`;
    const lastGen = wx.getStorageSync('last_ai_gen_date');

    if (lastGen === todayStr) {
      wx.showModal({
        title: '频次提示',
        content: '为了节省大模型算力资源，今日您已经生成过一次档案解读。是否确认再次生成并覆盖？',
        confirmText: '继续生成',
        success: (res) => {
          if (res.confirm) {
            this.doRequestAiReport(todayStr);
          }
        }
      });
    } else {
      this.doRequestAiReport(todayStr);
    }
  },

  doRequestAiReport(todayStr) {
    this.setData({ isAiLoading: true, llmReport: null });

    // 组装发给大模型的数据上下文
    let st = this.data.st;
    const userData = {
      "康复部位": st.rehabPart || "肩部",
      "康复阶段": st.rehabPhase || "暂无数据",
      "当前目标": st.target || "暂无数据",
      "生理年龄": st.age || "暂无",
      "身高cm": st.height || "暂无",
      "体重kg": st.weight || "暂无",
      "BMI指数": this.data.bmi || "暂无",
      "当前系统规则阶段建议": this.data.suggestionText || "暂无信息"
    };

    aiHelper.generateHealthReport(userData).then(res => {
      if (res.success) {
        wx.setStorageSync('last_ai_gen_date', todayStr);
        this.setData({ llmReport: res.data, isAiLoading: false });
      } else {
        wx.showToast({ title: res.msg || '获取失败', icon: 'none' });
        this.setData({ isAiLoading: false });
      }
    }).catch(err => {
      wx.showToast({ title: '网络或AI请求异常', icon: 'none' });
      this.setData({ isAiLoading: false });
    });
  }
});