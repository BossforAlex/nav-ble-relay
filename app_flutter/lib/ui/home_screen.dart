/// 主界面（导航转发页）
///
/// 布局：顶部状态卡片 → 中间导航预览 → 详细数据卡片
/// 启停服务通过右下角 FAB 控制，启动后自动跳转到"发现设备"页
///
/// 设备列表已迁移到独立的"发现设备"页面（DevicesScreen）
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/ble_service.dart';
import '../main.dart';
import '../protocol/amap_protocol.dart';
import '../services/broadcast_service.dart';
import 'main_navigation.dart';
import 'widgets/nav_preview.dart';
import 'widgets/status_card.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> with WidgetsBindingObserver {
  bool _relayWired = false;
  /// v0.6.4: 防止 BLE 写入并发 + 数据不丢失。
  /// _relaying=true 时新数据不丢弃，而是设 _pendingRelay=true，
  /// 当前 relay 完成后自动处理待发送数据。
  bool _relaying = false;
  bool _pendingRelay = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    WidgetsBinding.instance.addPostFrameCallback((_) => _wireRelay());
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      context.read<BroadcastService>().notifyListeners();
    }
  }

  /// 绑定中继：广播数据变化时转发到 BLE
  void _wireRelay() {
    if (_relayWired) return;
    final broadcast = context.read<BroadcastService>();
    broadcast.addListener(() { _relayToBle(broadcast); });
    _relayWired = true;
  }

  /// v0.6.4 修复：async + await 保证 BLE 按序到达，同时用 _pendingRelay
  /// 标记避免高频导航数据在 relay 期间被丢弃。
  ///
  /// 发送顺序：先 state（设置导航状态）→ 再 guide（填充数据），
  /// 确保 ESP32 在同一帧内拿到 mapState+guide 两个条件。
  Future<void> _relayToBle(BroadcastService broadcast) async {
    if (!mounted) return;
    // v0.6.4: 不丢弃数据，标记 pending 等当前 relay 完成后处理
    if (_relaying) {
      _pendingRelay = true;
      return;
    }
    _relaying = true;
    _pendingRelay = false;
    try {
      final ble = context.read<BleService>();
      final settings = context.read<SettingsService>();
      if (!ble.isConnected) return;

      if (broadcast.mapState >= 0) {
        await ble.sendMapState(broadcast.mapState, broadcast.crossMap);
        broadcast.relayCount++;
      }
      final guide = broadcast.guideInfo;
      if (guide != null) {
        await ble.sendGuideInfo(guide, compact: settings.compactMode);
        broadcast.relayCount++;
      }
      final driveWay = broadcast.driveWayInfo;
      if (driveWay != null) {
        await ble.sendDriveWay(driveWay);
        broadcast.relayCount++;
      }
      final tmc = broadcast.tmcSegmentInfo;
      if (tmc != null) {
        await ble.sendTmcSegment(tmc);
        broadcast.relayCount++;
      }
      final loc = broadcast.locationInfo;
      if (loc != null) {
        await ble.sendLocation(loc);
        broadcast.relayCount++;
      }
      broadcast.lastRelayAt = DateTime.now();
      // 不调用 broadcast.notifyListeners()，避免 self-trigger 无限循环。
      // UI 更新由 BroadcastService 自身数据变更时触发。
    } finally {
      _relaying = false;
    }
    // v0.6.4: relay 期间有新的导航数据到达，立即处理
    if (_pendingRelay && mounted) {
      _relayToBle(context.read<BroadcastService>());
    }
  }

  Future<void> _start() async {
    final ble = context.read<BleService>();
    final broadcast = context.read<BroadcastService>();
    await broadcast.start();
    await ble.start();
    // 启动后自动跳转到"发现设备"页，让用户能立即看到扫描到的 ESP32
    // 并点击连接（用户反馈：启动时蓝牙与广播读取的 bug 修复）
    if (!mounted) return;
    final navState = context.findAncestorStateOfType<MainNavigationState>();
    navState?.switchToTab(1);
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('服务已启动，请在"发现设备"页选择 ESP32 连接'),
        duration: Duration(seconds: 3),
      ),
    );
  }

  Future<void> _stop() async {
    final ble = context.read<BleService>();
    final broadcast = context.read<BroadcastService>();
    await ble.stop();
    await broadcast.stop();
    broadcast.reset();
  }

  Future<void> _selfTest() async {
    await context.read<BroadcastService>().sendSelfTest();
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('已发送测试广播，查看状态卡片'),
        duration: Duration(seconds: 2),
      ),
    );
  }

  /// v0.5.8: BLE 直连测试 — 发一条测试 JSON 到 ESP32，验证 BLE 写入通道
  Future<void> _bleTest() async {
    final ble = context.read<BleService>();
    if (!ble.isConnected) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('BLE 未连接，无法测试'), duration: Duration(seconds: 2)),
      );
      return;
    }
    final ok = await ble.sendTestPacket();
    if (!mounted) return;
    final err = ble.lastError;
    final summary = ble.discoverySummary;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(ok
            ? 'BLE 测试包已发送，查看 ESP32 串口'
            : '写入失败: ${err.isNotEmpty ? err : "未知错误"}\n${summary.isNotEmpty ? summary : ""}'),
        duration: Duration(seconds: ok ? 2 : 6),
      ),
    );
  }

  /// v0.5.8: 模拟导航广播 — 绕过 Android 原生层，直接生成 180km/h 导航数据
  void _simulateNav() {
    context.read<BroadcastService>().simulateNavigation();
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('模拟导航数据已生成（180km/h），查看 ESP32 串口是否收到'),
        duration: Duration(seconds: 3),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final running = ble.isRunning;

    return Scaffold(
      appBar: AppBar(
        title: const Text('导航转发'),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 80),
          children: [
            // 顶部状态卡片
            const StatusCard(),
            const SizedBox(height: 16),
            // 中间导航预览
            const NavPreview(),
            const SizedBox(height: 16),
            // 详细数据卡片
            const _DetailCards(),
            if (running) ...[
              const SizedBox(height: 16),
              // v0.5.8: 诊断按钮组
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _bleTest,
                      icon: const Icon(Icons.bluetooth_connected, size: 18),
                      label: const Text('BLE 测试'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _simulateNav,
                      icon: const Icon(Icons.speed, size: 18),
                      label: const Text('模拟导航'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              OutlinedButton.icon(
                onPressed: _selfTest,
                icon: const Icon(Icons.bug_report_outlined),
                label: const Text('测试广播 / TEST BROADCAST'),
              ),
            ],
          ],
        ),
      ),
      // 启停服务移到右下角悬浮按钮
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          if (running) ...[
            FloatingActionButton.extended(
              heroTag: 'stop_btn',
              onPressed: _stop,
              icon: const Icon(Icons.stop_rounded),
              label: const Text('停止'),
              backgroundColor: Theme.of(context).colorScheme.errorContainer,
              foregroundColor: Theme.of(context).colorScheme.onErrorContainer,
            ),
          ] else ...[
            FloatingActionButton.extended(
              heroTag: 'start_btn',
              onPressed: _start,
              icon: const Icon(Icons.play_arrow_rounded),
              label: const Text('启动服务'),
              backgroundColor: const Color(0xFF008375),
              foregroundColor: Colors.white,
            ),
          ],
        ],
      ),
    );
  }
}

/// 详细数据卡片：导航状态、引导信息、车道、路况、定位
class _DetailCards extends StatelessWidget {
  const _DetailCards();

  @override
  Widget build(BuildContext context) {
    return Consumer<BroadcastService>(
      builder: (context, broadcast, _) {
        return Column(
          children: [
            _DataSection(
              title: '导航状态 / Navigation',
              icon: Icons.navigation_outlined,
              rows: [
                _kv('状态 / State', broadcast.mapStateText),
                _kv('路口图 / Cross', broadcast.crossMap ?? '—'),
              ],
            ),
            const SizedBox(height: 12),
            _GuideSection(broadcast: broadcast),
            const SizedBox(height: 12),
            _DriveWaySection(broadcast: broadcast),
            const SizedBox(height: 12),
            _TmcSection(broadcast: broadcast),
            const SizedBox(height: 12),
            _LocationSection(broadcast: broadcast),
          ],
        );
      },
    );
  }

  _RowData _kv(String label, String value) => _RowData(label, value);
}

class _RowData {
  final String label;
  final String value;
  const _RowData(this.label, this.value);
}

/// 数据卡片基类
class _DataSection extends StatelessWidget {
  const _DataSection({
    required this.title,
    required this.icon,
    required this.rows,
  });

  final String title;
  final IconData icon;
  final List<_RowData> rows;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Icon(icon, size: 18, color: colorScheme.primary),
                const SizedBox(width: 8),
                Text(
                  title,
                  style: theme.textTheme.titleSmall?.copyWith(
                    color: colorScheme.onSurfaceVariant,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),
            for (final r in rows) ...[
              _KVRow(label: r.label, value: r.value),
              if (r != rows.last) const SizedBox(height: 6),
            ],
          ],
        ),
      ),
    );
  }
}

class _KVRow extends StatelessWidget {
  const _KVRow({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            flex: 2,
            child: Text(
              label,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
          Expanded(
            flex: 3,
            child: Text(
              value,
              textAlign: TextAlign.right,
              style: theme.textTheme.bodySmall?.copyWith(
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

/// 引导信息卡片
class _GuideSection extends StatelessWidget {
  const _GuideSection({required this.broadcast});
  final BroadcastService broadcast;

  @override
  Widget build(BuildContext context) {
    final g = broadcast.guideInfo;
    return _DataSection(
      title: '引导信息 / Guidance',
      icon: Icons.turn_slight_right_outlined,
      rows: [
        _RowData('当前道路 / Road', g?.curRoadName.isNotEmpty == true ? g!.curRoadName : '—'),
        _RowData('下条道路 / Next', g?.nextRoadName.isNotEmpty == true ? g!.nextRoadName : '—'),
        _RowData('剩余路程 / Remain',
            (g != null && g.routeRemainDis > 0)
                ? '${(g.routeRemainDis / 1000).toStringAsFixed(1)} 公里 / ${g.routeRemainTime ~/ 60} 分钟'
                : '—'),
        _RowData('当前车速 / Speed', (g != null && g.curSpeed > 0) ? '${g.curSpeed} km/h' : '—'),
        _RowData('限速 / Limit', (g != null && g.limitedSpeed > 0) ? '${g.limitedSpeed} km/h' : '—'),
        _RowData('摄像头 / Camera', (g != null && g.cameraDist > 0) ? '${g.cameraDist} m' : '—'),
        _RowData('服务区 / SAPA', (g != null && g.sapaDist > 0) ? '${g.sapaName} ${g.sapaDist}m' : '—'),
        _RowData('红绿灯 / Light', (g != null && g.trafficLightNum > 0) ? '${g.trafficLightNum}' : '—'),
      ],
    );
  }
}

/// 车道信息卡片
class _DriveWaySection extends StatelessWidget {
  const _DriveWaySection({required this.broadcast});
  final BroadcastService broadcast;

  @override
  Widget build(BuildContext context) {
    final d = broadcast.driveWayInfo;
    final enabled = d != null && d.enabled && d.lanes.isNotEmpty;
    final symbols = enabled
        ? d!.lanes.map((l) => AmapProtocol.laneSymbolLabel(l.backIcon)).join(' ')
        : '—';
    return _DataSection(
      title: '车道指引 / Drive Way',
      icon: Icons.view_array_outlined,
      rows: [
        _RowData('车道数 / Count', enabled ? '${d!.size} 车道' : '—'),
        _RowData('详情 / Detail', symbols),
      ],
    );
  }
}

/// 路况光柱卡片
class _TmcSection extends StatelessWidget {
  const _TmcSection({required this.broadcast});
  final BroadcastService broadcast;

  @override
  Widget build(BuildContext context) {
    final t = broadcast.tmcSegmentInfo;
    final enabled = t?.enabled == true;
    return _DataSection(
      title: '路况光柱 / Traffic Bar',
      icon: Icons.traffic_outlined,
      rows: [
        _RowData('总长 / Total',
            enabled ? '${(t!.totalDistance / 1000).toStringAsFixed(1)} 公里' : '—'),
        _RowData('剩余 / Remain',
            enabled ? '${(t!.residualDistance / 1000).toStringAsFixed(1)} 公里' : '—'),
        _RowData('段数 / Segs', enabled ? '${t!.size}' : '—'),
      ],
    );
  }
}

/// 定位信息卡片
class _LocationSection extends StatelessWidget {
  const _LocationSection({required this.broadcast});
  final BroadcastService broadcast;

  @override
  Widget build(BuildContext context) {
    final l = broadcast.locationInfo;
    return _DataSection(
      title: '定位信息 / Location',
      icon: Icons.my_location_outlined,
      rows: [
        _RowData('当前车速 / Speed', (l != null && l.speed > 0) ? '${l.speed} km/h' : '—'),
        _RowData('方向 / Bearing', (l != null && l.bearing > 0) ? '${l.bearing}°' : '—'),
        _RowData('精度 / Acc', (l != null && l.accuracy > 0) ? '${l.accuracy} m' : '—'),
      ],
    );
  }
}
