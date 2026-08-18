import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ats_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/metric_card.dart';
import '../widgets/power_flow_painter.dart';
import '../widgets/toast_helper.dart';

class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final state = provider.state;
    final isConnected = provider.isConnected;

    return RefreshIndicator(
      onRefresh: provider.manualRefresh,
      color: AtsColors.primary,
      backgroundColor: AtsColors.surface,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (!isConnected) ...[
            Container(
              margin: const EdgeInsets.only(bottom: 16),
              padding: const EdgeInsets.all(14),
              decoration: BoxDecoration(
                color: AtsColors.warning.withValues(alpha: 0.12),
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: AtsColors.warning.withValues(alpha: 0.4)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.sync_problem, color: AtsColors.warning, size: 24),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      provider.lastError.isEmpty
                          ? 'Connecting to ESP32 at ${provider.baseUrl}'
                          : provider.lastError,
                      style: const TextStyle(fontSize: 12, color: AtsColors.textSecondary),
                    ),
                  ),
                ],
              ),
            ),
          ],

          // Trip or Emergency warning
          if (state.protTrip || state.emergency) ...[
            Container(
              margin: const EdgeInsets.only(bottom: 16),
              padding: const EdgeInsets.all(14),
              decoration: BoxDecoration(
                color: AtsColors.danger.withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: AtsColors.danger.withValues(alpha: 0.4)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.warning_amber_rounded, color: AtsColors.danger, size: 28),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          state.emergency
                              ? 'EMERGENCY SHUTOFF ACTIVE'
                              : 'PROTECTION TRIP: ${state.faultName}',
                          style: const TextStyle(
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                            color: Colors.white,
                          ),
                        ),
                        Text(
                          state.protReason.isNotEmpty ? state.protReason : 'All load branches isolated.',
                          style: const TextStyle(fontSize: 10, color: AtsColors.textSecondary),
                        ),
                      ],
                    ),
                  ),
                  TextButton(
                    onPressed: () async {
                      final r = state.emergency
                          ? await provider.restorePower()
                          : await provider.clearFault();
                      ToastHelper.show(context, r.ok ? 'Restored — relays re-energised' : 'Restore failed: ${r.message}', r.ok ? 'success' : 'error');
                    },
                    style: TextButton.styleFrom(
                      backgroundColor: AtsColors.danger,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                    ),
                    child: Text(state.emergency ? 'RESTORE' : 'RESET', style: const TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                  ),
                ],
              ),
            ),
          ],

          // 4 Key Metrics Bento Grid
          GridView.count(
            crossAxisCount: 2,
            crossAxisSpacing: 12,
            mainAxisSpacing: 12,
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            childAspectRatio: 1.25,
            children: [
              MetricCard(
                title: 'Line Voltage',
                value: state.voltage.toStringAsFixed(1),
                unit: 'V',
                icon: Icons.bolt,
                accentColor: AtsColors.primary,
                subtitle: state.pzemOK ? 'PZEM online' : 'PZEM offline',
                status: state.voltage > state.ovVolt && state.ovVolt > 0
                    ? 'HIGH'
                    : state.voltage < state.uvVolt && state.uvVolt > 0
                        ? 'LOW'
                        : 'NORMAL',
                statusColor: (state.voltage > state.ovVolt && state.ovVolt > 0) ||
                        (state.voltage < state.uvVolt && state.uvVolt > 0)
                    ? AtsColors.warning
                    : AtsColors.success,
              ),
              MetricCard(
                title: 'Load Current',
                value: state.current.toStringAsFixed(2),
                unit: 'A',
                icon: Icons.show_chart,
                accentColor: const Color(0xFF06B6D4),
                subtitle: 'Limit ${state.ocCurr.toInt()}A',
                status: state.ocCurr > 0
                    ? '${((state.current / state.ocCurr) * 100).toInt()}% Load'
                    : '0% Load',
                statusColor: const Color(0xFF06B6D4),
              ),
              MetricCard(
                title: 'Active Power',
                value: (state.power / 1000).toStringAsFixed(2),
                unit: 'kW',
                icon: Icons.lightbulb_outline,
                accentColor: AtsColors.warning,
                subtitle: '${state.power.toInt()} Watts',
                status: state.pzemOK ? 'Live' : 'Idle',
                statusColor: state.pzemOK ? AtsColors.success : AtsColors.textMuted,
              ),
              MetricCard(
                title: 'Energy',
                value: state.energy.toStringAsFixed(2),
                unit: 'kWh',
                icon: Icons.speed,
                accentColor: AtsColors.success,
                subtitle: 'Active meter ${state.activeMeter + 1}',
                status: 'Total',
                statusColor: AtsColors.success,
              ),
            ],
          ),

          const SizedBox(height: 16),

          // Power Flow Topology Card
          PowerFlowWidget(
            state: state,
            onSelectMeter: (idx) async {
              final r = await provider.switchSource(idx);
              ToastHelper.show(context, r.ok ? 'Switched to Meter ${idx + 1}' : 'Switch failed: ${r.message}', r.ok ? 'success' : 'error');
            },
          ),

          const SizedBox(height: 16),

          // Energy summary strip
          _EnergyStrip(state: state),

          const SizedBox(height: 16),

          // Quick Control Actions Row
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: () async {
                    final r = await provider.toggleBypass(!state.bypass);
                    ToastHelper.show(context, r.ok ? (state.bypass ? 'Bypass disabled' : 'Bypass enabled') : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
                  },
                  style: ElevatedButton.styleFrom(
                    backgroundColor: state.bypass ? AtsColors.warning : AtsColors.surface,
                    foregroundColor: state.bypass ? Colors.black : Colors.white,
                    side: const BorderSide(color: AtsColors.border),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                    padding: const EdgeInsets.symmetric(vertical: 12),
                  ),
                  icon: const Icon(Icons.sync_alt, size: 16),
                  label: Text(
                    state.bypass ? 'BYPASS: ON' : 'BYPASS MODE',
                    style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w700),
                  ),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: () => _confirmEmergency(context, provider),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: AtsColors.danger,
                    foregroundColor: Colors.white,
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                    padding: const EdgeInsets.symmetric(vertical: 12),
                  ),
                  icon: const Icon(Icons.power_settings_new, size: 16),
                  label: const Text(
                    'E-STOP CUTOFF',
                    style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700),
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Future<void> _confirmEmergency(BuildContext context, AtsProvider provider) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AtsColors.surface,
        title: const Text('EMERGENCY SHUTOFF', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w700, color: AtsColors.danger)),
        content: const Text(
          'This de-energises ALL relays immediately and latches the emergency state until a meter switch is performed.',
          style: TextStyle(fontSize: 12, color: AtsColors.textSecondary),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('CANCEL')),
          ElevatedButton(
            onPressed: () => Navigator.pop(ctx, true),
            style: ElevatedButton.styleFrom(backgroundColor: AtsColors.danger, foregroundColor: Colors.white),
            child: const Text('CONFIRM CUTOFF'),
          ),
        ],
      ),
    );

    if (confirmed == true && context.mounted) {
      final r = await provider.emergencyCutoff();
      ToastHelper.show(context, r.ok ? 'EMERGENCY CUTOFF ENGAGED' : 'Failed: ${r.message}', r.ok ? 'error' : 'error');
    }
  }
}

class _EnergyStrip extends StatelessWidget {
  final dynamic state;

  const _EnergyStrip({required this.state});

  @override
  Widget build(BuildContext context) {
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
            'ENERGY ANALYTICS',
            style: TextStyle(
              fontSize: 11,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.8,
              color: AtsColors.textSecondary,
            ),
          ),
          const SizedBox(height: 12),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              _Stat(label: 'Today', value: '${state.todayUsed.toStringAsFixed(3)}', unit: 'kWh'),
              _Stat(label: 'This Month', value: '${state.thisMonth.toStringAsFixed(2)}', unit: 'kWh'),
              _Stat(label: 'Last Month', value: '${state.lastMonth.toStringAsFixed(2)}', unit: 'kWh'),
            ],
          ),
        ],
      ),
    );
  }
}

class _Stat extends StatelessWidget {
  final String label;
  final String value;
  final String unit;

  const _Stat({required this.label, required this.value, required this.unit});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(
          label.toUpperCase(),
          style: const TextStyle(fontSize: 9, letterSpacing: 0.5, color: AtsColors.textMuted),
        ),
        const SizedBox(height: 4),
        Text(
          value,
          style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w700, color: Colors.white),
        ),
        Text(unit, style: const TextStyle(fontSize: 9, color: AtsColors.textMuted)),
      ],
    );
  }
}