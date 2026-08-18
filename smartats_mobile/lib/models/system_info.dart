class SystemInfoModel {
  final String fw;
  final String build;
  final String chip;
  final int flash;
  final int heap;
  final int cpu;
  final int uptime;
  final String rstReason;
  final int rssi;
  final String hostname;
  final String ip;
  final String timeSrc;
  final int meterCount;
  final bool rtcOK;
  final bool rtcLostPower;
  final String rtcTime;
  final String lastSync;

  const SystemInfoModel({
    required this.fw,
    required this.build,
    required this.chip,
    required this.flash,
    required this.heap,
    required this.cpu,
    required this.uptime,
    required this.rstReason,
    required this.rssi,
    required this.hostname,
    required this.ip,
    required this.timeSrc,
    required this.meterCount,
    required this.rtcOK,
    required this.rtcLostPower,
    required this.rtcTime,
    required this.lastSync,
  });

  factory SystemInfoModel.initial() {
    return SystemInfoModel(
      fw: '-',
      build: '-',
      chip: '-',
      flash: 0,
      heap: 0,
      cpu: 0,
      uptime: 0,
      rstReason: '-',
      rssi: 0,
      hostname: '-',
      ip: '-',
      timeSrc: '-',
      meterCount: 0,
      rtcOK: false,
      rtcLostPower: false,
      rtcTime: '-',
      lastSync: '-',
    );
  }

  factory SystemInfoModel.fromJson(Map<String, dynamic> json) {
    return SystemInfoModel(
      fw: json['fw'] ?? '-',
      build: json['build'] ?? '-',
      chip: json['chip'] ?? '-',
      flash: (json['flash'] ?? 0) as int,
      heap: (json['heap'] ?? 0) as int,
      cpu: (json['cpu'] ?? 0) as int,
      uptime: (json['uptime'] ?? 0) as int,
      rstReason: json['rstReason'] ?? '-',
      rssi: (json['rssi'] ?? 0) as int,
      hostname: json['hostname'] ?? '-',
      ip: json['ip'] ?? '-',
      timeSrc: json['timeSrc'] ?? '-',
      meterCount: (json['meterCount'] ?? 0) as int,
      rtcOK: json['rtcOK'] ?? false,
      rtcLostPower: json['rtcLostPower'] ?? false,
      rtcTime: json['rtcTime'] ?? '-',
      lastSync: json['lastSync'] ?? '-',
    );
  }
}