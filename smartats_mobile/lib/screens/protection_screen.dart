import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ats_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/toast_helper.dart';

class ProtectionScreen extends StatefulWidget {
  const ProtectionScreen({super.key});

  @override
  State<ProtectionScreen> createState() => _ProtectionScreenState();
}

class _ProtectionScreenState extends State<ProtectionScreen> {
  double _ovVolt = 250;
  double _uvVolt = 180;
  double _ocCurr = 16;
  int _ovRec = 5;
  int _uvRec = 5;
  int _ocRec = 10;
  bool _initialized = false;

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final state = provider.state;

    if (!_initialized && state.ovVolt > 0) {
      _ovVolt = state.ovVolt;
      _uvVolt = state.uvVolt;
      _ocCurr = state.ocCurr;
      _ovRec = state.ovRec;
      _uvRec = state.uvRec;
      _ocRec = state.ocRec;
      _initialized = true;
    }

    final faultActive = state.protTrip || state.emergency;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Auto-recovery / fault status banner
        _buildFaultStatus(state),

        if (faultActive) ...[
          const SizedBox(height: 12),
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: () async {
                final r = state.emergency
                    ? await provider.restorePower()
                    : await provider.clearFault();
                ToastHelper.show(context, r.ok ? 'Restored — relays re-energised' : 'Restore failed: ${r.message}', r.ok ? 'success' : 'error');
              },
              style: ElevatedButton.styleFrom(
                backgroundColor: AtsColors.danger,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 14),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
              ),
              icon: const Icon(Icons.refresh, size: 16),
              label: Text(
                state.emergency ? 'RESTORE POWER' : 'CLEAR FAULT / RECOVER',
                style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w700),
              ),
            ),
          ),
        ],

        const SizedBox(height: 16),

        const Text(
          'SAFETY & PROTECTION MATRIX',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 14),

        _buildSliderCard(
          title: 'Over-Voltage Trip Threshold',
          value: '${_ovVolt.toInt()} V',
          min: 220,
          max: 280,
          current: _ovVolt,
          onChanged: (val) => setState(() => _ovVolt = val),
          accentColor: AtsColors.danger,
        ),

        const SizedBox(height: 12),

        _buildSliderCard(
          title: 'Under-Voltage Trip Threshold',
          value: '${_uvVolt.toInt()} V',
          min: 150,
          max: 220,
          current: _uvVolt,
          onChanged: (val) => setState(() => _uvVolt = val),
          accentColor: AtsColors.warning,
        ),

        const SizedBox(height: 12),

        _buildSliderCard(
          title: 'Over-Current Trip Threshold',
          value: '${_ocCurr.toInt()} A',
          min: 5,
          max: 40,
          current: _ocCurr,
          onChanged: (val) => setState(() => _ocCurr = val),
          accentColor: const Color(0xFF06B6D4),
        ),

        const SizedBox(height: 12),

        const Text(
          'AUTO-RECOVERY WINDOWS',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 12),

        _buildRecoveryRow('OV Recovery', _ovRec, (v) => setState(() => _ovRec = v)),
        const SizedBox(height: 8),
        _buildRecoveryRow('UV Recovery', _uvRec, (v) => setState(() => _uvRec = v)),
        const SizedBox(height: 8),
        _buildRecoveryRow('OC Recovery', _ocRec, (v) => setState(() => _ocRec = v)),

        const SizedBox(height: 20),

        ElevatedButton(
          onPressed: () async {
            final r = await provider.updateProtection(
              ov: _ovVolt,
              uv: _uvVolt,
              oc: _ocCurr,
              ovRec: _ovRec,
              uvRec: _uvRec,
              ocRec: _ocRec,
            );
            ToastHelper.show(context, r.ok ? 'Protection parameters saved to ESP32' : 'Save failed: ${r.message}', r.ok ? 'success' : 'error');
          },
          style: ElevatedButton.styleFrom(
            backgroundColor: AtsColors.primary,
            foregroundColor: Colors.white,
            padding: const EdgeInsets.symmetric(vertical: 14),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
          child: const Text('SAVE PARAMETERS TO HARDWARE', style: TextStyle(fontWeight: FontWeight.w700, fontSize: 11)),
        ),

        const SizedBox(height: 12),

        SizedBox(
          width: double.infinity,
          child: OutlinedButton.icon(
            onPressed: () async {
              final r = await provider.emergencyCutoff();
              ToastHelper.show(context, r.ok ? 'EMERGENCY CUTOFF ENGAGED' : 'Failed: ${r.message}', 'error');
            },
            style: OutlinedButton.styleFrom(
              foregroundColor: AtsColors.danger,
              side: BorderSide(color: AtsColors.danger.withValues(alpha: 0.5)),
              padding: const EdgeInsets.symmetric(vertical: 14),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
            ),
            icon: const Icon(Icons.power_settings_new, size: 16),
            label: const Text('TRIGGER EMERGENCY TRIP', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700)),
          ),
        ),
      ],
    );
  }

  Widget _buildFaultStatus(dynamic state) {
    final faultStatus = state.faultStatus;
    final recovering = faultStatus == 'recovering';
    final tripped = state.protTrip || state.emergency;

    final Color color;
    final String label;
    String detail = state.protReason.isNotEmpty ? state.protReason : '';

    if (state.emergency) {
      color = AtsColors.danger;
      label = 'EMERGENCY SHUTOFF ACTIVE';
    } else if (tripped) {
      color = AtsColors.danger;
      label = 'PROTECTION TRIPPED • ${state.faultName}';
    } else if (recovering) {
      color = AtsColors.warning;
      label = 'AUTO-RECOVERING';
      detail = 'Stability window: ${state.recoverCountdown}s / ${state.recoverDelay}s';
    } else {
      color = AtsColors.success;
      label = 'SYSTEM NORMAL';
    }

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: color.withValues(alpha: 0.4)),
      ),
      child: Row(
        children: [
          Icon(
            tripped ? Icons.warning_amber_rounded : Icons.check_circle_outline,
            color: color,
            size: 24,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  label,
                  style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: Colors.white),
                ),
                if (detail.isNotEmpty)
                  Text(
                    detail,
                    style: const TextStyle(fontSize: 10, color: AtsColors.textSecondary),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSliderCard({
    required String title,
    required String value,
    required double min,
    required double max,
    required double current,
    required ValueChanged<double> onChanged,
    required Color accentColor,
  }) {
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
              Text(
                title,
                style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: Colors.white),
              ),
              Text(
                value,
                style: TextStyle(fontSize: 14, fontWeight: FontWeight.w700, color: accentColor),
              ),
            ],
          ),
          Slider(
            value: current.clamp(min, max),
            min: min,
            max: max,
            activeColor: accentColor,
            inactiveColor: AtsColors.background,
            onChanged: onChanged,
          ),
        ],
      ),
    );
  }

  Widget _buildRecoveryRow(String title, int seconds, ValueChanged<int> onChanged) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
      decoration: BoxDecoration(
        color: AtsColors.surface,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AtsColors.border),
      ),
      child: Row(
        children: [
          Expanded(
            child: Text(title, style: const TextStyle(fontSize: 11, color: AtsColors.textSecondary)),
          ),
          DropdownButton<int>(
            value: seconds,
            dropdownColor: AtsColors.surfaceElevated,
            style: const TextStyle(fontSize: 12, color: Colors.white),
            underline: const SizedBox.shrink(),
            items: [1, 2, 3, 5, 10, 15, 30, 60, 120, 180]
                .map((s) => DropdownMenuItem(value: s, child: Text('${s}s')))
                .toList(),
            onChanged: (v) => onChanged(v ?? seconds),
          ),
        ],
      ),
    );
  }
}