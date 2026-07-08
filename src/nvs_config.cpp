#include "nvs_config.h"
#include "config.h"
#include <Preferences.h>

// ============================================================
//  NVS 存储实例（内部私有）
// ============================================================
static Preferences s_prefs;

// ============================================================
//  配置项内存缓存（内部私有）
// ============================================================
static String s_sta_ssid     = "";
static String s_sta_password = "";
static float  s_warmup_sec   = 0.8f;
static String s_sta_name     = "";
static String s_mqtt_broker  = "";
static int    s_mqtt_port    = 1883;

// ============================================================
//  NVS 初始化
// ============================================================
void nvs_config_init() {
    s_prefs.begin("camera_cfg", false);

    s_sta_ssid     = s_prefs.getString("sta_ssid",    FACTORY_WIFI_SSID);
    s_sta_password = s_prefs.getString("sta_pass",    FACTORY_WIFI_PASSWORD);
    s_warmup_sec   = s_prefs.getFloat("warmup_sec",  0.8f);
    s_sta_name     = s_prefs.getString("sta_name",    FACTORY_DEVICE_NAME);
    s_mqtt_broker  = s_prefs.getString("mqtt_broker", FACTORY_MQTT_BROKER);
    s_mqtt_port    = s_prefs.getInt("mqtt_port",      FACTORY_MQTT_PORT);

    Serial.printf("[NvsConfig] Loaded SSID: %s, Station: %s, Broker: %s:%d\n",
                  s_sta_ssid.c_str(), s_sta_name.c_str(),
                  s_mqtt_broker.c_str(), s_mqtt_port);
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

// ============================================================
//  Setter 实现（含变化检测 + NVS 写入）
// ============================================================
bool nvs_set_sta_ssid(const String& val) {
    if (val.length() == 0 || val == s_sta_ssid) return false;
    s_sta_ssid = val;
    s_prefs.putString("sta_ssid", val);
    return true;
}

bool nvs_set_sta_password(const String& val) {
    if (val == s_sta_password) return false;
    s_sta_password = val;
    s_prefs.putString("sta_pass", val);
    return true;
}

bool nvs_set_warmup_sec(float val) {
    if (val < 0.1f || val > 10.0f) return false;
    if (val == s_warmup_sec) return false;
    s_warmup_sec = val;
    s_prefs.putFloat("warmup_sec", val);
    return true;
}

bool nvs_set_station_name(const String& val) {
    if (val.length() == 0 || val == s_sta_name) return false;
    s_sta_name = val;
    s_prefs.putString("sta_name", val);
    return true;
}

bool nvs_set_mqtt_broker(const String& val) {
    if (val.length() == 0 || val == s_mqtt_broker) return false;
    s_mqtt_broker = val;
    s_prefs.putString("mqtt_broker", val);
    return true;
}

bool nvs_set_mqtt_port(int val) {
    if (val <= 0 || val == s_mqtt_port) return false;
    s_mqtt_port = val;
    s_prefs.putInt("mqtt_port", val);
    return true;
}
