#include "nvs_config.h"
#include "config.h"
#include <Preferences.h>

// ============================================================
//  NVS 存储实例（内部私有）
// ============================================================
static Preferences s_prefs;

// NVS 命名空间与键名常量，集中管理避免拼写错误
static const char NVS_NAMESPACE[]       = "camera_cfg";
static const char NVS_KEY_SSID[]        = "sta_ssid";
static const char NVS_KEY_PASS[]        = "sta_pass";
static const char NVS_KEY_WARMUP[]      = "warmup_sec";
static const char NVS_KEY_BRIGHT_THRESH[] = "bright_thresh";
static const char NVS_KEY_NAME[]        = "sta_name";
static const char NVS_KEY_BROKER[]      = "mqtt_broker";
static const char NVS_KEY_PORT[]        = "mqtt_port";
// 突变检测参数键名（均已控制在 15 字节内）
static const char NVS_KEY_MUT_EN[]      = "mut_enable";
static const char NVS_KEY_MUT_INTVL[]   = "mut_interval";
static const char NVS_KEY_MUT_THRESH[]  = "mut_thresh";
static const char NVS_KEY_MUT_MBLK[]   = "mut_min_blk";
static const char NVS_KEY_BUILD_TIME[]  = "bld_time";

// ============================================================
//  配置项内存缓存（内部私有）
// ============================================================
static String s_sta_ssid        = "";
static String s_sta_password    = "";
static float  s_warmup_sec      = 0.8f;
static int    s_bright_thresh   = AMBIENT_BRIGHTNESS_THRESHOLD;
static String s_sta_name        = "";
static String s_mqtt_broker     = "";
static int    s_mqtt_port       = 1883;
// 突变检测参数缓存
static bool   s_mut_enable        = true;
static int    s_mut_interval_sec  = 10;
static float  s_mut_block_thresh  = 0.15f;
static int    s_mut_min_blocks    = 3;

// ============================================================
//  恢复出厂默认值并写入 NVS
// ============================================================
void nvs_reset_to_factory_defaults() {
    Serial.println("[NvsConfig] Resetting and writing factory defaults to NVS...");

    s_sta_ssid         = FACTORY_WIFI_SSID;
    s_sta_password     = FACTORY_WIFI_PASSWORD;
    s_warmup_sec       = 0.8f;
    s_bright_thresh    = AMBIENT_BRIGHTNESS_THRESHOLD;
    s_sta_name         = FACTORY_DEVICE_NAME;
    s_mqtt_broker      = FACTORY_MQTT_BROKER;
    s_mqtt_port        = FACTORY_MQTT_PORT;
    s_mut_enable       = true;
    s_mut_interval_sec = 10;
    s_mut_block_thresh = 0.15f;
    s_mut_min_blocks   = 3;

    s_prefs.begin(NVS_NAMESPACE, false); // 读写模式
    s_prefs.putString(NVS_KEY_SSID,          s_sta_ssid);
    s_prefs.putString(NVS_KEY_PASS,          s_sta_password);
    s_prefs.putFloat(NVS_KEY_WARMUP,         s_warmup_sec);
    s_prefs.putInt(NVS_KEY_BRIGHT_THRESH,    s_bright_thresh);
    s_prefs.putString(NVS_KEY_NAME,          s_sta_name);
    s_prefs.putString(NVS_KEY_BROKER,        s_mqtt_broker);
    s_prefs.putInt(NVS_KEY_PORT,             s_mqtt_port);
    s_prefs.putBool(NVS_KEY_MUT_EN,          s_mut_enable);
    s_prefs.putInt(NVS_KEY_MUT_INTVL,        s_mut_interval_sec);
    s_prefs.putFloat(NVS_KEY_MUT_THRESH,     s_mut_block_thresh);
    s_prefs.putInt(NVS_KEY_MUT_MBLK,         s_mut_min_blocks);
    s_prefs.putString(NVS_KEY_BUILD_TIME,    __DATE__ " " __TIME__);
    s_prefs.end();

    Serial.println("[NvsConfig] Factory defaults successfully written to NVS.");
}

// ============================================================
//  NVS 初始化
// ============================================================
void nvs_config_init() {
    s_prefs.begin(NVS_NAMESPACE, false);

    const char current_build[] = __DATE__ " " __TIME__;
    String saved_build = s_prefs.getString(NVS_KEY_BUILD_TIME, "");

    if (saved_build != current_build) {
        Serial.printf("[NvsConfig] New firmware build detected (Saved: '%s' vs Current: '%s').\n",
                      saved_build.c_str(), current_build);
        s_prefs.end();
        nvs_reset_to_factory_defaults();
    } else {
        Serial.printf("[NvsConfig] Firmware build unchanged ('%s'). Retaining existing config.\n",
                      current_build);
        s_sta_ssid         = s_prefs.getString(NVS_KEY_SSID,          FACTORY_WIFI_SSID);
        s_sta_password     = s_prefs.getString(NVS_KEY_PASS,          FACTORY_WIFI_PASSWORD);
        s_warmup_sec       = s_prefs.getFloat(NVS_KEY_WARMUP,        0.8f);
        s_bright_thresh    = s_prefs.getInt(NVS_KEY_BRIGHT_THRESH,    AMBIENT_BRIGHTNESS_THRESHOLD);
        s_sta_name         = s_prefs.getString(NVS_KEY_NAME,          FACTORY_DEVICE_NAME);
        s_mqtt_broker      = s_prefs.getString(NVS_KEY_BROKER,   FACTORY_MQTT_BROKER);
        s_mqtt_port        = s_prefs.getInt(NVS_KEY_PORT,        FACTORY_MQTT_PORT);
        s_mut_enable       = s_prefs.getBool(NVS_KEY_MUT_EN,    true);
        s_mut_interval_sec = s_prefs.getInt(NVS_KEY_MUT_INTVL,  10);
        s_mut_block_thresh = s_prefs.getFloat(NVS_KEY_MUT_THRESH, 0.15f);
        s_mut_min_blocks   = s_prefs.getInt(NVS_KEY_MUT_MBLK,   3);
        s_prefs.end();
    }

    Serial.printf("[NvsConfig] Loaded SSID: %s, Station: %s, Broker: %s:%d, Mutation: %s\n",
                  s_sta_ssid.c_str(), s_sta_name.c_str(),
                  s_mqtt_broker.c_str(), s_mqtt_port,
                  s_mut_enable ? "ON" : "OFF");
}

// ============================================================
//  Getter 实现（函数声明在 web_config.h）
// ============================================================
String get_sta_ssid()     { return s_sta_ssid; }
String get_sta_password() { return s_sta_password; }
float  get_warmup_sec()   { return s_warmup_sec; }
String get_station_name() { return s_sta_name; }
String get_mqtt_broker()  { return s_mqtt_broker; }
int    get_mqtt_port()    { return s_mqtt_port; }
// 突变检测参数 Getter
bool  get_mutation_enable()       { return s_mut_enable; }
int   get_mutation_interval_sec() { return s_mut_interval_sec; }
float get_mutation_block_thresh() { return s_mut_block_thresh; }
int   get_mutation_min_blocks()   { return s_mut_min_blocks; }

// ============================================================
//  Setter 实现（含变化检测 + NVS 写入）
// ============================================================
bool nvs_set_sta_ssid(const String& val) {
    if (val.length() == 0 || val == s_sta_ssid) return false;
    s_sta_ssid = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putString(NVS_KEY_SSID, val);
    s_prefs.end();
    return true;
}

bool nvs_set_sta_password(const String& val) {
    if (val == s_sta_password) return false;
    s_sta_password = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putString(NVS_KEY_PASS, val);
    s_prefs.end();
    return true;
}

bool nvs_set_warmup_sec(float val) {
    if (val < 0.1f || val > 10.0f) return false;
    if (val == s_warmup_sec) return false;
    s_warmup_sec = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putFloat(NVS_KEY_WARMUP, val);
    s_prefs.end();
    return true;
}

bool nvs_set_brightness_thresh(int val) {
    if (val < 0 || val > 255 || val == s_bright_thresh) return false;
    s_bright_thresh = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putInt(NVS_KEY_BRIGHT_THRESH, val);
    s_prefs.end();
    return true;
}

int get_brightness_thresh() {
    return s_bright_thresh;
}

bool nvs_set_station_name(const String& val) {
    if (val.length() == 0 || val == s_sta_name) return false;
    s_sta_name = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putString(NVS_KEY_NAME, val);
    s_prefs.end();
    return true;
}

bool nvs_set_mqtt_broker(const String& val) {
    if (val.length() == 0 || val == s_mqtt_broker) return false;
    s_mqtt_broker = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putString(NVS_KEY_BROKER, val);
    s_prefs.end();
    return true;
}

bool nvs_set_mqtt_port(int val) {
    if (val <= 0 || val == s_mqtt_port) return false;
    s_mqtt_port = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putInt(NVS_KEY_PORT, val);
    s_prefs.end();
    return true;
}

// ============================================================
//  突变检测参数 Setter
// ============================================================
bool nvs_set_mutation_enable(bool val) {
    if (val == s_mut_enable) return false;
    s_mut_enable = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putBool(NVS_KEY_MUT_EN, val);
    s_prefs.end();
    return true;
}

bool nvs_set_mutation_interval_sec(int val) {
    if (val < 5 || val > 300 || val == s_mut_interval_sec) return false;
    s_mut_interval_sec = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putInt(NVS_KEY_MUT_INTVL, val);
    s_prefs.end();
    return true;
}

bool nvs_set_mutation_block_thresh(float val) {
    if (val < 0.05f || val > 0.50f) return false;
    if (fabsf(val - s_mut_block_thresh) < 1e-6f) return false;
    s_mut_block_thresh = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putFloat(NVS_KEY_MUT_THRESH, val);
    s_prefs.end();
    return true;
}

bool nvs_set_mutation_min_blocks(int val) {
    if (val < 1 || val > 32 || val == s_mut_min_blocks) return false;
    s_mut_min_blocks = val;
    s_prefs.begin(NVS_NAMESPACE, false);
    s_prefs.putInt(NVS_KEY_MUT_MBLK, val);
    s_prefs.end();
    return true;
}


