// 创建 WebSocket 连接
var ws = new WebSocket("ws://localhost:8080/");

// 连接成功时触发
ws.onopen = function() {
  console.log("WebSocket 连接已建立");
  ws.send("hello"); // 发送初始消息
  
  // 可以在这里设置定时发送消息
  // setInterval(() => ws.send("ping"), 1000);
};

// 接收到服务器消息时触发
ws.onmessage = function(e) {
  console.log("收到消息:", e.data);
  
  // 根据消息内容执行不同操作
  if (e.data === "close") {
    ws.close(); // 如果服务器发送"close"，则关闭连接
  }
};

// 连接关闭时触发
ws.onclose = function(e) {
  console.log("WebSocket 连接已关闭", e);
  
  // 可以在这里实现重连逻辑
  // setTimeout(() => {
  //   console.log("尝试重新连接...");
  //   ws = new WebSocket("ws://localhost:8080/");
  // }, 5000);
};

// 发生错误时触发
ws.onerror = function(error) {
  console.error("WebSocket 错误:", error);
  
  // 错误处理逻辑
  if (error.message.includes("ECONNREFUSED")) {
    console.error("无法连接到服务器，请确保服务器正在运行");
  }
};

// 发送消息的函数（可以在其他地方调用）
function sendMessage(msg) {
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(msg);
  } else {
    console.warn("WebSocket 未连接，无法发送消息");
  }
}

// 关闭连接的函数
function closeConnection() {
  if (ws.readyState === WebSocket.OPEN) {
    ws.close(1000, "用户主动关闭"); // 1000是正常关闭状态码
  }
}