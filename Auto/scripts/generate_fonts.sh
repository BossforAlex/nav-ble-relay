#!/bin/bash
# ══════════════════════════════════════════════════════════════
# LVGL 字体生成脚本
# 用法: ./scripts/generate_fonts.sh
#
# v0.9.2 字体策略:
#   - DejaVu Sans (系统自带) → 箭头符号 (←↑→↓↰↱↶↷ etc.)
#   - CJK 字体已移除（v0.9.0），改为手机端预渲染位图传输
#   - 所有箭头字体使用 4bpp 抗锯齿，减少 TFT 锯齿
# ══════════════════════════════════════════════════════════════
set -e

FONT_DIR="src/ui/fonts"
ARROW_FONT="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
ARROW_SYMBOLS="←↑→↓↖↗↘↙↰↱↶↷↺↻◎★"

# 确保 lv_font_conv 已安装
if ! command -v lv_font_conv &>/dev/null; then
    echo "[font] 安装 lv_font_conv ..."
    npm install -g lv_font_conv
fi

echo "[font] 箭头符号: $ARROW_SYMBOLS"
echo "[font] 箭头字体: $ARROW_FONT"

# 公共参数
LVGL_OPTS="--format lvgl --lv-include lvgl.h --no-compress --no-kerning"

# ══════════════════════════════════════════════════════════════
# 生成 48px 箭头字体（超大转向箭头，4bpp 抗锯齿）
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 arrows_48 (48px, 4bpp) ..."
lv_font_conv \
    --font "$ARROW_FONT" \
    --size 48 --bpp 4 $LVGL_OPTS \
    --symbols "$ARROW_SYMBOLS" \
    --lv-font-name "arrows_48" \
    -o "$FONT_DIR/arrows_48.c"

# ══════════════════════════════════════════════════════════════
# 生成 20px 车道箭头字体（4bpp 抗锯齿，减少 TFT 锯齿）
# v0.9.2: 从 2bpp 升级到 4bpp
# ══════════════════════════════════════════════════════════════
echo "[font] 生成 arrows_20 (20px, 4bpp) ..."
lv_font_conv \
    --font "$ARROW_FONT" \
    --size 20 --bpp 4 $LVGL_OPTS \
    --symbols "$ARROW_SYMBOLS" \
    --lv-font-name "arrows_20" \
    -o "$FONT_DIR/arrows_20.c"

echo ""
echo "[font] ✓ 所有箭头字体生成完成！"
ls -la "$FONT_DIR"/*.c 2>/dev/null