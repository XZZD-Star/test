const AI_CONFIG = {
  url: 'https://api.edgefn.net/v1/chat/completions',
  apiKey: 'sk-QN0S1IoGDkbnKEB527F334858c9440D494C5A506A36d5233',
  model: 'MiniMax-M2.5'
};

const SYSTEM_PROMPT = `你是一个“康复辅助解读助手”。
你的职责是基于用户的健康档案特征和康复训练记录，给出辅助性的分析总结、训练建议以及温和的鼓励。

【绝对禁止边界】：
1. 绝对不能使用“我诊断为”、“确诊”、“开药”、“治疗”等字眼。
2. 绝对不能代替专业医生的临床诊断，你只做已有数据层面的辅助解读。
3. 语气保持客观、专业但充满温度，要有服务感。

用户的个人档案及训练数据将会以JSON格式发送给你。如果遇到“暂无数据”或为“0”的空字段，你可以优雅地略过或提醒尚未记录全备，不需要死板纠结空位。

【必须严格返回JSON格式】
请你直接输出纯 JSON 字符串，不要输出任何 Markdown 代码块（不需要加 \`\`\`json 头），必须包含且仅包含以下5个 Key：
{
  "summary": "当前状态总结（1-2句短语）",
  "progress": "恢复进展判断（1-2句话讲述阶段情况）",
  "risks": ["风险提醒1", "风险提醒2"],
  "suggestions": ["日常建议1", "建议2", "建议3"],
  "encouragement": "一句极其温和且有力量的短句鼓励"
}`;

/**
 * 请求大模型生成健康解读报告
 * @param {Object} userData 组装好的用户生理与训练记录数据
 */
function generateHealthReport(userData) {
  return new Promise((resolve, reject) => {
    const userMessage = JSON.stringify(userData);
    
    wx.request({
      url: AI_CONFIG.url,
      method: 'POST',
      header: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${AI_CONFIG.apiKey}`
      },
      data: {
        model: AI_CONFIG.model,
        messages: [
          { role: 'system', content: SYSTEM_PROMPT },
          { role: 'user', content: userMessage }
        ],
        temperature: 0.6
      },
      success: (res) => {
        if (res.statusCode === 200 && res.data && res.data.choices && res.data.choices.length > 0) {
          try {
            let content = res.data.choices[0].message.content;
            // 鲁棒性处理：部分大模型在返回时可能会附带 <thinking> 思考过程标签或其他对话文本
            // 我们通过提取第一个 '{' 到最后一个 '}' 的内容来强行剥离纯 JSON
            const startIndex = content.indexOf('{');
            const endIndex = content.lastIndexOf('}');
            
            if (startIndex !== -1 && endIndex !== -1 && endIndex >= startIndex) {
              content = content.substring(startIndex, endIndex + 1);
            }
            
            const result = JSON.parse(content);
            resolve({ success: true, data: result });
          } catch(e) {
            console.error("AI JSON Parse Error:", e);
            resolve({ success: false, msg: '获取格式解析失败，请重新生成' });
          }
        } else {
          resolve({ success: false, msg: 'AI平台连接或返回异常' });
        }
      },
      fail: (err) => {
        reject(err);
      }
    });
  });
}

module.exports = {
  generateHealthReport
};
