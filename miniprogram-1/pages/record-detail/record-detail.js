Page({
  data: {
    date: '',
    finishedCount: 0,
    percent: 0,
    hr: 0,
    bo: 0,
    duration: "00:00",
    suggestion: ""
  },

  onLoad(options) {
    const d = options.date || "获取失败";
    const fn = parseInt(options.count || "0");
    const pc = parseInt(options.percent || "0");
    const h = parseInt(options.hr || "0");
    const b = parseInt(options.bo || "0");
    const dur = options.duration || "00:00";
    
    let sugg = "本次训练总结良好，保持节奏！";
    if (pc < 80) sugg = "本次训练尚未达标，可能是累了，注意合理休息。";
    if (h > 120) sugg = "记录显示心率偏高，建议注意训练强度。";
    if (b < 95 && b > 0) sugg = "记录显示血氧偏低，请关注呼吸节奏。";
    if (pc >= 100) sugg = "你已完美达成当次目标！请继续保持优秀的出勤率。";

    this.setData({
      date: d,
      finishedCount: fn,
      percent: pc,
      hr: h,
      bo: b,
      duration: dur,
      suggestion: sugg
    });
  }
});
