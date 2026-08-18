import 'package:flutter/material.dart';
import '../theme/app_theme.dart';

class ToastHelper {
  static void show(
    BuildContext context,
    String text, [
    String type = 'info',
  ]) {
    final color = switch (type) {
      'success' => AtsColors.success,
      'warning' => AtsColors.warning,
      'error' => AtsColors.danger,
      _ => AtsColors.primary,
    };
    final messenger = ScaffoldMessenger.of(context);
    messenger.hideCurrentSnackBar();
    messenger.showSnackBar(
      SnackBar(
        content: Text(
          text,
          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
        ),
        backgroundColor: color,
        behavior: SnackBarBehavior.floating,
        duration: const Duration(milliseconds: 2800),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
    );
  }
}