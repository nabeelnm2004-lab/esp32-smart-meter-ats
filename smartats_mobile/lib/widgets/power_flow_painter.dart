import 'package:flutter/material.dart';
import '../theme/app_theme.dart';
import '../models/system_state.dart';

class PowerFlowWidget extends StatelessWidget {
  final SystemStateModel state;
  final Function(int) onSelectMeter;

  const PowerFlowWidget({
    super.key,
    required this.state,
    required this.onSelectMeter,
  });

  @override
  Widget build(BuildContext context) {
    final count = state.meterCount;
    final activeIndex = state.activeMeter;

    final isEnergized = state.voltage > 0 &&
        !state.protTrip &&
        !state.emergency &&
        !state.testMode;

    return Container(
      decoration: BoxDecoration(
        color: AtsColors.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AtsColors.border),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Row(
                children: [
                  Container(
                    width: 8,
                    height: 8,
                    decoration: BoxDecoration(
                      color: isEnergized ? AtsColors.success : AtsColors.danger,
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 8),
                  const Text(
                    'ATS POWER FLOW',
                    style: TextStyle(
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 0.8,
                      color: AtsColors.textSecondary,
                    ),
                  ),
                ],
              ),
              Text(
                isEnergized ? 'BUS ENERGIZED' : 'BUS ISOLATED',
                style: TextStyle(
                  fontSize: 10,
                  fontWeight: FontWeight.w700,
                  color: isEnergized ? AtsColors.success : AtsColors.danger,
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),

          if (count == 0)
            const Padding(
              padding: EdgeInsets.symmetric(vertical: 24),
              child: Center(
                child: Text(
                  'No meter data — waiting for ESP32',
                  style: TextStyle(fontSize: 12, color: AtsColors.textMuted),
                ),
              ),
            )
          else
            ...List.generate(count, (idx) {
              final isSelected = idx == activeIndex;
              final isEnabled = state.enabled.length > idx ? state.enabled[idx] : true;
              final used = state.used.length > idx ? state.used[idx] : 0.0;
              final limit = state.limits.length > idx ? state.limits[idx] : 5.0;

              return InkWell(
                onTap: () => onSelectMeter(idx),
                borderRadius: BorderRadius.circular(12),
                child: Container(
                  margin: const EdgeInsets.only(bottom: 8),
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                  decoration: BoxDecoration(
                    color: isSelected
                        ? AtsColors.primary.withValues(alpha: 0.12)
                        : AtsColors.background,
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(
                      color: isSelected
                          ? AtsColors.primary.withValues(alpha: 0.5)
                          : AtsColors.border,
                    ),
                  ),
                  child: Row(
                    children: [
                      Icon(
                        Icons.bolt,
                        color: isSelected ? AtsColors.primary : AtsColors.textSecondary,
                        size: 20,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Row(
                              children: [
                                Text(
                                  'METER ${idx + 1}',
                                  style: TextStyle(
                                    fontSize: 13,
                                    fontWeight: isSelected ? FontWeight.w700 : FontWeight.w500,
                                    color: isSelected ? Colors.white : AtsColors.textSecondary,
                                  ),
                                ),
                                if (!isEnabled) ...[
                                  const SizedBox(width: 8),
                                  Container(
                                    padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                                    decoration: BoxDecoration(
                                      color: AtsColors.textMuted.withValues(alpha: 0.2),
                                      borderRadius: BorderRadius.circular(4),
                                    ),
                                    child: const Text(
                                      'OFF',
                                      style: TextStyle(fontSize: 8, fontWeight: FontWeight.w700, color: AtsColors.textMuted),
                                    ),
                                  ),
                                ],
                              ],
                            ),
                            Text(
                              '${used.toStringAsFixed(1)} / ${limit.toStringAsFixed(1)} kWh',
                              style: const TextStyle(
                                fontSize: 10,
                                color: AtsColors.textMuted,
                              ),
                            ),
                          ],
                        ),
                      ),
                      if (isSelected)
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                          decoration: BoxDecoration(
                            color: AtsColors.primary,
                            borderRadius: BorderRadius.circular(6),
                          ),
                          child: const Text(
                            'ACTIVE',
                            style: TextStyle(
                              fontSize: 9,
                              fontWeight: FontWeight.w700,
                              color: Colors.white,
                            ),
                          ),
                        ),
                    ],
                  ),
                ),
              );
            }),
        ],
      ),
    );
  }
}