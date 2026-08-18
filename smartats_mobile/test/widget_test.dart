import 'package:flutter_test/flutter_test.dart';
import 'package:smart_ats_mobile/models/system_state.dart';

void main() {
  test('SystemStateModel parses /api/status JSON', () {
    final json = {
      'voltage': 230.5,
      'current': 1.25,
      'power': 287.5,
      'energy': 1.234,
      'meterCount': 3,
      'maxMeters': 10,
      'activeMeter': 0,
      'emergency': false,
      'bypass': false,
      'pzemOK': true,
      'todayUsed': 0.456,
      'thisMonth': 3.210,
      'lastMonth': 12.050,
      'protTrip': false,
      'protReason': '',
      'faultName': '',
      'faultStatus': 'cleared',
      'recoverCountdown': 0,
      'recoverDelay': 0,
      'lastFaultEpoch': 0,
      'ovRec': 5,
      'uvRec': 5,
      'ocRec': 10,
      'resetDay': 1,
      'ovVolt': 250.0,
      'uvVolt': 180.0,
      'ocCurr': 16.0,
      'pzemWasReset': false,
      'pzemResetCount': 0,
      'wifiMode': 2,
      'staSsid': 'HomeNetwork',
      'staOK': true,
      'staIP': '192.168.1.100',
      'apIP': '192.168.4.1',
      'staRssi': -62,
      'staFallback': false,
      'otaReady': true,
      'testMode': false,
      'testLeft': 0,
      'uptime': 3600,
      'limits': [5.0, 5.0, 5.0],
      'enabled': [true, true, false],
      'used': [1.234, 0.567, 0.0],
      'daily': [0.1, 0.2, 0.3],
    };

    final state = SystemStateModel.fromJson(json);

    expect(state.voltage, 230.5);
    expect(state.meterCount, 3);
    expect(state.activeMeter, 0);
    expect(state.staOK, true);
    expect(state.limits.length, 3);
    expect(state.enabled[2], false);
    expect(state.systemMode, 'AUTO');
  });
}