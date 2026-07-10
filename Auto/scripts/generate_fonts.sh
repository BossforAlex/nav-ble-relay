#!/bin/bash
# ══════════════════════════════════════════════════════════════
# LVGL 字体生成脚本
# 用法: ./scripts/generate_fonts.sh
#
# 字体策略:
#   - DejaVu Sans (系统自带) → 箭头符号 (←↑→↓↰↱↶↷ etc.)
#   - DroidSansFallback (下载) → CJK 汉字 + ASCII
#   两个字体用 --symbols per font 合并，确保箭头 + 中文同时可用
# ══════════════════════════════════════════════════════════════
set -e

FONT_DIR="src/ui/fonts"
CHARSET="$FONT_DIR/charset.txt"
CJK_FONT="/tmp/DroidSansFallback.ttf"
ARROW_FONT="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
ARROW_SYMBOLS="←↑→↓↖↗↘↙↰↱↶↷↺↻◎★"

# 下载 CJK 字体
if [ ! -f "$CJK_FONT" ]; then
    echo "[font] 下载 DroidSansFallback.ttf ..."
    curl -L -o "$CJK_FONT" \
        "https://github.com/aosp-mirror/platform_frameworks_base/raw/master/data/fonts/DroidSansFallback.ttf"
fi

# 确保 lv_font_conv 已安装
if ! command -v lv_font_conv &>/dev/null; then
    echo "[font] 安装 lv_font_conv ..."
    npm install -g lv_font_conv
fi

# 读取字符集（去掉换行符）
SYMBOLS=$(tr -d '\n\r ' < "$CHARSET")
echo "[font] 字符集: ${#SYMBOLS} 个字符"
echo "[font] CJK 字体: $CJK_FONT"
echo "[font] 箭头字体: $ARROW_FONT"

# 公共参数
LVGL_OPTS="--format lvgl --lv-include lvgl.h --no-compress --no-kerning"

# ══════════════════════════════════════════════════════════════
# 生成 20px CJK 字体（道路名 + 通用文本 + 箭头）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 cjk_20 (20px) ..."
lv_font_conv \
    --font "$ARROW_FONT" --symbols "$ARROW_SYMBOLS" \
    --font "$CJK_FONT"  --symbols "$SYMBOLS" \
    --size 20 --bpp 2 $LVGL_OPTS \
    --lv-font-name "cjk_20" \
    -o "$FONT_DIR/cjk_20.c"

# ══════════════════════════════════════════════════════════════
# 生成 24px CJK 字体（距离标签等）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 cjk_24 (24px) ..."
lv_font_conv \
    --font "$ARROW_FONT" --symbols "$ARROW_SYMBOLS" \
    --font "$CJK_FONT"  --symbols "$SYMBOLS" \
    --size 24 --bpp 2 $LVGL_OPTS \
    --lv-font-name "cjk_24" \
    -o "$FONT_DIR/cjk_24.c"

# ══════════════════════════════════════════════════════════════
# 生成 14px CJK 字体（路线信息小字）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 cjk_14 (14px) ..."
lv_font_conv \
    --font "$ARROW_FONT" --symbols "$ARROW_SYMBOLS" \
    --font "$CJK_FONT"  --symbols "$SYMBOLS" \
    --size 14 --bpp 2 $LVGL_OPTS \
    --lv-font-name "cjk_14" \
    -o "$FONT_DIR/cjk_14.c"

# ══════════════════════════════════════════════════════════════
# 生成 48px 箭头字体（超大转向箭头，仅箭头符号）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 arrows_48 (48px) ..."
lv_font_conv \
    --font "$ARROW_FONT" \
    --size 48 --bpp 4 $LVGL_OPTS \
    --symbols "$ARROW_SYMBOLS" \
    --lv-font-name "arrows_48" \
    -o "$FONT_DIR/arrows_48.c"

# ══════════════════════════════════════════════════════════════
# 生成 20px 车道箭头字体（小箭头，用于车道条）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 arrows_20 (20px) ..."
lv_font_conv \
    --font "$ARROW_FONT" \
    --size 20 --bpp 2 $LVGL_OPTS \
    --symbols "$ARROW_SYMBOLS" \
    --lv-font-name "arrows_20" \
    -o "$FONT_DIR/arrows_20.c"

echo ""
echo "[font] ✓ 所有字体生成完成！"
ls -la "$FONT_DIR"/*.c 2>/dev/null