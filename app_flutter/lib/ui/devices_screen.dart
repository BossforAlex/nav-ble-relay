/// 发现的设备页面（原生蓝牙设备列表）
///
/// 用户需求：
///   - 考虑到固件和 mac 地址的不是唯一性和批量开发
///   - 发现在设备页面做成原生蓝牙设备页面，与设备蓝牙一致
///   - 设备扫描到哪些设备就展示哪些设备
///   - 通过 mac 地址区别设备类型并展示对应图标
///   - 选中设备连接才进行数据传输交互
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/ble_service.dart';

class DevicesScreen extends StatelessWidget {
  const DevicesScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('发现设备 / Devices'),
        actions: [
          Consumer<BleService>(
            builder: (context, ble, _) {
              final connected = ble.isConnected;
              return IconButton(
                tooltip: connected ? '断开连接' : '刷新扫描',
                icon: Icon(connected ? Icons.link_off : Icons.refresh),
                onPressed: () {
                  if (connected) {
                    ble.disconnect();
                  } else {
                    ble.rescan();
                  }
                },
              );
            },
          ),
        ],
      ),
      body: SafeArea(
        child: Consumer<BleService>(
          builder: (context, ble, _) {
            final devices = ble.allDevices;
            final status = ble.status;
            final isConnected = ble.isConnected;
            final isConnecting = status == BleStatus.connecting;
            final isScanning = ble.isScanning;

            return Column(
              children: [
                // 状态条
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.symmetric(
                      horizontal: 16, vertical: 10),
                  color: Theme.of(context).colorScheme.surfaceContainerHighest,
                  child: Row(
                    children: [
                      if (isScanning)
                        const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      else
                        Icon(Icons.bluetooth_searching,
                            size: 18,
                            color: Theme.of(context).colorScheme.primary),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          _statusText(ble),
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                      if (isConnected)
                        Chip(
                          label: Text(
                            ble.isOurDevice
                                ? '已连接 AutoNav HUD'
                                : '已连接（非 HUD）',
                            style: TextStyle(
                              fontSize: 11,
                              color: Theme.of(context).colorScheme.onPrimary,
                            ),
                          ),
                          backgroundColor: ble.isOurDevice
                              ? Theme.of(context).colorScheme.primary
                              : Theme.of(context).colorScheme.tertiary,
                          padding: EdgeInsets.zero,
                          materialTapTargetSize:
                              MaterialTapTargetSize.shrinkWrap,
                        ),
                    ],
                  ),
                ),
                // 设备列表
                Expanded(
                  child: devices.isEmpty
                      ? _emptyState(context, ble)
                      : ListView.separated(
                          itemCount: devices.length,
                          separatorBuilder: (_, __) => const Divider(
                              height: 1, indent: 56),
                          itemBuilder: (context, index) {
                            // 按 RSSI 降序排序（信号强的在前）
                            final sorted = List.of(devices)
                              ..sort((a, b) => b.rssi.compareTo(a.rssi));
                            final r = sorted[index];
                            final cat = BleService.classifyDevice(r);
                            final isCurrent = r.device.remoteId.str ==
                                ble.deviceAddress;
                            return _DeviceTile(
                              result: r,
                              category: cat,
                              isCurrent: isCurrent,
                              isConnecting: isConnecting,
                              isConnected: isConnected,
                              onTapConnect: () =>
                                  ble.connectTo(r),
                            );
                          },
                        ),
                ),
                // 底部操作条
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 8, 16, 12),
                  child: Row(
                    children: [
                      Expanded(
                        child: FilledButton.icon(
                          onPressed: isScanning || isConnecting || isConnected
                              ? null
                              : () => ble.startScan(),
                          icon: const Icon(Icons.search),
                          label: const Text('开始扫描'),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: OutlinedButton.icon(
                          onPressed: isScanning ? () => ble.stopScan() : null,
                          icon: const Icon(Icons.stop),
                          label: const Text('停止扫描'),
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  String _statusText(BleService ble) {
    if (ble.isConnected) {
      return ble.isOurDevice
          ? '已连接 AutoNav HUD: ${ble.deviceName} (${ble.deviceAddress})'
          : '已连接到 ${ble.deviceName}（非 AutoNav HUD，不会转发数据）';
    }
    if (ble.status == BleStatus.connecting) {
      return '正在连接 ${ble.deviceName}...';
    }
    if (ble.isScanning) {
      return '正在扫描蓝牙设备...（找到 ${ble.allDevices.length} 个）';
    }
    if (ble.status == BleStatus.bluetoothOff) {
      return '蓝牙未开启';
    }
    if (ble.status == BleStatus.error) {
      return '错误: ${ble.lastError}';
    }
    return '未启动，点击下方"开始扫描"按钮';
  }

  Widget _emptyState(BuildContext context, BleService ble) {
    final isScanning = ble.isScanning;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              isScanning ? Icons.bluetooth_searching : Icons.bluetooth_disabled,
              size: 64,
              color: Theme.of(context).colorScheme.outline,
            ),
            const SizedBox(height: 16),
            Text(
              isScanning ? '正在搜索附近蓝牙设备...' : '暂无发现的设备',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            Text(
              isScanning
                  ? '请保持 ESP32 已通电并正在广播'
                  : '点击下方"开始扫描"按钮开始查找设备',
              style: Theme.of(context).textTheme.bodySmall,
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}

/// 单个设备项
class _DeviceTile extends StatelessWidget {
  const _DeviceTile({
    required this.result,
    required this.category,
    required this.isCurrent,
    required this.isConnecting,
    required this.isConnected,
    required this.onTapConnect,
  });

  final dynamic result; // ScanResult
  final DeviceCategory category;
  final bool isCurrent;
  final bool isConnecting;
  final bool isConnected;
  final VoidCallback onTapConnect;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    final name = (result.advertisementData.advName as String?) ?? '';
    final mac = (result.device.remoteId.str as String?) ?? '';
    final rssi = (result.rssi as int?) ?? 0;
    final displayName = name.isNotEmpty ? name : 'Unknown';

    return ListTile(
      leading: _categoryIcon(category, isCurrent, colorScheme),
      title: Row(
        children: [
          Expanded(
            child: Text(
              displayName,
              style: theme.textTheme.bodyLarge?.copyWith(
                fontWeight: isCurrent ? FontWeight.w600 : FontWeight.normal,
              ),
              overflow: TextOverflow.ellipsis,
            ),
          ),
          const SizedBox(width: 8),
          // 信号强度条
          _rssiIndicator(rssi, colorScheme),
        ],
      ),
      subtitle: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SizedBox(height: 2),
          SelectableText(
            mac,
            style: theme.textTheme.bodySmall?.copyWith(
              fontFamily: 'RobotoMono',
              color: colorScheme.onSurfaceVariant,
            ),
          ),
          const SizedBox(height: 4),
          Wrap(
            spacing: 6,
            runSpacing: 4,
            children: [
              _typeChip(category, colorScheme),
              if (category == DeviceCategory.hud)
                _hintChip(
                  '可转发数据',
                  colorScheme.primary,
                  colorScheme.onPrimary,
                ),
            ],
          ),
        ],
      ),
      trailing: isCurrent
          ? (isConnected
              ? Chip(
                  label: Text(
                    '已连接',
                    style: TextStyle(
                      color: colorScheme.onPrimary,
                      fontSize: 11,
                    ),
                  ),
                  backgroundColor: colorScheme.primary,
                  padding: EdgeInsets.zero,
                  materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                )
              : const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2),
                ))
          : TextButton(
              onPressed: isConnecting || isConnected ? null : onTapConnect,
              child: const Text('连接'),
            ),
      isThreeLine: true,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
    );
  }

  Widget _categoryIcon(DeviceCategory cat, bool isCurrent, colorScheme) {
    final color = isCurrent ? colorScheme.primary : colorScheme.onSurfaceVariant;
    final iconData = switch (cat) {
      DeviceCategory.hud => Icons.dashboard,
      DeviceCategory.esp32 => Icons.memory,
      DeviceCategory.audio => Icons.headphones,
      DeviceCategory.computer => Icons.laptop,
      DeviceCategory.wearable => Icons.watch,
      DeviceCategory.unknown => Icons.bluetooth,
    };
    return CircleAvatar(
      backgroundColor: color.withValues(alpha: 0.1),
      child: Icon(iconData, color: color),
    );
  }

  Widget _rssiIndicator(int rssi, colorScheme) {
    // RSSI: -30 极好, -60 中等, -90 很弱
    final strength = (rssi + 100).clamp(0, 100) / 100;
    final bars = strength > 0.75
        ? 4
        : strength > 0.5
            ? 3
            : strength > 0.25
                ? 2
                : 1;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          '$rssi dBm',
          style: TextStyle(
            fontSize: 10,
            color: colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(width: 4),
        for (int i = 0; i < 4; i++)
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 0.5),
            child: Container(
              width: 3,
              height: 4.0 + i * 3.0,
              color: i < bars
                  ? colorScheme.primary
                  : colorScheme.outlineVariant,
            ),
          ),
      ],
    );
  }

  Widget _typeChip(DeviceCategory cat, colorScheme) {
    final label = BleService.categoryLabel(cat);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      decoration: BoxDecoration(
        color: colorScheme.primaryContainer.withValues(alpha: 0.4),
        borderRadius: BorderRadius.circular(4),
      ),
      child: Text(
        label,
        style: TextStyle(
          fontSize: 10,
          color: colorScheme.onSurfaceVariant,
        ),
      ),
    );
  }

  Widget _hintChip(String label, Color bg, Color fg) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(4),
      ),
      child: Text(
        label,
        style: TextStyle(fontSize: 10, color: fg),
      ),
    );
  }
}
