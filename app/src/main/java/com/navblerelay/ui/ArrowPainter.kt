package com.navblerelay.ui

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import kotlin.math.hypot

/**
 * 统一的导航箭头绘制器
 *
 * 参考高德地图官方导航 SDK 文档对转向图标/车道线的规范建议：
 * - 高德建议优先使用 NaviInfo.getIconBitmap()（含路网信息，更直观），
 *   当 Bitmap 为空时使用 getIconType() 自定义绘制；
 * - 路线转向箭头支持 setArrowColor / setArrowWidth / setTurnArrowIs3D 等接口自定义；
 * - 车道线信息通过 AMapLaneInfo 回调获取，官方提供 CreateLaneInfoImage 能力。
 *
 * 本项目通过 AmapAuto 公版广播协议接收导航数据，没有直接接入高德导航 SDK。
 * 路口转向大图标已改用统一的 ic_nav_*.xml 矢量资源；ArrowPainter 仅用于车道指引
 * 小图标绘制，对齐官方视觉风格：
 * - 使用粗线描边（约 24% 宽度）+ 圆角线端/连接，保证箭头不会断裂；
 * - 箭头头部使用实心三角形，方向与最后一段路径一致；
 * - 单一颜色，便于在车道蓝色背景上保持可读性。
 *
 * 相关文档：
 * - 导航实时数据获取：https://lbs.amap.com/api/android-navi-sdk/guide/navigation-map/navi-info
 * - 自定义其他图面元素：https://lbs.amap.com/api/android-navi-sdk/guide/custom-ui/custom-other-overlay
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

    private fun drawStraight(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx
        val baseY = cy + h * 0.30f
        val tipX = cx
        val tipY = cy - h * 0.40f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawLeft(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val p = Path().apply {
            moveTo(cx, cy + h * 0.30f)
            lineTo(cx, cy - h * 0.10f)
            lineTo(cx - w * 0.35f, cy - h * 0.10f)
        }
        canvas.drawPath(p, stroke)
        val baseX = cx
        val baseY = cy - h * 0.10f
        val tipX = cx - w * 0.55f
        val tipY = cy - h * 0.10f
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, h * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawRight(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val p = Path().apply {
            moveTo(cx, cy + h * 0.30f)
            lineTo(cx, cy - h * 0.10f)
            lineTo(cx + w * 0.35f, cy - h * 0.10f)
        }
        canvas.drawPath(p, stroke)
        val baseX = cx
        val baseY = cy - h * 0.10f
        val tipX = cx + w * 0.55f
        val tipY = cy - h * 0.10f
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, h * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawLeftFront(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx + w * 0.05f
        val baseY = cy + h * 0.32f
        val tipX = cx - w * 0.32f
        val tipY = cy - h * 0.32f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawRightFront(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx - w * 0.05f
        val baseY = cy + h * 0.32f
        val tipX = cx + w * 0.32f
        val tipY = cy - h * 0.32f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawLeftBack(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx + w * 0.05f
        val baseY = cy - h * 0.32f
        val tipX = cx - w * 0.32f
        val tipY = cy + h * 0.32f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawRightBack(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx - w * 0.05f
        val baseY = cy - h * 0.32f
        val tipX = cx + w * 0.32f
        val tipY = cy + h * 0.32f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawUturn(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val r = w * 0.22f
        val topY = cy - h * 0.15f
        val leftX = cx - r
        val rightX = cx + r
        val p = Path().apply {
            moveTo(cx, cy + h * 0.35f)
            lineTo(cx, topY + r)
            arcTo(leftX, topY - r, rightX, topY + r, 90f, 180f, false)
        }
        canvas.drawPath(p, stroke)
        val tipX = cx - r
        val tipY = topY
        val baseX = cx - r
        val baseY = topY + r * 0.9f
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, h * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawRoundaboutEnter(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val r = w * 0.18f
        val centerY = cy + h * 0.05f
        val p = Path().apply {
            addCircle(cx, centerY, r, Path.Direction.CCW)
        }
        canvas.drawPath(p, stroke)
        canvas.drawLine(cx, centerY - r, cx, cy - h * 0.40f, stroke)
        canvas.drawPath(triangleHead(cx, cy - h * 0.40f, cx, centerY - r, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawRoundaboutExit(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val r = w * 0.18f
        val centerY = cy + h * 0.05f
        val p = Path().apply {
            addCircle(cx, centerY, r, Path.Direction.CCW)
        }
        canvas.drawPath(p, stroke)
        canvas.drawLine(cx, centerY + r, cx + w * 0.35f, cy + h * 0.30f, stroke)
        canvas.drawPath(triangleHead(cx + w * 0.35f, cy + h * 0.30f, cx, centerY + r, h * HEAD_HALF_WIDTH_RATIO), fill)
    }

    private fun drawArrive(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, stroke: Paint, fill: Paint) {
        val baseX = cx
        val baseY = cy - h * 0.30f
        val tipX = cx
        val tipY = cy + h * 0.40f
        canvas.drawLine(baseX, baseY, tipX, tipY, stroke)
        canvas.drawPath(triangleHead(tipX, tipY, baseX, baseY, w * HEAD_HALF_WIDTH_RATIO), fill)
    }

    /**
     * 根据“尖端 + 底边中心”生成实心三角形箭头
     */
    private fun triangleHead(tipX: Float, tipY: Float, baseCenterX: Float, baseCenterY: Float, halfWidth: Float): Path {
        val dx = tipX - baseCenterX
        val dy = tipY - baseCenterY
        val len = hypot(dx, dy)
        if (len == 0f) return Path()
        val ux = dx / len
        val uy = dy / len
        // 垂直于方向的向量
        val px = -uy * halfWidth
        val py = ux * halfWidth
        return Path().apply {
            moveTo(tipX, tipY)
            lineTo(baseCenterX + px, baseCenterY + py)
            lineTo(baseCenterX - px, baseCenterY - py)
            close()
        }
    }
}
