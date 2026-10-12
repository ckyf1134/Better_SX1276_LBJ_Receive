#include "networks.hpp"
#include <cstring>
#include <string>
#include <cctype>
/* ------------------------------------------------ */
ESPTelnet telnet;
IPAddress ip;
uint16_t port = 23;
const char *time_zone = "CST-8";
const char *ntpServer0 = "ntp.ntsc.ac.cn";
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "ntp.tencent.com";
char CheCi[8];//用于合并显示车次
struct tm time_info{};
uint64_t TelnetCount=1;
uint64_t SerialCount=1;
uint64_t SDCount=1;
std::string TrainType;
int8_t lbjlx=0;
String WiFiType="";
String JiCheHao="";//机车号字符串
String ENU_Port="";//动车组首末端

bool isConnected() {
    return (WiFiClass::status() == WL_CONNECTED);
}

static const char *wifi_target_ssid = nullptr;
static uint32_t wifi_attempt_start = 0;
static bool wifi_connecting = false;
static bool wifi_primary_done = false;

bool isNumber(const std::string& str) {
    for (char const &c : str) {
        if (!std::isdigit((unsigned char) c)) return false; // 如果找到非数字字符，返回false
    }
    return true; // 如果没有找到非数字字符，返回true
}

/*std::string QDKG(const std::string& input1) { //去掉字符串中的空格
    std::string result1;
    for (char c : input1) {
        if (c != ' ') {
            result1 += c;
        }
    }
    return result1;
}*/
// 处理C风格字符串(char*)
std::string QDKG(const char* input) {
    std::string result;
    if (input) {
        for (const char* p = input; *p; ++p) {
            if (!std::isspace(static_cast<unsigned char>(*p))) {
                result += *p;
            }
        }
    }
    return result;
}

// 处理String类型(假设为自定义String类)
std::string QDKG(const String& input) {
    std::string result;
    for (size_t i = 0; i < input.length(); ++i) {
        if (!std::isspace(static_cast<unsigned char>(input[i]))) {
            result += input[i];
        }
    }
    return result;
}

// 处理std::string类型
std::string QDKG(const std::string& input) {
    std::string result;
    result.reserve(input.size()); // 预分配空间，提高性能
    for (char c : input) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}
// 将字符串中的所有小写字母转换为大写
std::string toUpperCase(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = (char) std::toupper((unsigned char) c);
    }
    return result;
}

static const char *singleCharTrainType(char c) {
    switch (c) {
        case 'G': return "高速动车";
        case 'C': return "城际动车";
        case 'D': return "普通动车";
        case 'S': return "市域列车";
        case 'Z': return "直达特快";
        default: return nullptr;
    }
}

// 判断当前已连接的目标是否为指定 SSID。
// 注意：必须比较字符串内容而不是指针。原先写成 wifi_target_ssid == WIFI_SSID，
// 那是比较地址；只有当两边恰好指向同一个宏字面量时才碰巧成立。
static bool ssidIs(const char *target, const char *expected) {
    return target != nullptr && expected != nullptr && strcmp(target, expected) == 0;
}

bool connectWiFi() {
    if (isConnected()) {
        wifi_connecting = false;
        wifi_primary_done = false;
        if (ssidIs(wifi_target_ssid, WIFI_SSID)) {
            WiFiType = "主";
        } else if (ssidIs(wifi_target_ssid, BACK_WIFI_SSID)) {
            WiFiType = "备";
        }
        return true;
    }
    WiFiType = "断";

    if (!wifi_primary_done) {
        if (connectWiFiFunction(WIFI_SSID, WIFI_PASSWORD)) {
            WiFiType = "主";
            return true;
        }
        if (!wifi_connecting) {
            wifi_primary_done = true;
        }
        return false;
    }

    if (connectWiFiFunction(BACK_WIFI_SSID, BACK_WIFI_PASSWORD)) {
        WiFiType = "备";
        return true;
    }
    return false;
}

bool connectWiFiFunction(const char *ssid, const char *password) {
    if (isConnected()) {
        wifi_connecting = false;
        WiFiType = ssidIs(wifi_target_ssid, WIFI_SSID) ? "主" : "备";
        return true;
    }
    if (!wifi_connecting || !ssidIs(wifi_target_ssid, ssid)) {
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFiClass::mode(WIFI_STA);
        WiFi.begin(ssid, password);
        wifi_target_ssid = ssid;
        wifi_attempt_start = millis();
        wifi_connecting = true;
        Serial.printf("尝试连接到WiFi: %s\n", ssid);
    }
    if (millis() - wifi_attempt_start < 8000) {
        return false;
    }
    wifi_connecting = false;
    return false;
}

void changeCpuFreq(uint32_t freq_mhz) {
    if (ets_get_cpu_frequency() != freq_mhz) setCpuFrequencyMhz(freq_mhz);
}

/* ------------------------------------------------ */

void timeAvailable(struct timeval *t) {
    tm ti2{};
    Serial.println("从SNTP服务器获取时间");
    getLocalTime(&time_info);
    Serial.println(&time_info, "SNTP服务器返回时间：%Y-%m-%d %H:%M:%S");
#ifdef HAS_RTC
    getLocalTime(&time_info);
    rtc.adjust(rtcLibtoC(time_info));
    // rtc.setDateTime(time_info);
    auto timer = esp_timer_get_time();
    ti2 = rtcLibtoC(rtc.now());
    // rtc.getDateTime(ti2);
    Serial.print(&ti2, "[eRTC] Time set to %Y-%m-%d %H:%M:%S ");
    Serial.printf("[%lld]\n", esp_timer_get_time() - timer);
#endif
}

//void printLocalTime()
//{
//    struct tm timeinfo;
//    if(!getLocalTime(&timeinfo)){
//        Serial.println("No time available (yet)");
//        return;
//    }
//    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
//}

void timeSync(struct tm &time) {
    time_t t = mktime(&time);
    struct timeval now = {.tv_sec = t};
    settimeofday(&now, nullptr);
}

char *fmtime(const struct tm &time) {
    static char buffer[20];
    sprintf(buffer, "%d-%02d-%02d %02d:%02d:%02d", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour,
            time.tm_min, time.tm_sec);
    return buffer;
}

char *fmtms(uint64_t ms) {
    static char buffer[40];
    if (ms < 60000) // seconds
        sprintf(buffer, "%.3f Seconds", (double) ms / 1000);
    else if (ms < 3600000) // minutes
        sprintf(buffer, "%llu Minutes %.3f Seconds", ms / 60000, (double) (ms % 60000) / 1000);
    else if (ms < 86400000) // hours
        sprintf(buffer, "%llu Hours %llu Minutes %.3f Seconds", ms / 3600000, ms % 3600000 / 60000,
                (double) (ms % 3600000 % 60000) / 1000);
    else // days
        sprintf(buffer, "%llu Days %llu Hours %llu Minutes %.3f Seconds", ms / 86400000, ms % 86400000 / 3600000,
                ms % 86400000 % 3600000 / 60000, (double) (ms % 86400000 % 3600000 % 60000) / 1000);

    return buffer;
}


int JSDL(float voltage) {
    // 电压范围限制
    if (voltage >= 4.2f) return 100.0f;
    if (voltage <= 3.2f) return 0.0f;
    // 简化的分段线性近似模型
    if (voltage >= 4.0f) return 80.0f + (voltage - 4.0f) * 100.0f;  // 4.0-4.2V → 80-100%
    if (voltage >= 3.7f) return 40.0f + (voltage - 3.7f) * 133.33f; // 3.7-4.0V → 40-80%
    if (voltage >= 3.4f) return 10.0f + (voltage - 3.4f) * 100.0f;  // 3.4-3.7V → 10-40%
    return (voltage - 3.2f) * 25.0f;
}
void onTelnetConnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println("已连接");
    TelnetCount=1;
    telnet.println("－－－－－－－－－－－－－－－－－－－－－－－");
    telnet.println("　　　中国铁路列车接近预警器Telnet信息台");
    telnet.printf("　　　　　%s\r\n",Logo2);
    telnet.printf("　　　　　 系统版本：%s\r\n",SysVer);
    getLocalTime(&time_info, 10);
    char timeStr[20];
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    telnet.printf(" 恭喜您已于[%s]连接成功\r\n",timeStr);
    telnet.println(AdsStr);
    telnet.println("　　　　　　【安　全　提　示】");
    telnet.println("　【严禁侵入铁路限界】　文明拍车，注意安全！");
    telnet.println("－－－－－－－－－－－－－－－－－－－－－－－");
    telnet.print(" ");
}

void onTelnetDisconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println("断开连接");
}

void onTelnetReconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println("重新连接");
}

void onTelnetConnectionAttempt(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println("尝试连接");
}

void onTelnetInput(String str) {
    bool ext_call = false;
    size_t args_count = 3;
    char strc[256];
    str.toCharArray(strc, 256);
    String args[3];
    bool toolong = false;
    for (size_t i = 0, c = 0; i < 256; i++) {
        if (strc[i] == 0)
            break;
        if (strc[i] == ' ') {
            i++;
            c++;
        }
        if (c > args_count - 1) {
            toolong = true;
            break;
        }
        args[c] += strc[i];
    }
    if (!toolong) {
        if (args[0] == "test") {
            if (args[1] == "test") {
                telnet.println("> 参数测试");
            } else
                telnet.println("> 未知命令");
        }
        if (args[0] == "log") {
            if (args[1] == "read") {
                bool valid = true;
                if (args[2].length() == 0)
                    valid = false;
                for (auto c: args[2]) {
                    if (!isDigit(c))
                        valid = false;
                }
                if (!valid)
                    telnet.println(">无效格式，请输入数字");
                else {
                    if (sd1.status())
                        sd1.printTel(std::stoi(args[2].c_str()), telnet);
//                        telPrintLog(std::stoi(args[2].c_str()));
                    else
                        telnet.println("> 无法访问T-Flash卡");
                }
            } else if (args[1] == "status") {
                if (sd1.status())
                    telnet.println("> True.");
                else
                    telnet.println("> False.");
            } else
                telnet.println("> 未知的命令。");
        }
        if (args[0] == "afc") {
            if (args[1] == "off") {
                prb_count = 0;
                prb_timer = 0;
                freq_correction = false;
                telnet.println("> 频率校正已禁用");
                Serial.println("[Telnet] > 频率校正已禁用");
            }
            else if (args[1] == "on") {
                freq_correction = true;
                telnet.println("> 频率校正已启用");
                Serial.println("[Telnet] > 频率校正已启用");
            }
        }
        if (args[0] == "ppm") {
            if (args[1].length() != 0 ){
                bool valid = true;
                bool point = false;
                for (auto c:args[1]){
                    if (!isDigit(c)) {
                        if (c == '.'&& !point)
                            point = true;
                        else
                            valid = false;
                    }
                }
                if (valid) {
                    ppm = std::stof(args[1].c_str());
                    tel_set_ppm = true;
                } else
                    telnet.println("> 无效格式，请输入浮点数");
            }
        }
    }
    if (str == "ping") {
        telnet.println("> pong");
        Serial.println("[Telnet] > pong");
        // disconnect the client
    } else if (str == "bye") {
        telnet.println("> 断开连接");
        telnet.disconnectClient();
    } else if (str == "read") {
        if (sd1.status())
            telPrintLog(1000);
        else
            telnet.println("无法访问T-Flash卡");
    } else if (str == "bat")
        telnet.printf("电池当前电压 %1.2fV\n", battery.readVoltage() * 2);
    else if (str == "rssi") {
        give_tel_rssi = true;
        ext_call = true;
    } else if (str == "gain") {
        give_tel_gain = true;
        ext_call = true;
    } else if (str == "time") {
        telPrintf(true, "请求的时间\n");
        Serial.printf("[Telnet] > 请求的时间\n");
        ext_call = true;
    }
    if (!ext_call)
        telnet.print("< ");
}




void setupTelnet() {
    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onTelnetInput);

    Serial.print("[Telnet] ");
    if (telnet.begin(port)) {
        Serial.println("正在运行");
        telnet_online = true;
    } else {
        Serial.println("错误");
    }
}


int16_t readDataLBJ(struct PagerClient::pocsag_data *p, struct lbj_data *l) {
    for (size_t i = 0; i < POCDAT_SIZE; i++) {
        if (p[i].is_empty)
            continue;
        switch (p[i].addr) {
            case LBJ_INFO_ADDR: {
                l->lbj_class[0] = '\0';
                if (!p[i].func && p[i].len == 5 && (p[i].str[0] == '-' || p[i].str[0] == '*') &&
                    (p[i].str[1] && p[i].str[2] && p[i].str[3] && p[i].str[4] != '-')) {

                    p[i].addr = LBJ_SYNC_ADDR; 
                    goto lbj_sync;
                } else {
                    l->type = 0;
                    lbjlx=0;
                    l->direction = (int8_t) p[i].func;
                    if (l->direction == 3)
                        l->FX = "上行";
                    else if (l->direction == 1)
                        l->FX = "下行";
                    else
                        l->FX = "调车";

                    if (p[i].str.length() >= 5 && p[i].str[0] != 'X') {
                        for (size_t c = 0; c < 5; c++) {
                            l->train[c] = p[i].str[c];

                        }
                    }
                    if (p[i].str.length() >= 10 && p[i].str[6] != 'X') {
                        for (size_t c = 6, v = 0; c < 9; c++, v++) {
                            l->speed[v] = p[i].str[c];
                        }
                        l->speed[3] = 0;
                    }
                    if (p[i].str.length() >= 15 && p[i].str[10] != 'X') {
                        size_t v = 0;
                        for (size_t c = 10; c < 15; c++, v++) {
                            l->position[v] = p[i].str[c];
                            if (c == 13 && p[i].str[c] != ' ') {
                                l->position[++v] = '.';
                            } else if (c == 13) {
                                // 0.x km is better than .x km, isn't it?
                                l->position[v] = '0';
                                l->position[++v] = '.';
                            }
                        }
                        l->position[v] = 0;
                    }
                    if (!(p[i].str.length() == 65 || p[i].str.length() >= 70)) {
                        break;
                    }
                }
            }
            case LBJ_INFO2_ADDR: {
                l->type = 1;
                lbjlx=1;
                String buffer;
                if (p[i].addr == LBJ_INFO_ADDR) {
                    if (p[i].str.length() == 65)
                        buffer = p[i].str.substring(15);
                    else if (p[i].str.length() == 70 || (p[i].str.length() >= 70 && p[i].str[67] == '0' && p[i].str[68] == '0' && p[i].str[69] == '0'))
                        buffer = p[i].str.substring(20);
                    else
                        break;
                } else
                    buffer = p[i].str;
                if (l->direction == -1){
                    l->direction = (int8_t) p[i].func;
                    if (l->direction == 3)
                        l->FX = "上行";
                    else if (l->direction == 1)
                        l->FX = "下行";
                    else
                        l->FX = "调车";
                }

                for (char &c: buffer) {
                    recodeBCD(&c, &l->info2_hex);
                }

                //机车注册号
                if (buffer.length() >= 12 && buffer[4] != 'X' && buffer[5] != 'X' && buffer[10] != 'X') {
                    for (size_t c = 4, v = 0; c < 12; c++, v++) l->loco[v] = buffer[c];
                    String type;
                    for (size_t c = 0; c < 3; c++) {
                        if (isdigit(l->loco[c]))
                            type += l->loco[c];
                    }
                    if (type.length() == 3 && type.toInt() < (sizeof locos / sizeof locos[0]) && type.toInt() >= 0) {
                        l->loco_type = locos[std::stoi(type.c_str())];
                    }
                }

                // 东经
                if (buffer.length() >= 39 && buffer[30] != 'X' && buffer[35] != 'X') {
                    for (size_t c = 30, v = 0; c < 39; c++, v++) {
                        l->pos_lon[v] = buffer[c];
                    }
                    for (size_t c = 30, v = 0; c < 33; c++, v++) {
                        l->pos_lon_deg[v] = buffer[c];
                    }
                    size_t v = 0;
                    for (size_t c = 33; c < 39; c++, v++) {
                        l->pos_lon_min[v] = buffer[c];
                        if (c == 34)
                            l->pos_lon_min[++v] = '.';
                    }
                }
                // 北纬
                if (buffer.length() >= 47 && buffer[39] != 'X' && buffer[40] != 'X' && buffer[45] != 'X') {
                    for (size_t c = 39, v = 0; c < 47; c++, v++) {
                        l->pos_lat[v] = buffer[c];
                    }
                    for (size_t c = 39, v = 0; c < 41; c++, v++) {
                        l->pos_lat_deg[v] = buffer[c];
                    }
                    size_t v = 0;
                    for (size_t c = 41; c < 47; c++, v++) {
                        l->pos_lat_min[v] = buffer[c];
                        if (c == 42)
                            l->pos_lat_min[++v] = '.';
                    }
                }
                // BCD to HEX and to ASCII 列车等级（车次冠字）
                if (l->info2_hex.length() >= 4 && l->info2_hex[0] != 'X') {
                    // this is very likely the most ugly code I've ever written, I apologize for that.
                    size_t c = 0;
                    for (size_t v = 0; v < 3; v++, c++) {
                        int8_t ch = hexToChar(l->info2_hex[v], l->info2_hex[v + 1]);
                        ++v;
                        if (ch > 0x1F && ch < 0x7F && ch != 0x22 && ch != 0x2C)
                            l->lbj_class[c] = ch;
                    }
                }
                // to GB2312 线路名称
                if (l->info2_hex.length() >= 20 && l->info2_hex[14] != 'X' && l->info2_hex[15] != 'X') { // Character 1
                    size_t c = 0;
                    for (size_t v = 14; v < 17; v++, c++) {
                        int8_t ch = hexToChar(l->info2_hex[v], l->info2_hex[v + 1]);
                        ++v;
                        if ((uint8_t )ch >= 0xA0 || ch == 0x20 || ch >= 0x2D && ch <= 0x7E && ch != 0x60)
                            l->route[c] = ch;
                    }
                }
                if (l->info2_hex.length() >= 25 && l->info2_hex[18] != 'X' && l->info2_hex[20] != 'X') {// Character 2
                    size_t c = 2;
                    for (size_t v = 18; v < 21; v++, c++) {
                        int8_t ch = hexToChar(l->info2_hex[v], l->info2_hex[v + 1]);
                        ++v;
                        if ((uint8_t )ch >= 0xA0 || ch == 0x20 || ch >= 0x2D && ch <= 0x7E && ch != 0x60)
                            l->route[c] = ch;
                    }
                }
                if (l->info2_hex.length() >= 30 && l->info2_hex[22] != 'X' && l->info2_hex[25] != 'X') {// Character 3,4
                    size_t c = 4;
                    for (size_t v = 22; v < 29; v++, c++) {
                        int8_t ch = hexToChar(l->info2_hex[v], l->info2_hex[v + 1]);
                        ++v;
                        if ((uint8_t )ch >= 0xA0 || ch == 0x20 || ch >= 0x2D && ch <= 0x7E && ch != 0x60)
                            l->route[c] = ch;
                    }
                }
            for (int a=0; a<8; a++) {
                if ((l->loco[0]=='3' or l->loco[0]=='4') && a>=3) l->loco_num[a-3] = l->loco[a];//对于大机和动车组，将l->loco的后5位写入l->loco_num中，动车组的车组号后续再处理
                else if (a >= 4) l->loco_num[a-4] = l->loco[a]; // 对于普速机车，将l->loco的后4位写入l->loco_num中
            }
            if (l->loco[0] == '3' && l->loco[1] <= '3'){//对于动车组，车组号只保留前4位
                ENU_Port=( l->loco_num[4]==1)?"首端":"末端";
                l->loco_num[4]='\0';
            }

                gbk2utf8(l->route, l->route_utf8, 17);
                break;
            }
            case LBJ_SYNC_ADDR: {
                lbj_sync:
                l->type = 2;
                lbjlx=2;
                l->lbj_class[0] = '\0';
                l->train[0]='\0';
                if (p[i].str.length() >= 5 && p[i].str[0] != 'X') {
                    for (size_t c = 1, v = 0; c < 5; c++, v++) {
                        l->time[v] = p[i].str[c];
                        if (c == 2)
                            l->time[++v] = ':';
                    }
                }
                break;
            }
        }
    }
    String xstr="型";//机车车型与车号的连接字
    if(l->loco[0]=='3' && l->loco[1]>='0' && l->loco[1]<='2')xstr="系";
    else if(l->loco[0]=='4')xstr="车";
    if (l->loco[0]>'0')
    {
	JiCheHao = l->loco_type + xstr + String(l->loco_num) + "号";
	if (String(l->loco) != "-" && l->info2_hex.length() > 14 && l->info2_hex[12] == '3' && (l->info2_hex[13] == '1' or l->info2_hex[13] == '2')) JiCheHao+=String(l->info2_hex[13])+"端";	//重联型机车1/2端
    }
    else JiCheHao = "";
    if(l->loco[0]=='3' && l->loco[1]>='0' && l->loco[1]<='2')JiCheHao+=ENU_Port;
    TrainType="";
    sprintf(CheCi,"%s%s",l->lbj_class,l->train);//将车次冠字和列车车次合并到CheCi中
    std::string checi1=QDKG(CheCi);//用于存放去除空格后的车次
    checi1=toUpperCase(checi1);//若有小写字母则转为大写字母
    l->CheCi=String(checi1.c_str());//将处理后的车次写入结构的车次CheCi元素
    //-------------------------------获取列车类型的独家核心逻辑开始-------------------------------
    struct RangeEntry {
    int upperBound;
    const char* typeName;};
    int ccb = checi1.length();
    const char *singleType = singleCharTrainType(checi1[0]);
    if (lbjlx==0){
       if(ccb<5)TrainType="旅客列车";
       if (ccb==5 && singleType != nullptr) TrainType=singleType;
    }
    int checi2=0;
    for(int ci=0;checi1[ci]!='\0';ci++){
      if(isdigit(checi1[ci]))checi2=checi2 * 10 + (checi1[ci]-'0');
      else checi2=checi2 * 10;//若当前位非数字则按0处理
    }
//4位纯数字车次字符串转换为数字完毕
    if (lbjlx==1){
        if (singleType != nullptr) TrainType=singleType;
	//以下处理4位纯数字客车及其转义车次
        if(ccb == 4 && (isNumber(checi1)||checi1[0]=='V'||checi1[0]=='B'||checi1[0]=='U')) {
            const RangeEntry trainTypeTablePS[] = {{4000, "直通普快"},{5998, "管内普快"},{7598, "普客"},{8998, "通勤车"},{INT_MAX, "未知普客"}};
            constexpr size_t tableSize = sizeof(trainTypeTablePS) / sizeof(trainTypeTablePS[0]);
            for (size_t i = 0; i < tableSize; ++i) {
                if (checi2 <= trainTypeTablePS[i].upperBound) {
                    TrainType = trainTypeTablePS[i].typeName;
                    break;
                }
	        }
        }
    }
    if(checi1[0] == 'T' || checi1[0] == 'Q'){
		RangeEntry VeryFastTrainTypeTable[] = {{5000, "直通特快"},{9800, "管内特快"},{INT_MAX, "特快列车"}};
		constexpr size_t VeryFastTableSize = sizeof(VeryFastTrainTypeTable) / sizeof(VeryFastTrainTypeTable[0]);
		for (size_t i = 0; i < VeryFastTableSize; ++i) {
			if (checi2 <= VeryFastTrainTypeTable[i].upperBound) {
			    TrainType = VeryFastTrainTypeTable[i].typeName;
			    break;
	 		}
		}
    }
    else if(checi1[0] == 'K' || checi1[0] == 'W'){
	    RangeEntry fastTrainTypeTable[] = {{5000, "直通快速"},{6000, "快速临客"},{9850, "管内快速"},{INT_MAX, "快速列车"}};
	    constexpr size_t fastTableSize = sizeof(fastTrainTypeTable) / sizeof(fastTrainTypeTable[0]);
	    for (size_t i = 0; i < fastTableSize; ++i) {
	        if (checi2 <= fastTrainTypeTable[i].upperBound) {
	           TrainType = fastTrainTypeTable[i].typeName;
	           break;
	        }
	    }
    }
    else if(checi1[0] == 'L'){
        const RangeEntry TEMPTrainTypeTable[] = {{7000, "直通临客"},{9850, "管内临客"},{INT_MAX, "临时客车"}};
        // 识别临客列车类型
        constexpr size_t specialTableSize = sizeof(TEMPTrainTypeTable) / sizeof(TEMPTrainTypeTable[0]);
        for (size_t i = 0; i < specialTableSize; ++i) {
            if (checi2 <= TEMPTrainTypeTable[i].upperBound) {
                TrainType = TEMPTrainTypeTable[i].typeName;
                break;
            }
        }
    }
    else if(checi1[0] == 'Y'){
        // 旅游列车类型查找表
        const RangeEntry touristTrainTypeTable [] = {{499, "直通旅游"},{980, "管内旅游"},{INT_MAX, "旅游专列"}};
        constexpr size_t touristTableSize = sizeof (touristTrainTypeTable) /sizeof (touristTrainTypeTable [0]);
        for (size_t i = 0; i < touristTableSize; ++i) {
            if (checi2 <= touristTrainTypeTable [i].upperBound) {
                TrainType = touristTrainTypeTable [i].typeName;
                break;
            }
        }
    }
    if (ccb == 5 && isNumber(checi1)) {//处理5位纯数字货车车次
        // 货运列车类型查找表（按区间从小到大排序）
        const RangeEntry freightTrainTable[] = {{20000, "技术直达"},{30000, "直通货列"},{40000, "区间货列"},{45000, "摘挂货列"},{50000, "小运转"},{51000, "客单机"},{52000, "货单机"},{53000, "小运转单机"},{55000, "补机"},{56000, "试运转"},{57000, "工程车"},{58000, "路用车"},{59000, "救援列车"},{60000, "机车临时"},{70000, "自备车"},{71000, "超限货列"},{73000, "重载货列"},{75000, "保温冷藏货列"},{80000, "集装箱货列"},{81000, "五定班普快"},{82000, "五定班快运"},{85000, "煤炭直达"},{86000, "石油直达"},{87000, "特种货列"},{88000, "空车直达"},{90000, "特快货列"},{94000, "军用列车"},{95000, "[禁拍]特别军列"},{98000, "抢险救灾"},{INT_MAX, "未知货列"}};
        // 识别货运列车类型
        constexpr size_t tableSize = sizeof(freightTrainTable) / sizeof(freightTrainTable[0]);
        for (size_t i = 0; i < tableSize; ++i) {
            if (checi2 < freightTrainTable[i].upperBound) {
                TrainType = freightTrainTable[i].typeName;
                break;
            }
        }
    }
    if (checi1[0] == '0')//处理0字头
        {
        if ( checi1[1] == '0') //第2位也是0
            {
            if(checi2<=100) TrainType="有火回送动车组";
            else if(checi2<=300) TrainType="无火回送动车组";
            else if(checi2<=500) TrainType="无火临空";
            else  TrainType="临空列车";
            }
        else TrainType="车底回送";
        }
    if (checi1[0] == 'D' && checi1[1] == 'J') TrainType="动检列车";
    else if(checi1[0] == 'X') TrainType=(checi2<200)?"行邮特快":"行包快运"; 
    else if (checi1[0] == 'Q') TrainType="抢险列车";
    if ((l->loco[0] == '3' && l->loco[1] >= '5') || l->loco[0] == '4')TrainType="路用大机";//对于大机车，无论使用5xxxx标准车次还是使用"SHxxxx"之类的非标准车次，列车类型均为路用大机
    if((l->loco[0]=='3' || l->loco[0]=='9') && l->loco[1]>='9' && l->loco[2]<='9')//针对测试信号的特别处理
	  {
	    JiCheHao = l->loco_type + String(l->loco_num);
	    TrainType="测试列车";
	  }
    if(checi1=="-" || (lbjlx==1 && isNumber(checi1) && ccb < 4) )TrainType="[接收错误]";
    return 0;
}

int8_t hexToChar(int8_t hex1, int8_t hex2) {
    uint8_t a, b;
    if (isdigit(hex1))a = hex1 - 0x30;
    else a = hex1 - 0x37;
    if (isdigit(hex2))b = hex2 - 0x30;
    else b = hex2 - 0x37;
    return (int8_t) ((a << 4) | b);
}


void recodeBCD(const char *c, String *v) {
	

	switch (*c) {
        case '*': {
            *v += 'A';
            break;
        }
        case 'U': {
            *v += 'B';
            break;
        }
        case ' ': {
            *v += 'C';
            break;
        }
        case '-': {
            *v += 'D';
            break;
        }
        case ')': {
            *v += 'E';
            break;
        }
        case '(': {
            *v += 'F';
            break;
        }
        default: {
            *v += *c;
            break;
        }
        
    }
}


/*---------------------------------------------------------*/

int enc_unicode_to_utf8_one(unsigned long unic, unsigned char *pOutput) {
    assert(pOutput != nullptr);

    if (unic <= 0x0000007F) {
        // * U-00000000 - U-0000007F:  0xxxxxxx
        *pOutput = (unic & 0x7F);
        return 1;
    } else if (unic >= 0x00000080 && unic <= 0x000007FF) {
        // * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
        *(pOutput + 1) = (unic & 0x3F) | 0x80;
        *pOutput = ((unic >> 6) & 0x1F) | 0xC0;
        return 2;
    } else if (unic >= 0x00000800 && unic <= 0x0000FFFF) {
        // * U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
        *(pOutput + 2) = (unic & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 6) & 0x3F) | 0x80;
        *pOutput = ((unic >> 12) & 0x0F) | 0xE0;
        return 3;
    } else if (unic >= 0x00010000 && unic <= 0x001FFFFF) {
        // * U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 3) = (unic & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 12) & 0x3F) | 0x80;
        *pOutput = ((unic >> 18) & 0x07) | 0xF0;
        return 4;
    } else if (unic >= 0x00200000 && unic <= 0x03FFFFFF) {
        // * U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 4) = (unic & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 18) & 0x3F) | 0x80;
        *pOutput = ((unic >> 24) & 0x03) | 0xF8;
        return 5;
    } else if (unic >= 0x04000000 && unic <= 0x7FFFFFFF) {
        // * U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 5) = (unic & 0x3F) | 0x80;
        *(pOutput + 4) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 18) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 24) & 0x3F) | 0x80;
        *pOutput = ((unic >> 30) & 0x01) | 0xFC;
        return 6;
    }

    return 0;
}


void gbk2utf8(const char *gbk1, char *utf8s, size_t gbk_len) {
    uint16_t unic[gbk_len];
    uint8_t gbk[gbk_len];

    size_t c = 0;
    for (size_t i = 0; i < gbk_len; i++) {
        gbk[i] = (uint8_t) gbk1[i];
    }
    for (size_t i = 0; i < gbk_len; i++, c++) {
        if (gbk[i] < 0x80) {
            unic[c] = gbk[i];
        } else {
            unic[c] = ff_oem2uni((uint16_t) (gbk[i] << 8 | gbk[i + 1]), 936);
            i++;
        }
    }
    c = 0;
    size_t i = 0;
    uint8_t utf8[gbk_len * 2];
    for (; i < gbk_len * 2; i++, c++) {
        uint8_t ut8[4];
        int r = enc_unicode_to_utf8_one(unic[c], ut8);
        if (i + 4 < gbk_len * 2) {
            if (r == 1) utf8[i] = ut8[0];
            else if (r == 2) utf8[i] = ut8[0], utf8[++i] = ut8[1];
            else if (r == 3) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2];
            else if (r == 4) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2], utf8[++i] = ut8[3];
        }
    }
    for (size_t v = 0; v < gbk_len * 2; v++) {
        utf8s[v] = (char) utf8[v];
    }
}
std::string XHZL(float RSSI){//信号质量
    if(RSSI > -100) return "良好";
    else if(RSSI > -110)return "一般";
    else if(RSSI > -120)return "较差";
    else return "极差";

}
/*---------------------------------------------------------*/
void telPrintf(bool time_stamp, const char *format, ...) { // Generated by ChatGPT.
    char buffer[256]; // 创建一个足够大的缓冲区来容纳格式化后的字符串
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);

    // 输出到 Telnet
    if (telnet_online) { // code from Multimon-NG
        if (is_startline) {
            telnet.print("\r\n　");
            if (time_stamp && getLocalTime(&time_info, 1))
                telnet.printf("接收时间：%d-%02d-%02d %02d:%02d:%02d\r\n", time_info.tm_year + 1900, time_info.tm_mon + 1,time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer);
        if (nullptr != strchr(buffer, '\n')) {
            is_startline = true;
            telnet.print("\r\n　");
        }
    }
}

void telPrintLog(int chars) {
    File log = sd1.logFile('r');
    uint32_t pos = 0;
    uint32_t left = 0;
    if (chars < log.size())
        pos = log.size() - chars;
    else
        left = chars - log.size();
    String line;
    while (log.available()) {
        line = log.readStringUntil('\n');
        if (line) {
            telnet.print(line);
            telnet.print("\n");
        } else
            telPrintf(false, "内存卡日志读取失败！");
    }
    if (left)

        sd1.reopen();
}

void printDataSerial(PagerClient::pocsag_data *p, const struct lbj_data &l, const struct rx_info &r) {
    Serial.printf("\n\n\n－－－－－－－－－－－－－－－－－－－－－－－－－\n");
    Serial.printf("　　    第 %5u 条记录\n", SerialCount++);
    if (MaxCount==100 && SerialCount==100)return;
    else {
    Serial.printf("　接收时间：%d年%02d月%02d日%02d时%02d分%02d秒\n", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,time_info.tm_hour, time_info.tm_min,time_info.tm_sec);
    if(SerialCount >= MaxCount)SerialCount=1;//防止记录数溢出，即将到达最大值时复位1
    switch (l.type) {
        case 0: {

            	Serial.printf("　数据类型：LBJ基本信息\n");
            	Serial.printf("　%-4s列车: %s  \n　速　　度: %s KM/H\n　公 里 标: %s KM\n",l.FX.c_str(), QDKG(l.train).c_str(), QDKG(l.speed).c_str(), QDKG(l.position).c_str());
            	Serial.printf("　列车类型：%s\n",TrainType.c_str());
            	break;
        }
        case 1: {
            	Serial.printf("　数据类型：LBJ扩展信息\n");
                Serial.printf("　%-4s列车：%s次\n",l.FX.c_str(), l.CheCi.c_str());
                Serial.printf("　运行速度：%sKM/H\n",QDKG(l.speed).c_str());
                Serial.printf("　线路名称：%s\n", QDKG(l.route_utf8).c_str());
                Serial.printf("　里程位置：%sKM\n", QDKG(l.position).c_str());
                if(JiCheHao.length() > 0)
                {
                    Serial.printf("　本务机车：%s\n", JiCheHao.c_str()); 
                    Serial.printf("　注册号码：%s\n", QDKG(l.loco).c_str());
                }
                //Serial.printf("　本务机车：%s%\n", JiCheHao.c_str());
                //Serial.printf("　注册号码：%s\n", l.loco);
            	Serial.printf("　列车类型：%s\n",TrainType.c_str());
                    std::string poslat = l.pos_lat;
                    if (l.pos_lat_deg[0] && l.pos_lat_min[0]) {
                        char buffer[20];
                        sprintf(buffer, "%-3s度%s分", l.pos_lat_deg,l. pos_lat_min);
                        poslat = buffer;
                    }
                    std::string poslon = l.pos_lon;
                    if (l.pos_lon_deg[0] && l.pos_lon_min[0]) {
                        char buffer[20];
                        sprintf(buffer, "%-3s度%s分", l.pos_lon_deg, l.pos_lon_min);
                    poslon = buffer;
                    }
                Serial.printf("　东　　经：%s\n", poslon.c_str());
                Serial.printf("　北　　纬：%s\n", poslat.c_str());
                break;
        }
        case 2: {
		Serial.printf("　数据类型：LBJ报时\n");
		Serial.printf("　当前时间： %s  ", l.time);
            break;
        }
    }
    Serial.printf("\n");
    Serial.printf("　信号强度：%-6.1fdBm\n　信号质量：%s\n", r.rssi,XHZL(r.rssi).c_str());
    int DLSY = JSDL(battery.readVoltage() * 2); // 计算电量百分比
    Serial.printf("　电池电压：%-6.2f伏特\n　大约剩余：%d％\n",battery.readVoltage()*2,DLSY);
 }
    if(r.rssi>-90){
        Serial.printf( "┌－－－－－－－－－－－－－－－－－┐\n");
        Serial.printf( "│　　　　　　　警　　告　　　　　　│\n");
        Serial.printf( "│　　　列车已接近，请注意安全！　　│\n");
        Serial.printf( "│　　　　　请立即下道避车！　　　　│\n");
        Serial.printf( "└－－－－－－－－－－－－－－－－－┘\n");
    }
    Serial.printf("－－－－－－－－－－－－－－－－－－－－－－－－－\n");
}

void appendDataLog(PagerClient::pocsag_data *p, const struct lbj_data &l, const struct rx_info &r) {
    sd1.appendBuffer("\n\n－－－－－－－－－－－－－－－－－－－－－－－－－\n");
    sd1.appendBuffer("　　　　　　第 %5u 条记录\n", SDCount++);
    if (MaxCount==100 && SDCount==100)return;
    else {
 
    if(SDCount >= MaxCount)SDCount=1;//防止记录数溢出，即将到达最大值时复位1
    sd1.appendBuffer("接收时间：%d年%02d月%02d日%02d时%02d分%02d秒\n", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,time_info.tm_hour, time_info.tm_min,time_info.tm_sec);
    switch (l.type) {
        case 0: {
            sd1.appendBuffer("数据类型：LBJ基础数据\n");
            sd1.appendBuffer("%-4s列车：%s\n速　　度: %s KM/H\n公 里 标: %s KM\n",l.FX.c_str(), QDKG(l.train).c_str(), QDKG(l.speed).c_str(), QDKG(l.position).c_str());
            sd1.appendBuffer("列车类型：%s\n",TrainType.c_str());
            break;
        }
        case 1: {
            sd1.appendBuffer("数据类型：LBJ扩展数据\n");
            sd1.appendBuffer("%-4s列车：%s次\n运行速度：%s KM/H\n线路名称：%s\n里程位置：%s KM\n",l.FX.c_str(),l.CheCi.c_str(),QDKG(l.speed).c_str(), QDKG(l.route_utf8).c_str(), QDKG(l.position).c_str());
            if(JiCheHao.length() > 0)
            {
                sd1.appendBuffer("本务机车：%s\n", JiCheHao.c_str()); 
                sd1.appendBuffer("注册号码：%s\n", QDKG(l.loco).c_str());
            }
            //sd1.appendBuffer("本务机车：%s\n机车注册号：%s\n",JiCheHao.c_str(),l.loco);
            sd1.appendBuffer("列车类型：%s\n",TrainType.c_str());
            std::string poslat = l.pos_lat;
            if (l.pos_lat_deg[0] && l.pos_lat_min[0]) {
                char buffer[20];
                sprintf(buffer, "%-3s°%s'", l.pos_lat_deg,l. pos_lat_min);
                poslat = buffer;
            }
            std::string poslon = l.pos_lon;
            if (l.pos_lon_deg[0] && l.pos_lon_min[0]) {
                char buffer[20];
                sprintf(buffer, "%-3s°%s'", l.pos_lon_deg, l.pos_lon_min);
                poslon = buffer;
            }
            sd1.appendBuffer("东　　经：%s\n", poslon.c_str());
            sd1.appendBuffer("北　　纬：%s\n", poslat.c_str());
            break;
        }
        case 2: {
            sd1.appendBuffer("数据类型：LBJ报时\n");
            sd1.appendBuffer("系统报时：%s\n", QDKG(l.time).c_str());
            break;
        }
    }

    sd1.appendBuffer("信号强度：%-6.1fdBm 　　信号质量：%s\n", r.rssi,XHZL(r.rssi).c_str());    
    sd1.appendBuffer("电池电压：%-6.2f伏特　　电量剩余：%d％\n",battery.readVoltage() * 2,JSDL(battery.readVoltage() * 2));
}
    sd1.appendBuffer("－－－－－－－－－－－－－－－－－－－－－－－－－\n");
    sd1.sendBufferLOG();
}

void printDataTelnet(PagerClient::pocsag_data *p, const struct lbj_data &l, const struct rx_info &r) {//Telnet数据

    telnet.printf("\r\n\r\n");
    telnet.println("－－－－－－－－－－－－－－－－－－－－－－－");
    telnet.printf("　　　　　　第 %5u 条记录\r\n", TelnetCount++);
    if (MaxCount==100 && TelnetCount==100)return;
    else {
     if(TelnetCount >= MaxCount)TelnetCount=1;//防止记录数溢出，即将到达最大值时复位1
    telnet.printf("　接收时间：%d年%02d月%02d日%02d时%02d分%02d秒\r\n", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,time_info.tm_hour, time_info.tm_min,time_info.tm_sec);//此处显示时间戳
    switch (l.type) {
        case 0: {
            telnet.println("　数据类型：LBJ基础数据");
            telnet.printf("　%-4s列车： %s 次\r\n　速　　度: %s KM/H\r\n　公 里 标: %s KM\r\n",l.FX.c_str(), QDKG(l.train).c_str(), QDKG(l.speed).c_str(), QDKG(l.position).c_str());
            telnet.printf("　列车类型：%s\r\n",TrainType.c_str());
            break;
        }
        case 1: {
            telnet.println("　数据类型：LBJ扩展数据");
            telnet.printf("　%-4s列车：%s次\r\n",l.FX.c_str(),l.CheCi.c_str());
            telnet.printf("　运行速度：%sKM/H\r\n",QDKG(l.speed).c_str());
            telnet.printf("　线路名称：%s\r\n", QDKG(l.route_utf8).c_str());
            telnet.printf("　里程位置：%sKM\r\n", QDKG(l.position).c_str());
            if(JiCheHao.length() > 0)
            {
                telnet.printf("　本务机车：%s\r\n", JiCheHao.c_str()); 
                telnet.printf("　注册号码：%s\r\n", QDKG(l.loco).c_str());
            }
            telnet.printf("　列车类型：%s\r\n",TrainType.c_str());
            std::string poslat = l.pos_lat;
            if (l.pos_lat_deg[0] && l.pos_lat_min[0]) {
                char buffer[20];
                sprintf(buffer, "%-3s度%s分", l.pos_lat_deg,l. pos_lat_min);
                poslat = buffer;
            }
            std::string poslon = l.pos_lon;
            if (l.pos_lon_deg[0] && l.pos_lon_min[0]) {
                char buffer[20];
                sprintf(buffer, "%-3s度%s分", l.pos_lon_deg, l.pos_lon_min);
                poslon = buffer;
            }
            telnet.printf("　东　　经：%s\r\n", poslon.c_str());
            telnet.printf("　北　　纬：%s\r\n", poslat.c_str());
            break;
        }
        
        case 2: {
            telnet.println("　数据类型：LBJ报时");
            telnet.printf("　系统报时：%s\r\n", l.time);
            break;
        }
    }
    telnet.printf("　信号强度：%-6.1f dBm\r\n　信号质量：%s\r\n", r.rssi,XHZL(r.rssi).c_str());
    int DLSY = JSDL(battery.readVoltage() * 2); // 计算电量百分比
    telnet.printf("　电池电压：%-6.2f伏特\r\n　大约剩余：%d％\r\n",battery.readVoltage()*2,DLSY);
}
    if (TelnetCount==1)
	{
	telnet.println("             【重要说明】");
    telnet.println(" 列车类型根据列车车次判断生成，结果仅供参考。");
    telnet.println(" 因《技规》已明确规定各路局管内车次具体范围，");
    telnet.println(" 故，各铁路局可能在某些情形下开行非标准车次，");
    telnet.println(" 对于非标准车次列车，其列车类型会与实际不符。");
	}

    if(r.rssi > -90){
        telnet.println(" ┌－－－－－－－－－－－－－－－－－┐");	//输出到TELNET
        telnet.println(" │　！　　　　　　　　　　　　　　　│");
        telnet.println(" │　　　　　　　警　　告　　　　　　│");
        telnet.println(" │　列车已经非常近，请务必注意安全！│");
        telnet.println(" │　　　　　请立即下道避车！　　　　│");
        telnet.println(" └－－－－－－－－－－－－－－－－－┘");
    }
    telnet.println("－－－－－－－－－－－－－－－－－－－－－－－");
}

void appendDataCSV(PagerClient::pocsag_data *p, const struct lbj_data &l, const struct rx_info &r) {
    char tiinfo[50];
//    sprintf(tiinfo, "%d-%02d-%02d,%02d:%02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    sprintf(tiinfo, "%d-%02d-%02d</th><th>%02d:%02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    String NullStr="";
    std::string poslat = l.pos_lat;
    if (l.pos_lat_deg[0] && l.pos_lat_min[0]) {
        char buffer[20];
        sprintf(buffer, "%3s°%s'", l.pos_lat_deg,l. pos_lat_min);
        poslat = buffer;
    }
    std::string poslon = l.pos_lon;
    if (l.pos_lon_deg[0] && l.pos_lon_min[0]) {
        char buffer[20];
        sprintf(buffer, "%3s°%s'", l.pos_lon_deg, l.pos_lon_min);
        poslon = buffer;
    }
    std::string posStr;
    //posStr = poslon + "," + poslat;
    posStr = poslon + "</th><th>" + poslat;
    //sd1.appendBufferCSV("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%6.1fdBm,%s,HEX:%s\n",tiinfo,l.time,(l.type==1?"扩展信息":(l.type==0?"基本信息":"报时")),l.FX.c_str(),l.CheCi.c_str(),l.speed,l.route_utf8,l.position,JiCheHao.c_str(),l.loco,TrainType.c_str(),posStr.c_str(),r.rssi,XHZL(r.rssi).c_str(),(l.type==1)?l.info2_hex.c_str():NullStr.c_str());
    sd1.appendBufferCSV("\n<tr style=\"display:none\"><th colspan=15 style=\"display:none\">　</th></tr>\n");
    sd1.appendBufferCSV("<tr onclick=\"alert('%s');\">",l.info2_hex.c_str());
    sd1.appendBufferCSV("<th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th><th> %s </th>",tiinfo,l.time,(l.type==1?"扩展信息":(l.type==0?"基本信息":"报时")),l.FX.c_str(),l.CheCi.c_str(),l.speed,l.route_utf8,l.position,JiCheHao.c_str(),l.loco);
    sd1.appendBufferCSV("<th> %s </th><th> %s </th><th> %6.1fdBm </th><th> %s </th></tr>\n",TrainType.c_str(),posStr.c_str(),r.rssi,XHZL(r.rssi).c_str());
    sd1.sendBufferCSV();
}

float getBias(float freq) {
    return (float) ((freq - TARGET_FREQ) * 1e6 / TARGET_FREQ);
}
