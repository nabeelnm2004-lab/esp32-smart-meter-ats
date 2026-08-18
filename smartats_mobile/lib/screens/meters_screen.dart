import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ats_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/toast_helper.dart';

class MetersScreen extends StatefulWidget {
  const MetersScreen({super.key});

  @override
  State<MetersScreen> createState() => _MetersScreenState();
}

class _MetersScreenState extends State<MetersScreen> {
  final List<TextEditingController> _limitControllers = [];
  bool _dirty = false;
  Timer? _resetDayTimer;
  int? _pendingResetDay;

  void _syncControllers(AtsProvider provider) {
    final count = provider.state.meterCount;
    while (_limitControllers.length < count) {
      final controller = TextEditingController();
      _limitControllers.add(controller);
      final i = _limitControllers.length - 1;
      final limit = provider.state.limits.length > i ? provider.state.limits[i] : 1.0;
      controller.text = limit.toStringAsFixed(1);
    }
    while (_limitControllers.length > count) {
      _limitControllers.removeLast().dispose();
    }
  }

  @override
  void dispose() {
    _resetDayTimer?.cancel();
    for (final c in _limitControllers) {
      c.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final state = provider.state;
    _syncControllers(provider);

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const Text(
          'METER BRANCHES & QUOTAS',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 6),
        Text(
          '${state.meterCount} of ${state.maxMeters} meter slots active • Active source: Meter ${state.activeMeter + 1}',
          style: const TextStyle(fontSize: 11, color: AtsColors.textMuted),
        ),
        const SizedBox(height: 14),

        ...List.generate(state.meterCount, (idx) => _buildMeterCard(context, provider, state, idx)),

        const SizedBox(height: 20),

        _buildSlotControls(context, provider, state),

        const SizedBox(height: 20),

        // Save all limits button
        ElevatedButton.icon(
          onPressed: () async {
            final limits = <int, double>{};
            var outOfRange = false;
            for (var i = 0; i < _limitControllers.length; i++) {
              final v = double.tryParse(_limitControllers[i].text);
              if (v == null) continue;
              if (v < 0.1 || v > 9999) {
                outOfRange = true;
                continue;
              }
              limits[i] = v;
            }
            if (outOfRange) {
              ToastHelper.show(context, 'Limits must be between 0.1 and 9999 kWh', 'error');
              return;
            }
            if (limits.isEmpty) {
              ToastHelper.show(context, 'Enter at least one valid limit (0.1–9999 kWh)', 'error');
              return;
            }
            final r = await provider.setLimits(limits);
            _dirty = false;
            setState(() {});
            ToastHelper.show(context, r.ok ? 'Energy limits saved to ESP32' : 'Save failed: ${r.message}', r.ok ? 'success' : 'error');
          },
          style: ElevatedButton.styleFrom(
            backgroundColor: AtsColors.primary,
            foregroundColor: Colors.white,
            padding: const EdgeInsets.symmetric(vertical: 14),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
          icon: const Icon(Icons.save, size: 16),
          label: Text(
            _dirty ? 'SAVE LIMITS • UNSAVED' : 'SAVE ALL ENERGY LIMITS',
            style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w700),
          ),
        ),

        const SizedBox(height: 12),

        OutlinedButton.icon(
          onPressed: () async {
            final r = await provider.resetEnergy();
            ToastHelper.show(context, r.ok ? 'Energy counters reset to 0.000' : 'Reset failed: ${r.message}', r.ok ? 'warning' : 'error');
          },
          style: OutlinedButton.styleFrom(
            foregroundColor: AtsColors.warning,
            side: const BorderSide(color: AtsColors.border),
            padding: const EdgeInsets.symmetric(vertical: 14),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
          icon: const Icon(Icons.restart_alt, size: 16),
          label: const Text('RESET ENERGY COUNTERS', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700)),
        ),
      ],
    );
  }

  Widget _buildMeterCard(BuildContext context, AtsProvider provider, dynamic state, int idx) {
    final isActive = idx == state.activeMeter;
    final isEnabled = state.enabled.length > idx ? state.enabled[idx] : true;
    final used = state.used.length > idx ? state.used[idx] : 0.0;
    final limit = state.limits.length > idx ? state.limits[idx] : 0.1;
    final percentage = limit > 0 ? (used / limit * 100).clamp(0, 100) : 0.0;
    final nearLimit = limit > 0 && used / limit > 0.8;

    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AtsColors.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(
          color: isActive ? AtsColors.primary.withValues(alpha: 0.6) : AtsColors.border,
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(Icons.bolt, size: 18, color: isActive ? AtsColors.primary : AtsColors.textMuted),
              const SizedBox(width: 8),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'METER ${idx + 1}',
                      style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w700, color: Colors.white),
                    ),
                    Text(
                      '${used.toStringAsFixed(3)} / ${limit.toStringAsFixed(1)} kWh',
                      style: const TextStyle(fontSize: 10, color: AtsColors.textMuted),
                    ),
                  ],
                ),
              ),
              if (isActive)
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: AtsColors.primary,
                    borderRadius: BorderRadius.circular(6),
                  ),
                  child: const Text('ACTIVE', style: TextStyle(fontSize: 9, fontWeight: FontWeight.w700)),
                ),
              Switch(
                value: isEnabled,
                activeThumbColor: AtsColors.success,
                onChanged: (val) async {
                  final r = await provider.setMeterEnabled(idx, val);
                  ToastHelper.show(context, r.ok ? 'Meter ${idx + 1} ${val ? 'ENABLED' : 'DISABLED'}' : 'Failed: ${r.message}', r.ok ? 'info' : 'error');
                },
              ),
            ],
          ),

          const SizedBox(height: 8),
          ClipRRect(
            borderRadius: BorderRadius.circular(4),
            child: LinearProgressIndicator(
              value: percentage / 100,
              minHeight: 6,
              backgroundColor: AtsColors.background,
              valueColor: AlwaysStoppedAnimation<Color>(
                nearLimit ? AtsColors.warning : AtsColors.success,
              ),
            ),
          ),
          const SizedBox(height: 4),
          Text(
            '${percentage.toInt()}% of quota used',
            style: const TextStyle(fontSize: 9, color: AtsColors.textMuted),
          ),

          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: _limitControllers[idx],
                  keyboardType: const TextInputType.numberWithOptions(decimal: true),
                  onChanged: (_) => setState(() => _dirty = true),
                  style: const TextStyle(fontSize: 12),
                  decoration: InputDecoration(
                    labelText: 'Limit (kWh)',
                    hintText: '> 1.0 kWh (suggested)',
                    border: OutlineInputBorder(borderRadius: BorderRadius.circular(10)),
                    contentPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                  ),
                ),
              ),
              const SizedBox(width: 8),
              if (!isActive)
                SizedBox(
                  height: 40,
                  child: ElevatedButton.icon(
                    onPressed: () async {
                      final r = await provider.switchSource(idx);
                      if (r.ok) {
                        ToastHelper.show(context, 'ATS transferred to Meter ${idx + 1}', 'success');
                      } else if (r.message.contains('emergency')) {
                        ToastHelper.show(context, 'Switch failed — emergency OFF is active. Use RESTORE on Dashboard/Protection.', 'error');
                      } else {
                        ToastHelper.show(context, 'Switch failed: ${r.message}', 'error');
                      }
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AtsColors.primary,
                      foregroundColor: Colors.white,
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                    ),
                    icon: const Icon(Icons.sync, size: 14),
                    label: const Text('SWITCH', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                  ),
                )
              else
                Container(
                  height: 40,
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                  alignment: Alignment.center,
                  decoration: BoxDecoration(
                    color: AtsColors.primary.withValues(alpha: 0.12),
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: AtsColors.primary.withValues(alpha: 0.4)),
                  ),
                  child: const Text('IN USE', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: AtsColors.primary)),
                ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildSlotControls(BuildContext context, AtsProvider provider, dynamic state) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AtsColors.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AtsColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'SLOT MANAGEMENT',
            style: TextStyle(
              fontSize: 11,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.8,
              color: AtsColors.textSecondary,
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () async {
                    final r = await provider.addMeter();
                    ToastHelper.show(context, r.ok ? 'New meter branch added' : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
                  },
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.success,
                    side: const BorderSide(color: AtsColors.border),
                  ),
                  icon: const Icon(Icons.add, size: 16),
                  label: const Text('ADD METER', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () async {
                    if (state.meterCount <= 1) {
                      ToastHelper.show(context, 'Cannot remove the last meter', 'error');
                      return;
                    }
                    final r = await provider.removeMeter(state.meterCount - 1);
                    ToastHelper.show(context, r.ok ? 'Last meter removed' : 'Failed: ${r.message}', r.ok ? 'warning' : 'error');
                  },
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.danger,
                    side: const BorderSide(color: AtsColors.border),
                  ),
                  icon: const Icon(Icons.remove, size: 16),
                  label: const Text('REMOVE LAST', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          const Text(
            'Monthly energy counter reset day:',
            style: TextStyle(fontSize: 10, color: AtsColors.textMuted),
          ),
          const SizedBox(height: 6),
          Row(
            children: [
              Expanded(
                child: Slider(
                  value: (_pendingResetDay ?? state.resetDay as num).clamp(1, 28).toDouble(),
                  min: 1,
                  max: 28,
                  divisions: 27,
                  activeColor: AtsColors.primary,
                  inactiveColor: AtsColors.background,
                  label: _pendingResetDay != null ? 'Day $_pendingResetDay • saving…' : 'Day ${state.resetDay}',
                  onChanged: (val) {
                    setState(() => _pendingResetDay = val.round());
                    _resetDayTimer?.cancel();
                    _resetDayTimer = Timer(const Duration(seconds: 3), () async {
                      final day = _pendingResetDay ?? (state.resetDay as num).round();
                      final r = await provider.setResetDay(day);
                      if (!mounted) return;
                      setState(() => _pendingResetDay = null);
                      ToastHelper.show(
                        context,
                        r.ok ? 'Reset day saved to Day $day' : 'Save failed: ${r.message}',
                        r.ok ? 'success' : 'error',
                      );
                    });
                  },
                ),
              ),
              const SizedBox(width: 8),
              Text(
                'Day ${_pendingResetDay ?? state.resetDay}',
                style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: AtsColors.primary),
              ),
            ],
          ),
        ],
      ),
    );
  }
}