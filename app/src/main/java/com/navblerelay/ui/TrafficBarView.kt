package com.navblerelay.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.navblerelay.R
import com.navblerelay.protocol.TmcSegment

/**
 * 线路路况光柱图（TMC）
 * 参考高德地图官方样式：横向彩色条，按距离比例显示畅通/缓行/拥堵/严重拥堵
 */
class TrafficBarView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.md_outline_variant)
        style = Paint.Style.FILL
    }
    private val segmentPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    private var segments: List<TmcSegment> = emptyList()
    private var totalDistance: Int = 0
    private val cornerRadius = 8f

    fun setData(list: List<TmcSegment>, total: Int) {
        segments = list
        totalDistance = total
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()

        canvas.drawRoundRect(RectF(0f, 0f, w, h), cornerRadius, cornerRadius, bgPaint)

        if (segments.isEmpty() || totalDistance <= 0) return

        var currentX = 0f
        segments.forEach { seg ->
            val segW = (seg.distance.toFloat() / totalDistance) * w
            segmentPaint.color = ContextCompat.getColor(context, colorForStatus(seg.status))
            canvas.drawRect(RectF(currentX, 0f, currentX + segW, h), segmentPaint)
            currentX += segW
        }
    }

    private fun colorForStatus(status: Int): Int = when (status) {
        1 -> R.color.tmc_smooth
        2 -> R.color.tmc_slow
        3 -> R.color.tmc_congested
        4 -> R.color.tmc_severe
        else -> R.color.tmc_unknown
    }
}
