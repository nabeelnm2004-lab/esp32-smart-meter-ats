import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/system_state.dart';
import '../models/system_info.dart';
import '../models/event_log_item.dart';
import '../models/wifi_network.dart';

class ApiResult {
  final bool ok;
  final String message;
  final int statusCode;

  const ApiResult({required this.ok, required this.message, this.statusCode = 200});

  static const ApiResult success = ApiResult(ok: true, message: 'ok');

  factory ApiResult.fromResponse(http.Response response) {
    String msg = '';
    if (response.body.isNotEmpty) {
      try {
        final data = jsonDecode(response.body);
        if (data is Map<String, dynamic>) {
          msg = data['msg'] ?? '';
        }
      } catch (_) {}
    }
    if (response.statusCode == 200) {
      return ApiResult(ok: true, message: msg, statusCode: response.statusCode);
    }
    return ApiResult(
      ok: false,
      message: msg.isNotEmpty ? msg : 'HTTP ${response.statusCode}',
      statusCode: response.statusCode,
    );
  }
}

class Esp32ApiService {
  String baseUrl;
  String? _basicAuth;

  Esp32ApiService({this.baseUrl = 'http://192.168.4.1'});

  void updateBaseUrl(String newUrl) {
    var url = newUrl.trim();
    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      url = 'http://$url';
    }
    baseUrl = url.replaceAll(RegExp(r'/+$'), '');
  }

  void setCredentials(String username, String password) {
    _basicAuth = 'Basic ${base64Encode(utf8.encode('$username:$password'))}';
  }

  void clearCredentials() {
    _basicAuth = null;
  }

  bool get isAuthed => _basicAuth != null;

  /// Verifies the currently set admin credentials against the device by
  /// calling an admin-only, read-only endpoint. Returns true on success,
  /// false when the device rejects the credentials (401), or null when the
  /// device is unreachable (no verdict).
  Future<bool?> verifyCredentials() async {
    try {
      final response = await _get('/api/backup', null, const Duration(seconds: 5));
      if (response.statusCode == 200) return true;
      if (response.statusCode == 401) return false;
      return false;
    } catch (e) {
      return null;
    }
  }

  Map<String, String> get _headers {
    final map = <String, String>{
      'Accept': 'application/json',
    };
    if (_basicAuth != null) {
      map['Authorization'] = _basicAuth!;
    }
    return map;
  }

  Uri _uri(String path, [Map<String, String>? query]) {
    final uri = Uri.parse('$baseUrl$path');
    if (query == null || query.isEmpty) return uri;
    return uri.replace(queryParameters: query);
  }

  Future<http.Response> _get(String path, [Map<String, String>? query, Duration? timeout]) async {
    return http
        .get(_uri(path, query), headers: _headers)
        .timeout(timeout ?? const Duration(seconds: 4));
  }

  // ================= PUBLIC =================

  Future<SystemStateModel> fetchSystemState() async {
    final response = await _get('/api/status', null, const Duration(seconds: 3));
    if (response.statusCode == 200) {
      final data = jsonDecode(response.body) as Map<String, dynamic>;
      return SystemStateModel.fromJson(data);
    }
    throw Exception('Status HTTP ${response.statusCode}');
  }

  Future<SystemInfoModel> fetchSystemInfo() async {
    final response = await _get('/api/sysinfo', null, const Duration(seconds: 3));
    if (response.statusCode == 200) {
      final data = jsonDecode(response.body) as Map<String, dynamic>;
      return SystemInfoModel.fromJson(data);
    }
    throw Exception('Sysinfo HTTP ${response.statusCode}');
  }

  Future<List<EventLogItem>> fetchEvents() async {
    final response = await _get('/api/events', null, const Duration(seconds: 4));
    if (response.statusCode == 200) {
      final data = jsonDecode(response.body) as Map<String, dynamic>;
      final list = data['events'] as List<dynamic>? ?? [];
      return list.map((e) => EventLogItem.fromJson(e as Map<String, dynamic>)).toList();
    }
    return [];
  }

  Future<Map<String, dynamic>> fetchWifiStatus() async {
    final response = await _get('/api/wifiStatus', null, const Duration(seconds: 3));
    if (response.statusCode == 200) {
      return jsonDecode(response.body) as Map<String, dynamic>;
    }
    return {};
  }

  // ================= MUTATING (Admin) =================

  Future<ApiResult> switchMeter(int index) async {
    try {
      final r = await _get('/api/switchMeter', {'m': '$index'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setLimits(Map<int, double> limits) async {
    try {
      final query = limits.map((k, v) => MapEntry('l$k', v.toString()));
      final r = await _get('/api/setLimits', query);
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setMeterEnabled(int index, bool enabled) async {
    try {
      final r = await _get('/api/setEnabled', {'idx': '$index', 'val': enabled ? '1' : '0'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> resetEnergy() async {
    try {
      final r = await _get('/api/resetEnergy');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setActiveMeters(int count) async {
    try {
      final r = await _get('/api/setActiveMeters', {'n': '$count'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> addMeter() async {
    try {
      final r = await _get('/api/addMeter');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> removeMeter(int index) async {
    try {
      final r = await _get('/api/removeMeter', {'idx': '$index'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> emergencyCutoff() async {
    try {
      final r = await _get('/api/emergency');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> clearEmergency() async {
    try {
      final r = await _get('/api/clearEmergency');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setBypass(bool enabled) async {
    try {
      final r = await _get('/api/setBypass', {'val': enabled ? '1' : '0'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setProtection({
    double? ov,
    double? uv,
    double? oc,
    int? ovRec,
    int? uvRec,
    int? ocRec,
  }) async {
    try {
      final query = <String, String>{};
      if (ov != null) query['ov'] = ov.toStringAsFixed(1);
      if (uv != null) query['uv'] = uv.toStringAsFixed(1);
      if (oc != null) query['oc'] = oc.toStringAsFixed(1);
      if (ovRec != null) query['ovrec'] = '${ovRec * 1000}';
      if (uvRec != null) query['uvrec'] = '${uvRec * 1000}';
      if (ocRec != null) query['ocrec'] = '${ocRec * 1000}';
      final r = await _get('/api/setProtection', query);
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> clearFault() async {
    try {
      final r = await _get('/api/clearFault');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setResetDay(int day) async {
    try {
      final r = await _get('/api/setResetDay', {'d': '$day'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setWiFi({required int mode, String? ssid, String? pass}) async {
    try {
      final query = <String, String>{'mode': '$mode'};
      if (ssid != null) query['ssid'] = ssid;
      if (pass != null) query['pass'] = pass;
      final r = await _get('/api/setWiFi', query);
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> connectWiFi({required String ssid, String? pass, int? mode}) async {
    try {
      final query = <String, String>{'ssid': ssid};
      if (pass != null) query['pass'] = pass;
      if (mode != null) query['mode'] = '$mode';
      final r = await _get('/api/connectWiFi', query);
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> disconnectWiFi() async {
    try {
      final r = await _get('/api/disconnectWiFi');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> forgetWiFi() async {
    try {
      final r = await _get('/api/forgetWiFi');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> clearEvents() async {
    try {
      final r = await _get('/api/clearEvents');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> restart() async {
    try {
      final r = await _get('/api/restart');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> factoryReset() async {
    try {
      final r = await _get('/api/factoryReset');
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> testMode(bool on) async {
    try {
      final r = await _get('/api/testMode', {'on': on ? '1' : '0'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> testRelay(int index) async {
    try {
      final r = await _get('/api/testRelay', {'idx': '$index'});
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<ApiResult> setTime({
    required int year,
    required int month,
    required int day,
    required int hour,
    required int minute,
    required int second,
  }) async {
    try {
      final r = await _get('/api/setTime', {
        'y': '$year',
        'mo': '$month',
        'd': '$day',
        'h': '$hour',
        'mi': '$minute',
        's': '$second',
      });
      return ApiResult.fromResponse(r);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  Future<String?> exportBackup() async {
    try {
      final response = await _get('/api/backup', null, const Duration(seconds: 6));
      if (response.statusCode == 200) return response.body;
      return null;
    } catch (e) {
      return null;
    }
  }

  Future<ApiResult> restoreBackup(String jsonBody) async {
    try {
      final response = await http
          .post(
            Uri.parse('$baseUrl/api/restore'),
            headers: {..._headers, 'Content-Type': 'application/json'},
            body: jsonBody,
          )
          .timeout(const Duration(seconds: 8));
      return ApiResult.fromResponse(response);
    } catch (e) {
      return ApiResult(ok: false, message: 'Request failed');
    }
  }

  // ================= Wi-Fi Scan (async start/poll) =================

  Future<bool> startWifiScan() async {
    try {
      final r = await _get('/api/scanWiFi/start', null, const Duration(seconds: 5));
      return r.statusCode == 200;
    } catch (e) {
      return false;
    }
  }

  /// One poll of the in-flight scan. The firmware reports "running" while
  /// the WiFi driver scans, "ok" (with networks) once finished, and "error"
  /// if the scan aborted. Unlike the old code this distinguishes a finished
  /// scan that found zero networks from one that is still in progress.
  Future<WifiScanPoll> pollWifiScan() async {
    try {
      final response = await _get('/api/scanWiFi', null, const Duration(seconds: 3));
      if (response.statusCode != 200) {
        return WifiScanPoll(state: WifiScanState.error, message: 'HTTP ${response.statusCode}');
      }
      final data = jsonDecode(response.body) as Map<String, dynamic>;
      final status = data['status'];
      if (status == 'ok') {
        final list = data['networks'] as List<dynamic>? ?? [];
        final networks = list
            .map((n) => WifiNetworkModel.fromJson(n as Map<String, dynamic>))
            .toList();
        return WifiScanPoll(state: WifiScanState.done, networks: networks);
      }
      if (status == 'error') {
        return WifiScanPoll(
          state: WifiScanState.error,
          message: data['msg'] as String? ?? 'scan failed',
        );
      }
      return const WifiScanPoll(state: WifiScanState.running);
    } catch (e) {
      return const WifiScanPoll(state: WifiScanState.error, message: 'request failed');
    }
  }
}

enum WifiScanState { running, done, error }

class WifiScanPoll {
  final WifiScanState state;
  final List<WifiNetworkModel> networks;
  final String message;

  const WifiScanPoll({
    required this.state,
    this.networks = const [],
    this.message = '',
  });
}