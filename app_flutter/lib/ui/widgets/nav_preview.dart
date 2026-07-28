/// 导航预览卡片：大号转向箭头 + 距离文字 + 车道信息 + 车速 + 限速
///
/// HUD cockpit style: oversized turn arrow, monospace distance, speed gauge,
/// speed-limit badge with overspeed pulse animation.
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/nav_data.dart';
import '../../protocol/amap_protocol.dart';
import '../../services/broadcast_service.dart';

class NavPreview extends StatelessWidget {
  const NavPreview({super.key});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;

    return Consumer<BroadcastService>(
      builder: (context, broadcast, _) {
        final guide = broadcast.guideInfo;
        final hasData = guide != null;

        final iconId = hasData ? (guide.icon < 0 ? 0 : guide.icon) : 0;
        final rotation = AmapProtocol.iconRotationAngle(iconId).toDouble();
        final turnLabel = hasData ? AmapProtocol.iconShortLabel(iconId) : '—';

        final distanceText = hasData && guide.segRemainDis > 0
            ? _formatDistance(guide.segRemainDis)
            : '—';

        final curRoad = hasData && guide.curRoadName.isNotEmpty
            ? guide.curRoadName
            : '未知道路';
        final nextRoad = hasData && guide.nextRoadName.isNotEmpty
            ? guide.nextRoadName
            : '未知道路';

        final curSpeed = hasData ? guide.curSpeed : 0;
        final limitedSpeed = hasData ? guide.limitedSpeed : 0;
        final overSpeed = limitedSpeed > 0 && curSpeed > limitedSpeed;

        // 车道信息
        final driveWay = broadcast.driveWayInfo;
        final hasLanes = driveWay != null && driveWay.enabled && driveWay.lanes.isNotEmpty;

        return Card(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // 标题行
                Row(
                  children: [
                    Container(
                      width: 32, height: 32,
                      decoration: BoxDecoration(
                        color: colorScheme.primary.withValues(alpha: 0.12),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Icon(Icons.navigation,
                        color: colorScheme.primary, size: 18),
                    ),
                    const SizedBox(width: 10),
                    Text('导航引导',
                      style: theme.textTheme.titleSmall?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    const Spacer(),
                    // 当前道路芯片
                    Container(
                      constraints: const BoxConstraints(maxWidth: 160),
                      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
                      decoration: BoxDecoration(
                        color: colorScheme.secondaryContainer,
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Text(curRoad,
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: colorScheme.onSecondaryContainer,
                          fontWeight: FontWeight.w600,
                        ),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 18),
                // 主预览区
                LayoutBuilder(
                  builder: (context, constraints) {
                    final compact = constraints.maxWidth < 380;
                    final guidance = _GuidanceBlock(
                      rotation: rotation, turnLabel: turnLabel,
                      distanceText: distanceText, nextRoad: nextRoad,
                    );
                    final speed = _SpeedGauge(
                      speed: curSpeed, limitedSpeed: limitedSpeed,
                      overSpeed: overSpeed,
                    );

                    if (compact) {
                      return Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [guidance, const SizedBox(height: 18), speed],
                      );
                    }

                    return IntrinsicHeight(
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Expanded(flex: 3, child: guidance),
                          const SizedBox(width: 20),
                          Expanded(flex: 2, child: speed),
                        ],
                      ),
                    );
                  },
                ),
                // 车道指引
                if (hasLanes) ...[
                  const SizedBox(height: 18),
                  _LaneStrip(
                    lanes: driveWay.lanes,
                    turnIcon: iconId,
                  ),
                ],
              ],
            ),
          ),
        );
      },
    );
  }

  String _formatDistance(int meters) {
    if (meters <= 0) return '';
    if (meters >= 1000) return '${(meters / 1000).toStringAsFixed(1)} km';
    return '$meters m';
  }
}

/// 转向指引块 — 大号旋转箭头 + 距离 + 目标道路
class _GuidanceBlock extends StatelessWidget {
  const _GuidanceBlock({
    required this.rotation, required this.turnLabel,
    required this.distanceText, required this.nextRoad,
  });
  final double rotation;
  final String turnLabel;
  final String distanceText;
  final String nextRoad;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _TurnArrow(rotation: rotation, color: colorScheme.primary, label: turnLabel),
        const SizedBox(height: 14),
        Text(distanceText,
          style: theme.textTheme.displaySmall?.copyWith(
            fontSize: 42, fontWeight: FontWeight.w700, height: 1.0,
            color: colorScheme.onSurface,
          ),
        ),
        const SizedBox(height: 6),
        Row(
          children: [
            Icon(Icons.arrow_forward_rounded, size: 14,
              color: colorScheme.onSurfaceVariant),
            const SizedBox(width: 4),
            Expanded(
              child: Text('前往 $nextRoad',
                style: theme.textTheme.bodyLarge?.copyWith(
                  color: colorScheme.onSurfaceVariant,
                ),
                maxLines: 2, overflow: TextOverflow.ellipsis,
              ),
            ),
          ],
        ),
      ],
    );
  }
}

/// 速度仪表块 — 限速徽章 + 大号数字 + 单位
class _SpeedGauge extends StatelessWidget {
  const _SpeedGauge({
    required this.speed, required this.limitedSpeed,
    required this.overSpeed,
  });
  final int speed;
  final int limitedSpeed;
  final bool overSpeed;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.4),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: colorScheme.outlineVariant.withValues(alpha: 0.3),
        ),
      ),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          _SpeedLimitBadge(speed: limitedSpeed, warning: overSpeed),
          const SizedBox(height: 12),
          _SpeedDisplay(speed: speed, overSpeed: overSpeed),
          const SizedBox(height: 2),
          Text('km/h',
            style: theme.textTheme.bodySmall?.copyWith(
              color: colorScheme.onSurfaceVariant,
              letterSpacing: 1.5,
            ),
          ),
        ],
      ),
    );
  }
}

/// 大号转向箭头 — 圆形背景 + 旋转动画 + 文字标签
class _TurnArrow extends StatelessWidget {
  const _TurnArrow({
    required this.rotation, required this.color, required this.label,
  });
  final double rotation;
  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        TweenAnimationBuilder<double>(
          tween: Tween<double>(begin: 0, end: rotation),
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOutCubic,
          builder: (context, value, child) {
            return Transform.rotate(
              angle: value * 3.14159265 / 180,
              child: child,
            );
          },
          child: Container(
            width: 72, height: 72,
            decoration: BoxDecoration(
              gradient: RadialGradient(
                colors: [
                  color.withValues(alpha: 0.18),
                  color.withValues(alpha: 0.04),
                ],
              ),
              shape: BoxShape.circle,
              border: Border.all(
                color: color.withValues(alpha: 0.3),
                width: 1.5,
              ),
            ),
            child: Icon(Icons.arrow_upward_rounded,
              size: 44, color: color, semanticLabel: label),
          ),
        ),
        const SizedBox(width: 12),
        Text(label,
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
            fontWeight: FontWeight.w700, color: color,
          ),
        ),
      ],
    );
  }
}

/// 限速标志 — 圆形红圈 + 超速脉冲动画
class _SpeedLimitBadge extends StatelessWidget {
  const _SpeedLimitBadge({required this.speed, required this.warning});
  final int speed;
  final bool warning;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    if (speed <= 0) {
      return Container(
        width: 68, height: 68,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          border: Border.all(
            color: theme.colorScheme.outlineVariant, width: 3,
          ),
        ),
        child: Center(
          child: Text('—',
            style: theme.textTheme.titleMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ),
      );
    }
    return AnimatedScale(
      scale: warning ? 1.06 : 1.0,
      duration: const Duration(milliseconds: 200),
      child: Container(
        width: 68, height: 68,
        decoration: BoxDecoration(
          color: warning ? theme.colorScheme.error : theme.colorScheme.surface,
          shape: BoxShape.circle,
          border: Border.all(color: theme.colorScheme.error, width: 3),
          boxShadow: warning
              ? [
                  BoxShadow(
                    color: theme.colorScheme.error.withValues(alpha: 0.5),
                    blurRadius: 16, spreadRadius: 4,
                  ),
                ]
              : null,
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text('限速',
              style: TextStyle(
                color: warning
                    ? theme.colorScheme.onError
                    : theme.colorScheme.error,
                fontSize: 10, fontWeight: FontWeight.w700,
                letterSpacing: 1,
              ),
            ),
            Text('$speed',
              style: TextStyle(
                color: warning
                    ? theme.colorScheme.onError
                    : theme.colorScheme.onSurface,
                fontSize: 24, fontWeight: FontWeight.w800,
                height: 1.0,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// 当前车速大号显示 — 超速时红色闪烁
class _SpeedDisplay extends StatelessWidget {
  const _SpeedDisplay({required this.speed, required this.overSpeed});
  final int speed;
  final bool overSpeed;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final color = overSpeed
        ? theme.colorScheme.error
        : theme.colorScheme.onSurface;
    return TweenAnimationBuilder<double>(
      tween: Tween<double>(begin: 0.92, end: 1.0),
      duration: const Duration(milliseconds: 400),
      curve: Curves.easeOutBack,
      builder: (context, scale, child) {
        return Transform.scale(
          scale: scale,
          child: Text('$speed',
            style: theme.textTheme.displayMedium?.copyWith(
              fontSize: 60, fontWeight: FontWeight.w800,
              color: color, height: 1.0,
            ),
          ),
        );
      },
    );
  }
}

/// 车道指引条 — 显示当前车道方向
class _LaneStrip extends StatelessWidget {
  const _LaneStrip({required this.lanes, required this.turnIcon});
  final List<LaneInfo> lanes;
  final int turnIcon;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.view_array_outlined, size: 14,
              color: colorScheme.onSurfaceVariant),
            const SizedBox(width: 6),
            Text('车道指引',
              style: theme.textTheme.labelMedium?.copyWith(
                color: colorScheme.onSurfaceVariant,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
        const SizedBox(height: 10),
        Row(
          children: lanes.map((lane) {
            final active = _isLaneActive(lane.backIcon, turnIcon);
            return Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 2),
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 250),
                  height: 36,
                  decoration: BoxDecoration(
                    color: active
                        ? colorScheme.primary
                        : colorScheme.surfaceContainerHighest,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: active
                          ? colorScheme.primary
                          : colorScheme.outlineVariant.withValues(alpha: 0.5),
                      width: active ? 1.5 : 1,
                    ),
                  ),
                  child: Center(
                    child: Text(
                      AmapProtocol.laneSymbolLabel(lane.backIcon),
                      style: TextStyle(
                        color: active
                            ? colorScheme.onPrimary
                            : colorScheme.onSurfaceVariant,
                        fontSize: 16,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                ),
              ),
            );
          }).toList(),
        ),
      ],
    );
  }

  bool _isLaneActive(int backIcon, int turnIcon) {
    switch (turnIcon) {
      case 1: case 9: case 20:
        return backIcon == 0 || backIcon == 2 || backIcon == 4 || backIcon == 7;
      case 2:
        return backIcon == 1 || backIcon == 2 || backIcon == 6 || backIcon == 7;
      case 3:
        return backIcon == 3 || backIcon == 4 || backIcon == 6 || backIcon == 7;
      case 4: return backIcon == 1 || backIcon == 2;
      case 5: return backIcon == 3 || backIcon == 4;
      case 8: case 19: return backIcon == 5;
      default: return true;
    }
  }
}
