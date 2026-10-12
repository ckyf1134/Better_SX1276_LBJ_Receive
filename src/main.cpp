#pragma execution_character_set("utf-8")
#include <RadioLib.h>
#include "networks.hpp"
#include "sdlog.hpp"
#include "customfont.h"
#include <esp_task_wdt.h>

#define WDT_TIMEOUT 20 // sec
#define FD_TASK_STACK_SIZE 16384
#define FD_TASK_TIMEOUT 750 // ms
#define FD_TASK_ATTEMPTS 3
#define LED_ON_TIME 200 // ms
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
const int pin = RADIO_BUSY_PIN;
float rssi_cache = 0;
extern bool have_cd;
bool shutdownFlag=false;//过期停机标志
float fers[32]{};
float actual_frequency = 0;
float freq_last = 0;
float car_fer_last = 0;
PagerClient pager(&radio);
uint64_t format_task_timer = 0;
uint64_t runtime_timer = 0;
uint64_t screen_timer = 0;
uint64_t led_timer = 0;
uint64_t timer4 = 0;
uint64_t prb_timer = 0;
uint64_t car_timer = 0;
uint32_t prb_count = 0;
uint32_t car_count = 0;
uint32_t ip_last = 0;
float ppm = INITIAL_PPM;

inline float actualFreq(float bias) {
    actual_frequency = (float) ((TARGET_FREQ * bias) / 1e6 + TARGET_FREQ);
    return actual_frequency;
}
bool freq_correction = AFC_ENABLE;
bool is_startline = true;
bool exec_init_f80 = false;
bool low_volt_warned = false;
bool oled_off = false;
bool give_tel_rssi = false;
bool give_tel_gain = false;
bool tel_set_ppm = false;
bool no_wifi = false;
bool have_cd = false;
bool telnet_online = false;
SD_LOG sd1;
struct rx_info rxInfo;
struct data_bond *db = nullptr;
int timeORip=0;
String ipa;
void formatDataTask(void *pVoid);

void simpleFormatTask();

void initFmtVars();

void handleSerialInput();

void handleCarrier();

void handlePreamble();

void revertFrequency();

void powerDown();

TaskHandle_t task_fd;
enum task_states {
    TASK_INIT = 0,
    TASK_CREATED = 1,
    TASK_RUNNING = 2,
    TASK_DONE = 3,
    TASK_TERMINATED = 4,
    TASK_CREATE_FAILED = 5,
    TASK_RUNNING_SCREEN = 6
};

task_states fd_state;

#ifdef HAS_DISPLAY
void pword(const char *msg, int xloc, int yloc) {
    int dspW = u8g2->getDisplayWidth();
    char glyph[2] = {0, 0};
    int strW = 0;
    for (const char *ptr = msg; *ptr; ++ptr) {
        glyph[0] = *ptr;
        int gw = u8g2->getStrWidth(glyph);
        if (xloc + strW + gw + 1 > dspW && strW > 0) {
            yloc += u8g2->getMaxCharHeight();
            xloc = 0;
            strW = 0;
        }
        xloc += u8g2->drawStr(xloc, yloc, glyph);
        strW += gw + 1;
    }
}

void showInitComp() {
    u8g2->clearBuffer();
    u8g2->setFont(FONT_12_GB2312);
    char buffer[32];
    if (!getLocalTime(&time_info, 0)){
        u8g2->setCursor(0, 12);
        u8g2->println("请连接WiFi网络SNTP校时"); }
    else {
         sprintf(buffer, "  %d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 12, buffer);
    }
    u8g2->sendBuffer();
}
// 修改 updateInfo 函数，显示电量百分比
void updateInfo() {
    // 更新banner

    u8g2->setFont(FONT_12_GB2312);
    char buffer[32];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 0, 128, 13);
    u8g2->setDrawColor(1);
    float voltage = battery.readVoltage() * 2;
    int batteryPercentage = JSDL(voltage); // 计算电量百分比

    if (!getLocalTime(&time_info, 0)) {
        u8g2->setCursor(0, 12);
        u8g2->printf("请连接网络校时 电%d%%", batteryPercentage);
    } else {
        if (WiFi.status() == WL_CONNECTED) {
            ipa = WiFi.localIP().toString();
            no_wifi = false;
        } else {
            ipa = "";
            WiFiType = "";
            no_wifi = true;
        }
        u8g2->setCursor(0, 12);
        if (++timeORip % 2 == 0) {
            if (!no_wifi) u8g2->printf("%s%-16s%d%%",WiFiType.c_str(), ipa.c_str(), batteryPercentage);
            else sprintf(buffer, "未连接WiFi网络  %d%%", batteryPercentage);
        } else {
            sprintf(buffer, "  %d-%02d-%02d %02d:%02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
        }
        u8g2->drawStr(0, 12, buffer);
    }

    if (batteryPercentage < 10 && !low_volt_warned) {
        Serial.printf("低电量警告，剩余电量%d％\n", batteryPercentage);
        sd1.append("低电量警告，剩余电量%d％\n", batteryPercentage);
        telnet.printf("┌－－－－－－－－－－－－－－－－－┐\r");
        telnet.printf("│ ！　 低电量警告，剩余电量%d％　　│\r", batteryPercentage);
        telnet.printf("└－－－－－－－－－－－－－－－－－┘\r");
        low_volt_warned = true;
    }
    u8g2->sendBuffer();
}

void showSTR(const String &str) {
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 0, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    pword(str.c_str(), 0, 19);
    u8g2->sendBuffer();
}
void showLBJ0(const struct lbj_data &l) {//LBJ基本信息，只有车次、速度、里程标、方向4项
    // box y 9->55
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 68);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setCursor(0, 24);
    u8g2->printf("%s列车：%s次",l.FX.c_str(),l.CheCi.c_str());
    u8g2->setCursor(0, 36);
    u8g2->printf("时速%s 里程%s", QDKG(l.speed).c_str(), QDKG(l.position).c_str());
    u8g2->setCursor(0, 48);
    if(rxInfo.rssi>=-90) u8g2->printf("信号强度：%3.1fdB 避车", rxInfo.rssi);
    else u8g2->printf("信号强度：%3.1f dB", rxInfo.rssi);
    u8g2->setCursor(10, 60);
    u8g2->printf("%02d:%02d:%02d  %12s",time_info.tm_hour,time_info.tm_min,time_info.tm_sec,TrainType.c_str());
    u8g2->setFont(FONT_12_GB2312);
    u8g2->sendBuffer();
}

void showLBJ1(const struct lbj_data &l) {//LBJ扩展信息 支持机车号和线路名
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 56);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setCursor(0, 24);// 第1行
    u8g2->printf("%s%s次 %sKM/h", l.FX.c_str(), l.CheCi.c_str(), QDKG(l.speed).c_str());
    u8g2->setCursor(0, 36);// 第2行
    if(rxInfo.rssi>=-90) u8g2->printf("%s%sKM 避车", QDKG(l.route_utf8).c_str(),QDKG(l.position).c_str());
    else u8g2->printf("%s %s公里", QDKG(l.route_utf8).c_str(),QDKG(l.position).c_str());
    u8g2->setCursor(0, 48); // 第3行
    u8g2->printf("%s",JiCheHao.c_str());
    u8g2->setDrawColor(0);//第4行
    u8g2->drawBox(0, 60, 128, 8);
    u8g2->setDrawColor(1);
    u8g2->setCursor(0, 60);
    u8g2->printf("%02d:%02d:%02d  %12s",time_info.tm_hour,time_info.tm_min,time_info.tm_sec,TrainType.c_str());
    u8g2->sendBuffer();
}


void showLBJ2(const struct lbj_data &l) {   //LBJ设备报时
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128,68);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setCursor(0, 32);
    if(rxInfo.rssi>=-90) u8g2->printf("机车报时：%s [避车]", l.time);
    else u8g2->printf("机车报时：%s ", l.time);
    u8g2->setCursor(0, 46);
    u8g2->printf("信号强度：%3.1f dB", rxInfo.rssi);
    u8g2->setCursor(0, 60);
    u8g2->printf("网络时间：%02d:%02d:%02d", time_info.tm_hour, time_info.tm_min,time_info.tm_sec);
    u8g2->sendBuffer();
}

#endif


void dualPrintf(bool time_stamp, const char *format, ...) {
    char buffer[256]; // 创建一个足够大的缓冲区来容纳格式化后的字符串
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);

    // 输出到 Serial
    //Serial.print(buffer);

    // 输出到 Telnet
    if (telnet_online) {
        if (is_startline) {
            telnet.print("\r  ");
            if (time_stamp && getLocalTime(&time_info, 1))
                telnet.printf("\r[%d-%02d-%02d][%02d:%02d:%02d]\r", time_info.tm_year + 1900, time_info.tm_mon + 1,time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer);
        if (nullptr != strchr(buffer, '\n')) {
            is_startline = true;
            telnet.print("\r");
        }
    }
}

void dualPrint(const char *fmt) {
    Serial.print(fmt);
    telnet.print(fmt);
}

void dualPrintln(const char *fmt) {
    Serial.println(fmt);
    telnet.println(fmt);
}

String printResetReason(esp_reset_reason_t reset) {
    String str;
    switch (reset) {
        case ESP_RST_UNKNOWN:
            str = "未知复位事件";
            break;
        case ESP_RST_POWERON:
            str = "复位按钮被按下";
            break;
        case ESP_RST_EXT:
            str = "外部引脚重置";
            break;
        case ESP_RST_SW:
            str = "软件重启";
            break;
        case ESP_RST_PANIC:
            str = "程序异常导致重置";
            break;
        case ESP_RST_INT_WDT:
            str = "中断监视器重置";
            break;
        case ESP_RST_TASK_WDT:
            str = "任务监视器重置";
            break;
        case ESP_RST_WDT:
            str = "其他监视器重置";
            break;
        case ESP_RST_DEEPSLEEP:
            str = "退出睡眠模式后重置";
            break;
        case ESP_RST_BROWNOUT:
            str = "断电复位";
            break;
        case ESP_RST_SDIO:
            str = "T-Flash卡读写重置";
            break;
    }
    return str;
}

void powerDown() {
    Serial.println("设备进入深度睡眠，需要重新上电恢复。");
    ESP.deepSleep(ESP.deepSleepMax());
}

void LBJTEST() {
    PagerClient::pocsag_data pocdat[16];
    pocdat[0].str = "37012";
    pocdat[0].addr = 1234000;
    pocdat[0].func = 1;
    pocdat[0].is_empty = false;
    pocdat[0].len = 15;
    pocdat[1].str = "30479100018530U)*9UU*6 (-(202011719040139058291000";
    pocdat[1].addr = 1234002;
    pocdat[1].func = 1;
    pocdat[1].is_empty = false;
    pocdat[1].len = 0;
//    Serial.println("[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ ");
    dualPrintf(false,"[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ \n");
    struct lbj_data lbj;

     db = new data_bond; 
     db->pocsagData[0].addr = 1234000;
     db->pocsagData[0].str = "37012  15  1504";
     db->pocsagData[0].func = 1;
     db->pocsagData[0].is_empty = false;
     db->pocsagData[0].len = 15;
     db->pocsagData[1].str = "20202350018530U)*9UU*6 (-(202011719040139058291000";
     db->pocsagData[1].addr = 1234002;
     db->pocsagData[1].func = 1;
     db->pocsagData[1].is_empty = false;
     db->pocsagData[1].len = 0;
    readDataLBJ(pocdat, &lbj);
    printDataSerial(pocdat, lbj, rxInfo);
    appendDataLog(pocdat, lbj, rxInfo);
    printDataTelnet(pocdat, lbj, rxInfo);
    simpleFormatTask();
    rxInfo.rssi = 0;
    rxInfo.fer = 0;
    delete db;
}

int initPager() {//初始化SX1276默认值
    int state = radio.beginFSK(actualFreq(ppm), 4.8, 5.0, 12.5);
    RADIOLIB_ASSERT(state)
    state = radio.setGain(1);
    RADIOLIB_ASSERT(state)
    state = pager.begin(actualFreq(ppm), 1200, false, 2500);
    RADIOLIB_ASSERT(state)
    freq_last = actual_frequency;
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    RADIOLIB_ASSERT(state)
    return (state);
}
//endregion


// 主函数
void setup() {
    Serial.begin(115200);
    runtime_timer = millis64();
    esp_reset_reason_t reset_reason = esp_reset_reason();
    initBoard();
    sd1.setFS(SD);
    delay(1000);
    sntp_set_time_sync_notification_cb(timeAvailable);
    sntp_servermode_dhcp(1);
    configTzTime(time_zone,ntpServer0, ntpServer1, ntpServer2);

#ifdef HAS_RTC
    // rtc.begin();
    // rtc.getDateTime(time_info);
    time_info = rtcLibtoC(rtc.now());
    Serial.println(&time_info, "RTC时间：%Y-%m-%d %H:%M:%S ");
    timeSync(time_info); // sync system time from rtc
    Serial.printf("系统时间：%s\n", fmtime(time_info));
#endif

    //Serial.printf("RST: %s\n", printResetReason(reset_reason).c_str());
    if (have_sd) {
        sd1.begin("/LBJ_LOG");
        sd1.beginCSV("/LBJ_LOG");
        sd1.append("复位原因 %s\n", printResetReason(reset_reason).c_str());
#ifdef HAS_RTC
        sd1.append("RTC时间 %d-%02d-%02d %02d:%02d:%02d\n", time_info.tm_year + 1900, time_info.tm_mon + 1,
                   time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
#endif
    }

    if (u8g2) {
        showInitComp();
        u8g2->setFont(FONT_12_GB2312);
        u8g2->setCursor(0, 24);
        u8g2->printf("%s\n",Logo1);
        u8g2->setCursor(0, 38);
        u8g2->printf("%s\n",Logo2);
        u8g2->setCursor(0, 52);
        u8g2->printf("%s\n","    正在初始化……");
        u8g2->sendBuffer();
    }

    // Initialize SX1276
    dualPrintln("正在初始化通信芯片...\n");
    int state = initPager();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("初始化完毕"));
        Serial.printf("实际频率 %f MHz, ppm %.1f\n", actualFreq(ppm), ppm);
    } else {
        Serial.print(F("失败代码"));
        Serial.println(state);
        while (true);
    }

    // WiFi is connected in the background, Telnet starts in loop() once ready.
    connectWiFi();
    if (isConnected()) {
        ip = WiFi.localIP();
        Serial.println();
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet();
    } else {
        Serial.println("WiFi 将在后台自动连接，Telnet 服务就绪后启动。");
    }

//    if(WiFi.getSleep())
//        Serial.println("WIFI Sleep enabled.");

    // start thread watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(nullptr);
    // wdt_timer = millis64();

    digitalWrite(BOARD_LED, LED_OFF);
    Serial.printf("启动用时 %llu ms\n", millis64() - runtime_timer);
    sd1.append("启动用时 %llu ms\n", millis64() - runtime_timer);
    runtime_timer = 0;


    if (u8g2) {
        u8g2->setFont(FONT_12_GB2312);
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 42, 128, 14);
        u8g2->setDrawColor(1);
        u8g2->setCursor(0, 50);
        u8g2->printf("   版本：%s\n", SysVer);
        u8g2->setCursor(0, 62);
        u8g2->printf("   设备正在工作中…\n");
        u8g2->sendBuffer();
        Serial.printf("内存剩余: %d 字节\n", esp_get_free_heap_size());
    }
}

// Loop functions
void handleTelnetCall() {
    if (give_tel_rssi) {
        telnet.printf("> 接收信号强度（RSSI） %3.2f dBm.\n", radio.getRSSI(false, true));
        give_tel_rssi = false;
        telnet.print("< ");
    }
    if (give_tel_gain) {
        telnet.printf("> 增益 %d \n", radio.getGain());
        give_tel_gain = false;
        telnet.print("< ");
    }
    if (tel_set_ppm) {
        int16_t state = radio.setFrequency(actualFreq(ppm));
        if (state == RADIOLIB_ERR_NONE) {
            telnet.printf("> 实际频率 %f MHz\n", actualFreq(ppm));
            Serial.printf("[Telnet] > 实际频率 %f MHz\n", actualFreq(ppm));
        } else {
            telnet.printf(">  失败代码：  %d\n", state);
            Serial.printf("[Telnet] > 失败代码： %d\n", state);
        }
        telnet.printf("> ppm 设置为 %.f\n", ppm);
        tel_set_ppm = false;
        telnet.print("< ");
    }
}

void handleSync() {
    if (pager.gotSyncState()) {
        if (rxInfo.cnt < 5 && (rxInfo.timer == 0 || esp_timer_get_time() - rxInfo.timer < 11000)) {
            float rssi = radio.getRSSI(false, true);
            rxInfo.timer = esp_timer_get_time();
            rssi_cache += rssi;
            rxInfo.cnt++;
        }
        if (rxInfo.fer == 0) rxInfo.fer = radio.getFrequencyError();
    }
}


void handleTelnet() {
    if (isConnected() && !telnet_online) {
        ip = WiFi.localIP();
        Serial.printf("连接到WIFI： %s 成功.\n", WiFiType);
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet();
    }
    telnet.loop();
}

void checkNetwork() {
    static uint32_t last_network_check = 0;
    if (millis64() - last_network_check < 1000) {
        return;
    }
    last_network_check = millis64();
    IPAddress current_ip = WiFi.localIP();
    uint32_t ipv4 = (uint32_t) current_ip;
    if (ip_last != ipv4) {
        Serial.print("列车接近预警器IP地址：");
        Serial.print(current_ip);
        Serial.print("\n");
        ip_last = ipv4;
    }
    // 试用期检查不需要每圈循环都做，每分钟检查一次即可。
    static uint32_t last_expiry_check = 0;
    if (millis64() - last_expiry_check < 60000) {
        return;
    }
    last_expiry_check = millis64();
    if (!isConnected() || !getLocalTime(&time_info, 0)) {
        return;
    }
    //判断是否试用版，是否过期
    char DateStr[12];
    sprintf(DateStr, "%d-%02d-%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday);
    // 将字符串转换为时间值进行比较
    struct tm valDate_tm = {};
    strptime(ValDate, "%Y-%m-%d", &valDate_tm);
    time_t valTime = mktime(&valDate_tm);
    struct tm currentDate_tm = {};
    strptime(DateStr, "%Y-%m-%d", &currentDate_tm);
    time_t currentTime = mktime(&currentDate_tm);
    // 比较时间值而非字符串
    if (VerFlag == "test" && currentTime >= valTime && !shutdownFlag) {
        shutdownFlag = true;
#ifdef HAS_DISPLAY
        u8g2->clearBuffer();
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 0, 128, 64);
        u8g2->setDrawColor(1);
        u8g2->setFont(FONT_12_GB2312);
        u8g2->setCursor(0, 12);
        u8g2->println("当前固件为测试版");
        u8g2->setCursor(0, 26);
        u8g2->println("试用期已过，停止使用");
        u8g2->setCursor(0, 40);
        u8g2->println("请更新至正式版");
#endif
  
        dualPrintln("┌－－－－－－－－－－－－－－－－－┐");
        dualPrintln("│　　　当前固件为测试版　　　　　　│");
        dualPrintln("│现已超过试用期，已停用全部网络功能│");
        dualPrintln("│　　　　请更新至正式版　　　　　　│");
        dualPrintln("└－－－－－－－－－－－－－－－－－┘");

        sd1.append("┌－－－－－－－－－－－－－－－－－┐\r\n");
        sd1.append("│　　　当前固件为测试版　　　　　　│\r\n");
        sd1.append("│现已超过试用期，已停用全部网络功能│\r\n");
        sd1.append("│　　　　请更新至正式版　　　　　　│\r\n");
        sd1.append("└－－－－－－－－－－－－－－－－－┘\r\n");
#ifdef HAS_DISPLAY
        updateInfo();
#endif
        delay(2000);
        esp_task_wdt_delete(nullptr); // 删除当前任务的WDT
        powerDown();
    }
}

// LOOP
void loop() {
    if(shutdownFlag)return; // 如果是试用版过期，停止运行
    connectWiFi();
    esp_task_wdt_reset();
    if (car_timer != 0 && millis64() - car_timer > 700 && prb_timer == 0 && rxInfo.timer == 0) {
        car_count = 0;
        revertFrequency();
        car_fer_last = 0;
        car_timer = 0;
        //Serial.println("载波信号超时未连接");
    }

    if (prb_timer != 0 && millis64() - prb_timer > 600 && rxInfo.timer == 0) {
        prb_count = 0;
        revertFrequency();
        for (auto &i: fers) {
            i = 0;
        }
        prb_timer = 0;
        //Serial.println("未能成功接收到数据包");
    }

    // if task complete, de-initialize
    if (fd_state == TASK_DONE) {
        if (task_fd != nullptr) {
            vTaskDelete(task_fd);
            task_fd = nullptr;
        }
        initFmtVars();
        fd_state = TASK_INIT;
        format_task_timer = 0;
    } else if (fd_state == TASK_CREATE_FAILED) {
        initFmtVars();
        // changeCpuFreq(240);
        format_task_timer = 0;
        fd_state = TASK_INIT;
    }

    if (millis64() - led_timer > LED_ON_TIME && led_timer != 0 && fd_state == TASK_INIT) {
        digitalWrite(BOARD_LED, LED_OFF);
        led_timer = 0;
        changeCpuFreq(240);
    }

    handleSerialInput();
    checkNetwork();
    handleTelnet();
    handleTelnetCall();

    if (millis64() > 60000 && format_task_timer == 0 && !exec_init_f80) //待机1分钟后，CPU频率降到80MHz
    {
        setCpuFrequencyMhz(80);
        exec_init_f80 = true;
    }
#ifdef HAS_DISPLAY
    //主屏OLED显示
    if (screen_timer == 0) screen_timer = millis64();
    else if (millis64() - screen_timer > 1000) { // 1秒一刷校时
#ifdef HAS_OLED_TIMEOUT
        if (!oled_off)oled_off = true;
#endif
        updateInfo();
        screen_timer = millis64();
    }
#ifdef HAS_OLED_TIMEOUT
    if (millis64() - timer4 >= OLED_TIMEOUT && timer4 != 0 && !oled_off) {
        u8g2->clearBuffer();
        oled_off = true;
        u8g2->setPowerSave(true);
    }
#endif

#endif
     /*if (millis64()%5000 == 0){
         sd1.append("[D] 当前运行时间 %lu ms.\n",millis64());
         sd1.append("[D] 测试输出：\n");
         LBJTEST();
     }*/

     if (millis64() - format_task_timer >= 200 && format_task_timer != 0) {
       //Serial.printf("LED LOW [%llu]\n", millis64() - format_task_timer);
         digitalWrite(BOARD_LED, LED_OFF);
         if (fd_state == TASK_DONE || fd_state == TASK_INIT) format_task_timer = 0;
     }
    if (millis64() - format_task_timer >= FD_TASK_TIMEOUT && (fd_state == TASK_RUNNING || fd_state == TASK_CREATED)
        && task_fd != nullptr && format_task_timer != 0) {
        vTaskDelete(task_fd);
        task_fd = nullptr;
        initFmtVars();
        //Serial.printf("LED LOW [%llu]\n", millis64() - format_task_timer);
        digitalWrite(BOARD_LED, LED_OFF);
        format_task_timer = 0;
        led_timer = 0;
        changeCpuFreq(240);
        fd_state = TASK_INIT;
    }
    

    if (millis64() - timer4 >= 60000 && timer4 != 0 && ets_get_cpu_frequency() != 80)changeCpuFreq(80);

    handleCarrier();
    handlePreamble();

    handleSync();

    // the number of batches to wait for
    // 2 batches will usually be enough to fit short and medium messages
    if (pager.available() >= 2 && fd_state == TASK_INIT) { 
        setCpuFrequencyMhz(240);
        db = new data_bond;
        runtime_timer = millis64();
        timer4 = millis64();
        int state = pager.readDataMSA(db->pocsagData, 0);
//        sd1.append("[PHY-LAYER][D] AVAILABLE > 2.\n");
        rxInfo.rssi = rssi_cache / (float) rxInfo.cnt;
        rssi_cache = 0;
        rxInfo.cnt = 0;
        rxInfo.timer = 0;
        prb_timer = 0;
        car_timer = 0;
        if (prb_count >= 32) prb_count = 31;
        if (prb_count > 0) rxInfo.fer = fers[prb_count - 1];
        for (int i = 0; i < prb_count; ++i) fers[i] = 0;
        prb_count = 0;
        car_count = 0;
        car_fer_last = 0;
        rxInfo.ppm = getBias(actual_frequency);

        if (state == RADIOLIB_ERR_NONE) {
            freq_last = actual_frequency;
           // Serial.printf("解析成功！\n");
            digitalWrite(BOARD_LED, LED_ON);
            format_task_timer = millis64();
            led_timer = millis64();

           // sd1.append(2, "正在格式化输出...\n");
            // formatDataTask();
            auto x_ret = xTaskCreatePinnedToCore(formatDataTask, "task_fd",FD_TASK_STACK_SIZE, nullptr,2, &task_fd, ARDUINO_RUNNING_CORE);
            if (x_ret == pdPASS) {
                fd_state = TASK_CREATED;
                delay(1);
            }
            else if (x_ret == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {
                int x_ret1;
                for (int i = 0; i < FD_TASK_ATTEMPTS; ++i) {
                    x_ret1 = xTaskCreatePinnedToCore(formatDataTask, "task_fd",FD_TASK_STACK_SIZE, nullptr,2, &task_fd, ARDUINO_RUNNING_CORE);
                    if (x_ret1 == pdPASS) {
                        fd_state = TASK_CREATED;
                        delay(1);
                        break;
                    }
                }
                if (x_ret1 != pdPASS) {
                    fd_state = TASK_CREATE_FAILED;
                    simpleFormatTask();
                    digitalWrite(BOARD_LED, LED_OFF);
                }
            }
            else {
                fd_state = TASK_CREATE_FAILED;
                digitalWrite(BOARD_LED, LED_OFF);
            }

        }
        else if (state == RADIOLIB_ERR_MSG_CORRUPT) revertFrequency();
        if (fd_state == TASK_INIT)initFmtVars();
        else if (fd_state == TASK_CREATED && task_fd == nullptr) fd_state = TASK_DONE;

    }
}

void revertFrequency() {
    if (actual_frequency != freq_last) {
        actual_frequency = freq_last;
        (void) radio.setFrequency(actual_frequency);
    }
}

void handleCarrier() {
    if (pager.gotCarrierState() && !pager.gotPreambleState() && !pager.gotSyncState() && freq_correction &&
        prb_timer == 0) {
        if (car_count == 0)
            car_timer = millis64();
        ++car_count;
        if (car_count < 64) {
            float fei = radio.getFrequencyError();
            if (abs(fei) > 1000.0 && car_count != 1 &&
                abs(fei - car_fer_last) < 500) {
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fei * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state == RADIOLIB_ERR_NONE)actual_frequency = target_freq;
            }
            car_fer_last = fei;
        }
    }
}

void handlePreamble() {
    if (pager.gotPreambleState() && !pager.gotSyncState() && freq_correction) {
        if (prb_count == 0) prb_timer = millis64();
        ++prb_count;
        if (prb_count < 32) {
            fers[prb_count - 1] = radio.getFrequencyError();
            if (abs(fers[prb_count - 1]) > 1000.0 && prb_count != 1 &&
                abs(fers[prb_count - 1] - fers[prb_count - 2]) < 500) {
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fers[prb_count - 1] * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state == RADIOLIB_ERR_NONE) actual_frequency = target_freq;

            }
        }
    }
}

void getCoreFreq(void *pVoid) {
   // Serial.printf("Core %d Frequency %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
    vTaskDelete(nullptr);
}

void handleSerialInput() {
    if (Serial.available()) {
        String in = Serial.readStringUntil('\n');
        in.trim();
        if (in == "ping")
            Serial.println("$ Pong");
        else if (in == "task state")
            Serial.println("$ Task state " + String(fd_state));
        else if (in == "rtc") {
#ifdef HAS_RTC
            // rtc.getDateTime(time_info);
            // DateTime now = rtc.now();
            time_info = rtcLibtoC(rtc.now());
            float temp = rtc.getTemperature();
           // Serial.print(&time_info, "$ [eRTC] %Y-%m-%d %H:%M:%S ");
            //Serial.printf("Temp: %.2f °C\n", temp);
#endif
        } else if (in == "time") {
            getLocalTime(&time_info, 1);
            Serial.printf("$ 系统时间 %s, Up time %llu ms (%s)\n", fmtime(time_info), millis64(), fmtms(millis64()));
        } 
        else if (in == "sd end") {
            if (!sd1.status())
                Serial.println("$ 没有内存卡.");
            else {
                sd1.append("SD卡将被卸载\n");
                sd1.end();
            }
        } else if (in == "sd begin") {
                SD_LOG::reopenSD();
                sd1.begin("/LBJ_LOG");
                sd1.beginCSV("/LBJ_LOG");
                sd1.append("SD卡已重新挂载\n");
                Serial.println("SD卡已重新打开。");
        } else if (in == "mem") {
            Serial.printf("内存剩余: %d 字节\n", esp_get_free_heap_size());
        } else if (in == "rst") {
            esp_reset_reason_t reason = esp_reset_reason();
            Serial.printf("$ 重置: %s\n", printResetReason(reason).c_str());
        } 
          else if (in == "ppm read") {
            Serial.printf("$ ppm %.1f\n", ppm);
        } else if (in == "afc off") {
            prb_count = 0;
            prb_timer = 0;
            car_count = 0;
            car_timer = 0;
            freq_correction = false;
            Serial.println("$ 频率校正已禁用");
        } else if (in == "afc on") {
            freq_correction = true;
            Serial.println("$ 频率校正已启用");
        } else if (in == "rssi") {
            Serial.printf("$ 信号强度 %3.2f dBm.\n", radio.getRSSI(false, true));
        } else if (in == "gain") {
            Serial.printf("$ Gain Pos %d \n", radio.getGain());
        } else if (in == "cpu") {
            xTaskCreatePinnedToCore(getCoreFreq, "get_freq", 2048, nullptr,
                                    1, nullptr, 0);
            Serial.printf("核心 %d 频率 %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
        }
    }
}

void initFmtVars() {
    runtime_timer = 0;
    rxInfo.rssi = 0;
    rxInfo.fer = 0;
    rxInfo.ppm = 0;
    if (db != nullptr) {
        delete db;
        db = nullptr;
    }
}

void formatDataTask(void *pVoid) {
    fd_state = TASK_RUNNING;
    for (auto &i: db->pocsagData) {
        if (i.is_empty)continue;
        db->str = db->str + "  " + i.str;
    }


    //-------------------------------------------关键功能性函数调用开始-------------------------------------------------
    readDataLBJ(db->pocsagData, &db->lbjData);          // 读取数据到LBJ数据结构
    printDataSerial(db->pocsagData, db->lbjData, rxInfo);    // 输出数据到串口
    appendDataLog(db->pocsagData, db->lbjData, rxInfo);     // 输出数据到日志文件
    printDataTelnet(db->pocsagData, db->lbjData, rxInfo);    // 输出数据到Telnet
    //-------------------------------------------关键功能性函数调用结束--------------------------------------------------

#ifdef HAS_DISPLAY
    if(NearAlarm && rxInfo.rssi > -90)//把接近警告输出到主屏
    {
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 12, 128, 50);
        u8g2->setDrawColor(1);
        u8g2->setCursor(0, 24);
        u8g2->setFont(FONT_12_GB2312);
        u8g2->printf("       警    告\n");
        u8g2->setCursor(0, 38);
        u8g2->printf("  列车接近，注意安全！\n");
        u8g2->setCursor(0, 52);
        u8g2->printf("    请立即下道避车！\n");
    }
#endif

#ifdef HAS_DISPLAY
    fd_state = TASK_RUNNING_SCREEN;
    if (u8g2) {
#ifdef HAS_OLED_TIMEOUT
        if (oled_off) {
            oled_off = false;
            u8g2->setPowerSave(false);
            u8g2->clearBuffer();
            updateInfo();
        }
#endif
        if (db->lbjData.type == 0)    showLBJ0(db->lbjData);
        else if (db->lbjData.type == 1) showLBJ1(db->lbjData);
        else if (db->lbjData.type == 2) showLBJ2(db->lbjData);
        
    }
#endif

    //delay(500);// 延时500毫秒，错峰输出到SD卡
    appendDataCSV(db->pocsagData, db->lbjData, rxInfo); // 输出数据到CSV汇总表
    fd_state = TASK_DONE;
    task_fd = nullptr;
    vTaskDelete(nullptr);
}

void simpleFormatTask() { // 简单格式化输出任务
    for (auto &i: db->pocsagData) {
        if (i.is_empty)continue;
        db->str += String(i.addr) + "/" + String(i.func) + ":" + i.str + "\n ";
    }
#ifdef HAS_OLED_TIMEOUT
    if (oled_off) {
        oled_off = false;
        u8g2->setPowerSave(false);
        u8g2->clearBuffer();
        updateInfo();
    }
#endif
#ifdef HAS_DISPLAY
    showSTR(db->str);
#endif
}
