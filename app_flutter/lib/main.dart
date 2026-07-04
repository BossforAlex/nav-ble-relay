/// 导航 BLE 转发 - 应用入口
///
/// - Material 3 + 动态颜色（Material You）
/// - google_fonts 字体
/// - 深色 / 浅色主题切换
/// - Provider 状态管理（[BleService] / [BroadcastService] / [SettingsService]）
library;

import 'package:dynamic_color/dynamic_color.dart';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ble/ble_service.dart';
import 'services/broadcast_service.dart';
import 'ui/home_screen.dart';
import 'ui/settings_screen.dart';

/// 主题模式键
const String kPrefThemeMode = 'theme_mode';
/// 详细日志
const String kPrefLogDetail = 'log_detail';
/// ESP32 简化模式
const String kPrefCompactMode = 'compact_mode';
/// 保持屏幕常亮
const String kPrefKeepScreenOn = 'keep_screen_on';
/// 开机自启
const String kPrefAutoStart = 'auto_start';
/// 目标设备 MAC
const String kPrefTargetMac = 'target_device_mac';

late SharedPreferences _prefs;

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  _prefs = await SharedPreferences.getInstance();
  runApp(const NavBleRelayApp());
}

/// 全局共享的 SharedPreferences 实例
SharedPreferences get prefs => _prefs;

class NavBleRelayApp extends StatelessWidget {
  const NavBleRelayApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => SettingsService()),
        ChangeNotifierProvider(create: (_) => BleService()),
        ChangeNotifierProvider(create: (_) => BroadcastService()),
      ],
      child: Consumer<SettingsService>(
        builder: (context, settings, _) {
          return DynamicColorBuilder(
            builder: (lightDynamic, darkDynamic) {
              return MaterialApp(
                title: '导航BLE转发',
                debugShowCheckedModeBanner: false,
                theme: _buildTheme(lightDynamic, Brightness.light),
                darkTheme: _buildTheme(darkDynamic, Brightness.dark),
                themeMode: settings.themeMode,
                initialRoute: '/',
                routes: {
                  '/': (context) => const HomeScreen(),
                  '/settings': (context) => const SettingsScreen(),
                },
              );
            },
          );
        },
      ),
    );
  }

  /// 构建 Material 3 主题，动态颜色不可用时回退到品牌色
  ThemeData _buildTheme(ColorScheme? dynamicScheme, Brightness brightness) {
    final isLight = brightness == Brightness.light;
    // 品牌回退色板（与原 res/values/colors.xml 对齐）
    final fallback = isLight
        ? const ColorScheme.light(
            primary: Color(0xFF415F91),
            onPrimary: Colors.white,
            primaryContainer: Color(0xFFD6E3FF),
            onPrimaryContainer: Color(0xFF284777),
            secondary: Color(0xFF565E71),
            onSecondary: Colors.white,
            secondaryContainer: Color(0xFFDAE2F9),
            onSecondaryContainer: Color(0xFF3E4759),
            tertiary: Color(0xFF705575),
            surface: Color(0xFFF9F9FF),
            onSurface: Color(0xFF191C20),
            surfaceContainerHighest: Color(0xFFE0E2EC),
            onSurfaceVariant: Color(0xFF44474E),
            error: Color(0xFFBA1A1A),
            onError: Colors.white,
            outline: Color(0xFF74777F),
            outlineVariant: Color(0xFFC4C6D0),
          )
        : const ColorScheme.dark(
            primary: Color(0xFFAAC7FF),
            onPrimary: Color(0xFF002F66),
            primaryContainer: Color(0xFF284777),
            onPrimaryContainer: Color(0xFFD6E3FF),
            secondary: Color(0xFFBEC6DC),
            onSecondary: Color(0xFF283041),
            secondaryContainer: Color(0xFF3E4759),
            onSecondaryContainer: Color(0xFFDAE2F9),
            tertiary: Color(0xFFDDBDD2),
            surface: Color(0xFF111318),
            onSurface: Color(0xFFE2E2E9),
            surfaceContainerHighest: Color(0xFF34363C),
            onSurfaceVariant: Color(0xFFC4C6D0),
            error: Color(0xFFFFB4AB),
            onError: Color(0xFF690005),
            outline: Color(0xFF8E9099),
            outlineVariant: Color(0xFF44474E),
          );

    final scheme = dynamicScheme ?? fallback;
    // 先构建基础主题（brightness 由 colorScheme 推导，textTheme 颜色自动着色）
    final base = ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
    );
    return base.copyWith(
      // 应用 Noto Sans SC 字体，大数字使用等宽 RobotoMono 便于 HUD 读数
      textTheme: GoogleFonts.notoSansScTextTheme(base.textTheme).copyWith(
        displayLarge: GoogleFonts.robotoMono(
          fontSize: 96,
          fontWeight: FontWeight.w700,
          color: scheme.onSurface,
        ),
        displayMedium: GoogleFonts.robotoMono(
          fontSize: 64,
          fontWeight: FontWeight.w700,
          color: scheme.onSurface,
        ),
        displaySmall: GoogleFonts.robotoMono(
          fontWeight: FontWeight.w700,
          color: scheme.onSurface,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 2,
        clipBehavior: Clip.antiAlias,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(20),
        ),
      ),
      appBarTheme: AppBarTheme(
        centerTitle: true,
        backgroundColor: scheme.surface,
        foregroundColor: scheme.onSurface,
        elevation: 0,
        scrolledUnderElevation: 0,
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
        ),
      ),
    );
  }
}

/// 设置服务：主题模式与各设置项，统一通过 [SharedPreferences] 持久化
class SettingsService extends ChangeNotifier {
  ThemeMode _themeMode = _loadThemeMode();
  ThemeMode get themeMode => _themeMode;

  set themeMode(ThemeMode mode) {
    _themeMode = mode;
    _prefs.setString(kPrefThemeMode, mode.name);
    notifyListeners();
  }

  bool get logDetail => _prefs.getBool(kPrefLogDetail) ?? false;
  set logDetail(bool v) {
    _prefs.setBool(kPrefLogDetail, v);
    notifyListeners();
  }

  bool get compactMode => _prefs.getBool(kPrefCompactMode) ?? false;
  set compactMode(bool v) {
    _prefs.setBool(kPrefCompactMode, v);
    notifyListeners();
  }

  bool get keepScreenOn => _prefs.getBool(kPrefKeepScreenOn) ?? false;
  set keepScreenOn(bool v) {
    _prefs.setBool(kPrefKeepScreenOn, v);
    notifyListeners();
  }

  bool get autoStart => _prefs.getBool(kPrefAutoStart) ?? false;
  set autoStart(bool v) {
    _prefs.setBool(kPrefAutoStart, v);
    notifyListeners();
  }

  String get targetMac => _prefs.getString(kPrefTargetMac) ?? '';
  set targetMac(String v) {
    _prefs.setString(kPrefTargetMac, v.toUpperCase());
    notifyListeners();
  }

  /// 校验目标设备 MAC 是否允许连接
  bool isTargetDeviceAllowed(String? address) {
    final target = targetMac;
    if (target.isEmpty || address == null) return true;
    return address.toLowerCase() == target.toLowerCase();
  }

  /// 切换主题模式：系统 → 浅色 → 深色 → 系统
  void cycleThemeMode() {
    switch (_themeMode) {
      case ThemeMode.system:
        themeMode = ThemeMode.light;
        break;
      case ThemeMode.light:
        themeMode = ThemeMode.dark;
        break;
      case ThemeMode.dark:
        themeMode = ThemeMode.system;
        break;
    }
  }

  static ThemeMode _loadThemeMode() {
    final name = _prefs.getString(kPrefThemeMode);
    return ThemeMode.values.firstWhere(
      (m) => m.name == name,
      orElse: () => ThemeMode.system,
    );
  }

  /// 恢复默认设置
  void reset() {
    _prefs.remove(kPrefLogDetail);
    _prefs.remove(kPrefCompactMode);
    _prefs.remove(kPrefKeepScreenOn);
    _prefs.remove(kPrefAutoStart);
    _prefs.remove(kPrefTargetMac);
    _prefs.remove(kPrefThemeMode);
    _themeMode = ThemeMode.system;
    notifyListeners();
  }
}
