const app = getApp();

function _getConfig() {
  const { apiBaseUrl, productId, deviceName, authInfo } = getApp().globalData;
  return { apiBaseUrl, productId, deviceName, authInfo };
}

/**
 * 获取设备实时数据 (从 OneNET 云端通过 GET 请求拉取属性)
 */
function getDeviceData() {
  const cfg = _getConfig();
  return new Promise((resolve, reject) => {
    wx.request({
      url: `${cfg.apiBaseUrl}/thingmodel/query-device-property?product_id=${cfg.productId}&device_name=${cfg.deviceName}`,
      method: "GET",
      header: { 'Authorization': cfg.authInfo },
      timeout: 3000,
      success: (res) => {
        if (res.data && res.data.code === 0) {
          const dataArr = res.data.data || [];
          let formattedData = {
            hr: 0, bo: 0, elbow_flex: 0, front_raise: 0, side_raise: 0, shoulder_raise: 0
          };
          dataArr.forEach(item => {
            if(item.identifier === "HeartRate") formattedData.hr = Number(item.value) || 0;
            if(item.identifier === "BloodOxygen") formattedData.bo = Number(item.value) || 0;
            if(item.identifier === "elbow_flex_count") formattedData.elbow_flex = Number(item.value) || 0;
            if(item.identifier === "front_raise_count") formattedData.front_raise = Number(item.value) || 0;
            if(item.identifier === "side_raise_count") formattedData.side_raise = Number(item.value) || 0;
            if(item.identifier === "shoulder_raise_count") formattedData.shoulder_raise = Number(item.value) || 0;
          });
          resolve({ success: true, data: formattedData, raw: res.data });
        } else {
          resolve({ success: false, error: "设备数据获取失败", raw: res.data });
        }
      },
      fail: (err) => {
        reject(err);
      }
    });
  });
}

/**
 * 仅检测设备状态，不处理复杂业务数据映射
 */
function checkDeviceState() {
  const cfg = _getConfig();
  return new Promise((resolve, reject) => {
    wx.request({
      url: `${cfg.apiBaseUrl}/thingmodel/query-device-property?product_id=${cfg.productId}&device_name=${cfg.deviceName}`,
      method: "GET",
      header: { 'Authorization': cfg.authInfo },
      timeout: 3000,
      success: (res) => {
        if (res.data && res.data.code === 0) {
          resolve({ success: true, msg: "已连接" });
        } else {
          resolve({ success: false, msg: `平台报错(${res.data ? res.data.code : '异常'})` });
        }
      },
      fail: (err) => {
        reject(err);
      }
    });
  });
}

/**
 * [新功能预留] 使用 POST 下发用户的每日计划参数给硬件设备
 */
function sendPlanToDevice(planData) {
  const cfg = _getConfig();
  
  // 根据设备的物模型标识符拼接上报数据，改为键值对 Object 格式
  const payload = {};
  Object.keys(planData).forEach(key => {
    payload['target_' + key] = planData[key];
  });

  return new Promise((resolve, reject) => {
    wx.request({
      url: `${cfg.apiBaseUrl}/thingmodel/set-device-property`,
      method: "POST",
      header: { 'Authorization': cfg.authInfo },
      data: {
        product_id: cfg.productId,
        device_name: cfg.deviceName,
        params: payload
      },
      success: (res) => {
        if (res.data && res.data.code === 0) {
          resolve({ success: true, msg: "下发成功" });
        } else {
          resolve({ success: false, msg: "下发失败" });
        }
      },
      fail: (err) => reject(err)
    });
  });
}

/**
 * 下发 test 属性 (仅限测试使用)
 */
function sendTestValue(val) {
  const cfg = _getConfig();
  return new Promise((resolve, reject) => {
    wx.request({
      url: `${cfg.apiBaseUrl}/thingmodel/set-device-property`,
      method: "POST",
      header: { 'Authorization': cfg.authInfo },
      data: {
        product_id: cfg.productId,
        device_name: cfg.deviceName,
        params: {
          "test": val
        }
      },
      success: (res) => {
        // OneNET 返回 code: 0 代表真正的业务成功
        if (res.data && res.data.code === 0) {
          resolve({ success: true, msg: "下发成功" });
        } else {
          resolve({ success: false, msg: res.data ? res.data.msg : "云平台返回错误" });
        }
      },
      fail: (err) => reject(err)
    });
  });
}

module.exports = {
  getDeviceData,
  checkDeviceState,
  sendPlanToDevice,
  sendTestValue
};
