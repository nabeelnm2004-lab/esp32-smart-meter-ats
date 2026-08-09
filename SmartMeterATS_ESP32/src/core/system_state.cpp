#include "system_state.h"

namespace core {
namespace state {

SemaphoreHandle_t stateMux = nullptr;

MeterConfig meters[config::MAX_METERS];
uint8_t     activeMeterCount = config::DEFAULT_ACTIVE_METERS;
int         activeMeter      = 0;

int           pendingMeter  = -1;
unsigned long switchOffTime = 0;

bool          emergencyOff  = false;
bool          bypassMode    = false;
bool          testMode      = false;
unsigned long testModeStart = 0;

float liveVoltage    = 0.0f;
float liveCurrent    = 0.0f;
float livePower      = 0.0f;
float liveEnergy     = 0.0f;
float pzemEnergyBase = 0.0f;
bool  pzemOK         = false;

unsigned long lastEnergySampleMs = 0;
bool          pzemWasReset       = false;
uint16_t      pzemResetCount     = 0;
float         prevLiveEnergy     = -1.0f;

float todayUsed        = 0.0f;
float currentMonthUsed = 0.0f;
float lastMonthUsed    = 0.0f;
float dailyUsage[config::DAILY_HISTORY_DAYS] = {0};
int   dailyIndex       = 0;

int lastResetMonth   = -1;
int lastResetYear    = -1;
int monthlyResetDay  = config::RESET_DAY_DEFAULT;
int lastTrackedDay   = -1;
int lastTrackedMonth = -1;
int lastDay          = -1;

float  ovVoltThresh = config::OVER_VOLTAGE_DEFAULT;
float  uvVoltThresh = config::UNDER_VOLTAGE_DEFAULT;
float  ocCurrThresh = config::OVER_CURRENT_DEFAULT;
bool   protTrip     = false;
String protReason   = "";

uint8_t       protFaultType      = PROT_FAULT_NONE;
bool          protRecovering     = false;
unsigned long protRecoverStartMs = 0;
uint32_t      lastFaultEpoch     = 0;

unsigned long ovRecoveryMs = config::OV_RECOVERY_MS_DEFAULT;
unsigned long uvRecoveryMs = config::UV_RECOVERY_MS_DEFAULT;
unsigned long ocRecoveryMs = config::OC_RECOVERY_MS_DEFAULT;

bool     rtcOK             = false;
bool     rtcLostPower      = false;
uint32_t bootEpoch         = 0;
uint32_t lastTimeSyncEpoch = 0;

uint8_t wifiMode    = config::WIFI_MODE_AP_ONLY;
String  staSsid     = "";
String  staPass     = "";
String  otaPassword = config::DEFAULT_OTA_PASSWORD;

bool          restartPending = false;
unsigned long restartAtMs    = 0;
bool          otaInProgress  = false;

void begin() {
  stateMux = xSemaphoreCreateRecursiveMutex();
}

}  // namespace state
}  // namespace core
