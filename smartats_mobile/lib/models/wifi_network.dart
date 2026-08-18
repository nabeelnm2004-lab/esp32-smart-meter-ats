class WifiNetworkModel {
  final String ssid;
  final int rssi;
  final bool encrypted;
  final int channel;

  const WifiNetworkModel({
    required this.ssid,
    required this.rssi,
    required this.encrypted,
    required this.channel,
  });

  factory WifiNetworkModel.fromJson(Map<String, dynamic> json) {
    return WifiNetworkModel(
      ssid: json['ssid'] ?? '',
      rssi: (json['rssi'] ?? 0) as int,
      encrypted: json['encrypted'] ?? false,
      channel: (json['channel'] ?? 0) as int,
    );
  }
}