import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ats_provider.dart';
import '../theme/app_theme.dart';
import '../models/wifi_network.dart';
import '../widgets/toast_helper.dart';

class WifiScreen extends StatefulWidget {
  const WifiScreen({super.key});

  @override
  State<WifiScreen> createState() => _WifiScreenState();
}

class _WifiScreenState extends State<WifiScreen> {
  int _selectedMode = 2;

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final state = provider.state;

    if (_selectedMode != state.wifiMode && !provider.wifiScanRunning) {
      _selectedMode = state.wifiMode;
    }

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const Text(
          'WI-FI NETWORK',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 14),

        _buildModeCard(context, provider, state),
        const SizedBox(height: 12),
        _buildStatusCard(state),
        const SizedBox(height: 12),
        _buildScanCard(context, provider),

        const SizedBox(height: 16),

        const Text(
          'SAVE STATION CONFIGURATION',
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
            color: AtsColors.textSecondary,
          ),
        ),
        const SizedBox(height: 12),
        _buildStationConfig(context, provider),
      ],
    );
  }

  Widget _buildModeCard(BuildContext context, AtsProvider provider, dynamic state) {
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
          const Text('Wi-Fi MODE', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700, color: AtsColors.textSecondary)),
          const SizedBox(height: 10),
          SegmentedButton<int>(
            segments: const [
              ButtonSegment(value: 0, label: Text('AP Only', style: TextStyle(fontSize: 10)), icon: Icon(Icons.wifi_tethering, size: 16)),
              ButtonSegment(value: 1, label: Text('STA Only', style: TextStyle(fontSize: 10)), icon: Icon(Icons.wifi, size: 16)),
              ButtonSegment(value: 2, label: Text('AP + STA', style: TextStyle(fontSize: 10)), icon: Icon(Icons.wifi_tethering, size: 16)),
            ],
            selected: {_selectedMode},
            onSelectionChanged: (sel) async {
              final mode = sel.first;
              setState(() => _selectedMode = mode);
              final r = await provider.setWiFi(mode: mode);
              ToastHelper.show(context, r.ok ? 'Wi-Fi mode saved' : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
            },
            style: SegmentedButton.styleFrom(
              backgroundColor: AtsColors.background,
              selectedBackgroundColor: AtsColors.primary.withValues(alpha: 0.25),
              foregroundColor: AtsColors.textSecondary,
              selectedForegroundColor: Colors.white,
              side: const BorderSide(color: AtsColors.border),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildStatusCard(dynamic state) {
    final staOK = state.staOK;
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
              Icon(Icons.wifi, size: 18, color: staOK ? AtsColors.success : AtsColors.danger),
              const SizedBox(width: 8),
              const Text('STATION LINK', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700, color: AtsColors.textSecondary)),
              const Spacer(),
              Text(
                staOK ? 'CONNECTED' : 'DISCONNECTED',
                style: TextStyle(
                  fontSize: 10,
                  fontWeight: FontWeight.w700,
                  color: staOK ? AtsColors.success : AtsColors.danger,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          _kv('SSID', state.staSsid.isEmpty ? '—' : state.staSsid),
          _kv('Station IP', state.staIP.isEmpty ? '—' : state.staIP),
          _kv('AP IP', state.apIP.isEmpty ? '—' : state.apIP),
          _kv('Signal', state.staRssi == 0 ? '—' : '${state.staRssi} dBm'),
          _kv('Fallback', state.staFallback ? 'AP-only fallback active' : 'No'),
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

  Widget _buildScanCard(BuildContext context, AtsProvider provider) {
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
              const Text('SCANNED NETWORKS', style: TextStyle(fontSize: 11, fontWeight: FontWeight.w700, color: AtsColors.textSecondary)),
              const Spacer(),
              if (provider.wifiScanRunning)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CircularProgressIndicator(strokeWidth: 2, color: AtsColors.primary),
                )
              else
                TextButton.icon(
                  onPressed: () => provider.runWifiScan(),
                  style: TextButton.styleFrom(
                    foregroundColor: AtsColors.primary,
                    padding: const EdgeInsets.symmetric(horizontal: 8),
                    minimumSize: const Size(0, 32),
                  ),
                  icon: const Icon(Icons.radar, size: 14),
                  label: const Text('SCAN', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
            ],
          ),
          const SizedBox(height: 8),
          if (provider.networks.isEmpty && !provider.wifiScanRunning)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 12),
              child: Center(
                child: Text(
                  provider.scanMessage.isEmpty
                      ? 'Tap SCAN to discover nearby Wi-Fi networks'
                      : provider.scanMessage,
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 11,
                    color: provider.scanMessage.isEmpty ? AtsColors.textMuted : AtsColors.warning,
                  ),
                ),
              ),
            )
          else
            ...provider.networks.map((net) => _buildNetworkRow(context, provider, net)),
        ],
      ),
    );
  }

  Widget _buildNetworkRow(BuildContext context, AtsProvider provider, WifiNetworkModel net) {
    final strength = net.rssi >= -60 ? 'Strong' : net.rssi >= -75 ? 'Good' : 'Weak';
    return ListTile(
      dense: true,
      contentPadding: EdgeInsets.zero,
      leading: Icon(net.encrypted ? Icons.lock_outline : Icons.public, size: 18, color: AtsColors.textSecondary),
      title: Text(net.ssid, style: const TextStyle(fontSize: 12, color: Colors.white, fontWeight: FontWeight.w600)),
      subtitle: Text('CH ${net.channel} • $strength (${net.rssi} dBm)', style: const TextStyle(fontSize: 9, color: AtsColors.textMuted)),
      trailing: SizedBox(
        height: 32,
        child: ElevatedButton(
          onPressed: () => _showConnectDialog(context, provider, net.ssid),
          style: ElevatedButton.styleFrom(
            backgroundColor: AtsColors.primary,
            foregroundColor: Colors.white,
            padding: const EdgeInsets.symmetric(horizontal: 12),
          ),
          child: const Text('JOIN', style: TextStyle(fontSize: 9, fontWeight: FontWeight.w700)),
        ),
      ),
    );
  }

  void _showConnectDialog(BuildContext context, AtsProvider provider, String ssid) {
    final passController = TextEditingController();

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AtsColors.surface,
        title: Text('Join $ssid', style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w700)),
        content: TextField(
          controller: passController,
          obscureText: true,
          autocorrect: false,
          enableSuggestions: false,
          keyboardType: TextInputType.visiblePassword,
          decoration: const InputDecoration(
            labelText: 'Password',
            hintText: 'Leave empty for open network',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('CANCEL')),
          ElevatedButton(
            onPressed: () async {
              final r = await provider.connectWiFi(
                ssid: ssid,
                pass: passController.text.trim().isEmpty ? null : passController.text.trim(),
                mode: _selectedMode == 0 ? 1 : _selectedMode,
              );
              Navigator.pop(ctx);
              ToastHelper.show(context, r.ok ? 'Connecting to "$ssid"...' : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
            },
            child: const Text('CONNECT'),
          ),
        ],
      ),
    );
  }

  Widget _buildStationConfig(BuildContext context, AtsProvider provider) {
    final ssidController = TextEditingController(text: provider.state.staSsid);
    final passController = TextEditingController();

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AtsColors.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AtsColors.border),
      ),
      child: Column(
        children: [
          TextField(
            controller: ssidController,
            style: const TextStyle(fontSize: 12),
            decoration: InputDecoration(
              labelText: 'SSID',
              border: OutlineInputBorder(borderRadius: BorderRadius.circular(10)),
              contentPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
            ),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: passController,
            obscureText: true,
            autocorrect: false,
            enableSuggestions: false,
            keyboardType: TextInputType.visiblePassword,
            style: const TextStyle(fontSize: 12),
            decoration: InputDecoration(
              labelText: 'Password',
              border: OutlineInputBorder(borderRadius: BorderRadius.circular(10)),
              contentPadding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: () async {
                    final r = await provider.setWiFi(
                      mode: _selectedMode,
                      ssid: ssidController.text.trim(),
                      pass: passController.text.trim().isEmpty ? null : passController.text.trim(),
                    );
                    ToastHelper.show(context, r.ok ? 'Wi-Fi configuration saved' : 'Failed: ${r.message}', r.ok ? 'success' : 'error');
                  },
                  style: ElevatedButton.styleFrom(
                    backgroundColor: AtsColors.primary,
                    foregroundColor: Colors.white,
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.save, size: 14),
                  label: const Text('SAVE', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () async {
                    final r = await provider.disconnectWiFi();
                    ToastHelper.show(context, r.ok ? 'Station disconnected' : 'Failed: ${r.message}', r.ok ? 'warning' : 'error');
                  },
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AtsColors.warning,
                    side: const BorderSide(color: AtsColors.border),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                  ),
                  icon: const Icon(Icons.link_off, size: 14),
                  label: const Text('DISCONNECT', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: TextButton.icon(
              onPressed: () async {
                final r = await provider.forgetWiFi();
                ToastHelper.show(context, r.ok ? 'Saved credentials erased. Device reverted to AP mode.' : 'Failed: ${r.message}', r.ok ? 'warning' : 'error');
              },
              style: TextButton.styleFrom(foregroundColor: AtsColors.danger),
              icon: const Icon(Icons.delete_forever, size: 14),
              label: const Text('FORGET SAVED NETWORK', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700)),
            ),
          ),
        ],
      ),
    );
  }
}