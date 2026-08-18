import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/ats_provider.dart';
import 'theme/app_theme.dart';
import 'screens/dashboard_screen.dart';
import 'screens/meters_screen.dart';
import 'screens/protection_screen.dart';
import 'screens/wifi_screen.dart';
import 'screens/system_screen.dart';
import 'widgets/toast_helper.dart';

void main() {
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AtsProvider()),
      ],
      child: const SmartAtsApp(),
    ),
  );
}

class SmartAtsApp extends StatelessWidget {
  const SmartAtsApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmartATS Mobile',
      debugShowCheckedModeBanner: false,
      theme: AtsTheme.darkTheme,
      home: const MainHomeScreen(),
    );
  }
}

class MainHomeScreen extends StatefulWidget {
  const MainHomeScreen({super.key});

  @override
  State<MainHomeScreen> createState() => _MainHomeScreenState();
}

class _MainHomeScreenState extends State<MainHomeScreen> {
  int _currentIndex = 0;

  final List<Widget> _screens = const [
    DashboardScreen(),
    MetersScreen(),
    ProtectionScreen(),
    WifiScreen(),
    SystemScreen(),
  ];

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<AtsProvider>();
    final state = provider.state;
    final connected = provider.isConnected;

    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(6),
              decoration: BoxDecoration(
                color: AtsColors.primary,
                borderRadius: BorderRadius.circular(8),
              ),
              child: const Icon(Icons.bolt, color: Colors.white, size: 18),
            ),
            const SizedBox(width: 10),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'SmartATS Mobile',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.w700, color: Colors.white),
                ),
                Text(
                  connected ? 'Connected • ${state.apIP}' : 'Connecting to ESP32...',
                  style: TextStyle(
                    fontSize: 10,
                    color: connected ? AtsColors.success : AtsColors.warning,
                  ),
                ),
              ],
            ),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings_input_antenna, size: 20),
            tooltip: 'Device IP',
            onPressed: () => _showIpDialog(context),
          ),
          IconButton(
            icon: Icon(
              provider.isUnlocked ? Icons.lock_open : Icons.lock_outline,
              size: 20,
            ),
            tooltip: provider.isUnlocked ? 'Lock admin' : 'Admin login',
            onPressed: () {
              if (provider.isUnlocked) {
                provider.lockAdmin();
                ToastHelper.show(context, 'Admin locked', 'info');
              } else {
                _showLoginDialog(context);
              }
            },
          ),
        ],
      ),
      body: _screens[_currentIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (idx) => setState(() => _currentIndex = idx),
        backgroundColor: AtsColors.surface,
        indicatorColor: AtsColors.primary.withValues(alpha: 0.2),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.dashboard_outlined), selectedIcon: Icon(Icons.dashboard, color: AtsColors.primary), label: 'Dashboard'),
          NavigationDestination(icon: Icon(Icons.tune_outlined), selectedIcon: Icon(Icons.tune, color: AtsColors.primary), label: 'Meters'),
          NavigationDestination(icon: Icon(Icons.shield_outlined), selectedIcon: Icon(Icons.shield, color: AtsColors.primary), label: 'Protection'),
          NavigationDestination(icon: Icon(Icons.wifi), selectedIcon: Icon(Icons.wifi, color: AtsColors.primary), label: 'Wi-Fi'),
          NavigationDestination(icon: Icon(Icons.memory_outlined), selectedIcon: Icon(Icons.memory, color: AtsColors.primary), label: 'System'),
        ],
      ),
    );
  }

  void _showIpDialog(BuildContext context) {
    final provider = context.read<AtsProvider>();
    final controller = TextEditingController(text: provider.baseUrl);

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AtsColors.surface,
        title: const Text('ESP32 Target Address', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w700)),
        content: TextField(
          controller: controller,
          keyboardType: TextInputType.url,
          decoration: const InputDecoration(
            hintText: 'http://192.168.4.1 or smartmeterats.local',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('CANCEL')),
          ElevatedButton(
            onPressed: () {
              provider.setDeviceIp(controller.text.trim());
              Navigator.pop(ctx);
            },
            child: const Text('CONNECT'),
          ),
        ],
      ),
    );
  }

  void _showLoginDialog(BuildContext context) {
    final provider = context.read<AtsProvider>();
    final controller = TextEditingController();

    showDialog(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setDialogState) {
          var verifying = false;

          void submit() async {
            final password = controller.text.trim();
            if (password.isEmpty || verifying) return;
            setDialogState(() => verifying = true);
            final result = await provider.unlockAdmin(password);
            if (!ctx.mounted) return;
            Navigator.pop(ctx);
            if (result == true) {
              ToastHelper.show(context, 'Admin unlocked', 'success');
            } else if (result == false) {
              ToastHelper.show(context, 'Wrong admin password', 'error');
            } else {
              ToastHelper.show(context, 'ESP32 unreachable — password not verified', 'error');
            }
          }

          return AlertDialog(
            backgroundColor: AtsColors.surface,
            title: const Text('Admin Login', style: TextStyle(fontSize: 14, fontWeight: FontWeight.w700)),
            content: TextField(
              controller: controller,
              obscureText: true,
              autocorrect: false,
              enableSuggestions: false,
              keyboardType: TextInputType.visiblePassword,
              onSubmitted: (_) => submit(),
              decoration: const InputDecoration(
                labelText: 'Admin password',
                border: OutlineInputBorder(),
              ),
            ),
            actions: [
              TextButton(
                onPressed: verifying ? null : () => Navigator.pop(ctx),
                child: const Text('CANCEL'),
              ),
              ElevatedButton(
                onPressed: verifying ? null : submit,
                child: verifying
                    ? const SizedBox(
                        width: 14,
                        height: 14,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                      )
                    : const Text('LOGIN'),
              ),
            ],
          );
        },
      ),
    );
  }
}