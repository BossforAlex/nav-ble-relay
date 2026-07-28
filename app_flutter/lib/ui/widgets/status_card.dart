/// 顶部状态卡片：BLE 连接状态 + 设备名 + 广播接收状态
///
/// Cockpit-style status bar with animated status dot, connection info,
/// and relay diagnostics.
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
        final successColor = colorScheme.tertiary;
        final pendingColor = colorScheme.primary;
        final stoppedColor = colorScheme.error;

        final dotColor = connected
            ? successColor
            : running
                ? pendingColor
                : stoppedColor;

        final statusText = connected
            ? '已连接 ESP32'
            : running
                ? '正在扫描'
                : '已停止';

        final statusTextColor = connected
            ? successColor
            : running
                ? pendingColor
                : colorScheme.onSurfaceVariant;

        return Card(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(20, 18, 20, 18),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // 顶部：状态点 + 状态文字 + 设备名 + 主题切换
                Row(
                  children: [
                    _StatusDot(color: dotColor),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(statusText,
                        style: theme.textTheme.titleMedium?.copyWith(
                          color: statusTextColor,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                    // 主题切换按钮
                    Container(
                      decoration: BoxDecoration(
                        color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.5),
                        borderRadius: BorderRadius.circular(10),
                      ),
                      child: IconButton(
                        icon: const Icon(Icons.brightness_6_outlined, size: 20),
                        tooltip: '切换主题',
                        visualDensity: VisualDensity.compact,
                        onPressed: () =>
                            context.read<SettingsService>().cycleThemeMode(),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 14),
                // 分隔线
                Divider(color: colorScheme.outlineVariant.withValues(alpha: 0.3)),
                const SizedBox(height: 14),
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
                      ? successColor
                      : running
                          ? pendingColor
                          : stoppedColor,
                ),
                const SizedBox(height: 8),
                // 广播接收状态行
                _InfoRow(
                  icon: Icons.broadcast_on_personal,
                  label: '广播',
                  value: running ? broadcast.broadcastAgoText : '未启动',
                  valueColor: broadcast.broadcastReceived > 0
                      ? successColor
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
                // 转发计数
                if (connected && broadcast.relayCount > 0) ...[
                  const SizedBox(height: 8),
                  _InfoRow(
                    icon: Icons.send,
                    label: '转发',
                    value: '${broadcast.relayCount} 次 (${broadcast.lastRelayText})',
                    valueColor: successColor,
                  ),
                ],
                // BLE 写入错误
                if (connected && ble.lastError.isNotEmpty) ...[
                  const SizedBox(height: 8),
                  _InfoRow(
                    icon: Icons.error_outline,
                    label: '错误',
                    value: ble.lastError,
                    valueColor: colorScheme.error,
                  ),
                ],
                // 服务发现摘要
                if (connected && ble.discoverySummary.isNotEmpty) ...[
                  const SizedBox(height: 8),
                  _InfoRow(
                    icon: Icons.search,
                    label: '发现',
                    value: ble.discoverySummary,
                    valueColor: colorScheme.onSurfaceVariant,
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

/// 状态点 — 带脉冲动画
class _StatusDot extends StatefulWidget {
  const _StatusDot({required this.color});
  final Color color;

  @override
  State<_StatusDot> createState() => _StatusDotState();
}

class _StatusDotState extends State<_StatusDot>
    with SingleTickerProviderStateMixin {
  late AnimationController _controller;
  late Animation<double> _pulse;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1500),
    );
    _pulse = Tween<double>(begin: 0.6, end: 1.0).animate(
      CurvedAnimation(parent: _controller, curve: Curves.easeInOut),
    );
    _controller.repeat(reverse: true);
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _pulse,
      builder: (context, child) {
        return Container(
          width: 12,
          height: 12,
          decoration: BoxDecoration(
            color: widget.color,
            shape: BoxShape.circle,
            boxShadow: [
              BoxShadow(
                color: widget.color.withValues(alpha: 0.4 * _pulse.value),
                blurRadius: 10 * _pulse.value,
                spreadRadius: 1,
              ),
            ],
          ),
        );
      },
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
        Container(
          width: 28, height: 28,
          decoration: BoxDecoration(
            color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.5),
            borderRadius: BorderRadius.circular(7),
          ),
          child: Icon(icon, size: 15,
            color: (valueColor ?? theme.colorScheme.onSurfaceVariant)),
        ),
        const SizedBox(width: 10),
        Text('$label / ',
          style: theme.textTheme.bodyMedium?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        Expanded(
          child: Text(value,
            textAlign: TextAlign.right,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: valueColor ?? theme.colorScheme.onSurface,
              fontWeight: FontWeight.w600,
            ),
          ),
        ),
      ],
    );
  }
}
