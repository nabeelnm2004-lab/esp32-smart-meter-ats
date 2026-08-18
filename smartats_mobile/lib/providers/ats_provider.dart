import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/system_state.dart';
import '../models/system_info.dart';
import '../models/event_log_item.dart';
import '../models/wifi_network.dart';
import '../services/esp32_api_service.dart';

class AtsProvider extends ChangeNotifier {
  final Esp32ApiService apiService;

  SystemStateModel _state = SystemStateModel.initial();
  SystemInfoModel _sysInfo = SystemInfoModel.initial();
  List<EventLogItem> _events = [];
  List<WifiNetworkModel> _networks = [];
  bool _isLoading = false;
  bool _isUnlocked = false;
  String _lastError = '';
  Timer? _pollTimer;
  Timer? _sysInfoTimer;
  bool _wifiScanRunning = false;

  AtsProvider({Esp32ApiService? apiService})
      : apiService = apiService ?? Esp32ApiService() {
    startLivePolling();
  }

  SystemStateModel get state => _state;
  SystemInfoModel get sysInfo => _sysInfo;
  List<EventLogItem> get events => _events;
  List<WifiNetworkModel> get networks => _networks;
  bool get isLoading => _isLoading;
  bool get isUnlocked => _isUnlocked;
  String get lastError => _lastError;
  bool get isConnected => _state.isConnected;
  String get baseUrl => apiService.baseUrl;

  // ============= Connection =============

  void setDeviceIp(String ip) {
    apiService.updateBaseUrl(ip);
    fetchState();
  }

  Future<void> connectTo(String ip, {String? password}) async {
    apiService.updateBaseUrl(ip);
    if (password != null && password.isNotEmpty) {
      apiService.setCredentials('admin', password);
      _isUnlocked = true;
    }
    notifyListeners();
    await fetchState();
    await fetchSysInfo();
  }

  /// Stores the admin credential and immediately verifies it against the
  /// device. Returns true when the device accepted it, false when it was
  /// rejected (401), or null when the device was unreachable. On rejection
  /// the stored credential is cleared so the app stays locked.
  Future<bool?> unlockAdmin(String password) async {
    apiService.setCredentials('admin', password);
    final verified = await apiService.verifyCredentials();
    if (verified == true) {
      _isUnlocked = true;
    } else {
      apiService.clearCredentials();
      _isUnlocked = false;
    }
    notifyListeners();
    return verified;
  }

  void lockAdmin() {
    apiService.clearCredentials();
    _isUnlocked = false;
    notifyListeners();
  }

  // ============= Polling =============

  void startLivePolling() {
    _pollTimer?.cancel();
    _sysInfoTimer?.cancel();
    fetchState();
    fetchSysInfo();
    _pollTimer = Timer.periodic(const Duration(seconds: 3), (_) => fetchState());
    _sysInfoTimer = Timer.periodic(const Duration(seconds: 10), (_) => fetchSysInfo());
  }

  void stopLivePolling() {
    _pollTimer?.cancel();
    _sysInfoTimer?.cancel();
  }

  Future<void> fetchState() async {
    try {
      final fresh = await apiService.fetchSystemState();
      _state = fresh;
      _lastError = '';
    } catch (e) {
      _state = SystemStateModel.initial();
      _lastError = 'Connecting to ESP32 (${apiService.baseUrl})...';
    }
    notifyListeners();
  }

  Future<void> fetchSysInfo() async {
    try {
      _sysInfo = await apiService.fetchSystemInfo();
    } catch (_) {}
  }

  Future<void> fetchEvents() async {
    _events = await apiService.fetchEvents();
    notifyListeners();
  }

  Future<void> manualRefresh() async {
    _isLoading = true;
    notifyListeners();
    await fetchState();
    await fetchSysInfo();
    _isLoading = false;
    notifyListeners();
  }

  // ============= Actions =============

  Future<ApiResult> switchSource(int index) async {
    final result = await apiService.switchMeter(index);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> setLimits(Map<int, double> limits) async {
    final result = await apiService.setLimits(limits);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> setMeterEnabled(int index, bool enabled) async {
    final result = await apiService.setMeterEnabled(index, enabled);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> resetEnergy() async {
    final result = await apiService.resetEnergy();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> setActiveMeters(int count) async {
    final result = await apiService.setActiveMeters(count);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> addMeter() async {
    final result = await apiService.addMeter();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> removeMeter(int index) async {
    final result = await apiService.removeMeter(index);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> emergencyCutoff() async {
    final result = await apiService.emergencyCutoff();
    if (result.ok) await fetchState();
    return result;
  }

  /// Clears the Emergency OFF latch (and any protection fault that would
  /// block it) so relays re-energise the active meter.
  Future<ApiResult> restorePower() async {
    if (_state.protTrip) {
      final fault = await apiService.clearFault();
      if (!fault.ok) return fault;
    }
    final result = await apiService.clearEmergency();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> toggleBypass(bool enabled) async {
    final result = await apiService.setBypass(enabled);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> updateProtection({
    double? ov,
    double? uv,
    double? oc,
    int? ovRec,
    int? uvRec,
    int? ocRec,
  }) async {
    final result = await apiService.setProtection(
      ov: ov,
      uv: uv,
      oc: oc,
      ovRec: ovRec,
      uvRec: uvRec,
      ocRec: ocRec,
    );
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> clearFault() async {
    final result = await apiService.clearFault();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> setResetDay(int day) async {
    final result = await apiService.setResetDay(day);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> setWiFi({required int mode, String? ssid, String? pass}) async {
    final result = await apiService.setWiFi(mode: mode, ssid: ssid, pass: pass);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> connectWiFi({required String ssid, String? pass, int? mode}) async {
    final result = await apiService.connectWiFi(ssid: ssid, pass: pass, mode: mode);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> disconnectWiFi() async {
    final result = await apiService.disconnectWiFi();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> forgetWiFi() async {
    final result = await apiService.forgetWiFi();
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> clearEvents() async {
    final result = await apiService.clearEvents();
    if (result.ok) await fetchEvents();
    return result;
  }

  Future<ApiResult> restartDevice() async {
    return apiService.restart();
  }

  Future<ApiResult> factoryResetDevice() async {
    return apiService.factoryReset();
  }

  Future<ApiResult> setTestMode(bool on) async {
    final result = await apiService.testMode(on);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> testRelay(int index) async {
    final result = await apiService.testRelay(index);
    if (result.ok) await fetchState();
    return result;
  }

  Future<ApiResult> syncTime(DateTime dt) async {
    final result = await apiService.setTime(
      year: dt.year,
      month: dt.month,
      day: dt.day,
      hour: dt.hour,
      minute: dt.minute,
      second: dt.second,
    );
    if (result.ok) await fetchState();
    return result;
  }

  Future<String?> exportBackup() async {
    return apiService.exportBackup();
  }

  Future<ApiResult> restoreBackup(String json) async {
    final result = await apiService.restoreBackup(json);
    if (result.ok) await fetchState();
    return result;
  }

  // ============= Wi-Fi Scan =============

  String _scanMessage = '';

  bool get wifiScanRunning => _wifiScanRunning;

  /// Human-readable feedback about the last scan: empty when nothing has
  /// been scanned yet or the scan succeeded, otherwise an error/empty note.
  String get scanMessage => _scanMessage;

  Future<void> runWifiScan() async {
    if (_wifiScanRunning) return;
    _wifiScanRunning = true;
    _networks = [];
    _scanMessage = '';
    notifyListeners();

    final started = await apiService.startWifiScan();
    if (!started) {
      _wifiScanRunning = false;
      _scanMessage = 'Could not start the scan on the device';
      notifyListeners();
      return;
    }

    final deadline = DateTime.now().add(const Duration(seconds: 15));
    WifiScanPoll poll = const WifiScanPoll(state: WifiScanState.running);
    while (DateTime.now().isBefore(deadline) && poll.state == WifiScanState.running) {
      await Future.delayed(const Duration(milliseconds: 600));
      poll = await apiService.pollWifiScan();
    }

    if (poll.state == WifiScanState.done) {
      _networks = poll.networks;
      if (_networks.isEmpty) _scanMessage = 'No Wi-Fi networks found nearby';
    } else if (poll.state == WifiScanState.error) {
      _scanMessage = 'Scan failed: ${poll.message.isEmpty ? 'device error' : poll.message}';
    } else {
      _scanMessage = 'Scan timed out — try again';
    }

    _wifiScanRunning = false;
    notifyListeners();
  }

  @override
  void dispose() {
    stopLivePolling();
    super.dispose();
  }
}