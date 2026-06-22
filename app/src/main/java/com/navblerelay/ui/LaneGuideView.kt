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

/**
 * 路口车道指引图标视图
 *
 * 使用与高德 iOS Watch 导航图标一致的粗圆角箭头风格，
 * 绘制车道指引中每个车道的 backIcon（0-7）：
 * 0-直行; 1-左转; 2-直行和左转; 3-右转; 4-直行和右转;
 * 5-左转掉头; 6-左转和右转; 7-直行和左转和右转。
 */
class LaneGuideView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val laneColor = ContextCompat.getColor(context, R.color.lane_arrow)
    private val laneBgColor = ContextCompat.getColor(context, R.color.lane_bg)
    private val dividerColor = ContextCompat.getColor(context, R.color.lane_divider)

    private val lanes = mutableListOf<Int>()

    fun setLanes(newLanes: List<Int>) {
        lanes.clear()
        lanes.addAll(newLanes)
        requestLayout()
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desiredHeight = (resources.displayMetrics.density * 40).toInt()
        val cellWidth = (resources.displayMetrics.density * 40).toInt()
        val width = lanes.size * cellWidth + paddingStart + paddingEnd
        val height = desiredHeight + paddingTop + paddingBottom
        setMeasuredDimension(
            resolveSize(width, widthMeasureSpec),
            resolveSize(height, heightMeasureSpec)
        )
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (lanes.isEmpty()) return

        val contentWidth = width - paddingStart - paddingEnd
        val contentHeight = height - paddingTop - paddingBottom
        val cellWidth = contentWidth / lanes.size.toFloat()

        val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = laneBgColor }
        val dividerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = dividerColor
            strokeWidth = resources.displayMetrics.density * 1.5f
        }

        // 整体背景
        val bounds = RectF(
            paddingStart.toFloat(), paddingTop.toFloat(),
            width - paddingEnd.toFloat(), height - paddingBottom.toFloat()
        )
        canvas.drawRoundRect(bounds, 8f, 8f, bgPaint)

        // 绘制每个车道图标
        lanes.forEachIndexed { index, backIcon ->
            val left = paddingStart + index * cellWidth
            val cellBounds = RectF(left, bounds.top, left + cellWidth, bounds.bottom)
            LanePainter.draw(
                canvas, backIcon,
                RectF(
                    cellBounds.left + cellWidth * 0.12f,
                    cellBounds.top + cellHeight * 0.12f,
                    cellBounds.right - cellWidth * 0.12f,
                    cellBounds.bottom - cellHeight * 0.12f
                ),
                laneColor
            )

            // 车道分隔线
            if (index > 0) {
                val x = cellBounds.left
                canvas.drawLine(x, cellBounds.top + 6f, x, cellBounds.bottom - 6f, dividerPaint)
            }
        }
    }

    private val cellHeight: Float
        get() = (height - paddingTop - paddingBottom).toFloat()
}

/**
 * 车道图标绘制器
 *
 * 采用粗描边 + 圆角线端/连接，与高德 iOS HUD 转向图标风格一致。
 * 组合车道（2/4/6/7）通过绘制多个细箭头叠加表现。
 */
private object LanePainter {

    private const val STROKE_RATIO = 0.22f
    private const val HEAD_LENGTH_RATIO = 0.28f
    private const val HEAD_HALF_WIDTH_RATIO = 0.22f

    fun draw(canvas: Canvas, backIcon: Int, rect: RectF, color: Int) {
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

        when (backIcon) {
            0 -> drawStraight(canvas, rect, strokePaint, fillPaint)
            1 -> drawLeft(canvas, rect, strokePaint, fillPaint)
            2 -> {
                drawStraight(canvas, rect, thinner(strokePaint), fillPaint)
                drawLeft(canvas, shiftRect(rect, -0.15f, 0f), thinner(strokePaint), fillPaint)
            }
            3 -> drawRight(canvas, rect, strokePaint, fillPaint)
            4 -> {
                drawStraight(canvas, rect, thinner(strokePaint), fillPaint)
                drawRight(canvas, shiftRect(rect, 0.15f, 0f), thinner(strokePaint), fillPaint)
            }
            5 -> drawUturn(canvas, rect, strokePaint, fillPaint)
            6 -> {
                drawLeft(canvas, shiftRect(rect, -0.18f, 0f), thinner(strokePaint), fillPaint)
                drawRight(canvas, shiftRect(rect, 0.18f, 0f), thinner(strokePaint), fillPaint)
            }
            7 -> {
                drawStraight(canvas, rect, thinner(strokePaint), fillPaint)
                drawLeft(canvas, shiftRect(rect, -0.15f, 0f), thinner(strokePaint), fillPaint)
                drawRight(canvas, shiftRect(rect, 0.15f, 0f), thinner(strokePaint), fillPaint)
            }
        }
    }

    private fun thinner(paint: Paint): Paint = Paint(paint).apply { strokeWidth *= 0.65f }

    private fun shiftRect(rect: RectF, dxRatio: Float, dyRatio: Float): RectF {
        val dx = rect.width() * dxRatio
        val dy = rect.height() * dyRatio
        return RectF(rect.left + dx, rect.top + dy, rect.right + dx, rect.bottom + dy)
    }

    private fun drawStraight(canvas: Canvas, rect: RectF, stroke: Paint, fill: Paint) {
        val cx = rect.centerX()
        val halfW = rect.width() * HEAD_HALF_WIDTH_RATIO
        val headLen = rect.height() * HEAD_LENGTH_RATIO
        val bodyBottom = rect.bottom - rect.height() * 0.08f
        val bodyTop = rect.top + rect.height() * 0.25f
        canvas.drawLine(cx, bodyBottom, cx, bodyTop + headLen, stroke)
        canvas.drawPath(triangle(cx, bodyTop, halfW, headLen), fill)
    }

    private fun drawLeft(canvas: Canvas, rect: RectF, stroke: Paint, fill: Paint) {
        val halfW = rect.width() * HEAD_HALF_WIDTH_RATIO
        val headLen = rect.height() * HEAD_LENGTH_RATIO
        val startX = rect.centerX() + rect.width() * 0.05f
        val startY = rect.bottom - rect.height() * 0.08f
        val endX = rect.left + rect.width() * 0.18f
        val endY = rect.centerY()
        canvas.drawLine(startX, startY, startX, endY, stroke)
        canvas.drawLine(startX, endY, endX + headLen, endY, stroke)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.LEFT), fill)
    }

    private fun drawRight(canvas: Canvas, rect: RectF, stroke: Paint, fill: Paint) {
        val halfW = rect.width() * HEAD_HALF_WIDTH_RATIO
        val headLen = rect.height() * HEAD_LENGTH_RATIO
        val startX = rect.centerX() - rect.width() * 0.05f
        val startY = rect.bottom - rect.height() * 0.08f
        val endX = rect.right - rect.width() * 0.18f
        val endY = rect.centerY()
        canvas.drawLine(startX, startY, startX, endY, stroke)
        canvas.drawLine(startX, endY, endX - headLen, endY, stroke)
        canvas.drawPath(triangle(endX, endY, halfW, headLen, Point.RIGHT), fill)
    }

    private fun drawUturn(canvas: Canvas, rect: RectF, stroke: Paint, fill: Paint) {
        val halfW = rect.width() * HEAD_HALF_WIDTH_RATIO
        val headLen = rect.height() * HEAD_LENGTH_RATIO
        val cx = rect.centerX()
        val arcRect = RectF(
            rect.left + rect.width() * 0.18f,
            rect.top + rect.height() * 0.18f,
            rect.right - rect.width() * 0.18f,
            rect.bottom - rect.height() * 0.05f
        )
        val path = Path().apply {
            moveTo(cx, arcRect.bottom)
            lineTo(cx, arcRect.centerY())
            arcTo(arcRect, 90f, 180f)
            lineTo(arcRect.left - headLen, arcRect.top)
        }
        canvas.drawPath(path, stroke)
        canvas.drawPath(triangle(arcRect.left, arcRect.top, halfW, headLen, Point.LEFT), fill)
    }

    private enum class Point { CENTER, LEFT, RIGHT }

    private fun triangle(x: Float, y: Float, halfW: Float, len: Float, point: Point = Point.CENTER): Path {
        return Path().apply {
            when (point) {
                Point.CENTER -> {
                    moveTo(x, y - len)
                    lineTo(x - halfW, y + len)
                    lineTo(x + halfW, y + len)
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
            }
            close()
        }
    }
}
