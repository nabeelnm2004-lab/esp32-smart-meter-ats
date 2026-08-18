class EventLogItem {
  final String time;
  final String message;

  const EventLogItem({required this.time, required this.message});

  factory EventLogItem.fromJson(Map<String, dynamic> json) {
    return EventLogItem(
      time: json['t'] ?? '',
      message: json['m'] ?? '',
    );
  }
}