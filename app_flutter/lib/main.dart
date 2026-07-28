/// 导航 BLE 转发 - 应用入口
///
/// Design: Night-drive cockpit — dark surfaces, electric-blue instrument glow,
/// red warning accents, precision monospace HUD numerals.
/// Material 3 + dynamic color with tailored fallback palettes.
library;

import 'package:dynamic_color/dynamic_color.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ble/ble_service.dart';
import 'services/broadcast_service.dart';
import 'ui/main_navigation.dart';

const String kPrefThemeMode = 'theme_mode';
const String kPrefLogDetail = 'log_detail';
const String kPrefCompactMode = 'compact_mode';
const String kPrefKeepScreenOn = 'keep_screen_on';
const String kPrefAutoStart = 'auto_start';
const String kPrefTargetMac = 'target_device_mac';

late SharedPreferences _prefs;

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  _prefs = await SharedPreferences.getInstance();
  SystemChrome.setSystemUIOverlayStyle(const SystemUiOverlayStyle(
    statusBarColor: Colors.transparent,
    statusBarIconBrightness: Brightness.dark,
  ));
  runApp(const NavBleRelayApp());
}

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
                home: const MainNavigation(),
              );
            },
          );
        },
      ),
    );
  }

  ThemeData _buildTheme(ColorScheme? dynamicScheme, Brightness brightness) {
    final isLight = brightness == Brightness.light;
    final fallback = isLight
        ? const ColorScheme.light(
            primary: Color(0xFF0D47A1),
            onPrimary: Color(0xFFFFFFFF),
            primaryContainer: Color(0xFFD6E3FF),
            onPrimaryContainer: Color(0xFF001B3E),
            secondary: Color(0xFF37474F),
            onSecondary: Color(0xFFFFFFFF),
            secondaryContainer: Color(0xFFCFD8DC),
            onSecondaryContainer: Color(0xFF1C313A),
            tertiary: Color(0xFF00695C),
            onTertiary: Color(0xFFFFFFFF),
            tertiaryContainer: Color(0xFFB2DFDB),
            onTertiaryContainer: Color(0xFF00251A),
            surface: Color(0xFFF8FAFE),
            onSurface: Color(0xFF111318),
            surfaceContainerHighest: Color(0xFFE0E4EC),
            surfaceContainerLow: Color(0xFFF0F2F8),
            surfaceContainer: Color(0xFFE8EAF2),
            onSurfaceVariant: Color(0xFF43474E),
            error: Color(0xFFC62828),
            onError: Color(0xFFFFFFFF),
            errorContainer: Color(0xFFFFCDD2),
            onErrorContainer: Color(0xFF5F0000),
            outline: Color(0xFF72767E),
            outlineVariant: Color(0xFFC2C5CE),
          )
        : const ColorScheme.dark(
            primary: Color(0xFF64B5F6),
            onPrimary: Color(0xFF001F3F),
            primaryContainer: Color(0xFF0D47A1),
            onPrimaryContainer: Color(0xFFD6E3FF),
            secondary: Color(0xFF90A4AE),
            onSecondary: Color(0xFF1C313A),
            secondaryContainer: Color(0xFF263238),
            onSecondaryContainer: Color(0xFFCFD8DC),
            tertiary: Color(0xFF4DB6AC),
            onTertiary: Color(0xFF00251A),
            tertiaryContainer: Color(0xFF00695C),
            onTertiaryContainer: Color(0xFFB2DFDB),
            surface: Color(0xFF0A0E14),
            onSurface: Color(0xFFE1E2E8),
            surfaceContainerHighest: Color(0xFF1E2228),
            surfaceContainerLow: Color(0xFF0F1318),
            surfaceContainer: Color(0xFF161A20),
            onSurfaceVariant: Color(0xFFC2C5CE),
            error: Color(0xFFFF5252),
            onError: Color(0xFF3E0000),
            errorContainer: Color(0xFF8E0000),
            onErrorContainer: Color(0xFFFFCDD2),
            outline: Color(0xFF5C6068),
            outlineVariant: Color(0xFF2E3238),
          );

    final scheme = dynamicScheme ?? fallback;
    final base = ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
    );

    return base.copyWith(
      scaffoldBackgroundColor: scheme.surface,
      textTheme: GoogleFonts.notoSansScTextTheme(base.textTheme).copyWith(
        displayLarge: GoogleFonts.robotoMono(
          fontSize: 96, fontWeight: FontWeight.w700,
          color: scheme.onSurface, letterSpacing: -2,
        ),
        displayMedium: GoogleFonts.robotoMono(
          fontSize: 64, fontWeight: FontWeight.w700,
          color: scheme.onSurface, letterSpacing: -1.5,
        ),
        displaySmall: GoogleFonts.robotoMono(
          fontSize: 48, fontWeight: FontWeight.w700,
          color: scheme.onSurface, letterSpacing: -1,
        ),
        headlineLarge: GoogleFonts.robotoMono(
          fontSize: 32, fontWeight: FontWeight.w700,
          color: scheme.onSurface, letterSpacing: -0.5,
        ),
        headlineMedium: GoogleFonts.robotoMono(
          fontSize: 28, fontWeight: FontWeight.w600,
          color: scheme.onSurface,
        ),
        titleLarge: base.textTheme.titleLarge?.copyWith(
          fontWeight: FontWeight.w700,
          letterSpacing: -0.3,
        ),
        titleMedium: base.textTheme.titleMedium?.copyWith(
          fontWeight: FontWeight.w600,
          letterSpacing: -0.2,
        ),
        labelLarge: base.textTheme.labelLarge?.copyWith(
          fontWeight: FontWeight.w600,
          letterSpacing: 0.5,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: scheme.surfaceContainerLow,
        surfaceTintColor: Colors.transparent,
        margin: EdgeInsets.zero,
        clipBehavior: Clip.antiAlias,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: BorderSide(
            color: scheme.outlineVariant.withValues(alpha: 0.5),
          ),
        ),
      ),
      appBarTheme: AppBarTheme(
        centerTitle: false,
        backgroundColor: scheme.surface,
        foregroundColor: scheme.onSurface,
        elevation: 0,
        scrolledUnderElevation: 0,
        titleTextStyle: base.textTheme.titleLarge?.copyWith(
          color: scheme.onSurface,
          fontWeight: FontWeight.w700,
        ),
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          minimumSize: const Size(48, 48),
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          minimumSize: const Size(48, 48),
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 13),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
          side: BorderSide(color: scheme.outline),
        ),
      ),
      iconButtonTheme: IconButtonThemeData(
        style: IconButton.styleFrom(
          minimumSize: const Size(48, 48),
          tapTargetSize: MaterialTapTargetSize.padded,
        ),
      ),
      navigationBarTheme: NavigationBarThemeData(
        height: 72,
        backgroundColor: scheme.surfaceContainer,
        indicatorColor: scheme.primaryContainer,
        surfaceTintColor: Colors.transparent,
        labelTextStyle: WidgetStateProperty.resolveWith((states) {
          final selected = states.contains(WidgetState.selected);
          return base.textTheme.labelMedium?.copyWith(
            fontWeight: selected ? FontWeight.w700 : FontWeight.w500,
            color: selected ? scheme.onPrimaryContainer : scheme.onSurfaceVariant,
            letterSpacing: 0.3,
          );
        }),
      ),
      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        backgroundColor: scheme.inverseSurface,
        contentTextStyle: base.textTheme.bodyMedium?.copyWith(
          color: scheme.onInverseSurface,
        ),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
      floatingActionButtonTheme: FloatingActionButtonThemeData(
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
        elevation: 4,
        highlightElevation: 8,
      ),
      dividerTheme: DividerThemeData(
        color: scheme.outlineVariant.withValues(alpha: 0.6),
        thickness: 0.5,
        space: 0.5,
      ),
      chipTheme: ChipThemeData(
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        side: BorderSide.none,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 0),
        labelStyle: base.textTheme.labelSmall,
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
