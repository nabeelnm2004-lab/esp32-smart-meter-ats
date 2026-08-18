import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../providers/ats_provider.dart';
import '../theme/app_theme.dart';
import '../models/event_log_item.dart';
import '../widgets/toast_helper.dart';

class SystemScreen extends StatefulWidget {
  const SystemScreen({super.key});

  @override
  State<SystemScreen> createState() => _SystemScreenState();
}

class _SystemScreenState extends State<SystemScreen> {
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final info = provider.sysInfo;
    final state = provider.state;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const Text(
          'HARDWARE HEALTH',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 14),

        _buildHealthCard(context, provider, info, state),
        const SizedBox(height: 16),

        const Text(
          'MAINTENANCE & ADMIN',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 14),

        _buildMaintenanceCard(context, provider, state),
        const SizedBox(height: 16),

        const Text(
          'EVENT LOG',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 14),

        _buildEventLog(context, provider),
      ],
    );
  }

  Widget _buildHealthCard(BuildContext context, AtsProvider provider, dynamic info, dynamic state) {
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
          _kv('Firmware', info.fw),
          _kv('Build', info.build),
          _kv('Chip', info.chip),
          _kv('Flash', '${(info.flash / 1024 / 1024).toStringAsFixed(0)} MB'),
          _kv('Free Heap', '${(info.heap / 1024).toStringAsFixed(0)} KB'),
          _kv('CPU', '${info.cpu} MHz'),
          _kv('Uptime', _fmtUptime(info.uptime)),
          _kv('Reset reason', info.rstReason),
          _kv('Hostname', info.hostname),
          _kv('IP address', info.ip),
          _kv('Time source', info.timeSrc),
          _kv('RTC', info.rtcOK ? 'Online • ${info.rtcTime}' : 'Offline'),
          if (info.rtcLostPower) _kv('RTC battery', 'LOST POWER'),
          _kv('Meter slots', '${info.meterCount}'),
        ],
      ),
    );
  }

  Widget _buildMaintenanceCard(BuildContext context, AtsProvider provider, dynamic state) {
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
          const Text('RELAY TEST MODE', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700, color: AtsColors.textSecondary)),
          const SizedBox(height: 8),
          Row(
            children: [
              Icon(state.testMode ? Icons.build : Icons.build_outlined, size: 18, color: state.testMode ? AtsColors.warning : AtsColors.textMuted),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  state.testMode ? 'Test mode ACTIVE — ${state.testLeft}s remaining' : 'Relay test mode is off',
                  style: const TextStyle(fontSize: 11, color: AtsColors.textSecondary),
                ),
              ),
              ElevatedButton(
                onPressed: () async {
                  final r = await provider.setTestMode(!state.testMode);
                  ToastHelper.show(context, r.ok ? (state.testMode ? 'Test mode exited' : 'Test mode ENABLED (5m timeout)') : 'Failed: ${r.message}', r.ok ? 'warning' : 'error');
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: state.testMode ? AtsColors.warning : AtsColors.surface,
                  foregroundColor: state.testMode ? Colors.black : Colors.white,
                  side: const BorderSide(color: AtsColors.border),
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                ),
                child: Text(state.testMode ? 'EXIT' : 'ENTER', style: const TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
              ),
            ],
          ),
          if (state.testMode) ...[
            const SizedBox(height: 10),
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: () async {
                      final r = await provider.testRelay(0);
                      ToastHelper.show(context, r.ok ? 'Relay 1 toggled' : 'Failed: ${r.message}', r.ok ? 'info' : 'error');
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AtsColors.primary,
                      foregroundColor: Colors.white,
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                    ),
                    icon: const Icon(Icons.toggle_on, size: 14),
                    label: const Text('RELAY 1', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: () async {
                      final r = await provider.testRelay(1);
                      ToastHelper.show(context, r.ok ? 'Relay 2 toggled' : 'Failed: ${r.message}', r.ok ? 'info' : 'error');
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AtsColors.primary,
                      foregroundColor: Colors.white,
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                    ),
                    icon: const Icon(Icons.toggle_on, size: 14),
                    label: const Text('RELAY 2', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                  ),
                ),
              ],
            ),
          ],
          const Divider(color: AtsColors.border, height: 24),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () async {
                    final r = await provider.syncTime(DateTime.now());
                    ToastHelper.show(context, r.ok ? 'RTC synced with phone clock' : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
                  },
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.primary,
                    side: const BorderSide(color: AtsColors.border),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.schedule, size: 14),
                  label: const Text('SYNC RTC TIME', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () async {
                    final json = await provider.exportBackup();
                    if (json == null) {
                      ToastHelper.show(context, 'Backup failed or not authorized', 'error');
                    } else {
                      await _shareBackup(context, json);
                    }
                  },
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.success,
                    side: const BorderSide(color: AtsColors.border),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.download, size: 14),
                  label: const Text('BACKUP', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton.icon(
              onPressed: () => _showRestoreDialog(context, provider),
              style: OutlinedButton.styleFrom(
                foregroundColor: AtsColors.success,
                side: const BorderSide(color: AtsColors.border),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
              ),
              icon: const Icon(Icons.upload, size: 14),
              label: const Text('RESTORE CONFIGURATION', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
            ),
          ),
          const Divider(color: AtsColors.border, height: 24),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () => _confirmAction(
                    context,
                    'Restart Device',
                    'Soft reboot with all settings preserved.',
                    () => provider.restartDevice(),
                  ),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.warning,
                    side: const BorderSide(color: AtsColors.border),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.restart_alt, size: 14),
                  label: const Text('RESTART', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () => _confirmAction(
                    context,
                    'FACTORY RESET',
                    'ERASE ALL NVS settings and reboot with factory defaults. This CANNOT be undone.',
                    () => provider.factoryResetDevice(),
                  ),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.danger,
                    side: BorderSide(color: AtsColors.danger.withValues(alpha: 0.5)),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.factory, size: 14),
                  label: const Text('FACTORY RESET', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Future<void> _shareBackup(BuildContext context, String json) async {
    final copied = await _copyToClipboard(json);
    if (context.mounted) {
      ToastHelper.show(
        context,
        copied ? 'Backup JSON copied to clipboard (contains passwords — store securely)' : 'Backup JSON ready',
        'success',
      );
    }
  }

  Future<bool> _copyToClipboard(String text) async {
    await Clipboard.setData(ClipboardData(text: text));
    return true;
  }

  Future<void> _showRestoreDialog(BuildContext context, AtsProvider provider) async {
    final controller = TextEditingController();

    await showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AtsColors.surface,
        title: const Text('Restore Configuration', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w700)),
        content: TextField(
          controller: controller,
          maxLines: 6,
          style: const TextStyle(fontSize: 11, fontFamily: 'monospace'),
          decoration: const InputDecoration(
            hintText: 'Paste the backup JSON here',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('CANCEL')),
          ElevatedButton(
            onPressed: () async {
              final text = controller.text.trim();
              Navigator.pop(ctx);
              if (text.isEmpty) {
                ToastHelper.show(context, 'Empty JSON', 'error');
                return;
              }
              try {
                jsonDecode(text);
              } catch (_) {
                ToastHelper.show(context, 'Invalid JSON', 'error');
                return;
              }
              final r = await provider.restoreBackup(text);
              ToastHelper.show(context, r.ok ? 'Configuration restored successfully!' : 'Restore failed: ${r.message}', r.ok ? 'success' : 'error');
            },
            child: const Text('RESTORE'),
          ),
        ],
      ),
    );
  }

  Future<void> _confirmAction(
    BuildContext context,
    String title,
    String message,
    Future<dynamic> Function() action,
  ) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AtsColors.surface,
        title: Text(title, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w700)),
        content: Text(message, style: const TextStyle(fontSize: 12, color: AtsColors.textSecondary)),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('CANCEL')),
          ElevatedButton(
            onPressed: () => Navigator.pop(ctx, true),
            style: ElevatedButton.styleFrom(
              backgroundColor: title.contains('FACTORY') ? AtsColors.danger : AtsColors.warning,
              foregroundColor: Colors.white,
            ),
            child: const Text('CONFIRM'),
          ),
        ],
      ),
    );

    if (confirmed == true && context.mounted) {
      final r = await action();
      final result = r as dynamic;
      final ok = result?.ok ?? true;
      final msg = result?.message ?? '';
      ToastHelper.show(
        context,
        ok ? (title.contains('FACTORY') ? 'Factory reset done. Device rebooting...' : 'Device restarting...') : 'Failed: $msg',
        ok ? 'warning' : 'error',
      );
    }
  }

  Widget _buildEventLog(BuildContext context, AtsProvider provider) {
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
          Row(
            children: [
              Text('${provider.events.length} records', style: const TextStyle(fontSize: 10, color: AtsColors.textMuted)),
              const Spacer(),
              TextButton.icon(
                onPressed: () => provider.fetchEvents(),
                style: TextButton.styleFrom(foregroundColor: AtsColors.primary, minimumSize: const Size(0, 32), padding: const EdgeInsets.symmetric(horizontal: 8)),
                icon: const Icon(Icons.refresh, size: 14),
                label: const Text('REFRESH', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
              ),
              TextButton.icon(
                onPressed: () async {
                  final r = await provider.clearEvents();
                  ToastHelper.show(context, r.ok ? 'Event log cleared' : 'Failed: ${r.message}', r.ok ? 'info' : 'error');
                },
                style: TextButton.styleFrom(foregroundColor: AtsColors.danger, minimumSize: const Size(0, 32), padding: const EdgeInsets.symmetric(horizontal: 8)),
                icon: const Icon(Icons.delete_sweep, size: 14),
                label: const Text('CLEAR', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
              ),
            ],
          ),
          const Divider(color: AtsColors.border, height: 1),
          const SizedBox(height: 4),
          if (provider.events.isEmpty)
            const Padding(
              padding: EdgeInsets.symmetric(vertical: 16),
              child: Center(
                child: Text('No events yet — tap REFRESH', style: TextStyle(fontSize: 11, color: AtsColors.textMuted)),
              ),
            )
          else
            ...provider.events.reversed.map((e) => _buildEventRow(e)),
        ],
      ),
    );
  }

  Widget _buildEventRow(EventLogItem event) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 6,
            height: 6,
            margin: const EdgeInsets.only(top: 5),
            decoration: const BoxDecoration(
              color: AtsColors.primary,
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 10),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                event.time,
                style: const TextStyle(fontSize: 9, color: AtsColors.textMuted, fontFamily: 'monospace'),
              ),
              Text(
                event.message,
                style: const TextStyle(fontSize: 11, color: Colors.white),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _kv(String key, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(key, style: const TextStyle(fontSize: 10, color: AtsColors.textMuted)),
          Text(value, style: const TextStyle(fontSize: 11, color: Colors.white, fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }

  String _fmtUptime(int seconds) {
    if (seconds <= 0) return '—';
    final d = seconds ~/ 86400;
    final h = (seconds % 86400) ~/ 3600;
    final m = (seconds % 3600) ~/ 60;
    return '${d}d ${h}h ${m}m';
  }
}