/// 设置页：蓝牙服务信息、详细日志、ESP32 简化模式、屏幕常亮、自启、目标设备 MAC
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
  late final TextEditingController _macController;

  @override
  void initState() {
    super.initState();
    _macController =
        TextEditingController(text: context.read<SettingsService>().targetMac);
  }

  @override
  void dispose() {
    _macController.dispose();
    super.dispose();
  }

  bool _isValidMac(String mac) {
    return RegExp(r'^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$').hasMatch(mac);
  }

  void _saveMac() {
    final raw = _macController.text.trim();
    final service = context.read<SettingsService>();
    if (raw.isNotEmpty && !_isValidMac(raw)) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('MAC 格式不正确')),
      );
      return;
    }
    service.targetMac = raw;
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
          onPressed: () {
            _saveMac();
            Navigator.pop(context);
          },
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
                  label: '通知特征值',
                  value: BleConstants.charGuideUuid.toUpperCase(),
                ),
                _KVTile(label: '设备名前缀', value: BleConstants.deviceNamePrefix),
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
            // 目标设备
            _SectionCard(
              title: '目标设备 / Target Device',
              icon: Icons.devices,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 12, 16, 4),
                  child: Text(
                    '只允许该 MAC 的 ESP32 连接，留空则不限制',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  child: TextField(
                    controller: _macController,
                    decoration: const InputDecoration(
                      labelText: '目标设备 MAC',
                      hintText: '例如 AA:BB:CC:DD:EE:FF',
                      border: OutlineInputBorder(),
                      prefixIcon: Icon(Icons.device_hub),
                    ),
                    textCapitalization: TextCapitalization.characters,
                    onSubmitted: (_) => _saveMac(),
                  ),
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
                    _macController.clear();
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
