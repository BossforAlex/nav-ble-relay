/// 主界面
///
/// 布局：顶部状态卡片 → 中间导航预览 → 详细数据卡片 → 底部控制按钮
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/ble_service.dart';
import '../main.dart';
import '../protocol/amap_protocol.dart';
import '../services/broadcast_service.dart';
import 'widgets/nav_preview.dart';
import 'widgets/status_card.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> with WidgetsBindingObserver {
  bool _relayWired = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    // 推迟绑定中继，确保 Provider 就绪
    WidgetsBinding.instance.addPostFrameCallback((_) => _wireRelay());
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // 应用回到前台时刷新广播接收状态显示
    if (state == AppLifecycleState.resumed) {
      context.read<BroadcastService>().notifyListeners();
    }
  }

  /// 绑定中继：广播数据变化时转发到 BLE
  void _wireRelay() {
    if (_relayWired) return;
    final broadcast = context.read<BroadcastService>();
    broadcast.addListener(() => _relayToBle(broadcast));
    _relayWired = true;
  }

  void _relayToBle(BroadcastService broadcast) {
    if (!mounted) return;
    final ble = context.read<BleService>();
    final settings = context.read<SettingsService>();
    if (!ble.isConnected) return;

    final guide = broadcast.guideInfo;
    if (guide != null) {
      ble.sendGuideInfo(guide, compact: settings.compactMode);
    }
    final driveWay = broadcast.driveWayInfo;
    if (driveWay != null) {
      ble.sendDriveWay(driveWay);
    }
    final tmc = broadcast.tmcSegmentInfo;
    if (tmc != null) {
      ble.sendTmcSegment(tmc);
    }
    final loc = broadcast.locationInfo;
    if (loc != null) {
      ble.sendLocation(loc);
    }
  }

  Future<void> _start() async {
    final ble = context.read<BleService>();
    final broadcast = context.read<BroadcastService>();
    await broadcast.start();
    await ble.start();
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

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final running = ble.isRunning;

    return Scaffold(
      appBar: AppBar(
        title: const Text('导航BLE转发'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings_outlined),
            tooltip: '设置',
            onPressed: () => Navigator.pushNamed(context, '/settings'),
          ),
        ],
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
          children: [
            // 顶部状态卡片
            const StatusCard(),
            const SizedBox(height: 16),
            // 中间导航预览
            const NavPreview(),
            const SizedBox(height: 16),
            // 详细数据卡片
            const _DetailCards(),
            const SizedBox(height: 24),
            // 控制按钮
            _ControlButtons(
              running: running,
              onStart: _start,
              onStop: _stop,
              onTest: _selfTest,
            ),
          ],
        ),
      ),
    );
  }
}

/// 控制按钮组
class _ControlButtons extends StatelessWidget {
  const _ControlButtons({
    required this.running,
    required this.onStart,
    required this.onStop,
    required this.onTest,
  });

  final bool running;
  final VoidCallback onStart;
  final VoidCallback onStop;
  final VoidCallback onTest;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: FilledButton.icon(
                onPressed: running ? null : onStart,
                icon: const Icon(Icons.play_arrow_rounded),
                label: const Text('启动服务 / START'),
                style: FilledButton.styleFrom(
                  backgroundColor: const Color(0xFF008375),
                ),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: FilledButton.icon(
                onPressed: running ? onStop : null,
                icon: const Icon(Icons.stop_rounded),
                label: const Text('停止服务 / STOP'),
                style: FilledButton.styleFrom(
                  backgroundColor: theme.colorScheme.errorContainer,
                  foregroundColor: theme.colorScheme.onErrorContainer,
                ),
              ),
            ),
          ],
        ),
        if (running) ...[
          const SizedBox(height: 12),
          OutlinedButton.icon(
            onPressed: onTest,
            icon: const Icon(Icons.bug_report_outlined),
            label: const Text('测试广播 / TEST BROADCAST'),
          ),
        ],
      ],
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
