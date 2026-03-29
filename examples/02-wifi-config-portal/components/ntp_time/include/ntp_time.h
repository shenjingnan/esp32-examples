/* NTP 网络时间同步模块 - 头文件

    提供 NTP（网络时间协议）时间同步功能，用于从互联网获取精确时间。
    使用中国大陆 NTP 服务器，设置 UTC+8 时区（中国标准时间）。

    此示例代码属于公共领域（或根据您的选择采用 CC0 许可。）
*/

#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 NTP 时间同步服务
 *
 * 配置中国大陆 NTP 服务器，设置 UTC+8 时区，启动 SNTP 同步。
 * 此函数是幂等的（可安全重复调用），内部使用标志位防止重复初始化。
 * 应在 WiFi 连接成功获取 IP 地址后调用。
 */
void ntp_time_init(void);

/**
 * @brief 查询 NTP 时间是否已同步成功
 *
 * @return true 时间已同步，可以获取准确时间
 * @return false 时间尚未同步，需要等待
 */
bool ntp_time_is_synced(void);

/**
 * @brief 获取格式化的当前时间字符串
 *
 * 将当前时间格式化为 "YYYY-MM-DD HH:MM:SS" 格式的字符串。
 * 时间已按 UTC+8 时区转换为中国标准时间。
 *
 * @param buf      输出缓冲区，用于存放格式化后的时间字符串
 * @param buf_size 缓冲区大小（建议至少 20 字节）
 * @return true  获取成功（时间已同步）
 * @return false 获取失败（时间尚未同步或参数无效）
 */
bool ntp_time_get_str(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* NTP_TIME_H */
