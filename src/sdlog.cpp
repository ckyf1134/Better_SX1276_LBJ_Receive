#include "sdlog.hpp"
#include "common.h"

SD_LOG::SD_LOG() :
        filesys{},
        log_count{},
        filename{},
        filename_csv{},
        sd_log(false),
        sd_csv(false),
        sd_cd(false),
        is_newfile(false),
        is_newfile_csv(false),
        is_startline(true),
        is_startline_csv(true),
        size_checked(false),
        log_directory{},
        csv_directory{} {}

void SD_LOG::setFS(fs::FS &fs) {
    filesys = &fs;
}

void SD_LOG::getFilename(const char *path) {
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            Serial.println("打开内存卡目录失败！");
            Serial.println("无法向内存卡写入日志。");
            sd_log = false;
            return;
        }
    }
    if (!cwd.isDirectory()) {
        Serial.println("内存卡日志目录错误！");
        sd_log = false;
        return;
    }
    char last[32];
    // read index
    int counter = readIndex(cwd);

    // while (cwd.openNextFile()) counter++;

    sprintf(last, "LBJ_LOG_%04d.txt", counter);
    String last_path = String(String(path) + "/" + String(last));
    if (!filesys->exists(last_path)) {
        sprintf(last, "LBJ_LOG_%04d.txt", counter);
        last_path = String(String(path) + "/" + String(last));
    }
    File last_log = filesys->open(last_path);

    if (last_log && last_log.size() <= MAX_LOG_SIZE) {
        sprintf(filename, "%s", last_log.name());
        log_count = counter + 1;
    } else {
        sprintf(filename, "LBJ_LOG_%04d.txt", counter);
        while (filesys->exists(String(String(path) + "/" + String(filename))) && counter < 9999) {
            sprintf(filename, "LBJ_LOG_%04d.txt", ++counter);
        }
        is_newfile = true;
        log_count = counter + 1;
        // update index
        updateIndex(String(path), counter);
    }
    sd_log = true;
}

void SD_LOG::getFilenameCSV(const char *path) {
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            Serial.println("无法向内存卡写入CSV汇总表");
            sd_csv = false;
            return;
        }
    }
    if (!cwd.isDirectory()) {
        Serial.println("CSV汇总表目录错误！");
        sd_csv = false;
        return;
    }
    char last[32];
    int counter = readIndex(cwd);

    //while (cwd.openNextFile())counter++;  //原有被注释掉的代码

    sprintf(last, "LBJ_LOG_%04d.htm", counter);
    String last_path = String(String(path) + "/" + String(last));
    if (!filesys->exists(last_path)) {
        sprintf(last, "LBJ_LOG_%04d.htm", counter);
        last_path = String(String(path) + "/" + String(last));
    }
    File last_csv = filesys->open(last_path);

    if (last_csv && last_csv.size() <= MAX_LOG_SIZE) {
        sprintf(filename_csv, "%s", last_csv.name());
    } else {
        sprintf(filename_csv, "LBJ_LOG_%04d.htm", counter);
        while (filesys->exists(String(String(path) + "/" + String(filename_csv))) && counter < 9999) {
            sprintf(filename_csv, "LBJ_LOG_%04d.htm", ++counter);
        }
        is_newfile_csv = true;
        updateIndex(String(path), counter);
    }
    sd_csv = true;
}

int SD_LOG::begin(const char *path) {
    log_directory = path;
    getFilename(path);
    if (!sd_log) return -1;
    log_path = String(String(path) + '/' + filename);
    log = filesys->open(log_path, "a", true);
    if (!log) {
        Serial.println("打开日志文件失败！");
        Serial.println("未向SD卡写入日志文件！");
        sd_log = false;
        return -1;
    }
    if (log.size()==0) writeHeader();
    sd_log = true;
    return 0;
}

int SD_LOG::beginCSV(const char *path) {
    csv_directory = path;
    getFilenameCSV(path);
    if (!sd_csv)
        return -1;
    csv_path = String(String(path) + '/' + filename_csv);
    csv = filesys->open(csv_path, "a", true);
    if (!csv) {
        Serial.println("打开CSV汇总表文件失败！");
        Serial.println("未向SD卡写入CSV汇总表文件！");
        sd_csv = false;
        return -1;
    }
    //writeHeaderCSV();
    if (csv.size() == 0) writeHeaderCSV();
    sd_csv = true;
    return 0;
}

void SD_LOG::writeHeader() {
    log.println("－－－－－－－－－－－－－－－－－－－－－－－－－");
    if (is_newfile) {
        log.println("       中国铁路列车接近预警器LBJ接收日志");
        log.printf("　　　　　　%s\n",Logo2);
        log.printf("　　系统版本：%s\n",SysVer);
        const char *ads = AdsStr;
        if (ads && ads[0] != '\0') {
            log.printf("　　%s\n",ads);
        }
        log.println("－－－－－－－－－－－－－－－－－－－－－－－－－");
    } else {
       }
    log.printf("\n%s固件启动成功；耗时 %llu 毫秒\n",SysVer, millis64());
    if (getLocalTime(&timein, 0)){
        log.printf("本日志文件创建时间：%d-%02d-%02d %02d:%02d:%02d\n",timein.tm_year + 1900, timein.tm_mon + 1, timein.tm_mday, timein.tm_hour, timein.tm_min,timein.tm_sec);
        log.println("　　　　　　　　【说　　　明】");
        log.println("列车类型根据列车车次信息判断生成，结果仅供参考。");
        log.println("因《技规》已明确规定各路局的管内车次的具体范围，故，各铁路局可能在某些情形下开行一些非标准车次，对于非标准车次的列车，其列车类型会与实际不相符。");}
    log.println("－－－－－－－－－－－－－－－－－－－－－－－－－");
    log.flush();
}

void SD_LOG::writeHeaderCSV() { // TODO: needs more confirmation about title.
    csv.close();
    csv = filesys->open(csv_path, "a");
    if (is_newfile_csv && csv.size() < 200) {
        // 注意：printf 的格式串里没有占位符时不要再传多余实参，否则参数被静默忽略，
        // 一旦将来往格式串里加了 %s 就会读到错误的内存。这里统一用字面量输出。
        csv.print("<html>\n<head>\n<title>列车预警接收日志</title>\n");
        csv.print("<style>table{border-right:1px solid #3dff3dff;border-bottom:1px solid #3dff3dff}\nth{border-left:1px solid #3dff3dff;border-top:1px solid #3dff3dff;font:12pt}\n");
        csv.print("td{border-left:1px solid #3dff3dff;border-top:1px solid #3dff3dff;font:9pt}\n\n</style>\n</head><body><center><h2 color=#ffa3a3>中国铁路列车接近预警器LBJ接收日志</h2>\n");
        csv.print("<BR>固件版本：");
        csv.printf("%s", SysVer);
        csv.print("\n");
        csv.print("<table>\n   <tr><th>系统日期</th><th>系统时间</th><th>LBJ时间</th><th>LBJ数据类型</th><th>方向</th><th>车次</th><th>速度</th><th>线路</th><th>公里标</th><th>机车号／车组号</th><th>机车注册号</th><th>列车类型</th><th>经度</th><th>纬度</th><th>信号强度<br>dBm</th><th>信号质量</th></tr>\n");
    }
    csv.close();
    csv = filesys->open(csv_path, "a");
}

void SD_LOG::appendCSV(const char *format, ...) { // TODO: maybe implement item based csv append?
    if (!sd_csv)  return;
    if (!filesys->exists(csv_path)) {
        Serial.println("CSV汇总表文件无效！");
        csv.close();
        log.close();
        SD.end();
        sd_csv = false;
        sd_log = false;
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE) { // && !size_checked
        csv.close();
        beginCSV(csv_directory);
    }
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (nullptr != strchr(format, '\n')) is_startline_csv = true;
    csv.print(buffer);
    csv.flush();
}

void SD_LOG::append(const char *format, ...) {
    if (!sd_log)  return;
    if (!filesys->exists(log_path)) {
        Serial.printf("预警日志文件 %s 不可用!\n", log_path.c_str());
        log.close();
        csv.close();
        SD.end();
        sd_log = false;
        sd_csv = false;
        return;
    }
    if (log.size() >= MAX_LOG_SIZE) {// && !size_checked
        log.close();
        begin(log_directory);
    }
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (nullptr != strchr(format, '\n'))
        is_startline = true;
    log.print(buffer);
    log.flush();
}

void SD_LOG::append(int level, const char *format, ...) {
    if (level > LOG_VERBOSITY) return;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer(buffer);
    sendBufferLOG();
}

void SD_LOG::appendBuffer(const char *format, ...) {
    if (!sd_log) return;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (nullptr != strchr(format, '\n')) is_startline = true;
    large_buffer += buffer;
}

void SD_LOG::appendBuffer(int level, const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer(buffer);
}

void SD_LOG::sendBufferLOG() {
    if (!sd_log) return;
    if (!filesys->exists(log_path)) {
        Serial.println("预警日志文件无效！");
        log.close();
        csv.close();
        SD.end();
        sd_log = false;
        sd_csv = false;
        return;
    }
    if (log.size() >= MAX_LOG_SIZE) { // && !size_checked
        log.close();
        begin(log_directory);
    }
    log.print(large_buffer);
    log.flush();
    large_buffer = "";
}

void SD_LOG::appendBufferCSV(const char *format, ...) {
    if (!sd_csv)
        return;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (nullptr != strchr(format, '\n')) is_startline_csv = true;
    large_buffer_csv += buffer;
}

void SD_LOG::sendBufferCSV() {
    if (!sd_csv) {
        return;
    }
    if (!filesys->exists(csv_path)) {
        Serial.println("CSV汇总表文件无效！");
        csv.close();
        log.close();
        SD.end();
        sd_csv = false;
        sd_log = false;
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE) {// && !size_checked
        csv.close();
        beginCSV(csv_directory);
    }
    csv.print(large_buffer_csv);
    csv.flush();
    large_buffer_csv = "";
}

File SD_LOG::logFile(char op) {
    log.close();
    switch (op) {
        case 'r': {
            log = filesys->open(log_path, "r");
            break;
        }
        case 'w': {
            log = filesys->open(log_path, "w");
            break;
        }
        case 'a': {
            log = filesys->open(log_path, "a");
            break;
        }
        default:
            log = filesys->open(log_path, "a");
    }
    return log;
}

void SD_LOG::printTel(unsigned int chars, ESPTelnet &tel) {
    log.close();
    uint32_t pos, left{};
    File log_r = filesys->open(log_path, "r");
    if (chars < log_r.size())
        pos = log_r.size() - chars;
    else {
        left = chars - log_r.size();
        pos = 0;
    }
    // Serial.println("LEFT = " + String(chars - log_r.size()));
    String line;
    if (!log_r.seek(pos))
        Serial.println("查找日志失败！");
    while (log_r.available()) {
        line = log_r.readStringUntil('\n');
        if (line) {
            tel.print(line);
            tel.print("\n");
        } else
            tel.printf("日志读取失败！\n");
    }
    if (left && log_count > 1) {
        // Serial.printf("查找最后的文件号%u\n", left);
        char last_file_name[32];
        sprintf(last_file_name, "LBJ_LOG_%04d.txt", log_count - 2);
        String log_last_path = String(log_directory) + '/' + String(last_file_name);
        Serial.println(log_last_path);
        log_r = filesys->open(log_last_path, "r");
        if (left < log_r.size())
            pos = log_r.size() - left;
        else
            pos = 0;
        if (!log_r.seek(pos)) {
            Serial.println("查找日志失败！");
        }
        while (log_r.available()) {
            line = log_r.readStringUntil('\n');
            if (line) {
                tel.print(line);
                tel.print("\n");
            } else
                tel.print("读取日志失败！\n");
        }
    }
    log = filesys->open(log_path, "a");
}

void SD_LOG::disableSizeCheck() {
    if (log.size() >= MAX_LOG_SIZE) {
        log.close();
        begin(log_directory);
    }
    if (csv.size() >= MAX_CSV_SIZE) {
        csv.close();
        beginCSV(csv_directory);
    }
    size_checked = true;
}

void SD_LOG::enableSizeCheck() {
    size_checked = false;
}

void SD_LOG::reopen() {
    log.close();
    log = filesys->open(log_path, "a");
}

bool SD_LOG::status() const {
    return sd_log;
}

int SD_LOG::beginCD(const char *path) {
    return 0;
}

void SD_LOG::appendCD(const uint8_t *data, size_t size) {

}

void SD_LOG::endCD() {
    sd_cd = false;
}

void SD_LOG::end() {
    if (!sd_log) return;
    log.close();
    csv.close();
    SD.end();
    sd_csv = false;
    sd_log = false;
}

void SD_LOG::reopenSD() {
    SD.begin(SDCARD_CS, SDSPI);
    // sd_csv = true;
    // sd_log = true;
}

static int parseIndexCounter(const String &line) {
    String value;
    int pos = line.indexOf("COUNTER=");
    if (pos >= 0) {
        value = line.substring(pos + 8);
    } else {
        pos = line.indexOf("文件共计：");
        if (pos >= 0) {
            value = line.substring(pos + 18); // 5 Chinese chars + '：'
        } else {
            pos = line.indexOf("文件总计:");
            if (pos >= 0) {
                value = line.substring(pos + 16); // 5 Chinese chars + ':'
            }
        }
    }
    value.trim();
    if (value.endsWith("个")) {
        value = value.substring(0, value.length() - 3);
    }
    if (value.length() == 0) {
        return -1;
    }
    for (unsigned int i = 0; i < value.length(); i++) {
        if (!isdigit(value[i])) {
            return -1;
        }
    }
    return value.toInt();
}

int SD_LOG::createIndex(File cwd, const String &index_path) {
    int count = 0;
    File entry;
    while ((entry = cwd.openNextFile())) {
        String name = entry.name();
        if (name.startsWith("LBJ_LOG_") && (name.endsWith(".txt") || name.endsWith(".htm"))) {
            if (++count >= 10000) {
                break;
            }
        }
    }
    int last_number = count > 0 ? count - 1 : 0;
    File index = filesys->open(index_path, FILE_WRITE);
    if (index) {
        index.printf("COUNTER=%d\n", last_number);
        index.close();
    }
    return last_number;
}

int SD_LOG::readIndex(const File &cwd) {
    String index_path = String(cwd.path()) + "/INDEX.txt";
    if (!filesys->exists(index_path)) {
        return createIndex(cwd, index_path);
    }
    File index = filesys->open(index_path, FILE_READ, false);
    if (!index) {
        return createIndex(cwd, index_path);
    }
    int counter = -1;
    while (index.available()) {
        String line = index.readStringUntil('\n');
        int parsed = parseIndexCounter(line);
        if (parsed >= 0) {
            counter = parsed;
            break;
        }
    }
    index.close();
    if (counter < 0) {
        return createIndex(cwd, index_path);
    }
    return counter;
}

void SD_LOG::updateIndex(const String &path, int counter) {
    String index_path = path + "/INDEX.txt";
    File index = filesys->open(index_path, FILE_WRITE);
    if (index) {
        index.printf("COUNTER=%d\n", counter);
        index.close();
    }
}
