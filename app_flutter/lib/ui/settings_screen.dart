/// 设置页：蓝牙服务信息、通用设置、主题、关于
///
/// 设备选择功能已迁移到"发现设备"页面（DevicesScreen）
library;

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:provider/provider.dart';

import '../ble/ble_constants.dart';
import '../main.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  @override
  void initState() {
    super.initState();
  }

  @override
  void dispose() {
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final settings = context.watch<SettingsService>();
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('设置 Settings'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            // 蓝牙服务信息
            _SectionCard(
              title: '蓝牙服务 / BLE Service',
              icon: Icons.bluetooth,
              children: [
                _KVTile(
                  label: '服务 UUID',
                  value: BleConstants.serviceUuid.toUpperCase(),
                ),
                _KVTile(
                  label: '特征值 FFE1 (Guide)',
                  value: BleConstants.charGuideUuid.toUpperCase(),
                ),
                _KVTile(
                  label: '特征值 FFE2 (DriveWay)',
                  value: BleConstants.charDriveWayUuid.toUpperCase(),
                ),
                _KVTile(
                  label: '特征值 FFE3 (TMC)',
                  value: BleConstants.charTmcUuid.toUpperCase(),
                ),
                _KVTile(
                  label: '特征值 FFE4 (State)',
                  value: BleConstants.charStateUuid.toUpperCase(),
                ),
                _KVTile(
                  label: '特征值 FFE5 (Location)',
                  value: BleConstants.charLocationUuid.toUpperCase(),
                ),
                _KVTile(
                  label: 'ESP32 设备名',
                  value: BleConstants.deviceName,
                ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 8, 16, 12),
                  child: Text(
                    '说明：在"发现设备"页面扫描并选择 ESP32 设备连接，'
                    '连接成功后才会转发数据。批量开发场景下不再做 MAC 限制。',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            // 通用设置
            _SectionCard(
              title: '通用 / General',
              icon: Icons.tune,
              children: [
                SwitchListTile(
                  title: const Text('详细日志'),
                  subtitle: const Text('输出更多调试信息到 logcat'),
                  value: settings.logDetail,
                  onChanged: (v) => settings.logDetail = v,
                ),
                SwitchListTile(
                  title: const Text('ESP32 简化模式'),
                  subtitle: const Text('仅发送转向、路口、距离等必要字段，降低 ESP32-C3 解析负担'),
                  value: settings.compactMode,
                  onChanged: (v) => settings.compactMode = v,
                ),
                SwitchListTile(
                  title: const Text('保持屏幕常亮'),
                  subtitle: const Text('主界面保持屏幕不熄灭'),
                  value: settings.keepScreenOn,
                  onChanged: (v) {
                    settings.keepScreenOn = v;
                    _applyKeepScreenOn(v);
                  },
                ),
                SwitchListTile(
                  title: const Text('开机自启'),
                  subtitle: const Text('系统启动后自动开始转发'),
                  value: settings.autoStart,
                  onChanged: (v) => settings.autoStart = v,
                ),
              ],
            ),
            const SizedBox(height: 12),
            // 主题设置
            _SectionCard(
              title: '主题 / Theme',
              icon: Icons.palette_outlined,
              children: [
                ListTile(
                  leading: const Icon(Icons.brightness_6_outlined),
                  title: const Text('主题模式'),
                  trailing: DropdownButton<ThemeMode>(
                    value: settings.themeMode,
                    underline: const SizedBox(),
                    onChanged: (m) {
                      if (m != null) settings.themeMode = m;
                    },
                    items: const [
                      DropdownMenuItem(
                        value: ThemeMode.system,
                        child: Text('跟随系统'),
                      ),
                      DropdownMenuItem(
                        value: ThemeMode.light,
                        child: Text('浅色'),
                      ),
                      DropdownMenuItem(
                        value: ThemeMode.dark,
                        child: Text('深色'),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            // 恢复默认
            _SectionCard(
              title: '恢复 / Reset',
              icon: Icons.restart_alt_outlined,
              children: [
                ListTile(
                  leading: Icon(Icons.delete_outline, color: theme.colorScheme.error),
                  title: Text('恢复默认设置',
                      style: TextStyle(color: theme.colorScheme.error)),
                  subtitle: const Text('清除所有设置项'),
                  onTap: () {
                    settings.reset();
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('设置已恢复默认')),
                    );
                  },
                ),
              ],
            ),
            const SizedBox(height: 24),
            // 关于信息
            _SectionCard(
              title: '关于 / About',
              icon: Icons.info_outline,
              children: [
                _KVTile(label: '应用名', value: '导航BLE转发'),
                _KVTile(label: '版本', value: '1.0.0'),
                _KVTile(label: '作者', value: 'BossforAlex'),
                _KVTile(label: '开源协议', value: 'MIT License'),
                _KVTile(
                  label: '协议版本',
                  value: 'AmapAuto标准广播协议 20180813',
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  void _applyKeepScreenOn(bool enabled) {
    // 通过平台通道控制屏幕常亮（这里仅做状态保存，实际可在原生层实现）
  }
}

/// 分组卡片
class _SectionCard extends StatelessWidget {
  const _SectionCard({
    required this.title,
    required this.icon,
    required this.children,
  });

  final String title;
  final IconData icon;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
              child: Row(
                children: [
                  Icon(icon, size: 18, color: theme.colorScheme.primary),
                  const SizedBox(width: 8),
                  Text(
                    title,
                    style: theme.textTheme.titleSmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ),
            ...children,
          ],
        ),
      ),
    );
  }
}

/// 键值对行
class _KVTile extends StatelessWidget {
  const _KVTile({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            flex: 2,
            child: Text(
              label,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
          Expanded(
            flex: 3,
            child: SelectableText(
              value,
              textAlign: TextAlign.right,
              style: GoogleFonts.robotoMono(
                textStyle: theme.textTheme.bodyMedium,
                color: theme.colorScheme.onSurface,
                fontWeight: FontWeight.w500,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
