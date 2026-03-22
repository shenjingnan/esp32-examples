/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DNS_SERVER_MAX_ITEMS
#define DNS_SERVER_MAX_ITEMS 1
#endif

#define DNS_SERVER_CONFIG_SINGLE(queried_name, netif_key)  {        \
        .num_of_entries = 1,                                        \
        .item = { { .name = queried_name, .if_key = netif_key } }   \
        }

/**
 * @brief DNS 条目定义：名称 - IP（或使用网络接口的 IP 进行应答）
 *
 * @note 请使用字符串字面量（或确保它们在 dns_server 生命周期内有效）作为名称，
 * 因为我们不会复制配置值 `name` 和 `if_key`
 */
typedef struct dns_entry_pair {
    const char* name;       /**<! DNS 查询名称的精确匹配 */
    const char* if_key;     /**<! 使用此网络接口的 IP 进行应答，仅当为 NULL 时使用下面的静态 IP */
    esp_ip4_addr_t ip;      /**<! 用于应答此查询的固定 IP 地址，当 "if_key==NULL" 时使用 */
} dns_entry_pair_t;

/**
 * @brief DNS 服务器配置结构，定义应答 DNS（A 类型）查询的规则
 *
 * @note 如果要定义更多规则，可以在包含此头文件之前设置 `DNS_SERVER_MAX_ITEMS`
 * 使用 2 个条目和固定 IP 地址的示例：
 * \code{.c}
 * #define DNS_SERVER_MAX_ITEMS 2
 * #include "dns_server.h"
 *
 * dns_server_config_t config = {
 *   .num_of_entries = 2,
 *   .item = { {.name = "my-esp32.com", .ip = { .addr = ESP_IP4TOADDR( 192, 168, 4, 1) } } ,
 *             {.name = "my-utils.com", .ip = { .addr = ESP_IP4TOADDR( 192, 168, 4, 100) } } } };
 * start_dns_server(&config);
 * \endcode
 */
typedef struct dns_server_config {
    int num_of_entries;                             /**<! 配置结构中指定的规则数量 */
    dns_entry_pair_t item[DNS_SERVER_MAX_ITEMS];    /**<! 条目数组 */
} dns_server_config_t;

/**
 * @brief DNS 服务器句柄
 */
typedef struct dns_server_handle *dns_server_handle_t;

/**
 * @brief 设置并启动一个简单的 DNS 服务器，根据配置规则响应所有 A 类型查询（IPv4），
 * 规则为名称和 IPv4 地址或网络接口 ID（使用其 IPv4 地址响应）的配对
 *
 * @param config 配置结构，列出（名称，IP/网络接口 ID）配对
 * @return 成功返回 DNS 服务器句柄，失败返回 NULL
 */
dns_server_handle_t start_dns_server(dns_server_config_t *config);

/**
 * @brief 停止并销毁 DNS 服务器任务和结构
 * @param handle 要销毁的 DNS 服务器句柄
 */
void stop_dns_server(dns_server_handle_t handle);


#ifdef __cplusplus
}
#endif