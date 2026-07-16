/// 导航预览卡片：大号转向箭头 + 距离文字 + 当前道路名 + 车速 + 限速
///
/// HUD 风格：左侧大号转向箭头与距离，右侧车速与限速标志。
library;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

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

        // 转向图标 ID
        final iconId = hasData ? (guide.icon < 0 ? 0 : guide.icon) : 0;
        final rotation = AmapProtocol.iconRotationAngle(iconId).toDouble();
        final turnLabel = hasData
            ? AmapProtocol.iconShortLabel(iconId)
            : '—';

        // 距离文字
        final distanceText = hasData && guide.segRemainDis > 0
            ? _formatDistance(guide.segRemainDis)
            : '—';

        // 当前 / 下一道路
        final curRoad = hasData && guide.curRoadName.isNotEmpty
            ? guide.curRoadName
            : '未知道路';
        final nextRoad = hasData && guide.nextRoadName.isNotEmpty
            ? guide.nextRoadName
            : '未知道路';

        // 车速 / 限速
        final curSpeed = hasData ? guide.curSpeed : 0;
        final limitedSpeed = hasData ? guide.limitedSpeed : 0;
        final overSpeed = limitedSpeed > 0 && curSpeed > limitedSpeed;

        return Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // 标题行
                Row(
                  children: [
                    Icon(Icons.navigation_outlined,
                        color: colorScheme.primary, size: 20),
                    const SizedBox(width: 8),
                    Text(
                      '导航引导',
                      style: theme.textTheme.titleSmall?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                // 主预览区
                LayoutBuilder(
                  builder: (context, constraints) {
                    final compact = constraints.maxWidth < 380;
                    final guidance = _GuidanceBlock(
                      rotation: rotation,
                      turnLabel: turnLabel,
                      distanceText: distanceText,
                      nextRoad: nextRoad,
                      curRoad: curRoad,
                    );
                    final speed = _SpeedBlock(
                      speed: curSpeed,
                      limitedSpeed: limitedSpeed,
                      overSpeed: overSpeed,
                    );

                    if (compact) {
                      return Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          guidance,
                          const SizedBox(height: 16),
                          speed,
                        ],
                      );
                    }

                    return IntrinsicHeight(
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Expanded(flex: 3, child: guidance),
                          const SizedBox(width: 16),
                          Expanded(flex: 2, child: speed),
                        ],
                      ),
                    );
                  },
                ),
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

class _GuidanceBlock extends StatelessWidget {
  const _GuidanceBlock({
    required this.rotation,
    required this.turnLabel,
    required this.distanceText,
    required this.nextRoad,
    required this.curRoad,
  });

  final double rotation;
  final String turnLabel;
  final String distanceText;
  final String nextRoad;
  final String curRoad;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _TurnArrow(
          rotation: rotation,
          color: colorScheme.primary,
          label: turnLabel,
        ),
        const SizedBox(height: 12),
        Text(
          distanceText,
          style: theme.textTheme.displaySmall?.copyWith(
            fontSize: 40,
            fontWeight: FontWeight.w700,
            color: colorScheme.onSurface,
          ),
        ),
        const SizedBox(height: 4),
        Text(
          '前往 $nextRoad',
          style: theme.textTheme.bodyLarge?.copyWith(
            color: colorScheme.onSurfaceVariant,
          ),
          maxLines: 2,
          overflow: TextOverflow.ellipsis,
        ),
        const SizedBox(height: 8),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
          decoration: BoxDecoration(
            color: colorScheme.secondaryContainer,
            borderRadius: BorderRadius.circular(10),
          ),
          child: Text(
            curRoad,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: colorScheme.onSecondaryContainer,
              fontWeight: FontWeight.w500,
            ),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    );
  }
}

class _SpeedBlock extends StatelessWidget {
  const _SpeedBlock({
    required this.speed,
    required this.limitedSpeed,
    required this.overSpeed,
  });

  final int speed;
  final int limitedSpeed;
  final bool overSpeed;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        _SpeedLimitBadge(speed: limitedSpeed, warning: overSpeed),
        const SizedBox(height: 16),
        _SpeedDisplay(speed: speed, overSpeed: overSpeed),
        const SizedBox(height: 4),
        Text(
          'km/h',
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

/// 大号转向箭头（根据旋转角度旋转）
class _TurnArrow extends StatelessWidget {
  const _TurnArrow({
    required this.rotation,
    required this.color,
    required this.label,
  });

  final double rotation;
  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        // 旋转的箭头图标（随转向角度平滑旋转）
        TweenAnimationBuilder<double>(
          tween: Tween<double>(begin: 0, end: rotation),
          duration: const Duration(milliseconds: 250),
          curve: Curves.easeOutCubic,
          builder: (context, value, child) {
            return Transform.rotate(
              angle: value * 3.14159265 / 180,
              child: child,
            );
          },
          child: Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(
              color: color.withValues(alpha: 0.12),
              shape: BoxShape.circle,
            ),
            child: Icon(
              Icons.arrow_upward_rounded,
              size: 44,
              color: color,
              semanticLabel: label,
            ),
          ),
        ),
        const SizedBox(width: 12),
        // 转向文字
        Text(
          label,
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
                fontWeight: FontWeight.w700,
                color: color,
              ),
        ),
      ],
    );
  }
}

/// 限速标志（圆形红色背景白字）
class _SpeedLimitBadge extends StatelessWidget {
  const _SpeedLimitBadge({required this.speed, required this.warning});
  final int speed;
  final bool warning;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    if (speed <= 0) {
      return Container(
        width: 64,
        height: 64,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          border: Border.all(
            color: theme.colorScheme.outlineVariant,
            width: 3,
          ),
        ),
        child: Center(
          child: Text(
            '—',
            style: theme.textTheme.titleMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ),
      );
    }
    return AnimatedScale(
      scale: warning ? 1.08 : 1.0,
      duration: const Duration(milliseconds: 200),
      child: Container(
        width: 64,
        height: 64,
        decoration: BoxDecoration(
          color: warning ? theme.colorScheme.error : theme.colorScheme.surface,
          shape: BoxShape.circle,
          border: Border.all(color: theme.colorScheme.error, width: 3),
          boxShadow: warning
              ? [
                  BoxShadow(
                    color: theme.colorScheme.error.withValues(alpha: 0.45),
                    blurRadius: 12,
                    spreadRadius: 2,
                  ),
                ]
              : null,
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(
              '限速',
              style: TextStyle(
                color: warning ? theme.colorScheme.onError : theme.colorScheme.error,
                fontSize: 10,
                fontWeight: FontWeight.w600,
              ),
            ),
            Text(
              '$speed',
              style: TextStyle(
                color: warning ? theme.colorScheme.onError : theme.colorScheme.onSurface,
                fontSize: 22,
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// 当前车速大号显示
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
    return Text(
      '$speed',
      style: theme.textTheme.displayMedium?.copyWith(
        fontSize: 56,
        fontWeight: FontWeight.w800,
        color: color,
        height: 1.0,
      ),
    );
  }
}
