package com.navblerelay.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.navblerelay.R

/**
 * 路口转向大图标
 *
 * 替代原有易断裂的矢量图标，使用统一的 ArrowPainter 绘制：
 * - 粗描边 + 圆角连接，保证箭头整体连贯
 * - 实心三角箭头，可读性高
 * - 自动适配主题色（day/night）
 */
class NavArrowView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var iconType: Int = ArrowPainter.ICON_STRAIGHT
    private val arrowColor = ContextCompat.getColor(context, R.color.md_on_surface)

    fun setIconType(amapIconType: Int) {
        iconType = mapNavIcon(amapIconType)
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val padding = width * 0.10f
        val rect = RectF(padding, padding, width - padding, height - padding)
        ArrowPainter.draw(canvas, iconType, rect, arrowColor)
    }

    /**
     * AMapAutoProtocol 的 icon 编码（0-20）映射到 ArrowPainter 图标
     */
    private fun mapNavIcon(id: Int): Int = when (id) {
        0, 1, 9, 16, 20 -> ArrowPainter.ICON_STRAIGHT
        2 -> ArrowPainter.ICON_LEFT
        3 -> ArrowPainter.ICON_RIGHT
        4 -> ArrowPainter.ICON_LEFT_FRONT
        5 -> ArrowPainter.ICON_RIGHT_FRONT
        6 -> ArrowPainter.ICON_LEFT_BACK
        7 -> ArrowPainter.ICON_RIGHT_BACK
        8, 19 -> ArrowPainter.ICON_UTURN
        11, 17 -> ArrowPainter.ICON_ROUNDABOUT_ENTER
        12, 18 -> ArrowPainter.ICON_ROUNDABOUT_EXIT
        10, 13, 14, 15 -> ArrowPainter.ICON_ARRIVE
        else -> ArrowPainter.ICON_STRAIGHT
    }
}
