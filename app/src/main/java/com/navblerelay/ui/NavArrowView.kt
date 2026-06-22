package com.navblerelay.ui

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatImageView
import com.navblerelay.R

/**
 * 路口转向大图标
 *
 * 使用高德官方 iOS Watch 导航演示库（amap-demo/iOS-watch-navi）中的
 * default_navi_hud_*.png 转向指示图片资源，替换可读性较差的旧矢量图标：
 * - 图标圆润、粗壮、方向清晰；
 * - PNG 位图资源放置在 drawable-xxhdpi，与 iOS @3x 资源密度对应；
 * - 资源命名与 AMapAutoProtocol icon 编码一一对应（ic_nav_0 ~ ic_nav_20），
 *   便于直接根据广播中的 icon 字段加载对应图片；
 * - 0 / 1 / 20 等无专用图片的编码复用直行图标。
 */
class NavArrowView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    init {
        scaleType = ScaleType.FIT_CENTER
        setIconType(0)
    }

    fun setIconType(amapIconType: Int) {
        setImageResource(mapNavDrawable(amapIconType))
    }

    /**
     * AMapAutoProtocol 的 icon 编码（0-20）映射到对应的 PNG 图标资源。
     * 资源名称与编码一一对应（ic_nav_0 .. ic_nav_20），超出范围时回退到直行图标。
     */
    private fun mapNavDrawable(id: Int): Int = when (id) {
        0 -> R.drawable.ic_nav_0
        1 -> R.drawable.ic_nav_1
        2 -> R.drawable.ic_nav_2
        3 -> R.drawable.ic_nav_3
        4 -> R.drawable.ic_nav_4
        5 -> R.drawable.ic_nav_5
        6 -> R.drawable.ic_nav_6
        7 -> R.drawable.ic_nav_7
        8 -> R.drawable.ic_nav_8
        9 -> R.drawable.ic_nav_9
        10 -> R.drawable.ic_nav_10
        11 -> R.drawable.ic_nav_11
        12 -> R.drawable.ic_nav_12
        13 -> R.drawable.ic_nav_13
        14 -> R.drawable.ic_nav_14
        15 -> R.drawable.ic_nav_15
        16 -> R.drawable.ic_nav_16
        17 -> R.drawable.ic_nav_17
        18 -> R.drawable.ic_nav_18
        19 -> R.drawable.ic_nav_19
        20 -> R.drawable.ic_nav_20
        else -> R.drawable.ic_nav_9
    }
}
