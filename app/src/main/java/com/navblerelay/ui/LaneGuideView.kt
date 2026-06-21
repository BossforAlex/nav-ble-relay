package com.navblerelay.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.navblerelay.R
import com.navblerelay.protocol.LaneInfo

/**
 * 路口车道指引视图
 * 参考高德地图官方车道线样式：蓝色背景、白色箭头、车道间白色分隔线
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
    private val arrowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.lane_arrow)
        style = Paint.Style.FILL
    }

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
            drawArrow(canvas, lane.backIcon, cx, h / 2f, laneWidth * 0.55f, h * 0.55f)
        }

        // 车道分隔线
        for (i in 1 until count) {
            val x = laneWidth * i
            canvas.drawLine(x, h * 0.15f, x, h * 0.85f, dividerPaint)
        }
    }

    private fun drawArrow(canvas: Canvas, icon: Int, cx: Float, cy: Float, w: Float, h: Float) {
        val path = Path()
        when (icon) {
            0 -> drawStraight(path, cx, cy, w, h)
            1 -> drawLeftFront(path, cx, cy, w, h)
            2 -> drawRightFront(path, cx, cy, w, h)
            3 -> drawLeft(path, cx, cy, w, h)
            4 -> drawRight(path, cx, cy, w, h)
            5 -> drawLeftBack(path, cx, cy, w, h)
            6 -> drawRightBack(path, cx, cy, w, h)
            7 -> drawUturn(path, cx, cy, w, h)
            else -> drawStraight(path, cx, cy, w, h)
        }
        canvas.drawPath(path, arrowPaint)
    }

    private fun drawStraight(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        val halfW = w * 0.3f
        val halfH = h * 0.4f
        p.moveTo(cx - halfW, cy + halfH)
        p.lineTo(cx, cy - halfH)
        p.lineTo(cx + halfW, cy + halfH)
        p.lineTo(cx + halfW * 0.4f, cy + halfH)
        p.lineTo(cx + halfW * 0.4f, cy + halfH * 0.2f)
        p.lineTo(cx - halfW * 0.4f, cy + halfH * 0.2f)
        p.lineTo(cx - halfW * 0.4f, cy + halfH)
        p.close()
    }

    private fun drawLeft(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        val r = w * 0.35f
        p.moveTo(cx + r * 0.8f, cy + h * 0.35f)
        p.lineTo(cx + r * 0.8f, cy)
        p.quadTo(cx + r * 0.8f, cy - h * 0.35f, cx - r * 0.2f, cy - h * 0.35f)
        p.lineTo(cx - r, cy - h * 0.35f)
        p.lineTo(cx - r * 0.4f, cy - h * 0.55f)
        p.lineTo(cx + r * 0.8f, cy - h * 0.55f)
        p.quadTo(cx + r * 1.6f, cy - h * 0.55f, cx + r * 1.6f, cy)
        p.lineTo(cx + r * 1.6f, cy + h * 0.35f)
        p.close()
    }

    private fun drawRight(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        val r = w * 0.35f
        p.moveTo(cx - r * 0.8f, cy + h * 0.35f)
        p.lineTo(cx - r * 0.8f, cy)
        p.quadTo(cx - r * 0.8f, cy - h * 0.35f, cx + r * 0.2f, cy - h * 0.35f)
        p.lineTo(cx + r, cy - h * 0.35f)
        p.lineTo(cx + r * 0.4f, cy - h * 0.55f)
        p.lineTo(cx - r * 0.8f, cy - h * 0.55f)
        p.quadTo(cx - r * 1.6f, cy - h * 0.55f, cx - r * 1.6f, cy)
        p.lineTo(cx - r * 1.6f, cy + h * 0.35f)
        p.close()
    }

    private fun drawLeftFront(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        p.moveTo(cx - w * 0.1f, cy + h * 0.35f)
        p.lineTo(cx - w * 0.1f, cy - h * 0.05f)
        p.lineTo(cx - w * 0.35f, cy + h * 0.1f)
        p.lineTo(cx - w * 0.25f, cy - h * 0.25f)
        p.lineTo(cx + w * 0.1f, cy - h * 0.35f)
        p.lineTo(cx + w * 0.15f, cy - h * 0.15f)
        p.lineTo(cx + w * 0.05f, cy + h * 0.35f)
        p.close()
    }

    private fun drawRightFront(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        p.moveTo(cx + w * 0.1f, cy + h * 0.35f)
        p.lineTo(cx + w * 0.1f, cy - h * 0.05f)
        p.lineTo(cx + w * 0.35f, cy + h * 0.1f)
        p.lineTo(cx + w * 0.25f, cy - h * 0.25f)
        p.lineTo(cx - w * 0.1f, cy - h * 0.35f)
        p.lineTo(cx - w * 0.15f, cy - h * 0.15f)
        p.lineTo(cx - w * 0.05f, cy + h * 0.35f)
        p.close()
    }

    private fun drawLeftBack(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        p.moveTo(cx - w * 0.1f, cy - h * 0.35f)
        p.lineTo(cx - w * 0.1f, cy + h * 0.05f)
        p.lineTo(cx - w * 0.35f, cy - h * 0.1f)
        p.lineTo(cx - w * 0.25f, cy + h * 0.25f)
        p.lineTo(cx + w * 0.1f, cy + h * 0.35f)
        p.lineTo(cx + w * 0.15f, cy + h * 0.15f)
        p.lineTo(cx + w * 0.05f, cy - h * 0.35f)
        p.close()
    }

    private fun drawRightBack(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        p.moveTo(cx + w * 0.1f, cy - h * 0.35f)
        p.lineTo(cx + w * 0.1f, cy + h * 0.05f)
        p.lineTo(cx + w * 0.35f, cy - h * 0.1f)
        p.lineTo(cx + w * 0.25f, cy + h * 0.25f)
        p.lineTo(cx - w * 0.1f, cy + h * 0.35f)
        p.lineTo(cx - w * 0.15f, cy + h * 0.15f)
        p.lineTo(cx - w * 0.05f, cy - h * 0.35f)
        p.close()
    }

    private fun drawUturn(p: Path, cx: Float, cy: Float, w: Float, h: Float) {
        val r = w * 0.25f
        p.moveTo(cx + r, cy + h * 0.35f)
        p.lineTo(cx + r, cy)
        p.arcTo(cx - r, cy - h * 0.35f, cx + r * 3, cy + h * 0.15f, 90f, 180f, false)
        p.lineTo(cx - r, cy - h * 0.2f)
        p.lineTo(cx - r * 1.6f, cy - h * 0.45f)
        p.lineTo(cx - r * 0.4f, cy - h * 0.45f)
        p.lineTo(cx - r, cy - h * 0.2f)
        p.lineTo(cx + r * 0.2f, cy - h * 0.2f)
        p.lineTo(cx + r * 0.2f, cy + h * 0.35f)
        p.close()
    }
}
