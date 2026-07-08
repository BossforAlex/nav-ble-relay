/// 顶部状态卡片：BLE 连接状态 + 设备名 + 广播接收状态
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../ble/ble_service.dart';
import '../../main.dart';
import '../../services/broadcast_service.dart';

class StatusCard extends StatelessWidget {
  const StatusCard({super.key});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;

    return Consumer2<BleService, BroadcastService>(
      builder: (context, ble, broadcast, _) {
        final running = ble.isRunning;
        final connected = ble.isConnected;

        // 状态点颜色：已连接=绿，运行中=橙，停止=红
        final dotColor = connected
            ? const Color(0xFF008375)
            : running
                ? const Color(0xFFE07B39)
                : const Color(0xFFBA1A1A);

        final statusText = connected
            ? '已连接 ESP32'
            : running
                ? '正在扫描'
                : '已停止';

        final statusTextColor = connected
            ? const Color(0xFF008375)
            : running
                ? const Color(0xFFE07B39)
                : colorScheme.onSurfaceVariant;

        return Card(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(20, 18, 20, 18),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // 顶部：状态点 + 状态文字 + 设备名
                Row(
                  children: [
                    _StatusDot(color: dotColor),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(
                        statusText,
                        style: theme.textTheme.titleMedium?.copyWith(
                          color: statusTextColor,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                    // 主题切换按钮
                    IconButton(
                      icon: const Icon(Icons.brightness_6_outlined),
                      tooltip: '切换主题',
                      onPressed: () => context
                          .read<SettingsService>()
                          .cycleThemeMode(),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                // BLE 状态行
                _InfoRow(
                  icon: Icons.bluetooth,
                  label: '蓝牙',
                  value: connected
                      ? '已连接 ${ble.deviceAddress}'
                      : running
                          ? '正在扫描 ESP32...'
                          : '未启动',
                  valueColor: connected
                      ? const Color(0xFF008375)
                      : running
                          ? const Color(0xFFE07B39)
                          : const Color(0xFFBA1A1A),
                ),
                const SizedBox(height: 8),
                // 广播接收状态行
                _InfoRow(
                  icon: Icons.broadcast_on_personal,
                  label: '广播',
                  value: running ? broadcast.broadcastAgoText : '未启动',
                  valueColor: broadcast.broadcastReceived > 0
                      ? const Color(0xFF008375)
                      : colorScheme.onSurfaceVariant,
                ),
                // 设备名（连接时显示）
                if (connected && ble.deviceName.isNotEmpty) ...[
                  const SizedBox(height: 8),
                  _InfoRow(
                    icon: Icons.device_hub,
                    label: '设备',
                    value: ble.deviceName,
                  ),
                ],
                // v0.5.8: relay 计数（诊断 broadcast→BLE 是否通畅）
                if (connected && broadcast.relayCount > 0) ...[
                  const SizedBox(height: 8),
                  _InfoRow(
                    icon: Icons.send,
                    label: '转发',
                    value: '${broadcast.relayCount} 次 (${broadcast.lastRelayText})',
                    valueColor: const Color(0xFF008375),
                  ),
                ],
              ],
            ),
          ),
        );
      },
    );
  }
}

/// 状态点
class _StatusDot extends StatelessWidget {
  const _StatusDot({required this.color});
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 12,
      height: 12,
      decoration: BoxDecoration(
        color: color,
        shape: BoxShape.circle,
        boxShadow: [
          BoxShadow(
            color: color.withValues(alpha: 0.4),
            blurRadius: 8,
            spreadRadius: 1,
          ),
        ],
      ),
    );
  }
}

/// 单行信息
class _InfoRow extends StatelessWidget {
  const _InfoRow({
    required this.icon,
    required this.label,
    required this.value,
    this.valueColor,
  });

  final IconData icon;
  final String label;
  final String value;
  final Color? valueColor;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Row(
      children: [
        Icon(icon, size: 18, color: theme.colorScheme.onSurfaceVariant),
        const SizedBox(width: 8),
        Text(
          '$label / ',
          style: theme.textTheme.bodyMedium?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        Expanded(
          child: Text(
            value,
            textAlign: TextAlign.right,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: valueColor,
              fontWeight: FontWeight.w500,
            ),
          ),
        ),
      ],
    );
  }
}
