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
            padding: const EdgeInsets.all(20),
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
                      '导航引导 / Turn Preview',
                      style: theme.textTheme.titleSmall?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                // 主预览区
                IntrinsicHeight(
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      // 左侧：转向箭头 + 距离 + 道路
                      Expanded(
                        flex: 3,
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            // 大号转向箭头
                            _TurnArrow(
                              rotation: rotation,
                              color: colorScheme.primary,
                              label: turnLabel,
                            ),
                            const SizedBox(height: 12),
                            // 距离
                            Text(
                              distanceText,
                              style: theme.textTheme.displaySmall?.copyWith(
                                fontSize: 40,
                                fontWeight: FontWeight.w700,
                                color: colorScheme.onSurface,
                              ),
                            ),
                            const SizedBox(height: 4),
                            // 下一道路
                            Text(
                              '前往 $nextRoad',
                              style: theme.textTheme.bodyLarge?.copyWith(
                                color: colorScheme.onSurfaceVariant,
                              ),
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                            const SizedBox(height: 8),
                            // 当前道路（路口信息）
                            Container(
                              padding: const EdgeInsets.symmetric(
                                  horizontal: 10, vertical: 6),
                              decoration: BoxDecoration(
                                color: colorScheme.secondaryContainer,
                                borderRadius: BorderRadius.circular(12),
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
                        ),
                      ),
                      const SizedBox(width: 16),
                      // 右侧：车速 + 限速
                      Expanded(
                        flex: 2,
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            // 限速标志（圆形红色背景白字）
                            _SpeedLimitBadge(
                              speed: limitedSpeed,
                              warning: overSpeed,
                            ),
                            const SizedBox(height: 16),
                            // 当前车速
                            _SpeedDisplay(
                              speed: curSpeed,
                              overSpeed: overSpeed,
                            ),
                            const SizedBox(height: 4),
                            Text(
                              'km/h',
                              style: theme.textTheme.bodySmall?.copyWith(
                                color: colorScheme.onSurfaceVariant,
                                letterSpacing: 1.5,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
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
          color: warning ? const Color(0xFFBA1A1A) : const Color(0xFFE53935),
          shape: BoxShape.circle,
          border: Border.all(color: Colors.white, width: 3),
          boxShadow: warning
              ? [
                  BoxShadow(
                    color: const Color(0xFFBA1A1A).withValues(alpha: 0.5),
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
                color: Colors.white,
                fontSize: 10,
                fontWeight: FontWeight.w600,
              ),
            ),
            Text(
              '$speed',
              style: const TextStyle(
                color: Colors.white,
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
        ? const Color(0xFFBA1A1A)
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
