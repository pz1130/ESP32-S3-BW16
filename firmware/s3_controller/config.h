// ============ S3 控制中心配置 ============
// 1) 把家里的 WiFi 账号密码填到下面两个宏
// 2) 引脚默认按 ESP32-S3-DevKitC-1 设计，其他板子改这里即可

// --- 家里 WiFi（S3 作为 STA 接入，Web 服务跑在这张网上） ---
#define WIFI_SSID      "wifi7"
#define WIFI_PASSWORD  "18616952433"
#define WIFI_HOSTNAME  "bw16-s3"

// --- BW16 UART 链路（两根线 + 共地） ---
// S3 TX(GPIO17) -> BW16 RX(PA26/D8)
// S3 RX(GPIO18) <- BW16 TX(PA25/D7)
// GND <-> GND
#define BW16_UART_RX   18
#define BW16_UART_TX   17
#define BW16_BAUD      115200

// --- OLED（I2C 四线：3V3 / GND / SDA / SCL） ---
#define OLED_SDA       8
#define OLED_SCL       9
#define OLED_ADDR      0x3C
// 你的屏是 1.3寸 SH1106 就把下面改成 1；0.96寸 SSD1306 保持 0
#define OLED_USE_SH1106 0

// --- 4 按键（独立 GPIO 模块，每键一信号脚，另一端共 GND） ---
#define BTN_UP         10
#define BTN_DOWN       11
#define BTN_OK         12
#define BTN_BACK       13

// --- 串口命令超时（毫秒）。scan 命令在 BW16 侧阻塞约 2.5s，所以放宽 ---
#define BW16_TIMEOUT_MS     4000
#define BW16_SCAN_TIMEOUT   8000
#define BW16_STATUS_TIMEOUT 1500

// PCAP 下载的解码缓冲（BW16 每块原文 1024 字节，留余量）
#define BW16_PCAP_CHUNK_DEC 1100
