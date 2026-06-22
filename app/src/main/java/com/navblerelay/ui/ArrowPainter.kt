package com.navblerelay.ui

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF

/**
 * 统一的导航箭头绘制器（主要用于车道指引小图标）
 *
 * 参考高德地图官方导航 SDK 文档对转向图标/车道线的规范建议：
 * - 高德建议优先使用 NaviInfo.getIconBitmap()（含路网信息，更直观），
 *   当 Bitmap 为空时使用 getIconType() 自定义绘制；
 * - 路线转向箭头支持 setArrowColor / setArrowWidth / setTurnArrowIs3D 等接口自定义；
 * - 车道线信息通过 AMapLaneInfo 回调获取，官方提供 CreateLaneInfoImage 能力。
 *
 * 本项目通过 AmapAuto 公版广播协议接收导航数据，没有直接接入高德导航 SDK。
 * 路口转向大图标已改用高德 iOS Watch 导航演示库中的 default_navi_hud_*.png
 * 位图资源（ic_nav_0 .. ic_nav_20）；ArrowPainter 仅用于车道指引小图标绘制，
 * 对齐官方视觉风格：
 * - 使用粗线描边（约 24% 宽度）+ 圆角线端/连接，保证箭头不会断裂；
 * - 箭头头部使用实心三角形，方向与最后一段路径一致；
 * - 单一颜色，便于在车道蓝色背景上保持可读性。
 */
object ArrowPainter {

    const val ICON_STRAIGHT = 0
    const val ICON_LEFT = 1
    const val ICON_RIGHT = 2
    const val ICON_LEFT_FRONT = 3
    const val ICON_RIGHT_FRONT = 4
    const val ICON_LEFT_BACK = 5
    const val ICON_RIGHT_BACK = 6
    const val ICON_UTURN = 7
    const val ICON_ROUNDABOUT_ENTER = 8
    const val ICON_ROUNDABOUT_EXIT = 9
    const val ICON_ARRIVE = 10

    // 车道指引图标使用更粗的描边与更大的箭头头部，提升小尺寸下的可读性
    private const val STROKE_RATIO = 0.24f
    private const val HEAD_HALF_WIDTH_RATIO = 0.30f
    private const val HEAD_LENGTH_RATIO = 0.26f

    fun draw(canvas: Canvas, icon: Int, rect: RectF, color: Int) {
        if (rect.width() <= 0 || rect.height() <= 0) return

        val strokeWidth = rect.width() * STROKE_RATIO
        val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            this.color = color
            style = Paint.Style.STROKE
            this.strokeWidth = strokeWidth
            strokeCap = Paint.Cap.ROUND
            strokeJoin = Paint.Join.ROUND
        }
        val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            this.color = color
            style = Paint.Style.FILL
        }

        val cx = rect.centerX()
        val cy = rect.centerY()
        val w = rect.width()
        val h = rect.height()

        when (icon) {
            ICON_STRAIGHT -> drawStraight(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_LEFT -> drawLeft(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_RIGHT -> drawRight(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_LEFT_FRONT -> drawLeftFront(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_RIGHT_FRONT -> drawRightFront(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_LEFT_BACK -> drawLeftBack(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_RIGHT_BACK -> drawRightBack(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_UTURN -> drawUturn(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_ROUNDABOUT_ENTER -> drawRoundaboutEnter(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_ROUNDABOUT_EXIT -> drawRoundaboutExit(canvas, cx, cy, w, h, strokePaint, fillPaint)
            ICON_ARRIVE -> drawArrive(canvas, cx, cy, w, h, strokePaint, fillPaint)
        }
    }

    private fun drawStraight(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val bodyTop = cy - h * 0.12f
        val bodyBottom = cy + h * 0.42f

        canvas.drawLine(cx, bodyBottom, cx, bodyTop + headLen, strokePaint)
        canvas.drawPath(triangle(cx, bodyTop, halfW, headLen), fillPaint)
    }

    private fun drawLeft(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val path = Path().apply {
            moveTo(cx, cy + h * 0.40f)
            lineTo(cx, cy)
            lineTo(cx - w * 0.22f, cy)
        }
        canvas.drawPath(path, strokePaint)
        canvas.drawPath(triangle(cx - w * 0.22f - headLen, cy, halfW, headLen, Point.LEFT), fillPaint)
    }

    private fun drawRight(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val path = Path().apply {
            moveTo(cx, cy + h * 0.40f)
            lineTo(cx, cy)
            lineTo(cx + w * 0.22f, cy)
        }
        canvas.drawPath(path, strokePaint)
        canvas.drawPath(triangle(cx + w * 0.22f + headLen, cy, halfW, headLen, Point.RIGHT), fillPaint)
    }

    private fun drawLeftFront(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val endX = cx - w * 0.22f
        val endY = cy - h * 0.22f
        canvas.drawLine(cx + w * 0.18f, cy + h * 0.38f, endX + headLen * 0.6f, endY + headLen * 0.6f, strokePaint)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.LEFT_TOP), fillPaint)
    }

    private fun drawRightFront(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val endX = cx + w * 0.22f
        val endY = cy - h * 0.22f
        canvas.drawLine(cx - w * 0.18f, cy + h * 0.38f, endX - headLen * 0.6f, endY + headLen * 0.6f, strokePaint)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.RIGHT_TOP), fillPaint)
    }

    private fun drawLeftBack(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val endX = cx - w * 0.22f
        val endY = cy + h * 0.22f
        canvas.drawLine(cx + w * 0.18f, cy - h * 0.38f, endX + headLen * 0.6f, endY - headLen * 0.6f, strokePaint)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.LEFT_BOTTOM), fillPaint)
    }

    private fun drawRightBack(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val endX = cx + w * 0.22f
        val endY = cy + h * 0.22f
        canvas.drawLine(cx - w * 0.18f, cy - h * 0.38f, endX - headLen * 0.6f, endY - headLen * 0.6f, strokePaint)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.RIGHT_BOTTOM), fillPaint)
    }

    private fun drawUturn(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val rect = RectF(cx - w * 0.22f, cy - h * 0.10f, cx + w * 0.22f, cy + h * 0.34f)
        val path = Path().apply {
            moveTo(cx, cy + h * 0.34f)
            lineTo(cx, cy + h * 0.10f)
            arcTo(rect, 90f, 180f)
            lineTo(cx - w * 0.22f - headLen, cy - h * 0.10f)
        }
        canvas.drawPath(path, strokePaint)
        canvas.drawPath(triangle(cx - w * 0.22f - headLen, cy - h * 0.10f, halfW, headLen, Point.LEFT), fillPaint)
    }

    private fun drawRoundaboutEnter(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val ringRect = RectF(cx - w * 0.22f, cy - h * 0.05f, cx + w * 0.22f, cy + h * 0.39f)
        canvas.drawArc(ringRect, 0f, 360f, false, strokePaint)
        canvas.drawLine(cx, cy - h * 0.05f, cx, cy - h * 0.38f, strokePaint)
        canvas.drawPath(triangle(cx, cy - h * 0.38f - headLen * 0.7f, halfW, headLen), fillPaint)
    }

    private fun drawRoundaboutExit(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val ringRect = RectF(cx - w * 0.22f, cy - h * 0.32f, cx + w * 0.22f, cy + h * 0.12f)
        canvas.drawArc(ringRect, 0f, 360f, false, strokePaint)
        canvas.drawLine(cx + w * 0.05f, cy + h * 0.12f, cx + w * 0.28f, cy + h * 0.35f, strokePaint)
        canvas.drawPath(
            triangle(cx + w * 0.28f + headLen * 0.6f, cy + h * 0.35f + headLen * 0.6f, halfW, headLen, Point.RIGHT_BOTTOM),
            fillPaint
        )
    }

    private fun drawArrive(
        canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float,
        strokePaint: Paint, fillPaint: Paint
    ) {
        val halfW = w * HEAD_HALF_WIDTH_RATIO
        val headLen = h * HEAD_LENGTH_RATIO
        val bodyTop = cy - h * 0.42f
        val bodyBottom = cy + h * 0.12f

        canvas.drawLine(cx, bodyTop, cx, bodyBottom - headLen, strokePaint)
        canvas.drawPath(triangle(cx, bodyBottom, halfW, headLen, Point.BOTTOM), fillPaint)
    }

    private enum class Point { CENTER, LEFT, RIGHT, LEFT_TOP, RIGHT_TOP, LEFT_BOTTOM, RIGHT_BOTTOM, BOTTOM }

    private fun triangle(x: Float, y: Float, halfW: Float, len: Float, point: Point = Point.CENTER): Path {
        return Path().apply {
            when (point) {
                Point.CENTER, Point.BOTTOM -> {
                    moveTo(x, y + len)
                    lineTo(x - halfW, y - len)
                    lineTo(x + halfW, y - len)
                }
                Point.LEFT -> {
                    moveTo(x - len, y)
                    lineTo(x + len, y - halfW)
                    lineTo(x + len, y + halfW)
                }
                Point.RIGHT -> {
                    moveTo(x + len, y)
                    lineTo(x - len, y - halfW)
                    lineTo(x - len, y + halfW)
                }
                Point.LEFT_TOP -> {
                    moveTo(x - len * 0.7f, y - len * 0.7f)
                    lineTo(x + len * 0.6f, y - len * 0.3f)
                    lineTo(x + len * 0.3f, y + len * 0.6f)
                }
                Point.RIGHT_TOP -> {
                    moveTo(x + len * 0.7f, y - len * 0.7f)
                    lineTo(x - len * 0.6f, y - len * 0.3f)
                    lineTo(x - len * 0.3f, y + len * 0.6f)
                }
                Point.LEFT_BOTTOM -> {
                    moveTo(x - len * 0.7f, y + len * 0.7f)
                    lineTo(x + len * 0.6f, y + len * 0.3f)
                    lineTo(x + len * 0.3f, y - len * 0.6f)
                }
                Point.RIGHT_BOTTOM -> {
                    moveTo(x + len * 0.7f, y + len * 0.7f)
                    lineTo(x - len * 0.6f, y + len * 0.3f)
                    lineTo(x - len * 0.3f, y - len * 0.6f)
                }
                else -> {
                    moveTo(x, y + len)
                    lineTo(x - halfW, y - len)
                    lineTo(x + halfW, y - len)
                }
            }
            close()
        }
    }
}
