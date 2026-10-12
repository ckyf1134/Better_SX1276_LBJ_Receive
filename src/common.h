#include <cstdint>
#include "Version.h"

// ============================================================================
//  用户配置区
//  本文件是通用版配置模板。发布/分享本固件时请只保留占位符，不要填写真实值。
// ============================================================================

// ---------------------------- 首页显示 ----------------------------
#define Logo1       " 中国铁路列车接近预警"   // 首页 LOGO 第一行
#define Logo2       "       通用版"          // 首页 LOGO 第二行（可自行修改）
#define AdsStr      ""                       // 日志内的自定义说明，留空则不输出

// ---------------------------- WiFi 接入 ----------------------------
// 修改为你自己的网络。主用/备用各一组，连接不上主用会自动切换备用。
#define WIFI_SSID            "YOUR_WIFI_SSID"        // 主用 WLAN 名称
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"    // 主用 WLAN 密码（8-16 位）
#define BACK_WIFI_SSID       "YOUR_BACKUP_SSID"      // 备用 WLAN 名称
#define BACK_WIFI_PASSWORD   "YOUR_BACKUP_PASSWORD"  // 备用 WLAN 密码（8-16 位）

// ---------------------------- 运行参数 ----------------------------
const uint64_t MaxCount = 65000;   // 通用版单次通电最大记录数
const bool NearAlarm = false;      // 主屏接近提醒开关

// 版本标志：release 为正式版，test 为测试版（测试版受 ValDate 试用期限制）
#define VerFlag       "release"
#define ValDate       "2099-12-31" // 测试版到期时间（仅 VerFlag == "test" 时生效）

const bool LogFlag = false;  // 日志开关
const bool CSVFlag = false;  // CSV 开关
