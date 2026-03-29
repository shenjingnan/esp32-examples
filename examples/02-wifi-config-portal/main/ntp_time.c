/* NTP 网络时间同步模块 - 实现文件

    本模块实现了基于 SNTP（简单网络时间协议）的时间同步功能。
    当 WiFi 连接成功后，ESP32 会自动从 NTP 服务器获取精确时间，
    并将系统时钟设置为中国标准时间（UTC+8）。

    核心概念说明：
    - NTP（Network Time Protocol）：网络时间协议，用于通过网络同步计算机时钟
    - SNTP（Simple NTP）：NTP 的简化版本，适合嵌入式设备使用
    - UTC+8：协调世界时加 8 小时，即中国标准时间（CST）

    此示例代码属于公共领域（或根据您的选择采用 CC0 许可。）
*/

#include "ntp_time.h"

#include <esp_log.h>
#include <esp_sntp.h>

#include <string.h>
#include <time.h>

// 模块日志标签，用于串口日志输出时区分来源
static const char *TAG = "ntp_time";

// 同步成功标志 —— 当 SNTP 回调触发时置为 true
static bool s_time_synced = false;

// 初始化标志 —— 防止重复调用 esp_sntp_init()（该函数不可重复调用）
static bool s_initialized = false;

/**
 * @brief SNTP 时间同步通知回调函数
 *
 * 当 SNTP 完成一次时间同步操作后，ESP-IDF 会调用此回调函数通知我们同步结果。
 * 我们只关心同步成功的情况，此时将 s_time_synced 标志置为 true。
 *
 * @param tv   同步后的时间值（timeval 结构体）
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    s_time_synced = true;

    // 获取同步后的本地时间并格式化输出
    char time_str[32];
    if (ntp_time_get_str(time_str, sizeof(time_str))) {
        ESP_LOGI(TAG, "时间同步成功！当前时间: %s (UTC+8)", time_str);
    }
}

void ntp_time_init(void)
{
    // 幂等保护：esp_sntp_init() 只能调用一次，重复调用会报错
    if (s_initialized) {
        ESP_LOGW(TAG, "NTP 服务已经初始化，跳过重复初始化");
        return;
    }

    ESP_LOGI(TAG, "正在初始化 NTP 时间同步服务...");

    // ============================================================
    // 第一步：设置时区为中国标准时间（CST, UTC+8）
    // ============================================================
    // setenv("TZ", "CST-8", 1) 设置时区环境变量：
    //   - "CST" 是时区名称（中国标准时间 China Standard Time）
    //   - "-8" 表示本地时间比 UTC 快 8 小时（注意 POSIX 时区格式的符号是反的）
    //   - 第三个参数 1 表示覆盖已有值
    // tzset() 让 C 标准库的时区函数读取新的 TZ 环境变量
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "时区已设置为中国标准时间 (CST, UTC+8)");

    // ============================================================
    // 第二步：配置 SNTP 工作模式
    // ============================================================
    // SNTP_OPS_MODE_POLL：轮询模式，ESP32 会定期向 NTP 服务器发送时间请求
    // 这是最常用的工作模式，适合大多数应用场景
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // ============================================================
    // 第三步：设置 NTP 服务器
    // ============================================================
    // 使用中国大陆的公共 NTP 服务器，确保在国内环境下也能快速同步：
    //   - ntp.aliyun.com：阿里云 NTP 服务器，稳定可靠
    //   - ntp.tencent.com：腾讯云 NTP 服务器，作为备用
    //   - cn.ntp.org.cn：中国国家授时中心 NTP 服务器
    // 配置多个服务器可以提高同步成功率，SNTP 会自动选择响应最快的服务器
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "ntp.tencent.com");
    esp_sntp_setservername(2, "cn.ntp.org.cn");

    // ============================================================
    // 第四步：设置同步模式
    // ============================================================
    // SNTP_SYNC_MODE_IMMED：立即同步模式
    // 调用 esp_sntp_init() 后会立即开始第一次时间同步，无需等待
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    // ============================================================
    // 第五步：注册同步通知回调
    // ============================================================
    // 当时间同步完成时（无论成功或失败），都会触发此回调
    // 我们在回调中设置同步成功标志，供 ntp_time_is_synced() 查询
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // ============================================================
    // 第六步：启动 SNTP 服务
    // ============================================================
    // esp_sntp_init() 会根据以上配置启动 SNTP 服务
    // 注意：此函数调用后 SNTP 会在后台自动运行，不需要手动轮询
    esp_sntp_init();

    s_initialized = true;
    ESP_LOGI(TAG, "NTP 时间同步服务已启动，等待同步...");
}

bool ntp_time_is_synced(void)
{
    return s_time_synced;
}

bool ntp_time_get_str(char *buf, size_t buf_size)
{
    // 参数有效性检查
    if (buf == NULL || buf_size < 20) {
        return false;
    }

    // 检查时间是否已同步
    if (!s_time_synced) {
        return false;
    }

    // 获取当前时间戳
    time_t now = time(NULL);

    // 将时间戳转换为本地时间结构体（已通过 TZ 环境变量自动转为 UTC+8）
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 格式化为 "YYYY-MM-DD HH:MM:SS" 格式
    // strftime 是 C 标准库的时间格式化函数：
    //   %Y = 四位年份, %m = 两位月份, %d = 两位日期
    //   %H = 24小时制小时, %M = 分钟, %S = 秒
    size_t len = strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
    return len > 0;
}
