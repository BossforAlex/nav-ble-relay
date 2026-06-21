package com.navblerelay.ui

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatImageView
import com.navblerelay.R

/**
 * 路口转向大图标
 *
 * 使用统一的矢量图标资源（ic_nav_*.xml）替代易断裂/可读性差的旧图标：
 * - 8dp 粗描边 + 圆角线端/连接，保证箭头整体连贯；
 * - 实心三角箭头头部，方向清晰；
 * - 使用 @color/md_on_surface，自动适配 day/night；
 * - 如需替换为 iconfont.cn 下载的图标，只需覆盖对应的 ic_nav_*.xml 文件并保持命名即可。
 */
class NavArrowView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    init {
        setIconType(0)
        scaleType = ScaleType.FIT_CENTER
    }

    fun setIconType(amapIconType: Int) {
        setImageResource(mapNavDrawable(amapIconType))
    }

    /**
     * AMapAutoProtocol 的 icon 编码（0-20）映射到矢量图标资源
     */
    private fun mapNavDrawable(id: Int): Int = when (id) {
        0, 1, 9, 16, 20 -> R.drawable.ic_nav_straight
        2 -> R.drawable.ic_nav_left
        3 -> R.drawable.ic_nav_right
        4 -> R.drawable.ic_nav_left_front
        5 -> R.drawable.ic_nav_right_front
        6 -> R.drawable.ic_nav_left_back
        7 -> R.drawable.ic_nav_right_back
        8, 19 -> R.drawable.ic_nav_uturn
        11, 17 -> R.drawable.ic_nav_roundabout_enter
        12, 18 -> R.drawable.ic_nav_roundabout_exit
        10, 13, 14, 15 -> R.drawable.ic_nav_arrive
        else -> R.drawable.ic_nav_straight
    }
}
