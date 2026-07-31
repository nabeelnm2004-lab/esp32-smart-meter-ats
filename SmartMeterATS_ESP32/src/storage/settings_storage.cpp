#include "settings_storage.h"

#include <Arduino.h>
#include <Preferences.h>

#include "../core/config.h"
#include "../core/system_state.h"

namespace storage {
namespace settings {

using namespace core;

namespace {

Preferences prefs;

// RAM mirror of what NVS currently holds, so save() can write only the
// keys whose value actually changed. valid=false means "never written"
// (fresh flash or factory reset) and forces a full write.
struct SettingsShadow {
  float    lim[config::MAX_METERS];
  bool     en[config::MAX_METERS];
  float    daily[config::DAILY_HISTORY_DAYS];
  uint8_t  mcount;
  uint8_t  active;
  uint8_t  didx;
  float    today;
  float    curmon;
  float    lastmon;
  float    ov;
  float    uv;
  float    oc;
  bool     emerg;
  bool     bypass;
  uint8_t  lmon;
  uint16_t lyear;
  uint8_t  rday;
  uint8_t  wmode;
  String   wssid;
  String   wpass;
  String   otapass;
  bool     valid;
};

SettingsShadow shadow = {};

bool          dirty         = false;
unsigned long dirtySince    = 0;   // start of the current dirty period
unsigned long lastChange    = 0;   // most recent markDirty()
unsigned long lastWrite     = 0;   // last REAL flash write

}  // namespace

void markDirty() {
  const unsigned long now = millis();
  if (!dirty) dirtySince = now;
  dirty      = true;
  lastChange = now;
}

void handleDeferredSave() {
  if (!dirty) return;
  // After a factory reset wiped NVS we must not re-write it before the
  // pending reboot.
  if (state::restartPending) return;

  const unsigned long now     = millis();
  const bool          starved = (now - dirtySince) >= config::NVS_MAX_DEFER_MS;

  if (!starved && (now - lastChange) < config::NVS_DEBOUNCE_MS) return;
  if ((now - lastWrite) < config::NVS_MIN_WRITE_INTERVAL_MS) return;

  dirty = false;
  save();
}

void forceSave() {
  save();
}

void save() {
  float    limSnap[config::MAX_METERS];
  bool     enSnap[config::MAX_METERS];
  float    dailySnap[config::DAILY_HISTORY_DAYS];
  uint8_t  countSnap, activeSnap, didxSnap;
  float    todaySnap, curmonSnap, lastmonSnap, ovSnap, uvSnap, ocSnap;
  bool     emergSnap, bypassSnap;
  uint8_t  rstmonSnap, rdaySnap, wmodeSnap;
  uint16_t rstyrSnap;
  String   wssidSnap, wpassSnap, otapassSnap;

  STATE_LOCK();
  for (int i = 0; i < config::MAX_METERS; i++) {
    limSnap[i] = state::meters[i].energyLimit;
    enSnap[i]  = state::meters[i].enabled;
  }
  countSnap = state::activeMeterCount;
  activeSnap = (uint8_t)state::activeMeter;
  memcpy(dailySnap, state::dailyUsage, sizeof(dailySnap));
  didxSnap = (uint8_t)constrain(state::dailyIndex, 0, config::DAILY_HISTORY_DAYS - 1);
  todaySnap   = state::todayUsed;
  curmonSnap  = state::currentMonthUsed;
  lastmonSnap = state::lastMonthUsed;
  ovSnap      = state::ovVoltThresh;
  uvSnap      = state::uvVoltThresh;
  ocSnap      = state::ocCurrThresh;
  emergSnap   = state::emergencyOff;
  bypassSnap  = state::bypassMode;
  rstmonSnap  = (uint8_t)max(state::lastResetMonth, 0);
  rstyrSnap   = (uint16_t)max(state::lastResetYear, 0);
  rdaySnap    = (uint8_t)constrain(state::monthlyResetDay,
                                   config::RESET_DAY_MIN, config::RESET_DAY_MAX);
  wmodeSnap   = state::wifiMode;
  wssidSnap   = state::staSsid;
  wpassSnap   = state::staPass;
  otapassSnap = state::otaPassword;
  // Clear the dirty flag while still holding the lock: anything dirtied
  // AFTER this snapshot re-raises it and is picked up by the next flush
  // instead of being lost.
  dirty = false;
  STATE_UNLOCK();

  const bool force = !shadow.valid;

  // Cheap early-out: if nothing differs from what NVS already holds,
  // skip the flash entirely. Float compares are exact on purpose —
  // these values are only ever produced by the same code paths.
  if (!force) {
    bool changed =
        shadow.mcount != countSnap   || shadow.active  != activeSnap  ||
        shadow.didx   != didxSnap    ||
        shadow.today  != todaySnap   || shadow.curmon  != curmonSnap  ||
        shadow.lastmon!= lastmonSnap ||
        shadow.ov     != ovSnap      || shadow.uv      != uvSnap      ||
        shadow.oc     != ocSnap      ||
        shadow.emerg  != emergSnap   || shadow.bypass  != bypassSnap  ||
        shadow.lmon   != rstmonSnap  || shadow.lyear   != rstyrSnap   ||
        shadow.rday   != rdaySnap    || shadow.wmode   != wmodeSnap   ||
        shadow.wssid  != wssidSnap   || shadow.wpass   != wpassSnap   ||
        shadow.otapass!= otapassSnap ||
        memcmp(shadow.daily, dailySnap, sizeof(dailySnap)) != 0;
    for (int i = 0; !changed && i < config::MAX_METERS; i++) {
      changed = (shadow.lim[i] != limSnap[i]) || (shadow.en[i] != enSnap[i]);
    }
    if (!changed) {
      // lastWrite is deliberately NOT updated: the throttle tracks REAL
      // writes only.
      Serial.println(F("[NVS] No changes — flash write skipped"));
      return;
    }
  }

  prefs.begin(config::NVS_NAMESPACE, false);   // read-write
  char key[8];
  if (force || shadow.mcount != countSnap) prefs.putUChar("mcount", countSnap);
  for (int i = 0; i < config::MAX_METERS; i++) {
    if (force || shadow.lim[i] != limSnap[i]) {
      snprintf(key, sizeof(key), "lim%d", i);
      prefs.putFloat(key, limSnap[i]);
    }
    if (force || shadow.en[i] != enSnap[i]) {
      snprintf(key, sizeof(key), "en%d", i);
      prefs.putBool(key, enSnap[i]);
    }
  }
  if (force || shadow.active != activeSnap) prefs.putUChar("active", activeSnap);
  if (force || memcmp(shadow.daily, dailySnap, sizeof(dailySnap)) != 0) {
    prefs.putBytes("daily", dailySnap, sizeof(dailySnap));
  }
  if (force || shadow.didx    != didxSnap)    prefs.putUChar("didx",    didxSnap);
  if (force || shadow.today   != todaySnap)   prefs.putFloat("today",   todaySnap);
  if (force || shadow.curmon  != curmonSnap)  prefs.putFloat("curmon",  curmonSnap);
  if (force || shadow.lastmon != lastmonSnap) prefs.putFloat("lastmon", lastmonSnap);
  if (force || shadow.ov      != ovSnap)      prefs.putFloat("ov",      ovSnap);
  if (force || shadow.uv      != uvSnap)      prefs.putFloat("uv",      uvSnap);
  if (force || shadow.oc      != ocSnap)      prefs.putFloat("oc",      ocSnap);
  if (force || shadow.emerg   != emergSnap)   prefs.putBool("emerg",    emergSnap);
  if (force || shadow.bypass  != bypassSnap)  prefs.putBool("bypass",   bypassSnap);
  if (force || shadow.lmon    != rstmonSnap)  prefs.putUChar("lmon",    rstmonSnap);
  if (force || shadow.lyear   != rstyrSnap)   prefs.putUShort("lyear",  rstyrSnap);
  if (force || shadow.rday    != rdaySnap)    prefs.putUChar("rday",    rdaySnap);
  if (force || shadow.wmode   != wmodeSnap)   prefs.putUChar("wmode",   wmodeSnap);
  if (force || shadow.wssid   != wssidSnap)   prefs.putString("wssid",  wssidSnap);
  if (force || shadow.wpass   != wpassSnap)   prefs.putString("wpass",  wpassSnap);
  if (force || shadow.otapass != otapassSnap) prefs.putString("otapass", otapassSnap);
  prefs.end();

  // NVS now matches the snapshot — update the shadow so the next save
  // writes only keys dirtied after THIS snapshot.
  for (int i = 0; i < config::MAX_METERS; i++) {
    shadow.lim[i] = limSnap[i];
    shadow.en[i]  = enSnap[i];
  }
  memcpy(shadow.daily, dailySnap, sizeof(dailySnap));
  shadow.mcount  = countSnap;   shadow.active = activeSnap;  shadow.didx = didxSnap;
  shadow.today   = todaySnap;   shadow.curmon = curmonSnap;
  shadow.lastmon = lastmonSnap;
  shadow.ov      = ovSnap;      shadow.uv     = uvSnap;      shadow.oc   = ocSnap;
  shadow.emerg   = emergSnap;   shadow.bypass = bypassSnap;
  shadow.lmon    = rstmonSnap;  shadow.lyear  = rstyrSnap;   shadow.rday = rdaySnap;
  shadow.wmode   = wmodeSnap;
  shadow.wssid   = wssidSnap;   shadow.wpass  = wpassSnap;
  shadow.otapass = otapassSnap;
  shadow.valid   = true;

  lastWrite = millis();
  Serial.println(F("[NVS] Settings saved"));
}

void seedShadow() {
  for (int i = 0; i < config::MAX_METERS; i++) {
    shadow.lim[i] = state::meters[i].energyLimit;
    shadow.en[i]  = state::meters[i].enabled;
  }
  memcpy(shadow.daily, state::dailyUsage, sizeof(shadow.daily));
  shadow.mcount  = state::activeMeterCount;
  shadow.active  = (uint8_t)state::activeMeter;
  shadow.didx    = (uint8_t)constrain(state::dailyIndex, 0,
                                      config::DAILY_HISTORY_DAYS - 1);
  shadow.today   = state::todayUsed;
  shadow.curmon  = state::currentMonthUsed;
  shadow.lastmon = state::lastMonthUsed;
  shadow.ov      = state::ovVoltThresh;
  shadow.uv      = state::uvVoltThresh;
  shadow.oc      = state::ocCurrThresh;
  shadow.emerg   = state::emergencyOff;
  shadow.bypass  = state::bypassMode;
  shadow.lmon    = (uint8_t)max(state::lastResetMonth, 0);
  shadow.lyear   = (uint16_t)max(state::lastResetYear, 0);
  shadow.rday    = (uint8_t)constrain(state::monthlyResetDay,
                                      config::RESET_DAY_MIN, config::RESET_DAY_MAX);
  shadow.wmode   = state::wifiMode;
  shadow.wssid   = state::staSsid;
  shadow.wpass   = state::staPass;
  shadow.otapass = state::otaPassword;
  shadow.valid   = true;
}

void load() {
  for (int i = 0; i < config::MAX_METERS; i++) {
    state::meters[i].energyLimit = config::ENERGY_LIMIT_DEFAULT;
    state::meters[i].enabled     = true;
    state::meters[i].energyUsed  = 0.0f;
  }
  state::activeMeterCount = config::DEFAULT_ACTIVE_METERS;
  state::activeMeter      = 0;

  // Read-only open fails when the namespace has never been written.
  if (!prefs.begin(config::NVS_NAMESPACE, true)) {
    Serial.println(F("[NVS] No saved data - using defaults"));
    return;
  }

  state::activeMeterCount =
      constrain(prefs.getUChar("mcount", config::DEFAULT_ACTIVE_METERS),
                1, config::MAX_METERS);

  char key[8];
  for (int i = 0; i < config::MAX_METERS; i++) {
    snprintf(key, sizeof(key), "lim%d", i);
    state::meters[i].energyLimit = prefs.getFloat(key, config::ENERGY_LIMIT_DEFAULT);
    if (isnan(state::meters[i].energyLimit) || state::meters[i].energyLimit <= 0) {
      state::meters[i].energyLimit = config::ENERGY_LIMIT_DEFAULT;
    }
    snprintf(key, sizeof(key), "en%d", i);
    state::meters[i].enabled = prefs.getBool(key, true);
  }
  state::activeMeter =
      constrain(prefs.getUChar("active", 0), 0, state::activeMeterCount - 1);

  const size_t got = prefs.getBytes("daily", state::dailyUsage,
                                    sizeof(state::dailyUsage));
  if (got != sizeof(state::dailyUsage)) {
    for (int i = 0; i < config::DAILY_HISTORY_DAYS; i++) state::dailyUsage[i] = 0.0f;
  } else {
    for (int i = 0; i < config::DAILY_HISTORY_DAYS; i++) {
      if (isnan(state::dailyUsage[i]) || state::dailyUsage[i] < 0) {
        state::dailyUsage[i] = 0.0f;
      }
    }
  }
  state::dailyIndex = (int)constrain(prefs.getUChar("didx", 0), 0,
                                     config::DAILY_HISTORY_DAYS - 1);

  state::todayUsed        = prefs.getFloat("today",   0.0f);
  state::currentMonthUsed = prefs.getFloat("curmon",  0.0f);
  state::lastMonthUsed    = prefs.getFloat("lastmon", 0.0f);
  if (isnan(state::todayUsed)        || state::todayUsed < 0)        state::todayUsed = 0.0f;
  if (isnan(state::currentMonthUsed) || state::currentMonthUsed < 0) state::currentMonthUsed = 0.0f;
  if (isnan(state::lastMonthUsed)    || state::lastMonthUsed < 0)    state::lastMonthUsed = 0.0f;

  state::ovVoltThresh = prefs.getFloat("ov", config::OVER_VOLTAGE_DEFAULT);
  state::uvVoltThresh = prefs.getFloat("uv", config::UNDER_VOLTAGE_DEFAULT);
  state::ocCurrThresh = prefs.getFloat("oc", config::OVER_CURRENT_DEFAULT);
  if (isnan(state::ovVoltThresh) ||
      state::ovVoltThresh < config::OVER_VOLTAGE_MIN ||
      state::ovVoltThresh > config::OVER_VOLTAGE_MAX) {
    state::ovVoltThresh = config::OVER_VOLTAGE_DEFAULT;
  }
  if (isnan(state::uvVoltThresh) ||
      state::uvVoltThresh < config::UNDER_VOLTAGE_MIN ||
      state::uvVoltThresh > config::UNDER_VOLTAGE_MAX) {
    state::uvVoltThresh = config::UNDER_VOLTAGE_DEFAULT;
  }
  if (isnan(state::ocCurrThresh) ||
      state::ocCurrThresh < config::OVER_CURRENT_MIN ||
      state::ocCurrThresh > config::OVER_CURRENT_MAX) {
    state::ocCurrThresh = config::OVER_CURRENT_DEFAULT;
  }

  state::emergencyOff = prefs.getBool("emerg",  false);
  state::bypassMode   = prefs.getBool("bypass", false);

  // New keys "lmon"/"lyear"; fall back to the legacy "rstmon"/"rstyr"
  // names so existing installations migrate without losing state.
  state::lastResetMonth  = (int)prefs.getUChar("lmon",  prefs.getUChar("rstmon", 0));
  state::lastResetYear   = (int)prefs.getUShort("lyear", prefs.getUShort("rstyr", 0));
  state::monthlyResetDay = (int)prefs.getUChar("rday", config::RESET_DAY_DEFAULT);
  if (state::lastResetMonth < 1 || state::lastResetMonth > 12) state::lastResetMonth = -1;
  if (state::lastResetYear < config::MIN_VALID_YEAR) {
    state::lastResetMonth = -1;
    state::lastResetYear  = -1;
  }
  if (state::monthlyResetDay < config::RESET_DAY_MIN ||
      state::monthlyResetDay > config::RESET_DAY_MAX) {
    state::monthlyResetDay = config::RESET_DAY_DEFAULT;
  }

  state::wifiMode = prefs.getUChar("wmode", config::WIFI_MODE_AP_ONLY);
  if (state::wifiMode > config::WIFI_MODE_AP_STA) {
    state::wifiMode = config::WIFI_MODE_AP_ONLY;
  }
  state::staSsid = prefs.getString("wssid", "");
  state::staPass = prefs.getString("wpass", "");
  if (state::staSsid.length() > config::MAX_SSID_LEN) {
    state::staSsid = state::staSsid.substring(0, config::MAX_SSID_LEN);
  }
  if (state::staPass.length() > config::MAX_PASS_LEN) {
    state::staPass = state::staPass.substring(0, config::MAX_PASS_LEN);
  }
  if (state::staSsid.length() == 0 && state::wifiMode != config::WIFI_MODE_AP_ONLY) {
    state::wifiMode = config::WIFI_MODE_AP_ONLY;   // STA modes need credentials
  }

  state::otaPassword = prefs.getString("otapass", config::DEFAULT_OTA_PASSWORD);
  if (state::otaPassword.length() == 0) {
    state::otaPassword = config::DEFAULT_OTA_PASSWORD;
  }
  prefs.end();

  Serial.println(F("[NVS] Settings loaded"));
  if (state::emergencyOff) {
    Serial.println(F("[BOOT] Emergency state restored — all relays OFF"));
  }
}

void factoryReset() {
  prefs.begin(config::NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  shadow.valid = false;   // next save must write everything
}

}  // namespace settings
}  // namespace storage
