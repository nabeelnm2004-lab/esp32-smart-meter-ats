class SystemStateModel {
  final double voltage;
  final double current;
  final double power;
  final double energy;
  final int meterCount;
  final int maxMeters;
  final int activeMeter;
  final bool emergency;
  final bool bypass;
  final bool pzemOK;
  final double todayUsed;
  final double thisMonth;
  final double lastMonth;
  final bool protTrip;
  final String protReason;
  final String faultName;
  final String faultStatus;
  final int recoverCountdown;
  final int recoverDelay;
  final int lastFaultEpoch;
  final int ovRec;
  final int uvRec;
  final int ocRec;
  final int resetDay;
  final double ovVolt;
  final double uvVolt;
  final double ocCurr;
  final bool pzemWasReset;
  final int pzemResetCount;
  final int wifiMode;
  final String staSsid;
  final bool staOK;
  final String staIP;
  final String apIP;
  final int staRssi;
  final bool staFallback;
  final bool otaReady;
  final bool testMode;
  final int testLeft;
  final int uptime;
  final List<double> limits;
  final List<bool> enabled;
  final List<double> used;
  final List<double> daily;
  final bool isConnected;

  const SystemStateModel({
    required this.voltage,
    required this.current,
    required this.power,
    required this.energy,
    required this.meterCount,
    required this.maxMeters,
    required this.activeMeter,
    required this.emergency,
    required this.bypass,
    required this.pzemOK,
    required this.todayUsed,
    required this.thisMonth,
    required this.lastMonth,
    required this.protTrip,
    required this.protReason,
    required this.faultName,
    required this.faultStatus,
    required this.recoverCountdown,
    required this.recoverDelay,
    required this.lastFaultEpoch,
    required this.ovRec,
    required this.uvRec,
    required this.ocRec,
    required this.resetDay,
    required this.ovVolt,
    required this.uvVolt,
    required this.ocCurr,
    required this.pzemWasReset,
    required this.pzemResetCount,
    required this.wifiMode,
    required this.staSsid,
    required this.staOK,
    required this.staIP,
    required this.apIP,
    required this.staRssi,
    required this.staFallback,
    required this.otaReady,
    required this.testMode,
    required this.testLeft,
    required this.uptime,
    required this.limits,
    required this.enabled,
    required this.used,
    required this.daily,
    required this.isConnected,
  });

  factory SystemStateModel.initial() {
    return SystemStateModel(
      voltage: 0,
      current: 0,
      power: 0,
      energy: 0,
      meterCount: 0,
      maxMeters: 10,
      activeMeter: 0,
      emergency: false,
      bypass: false,
      pzemOK: false,
      todayUsed: 0,
      thisMonth: 0,
      lastMonth: 0,
      protTrip: false,
      protReason: '',
      faultName: '',
      faultStatus: 'cleared',
      recoverCountdown: 0,
      recoverDelay: 0,
      lastFaultEpoch: 0,
      ovRec: 5,
      uvRec: 5,
      ocRec: 10,
      resetDay: 1,
      ovVolt: 250,
      uvVolt: 180,
      ocCurr: 16,
      pzemWasReset: false,
      pzemResetCount: 0,
      wifiMode: 2,
      staSsid: '',
      staOK: false,
      staIP: '',
      apIP: '192.168.4.1',
      staRssi: 0,
      staFallback: false,
      otaReady: false,
      testMode: false,
      testLeft: 0,
      uptime: 0,
      limits: [],
      enabled: [],
      used: [],
      daily: [],
      isConnected: false,
    );
  }

  factory SystemStateModel.fromJson(Map<String, dynamic> json) {
    List<double> asDoubleList(List<dynamic>? raw) {
      return (raw ?? const []).map((e) => (e ?? 0).toDouble()).toList().cast<double>();
    }

    List<bool> asBoolList(List<dynamic>? raw) {
      return (raw ?? const []).map((e) => e == true || e == 1).toList().cast<bool>();
    }

    return SystemStateModel(
      voltage: (json['voltage'] ?? 0).toDouble(),
      current: (json['current'] ?? 0).toDouble(),
      power: (json['power'] ?? 0).toDouble(),
      energy: (json['energy'] ?? 0).toDouble(),
      meterCount: (json['meterCount'] ?? 0) as int,
      maxMeters: (json['maxMeters'] ?? 10) as int,
      activeMeter: (json['activeMeter'] ?? 0) as int,
      emergency: json['emergency'] ?? false,
      bypass: json['bypass'] ?? false,
      pzemOK: json['pzemOK'] ?? false,
      todayUsed: (json['todayUsed'] ?? 0).toDouble(),
      thisMonth: (json['thisMonth'] ?? 0).toDouble(),
      lastMonth: (json['lastMonth'] ?? 0).toDouble(),
      protTrip: json['protTrip'] ?? false,
      protReason: json['protReason'] ?? '',
      faultName: json['faultName'] ?? '',
      faultStatus: json['faultStatus'] ?? 'cleared',
      recoverCountdown: (json['recoverCountdown'] ?? 0) as int,
      recoverDelay: (json['recoverDelay'] ?? 0) as int,
      lastFaultEpoch: (json['lastFaultEpoch'] ?? 0) as int,
      ovRec: (json['ovRec'] ?? 5) as int,
      uvRec: (json['uvRec'] ?? 5) as int,
      ocRec: (json['ocRec'] ?? 10) as int,
      resetDay: (json['resetDay'] ?? 1) as int,
      ovVolt: (json['ovVolt'] ?? 250).toDouble(),
      uvVolt: (json['uvVolt'] ?? 180).toDouble(),
      ocCurr: (json['ocCurr'] ?? 16).toDouble(),
      pzemWasReset: json['pzemWasReset'] ?? false,
      pzemResetCount: (json['pzemResetCount'] ?? 0) as int,
      wifiMode: (json['wifiMode'] ?? 2) as int,
      staSsid: json['staSsid'] ?? '',
      staOK: json['staOK'] ?? false,
      staIP: json['staIP'] ?? '',
      apIP: json['apIP'] ?? '192.168.4.1',
      staRssi: (json['staRssi'] ?? 0) as int,
      staFallback: json['staFallback'] ?? false,
      otaReady: json['otaReady'] ?? false,
      testMode: json['testMode'] ?? false,
      testLeft: (json['testLeft'] ?? 0) as int,
      uptime: (json['uptime'] ?? 0) as int,
      limits: asDoubleList(json['limits'] as List<dynamic>?),
      enabled: asBoolList(json['enabled'] as List<dynamic>?),
      used: asDoubleList(json['used'] as List<dynamic>?),
      daily: asDoubleList(json['daily'] as List<dynamic>?),
      isConnected: true,
    );
  }

  String get systemMode {
    if (emergency) return 'EMERGENCY';
    if (bypass) return 'BYPASS';
    if (protTrip) return 'FAULT';
    return 'AUTO';
  }
}