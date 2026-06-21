package com.navblerelay.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.navblerelay.R
import com.navblerelay.protocol.LaneInfo

/**
 * 路口车道指引视图
 *
 * 参考高德地图官方车道线样式：
 * - 蓝色圆角背景
 * - 车道间白色分隔线
 * - 每个车道中央绘制统一的白色箭头图标，避免断裂/镂空
 */
class LaneGuideView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val laneBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.lane_bg)
        style = Paint.Style.FILL
    }
    private val dividerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.lane_divider)
        strokeWidth = 2f
    }
    private val arrowColor = ContextCompat.getColor(context, R.color.lane_arrow)

    private var lanes: List<LaneInfo> = emptyList()
    private val cornerRadius = 16f

    fun setLanes(list: List<LaneInfo>) {
        lanes = list
        requestLayout()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (lanes.isEmpty()) return

        val count = lanes.size.coerceAtLeast(1)
        val laneWidth = width.toFloat() / count
        val h = height.toFloat()

        // 整体蓝色背景
        val bgRect = RectF(0f, 0f, width.toFloat(), h)
        canvas.drawRoundRect(bgRect, cornerRadius, cornerRadius, laneBgPaint)

        // 每个车道箭头
        lanes.forEachIndexed { index, lane ->
            val cx = laneWidth * index + laneWidth / 2f
            val rect = RectF(
                cx - laneWidth * 0.42f,
                h * 0.15f,
                cx + laneWidth * 0.42f,
                h * 0.85f
            )
            ArrowPainter.draw(canvas, mapLaneIcon(lane.backIcon), rect, arrowColor)
        }

        // 车道分隔线
        for (i in 1 until count) {
            val x = laneWidth * i
            canvas.drawLine(x, h * 0.15f, x, h * 0.85f, dividerPaint)
        }
    }

    /**
     * DriveWayInfo.backIcon 取值映射到 ArrowPainter 图标
     * 0-直行; 1-左前; 2-右前; 3-左转; 4-右转; 5-左后; 6-右后; 7-掉头
     */
    private fun mapLaneIcon(backIcon: Int): Int = when (backIcon) {
        0 -> ArrowPainter.ICON_STRAIGHT
        1 -> ArrowPainter.ICON_LEFT_FRONT
        2 -> ArrowPainter.ICON_RIGHT_FRONT
        3 -> ArrowPainter.ICON_LEFT
        4 -> ArrowPainter.ICON_RIGHT
        5 -> ArrowPainter.ICON_LEFT_BACK
        6 -> ArrowPainter.ICON_RIGHT_BACK
        7 -> ArrowPainter.ICON_UTURN
        else -> ArrowPainter.ICON_STRAIGHT
    }
}
