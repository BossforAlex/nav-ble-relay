# UI Optimization Audit

Date: 2026-07-16

## Scope

- Flutter app UI: `app_flutter/lib/main.dart`, `app_flutter/lib/ui/**`.
- ESP32 display UI: `Auto/src/ui/ui.c`, `Auto/src/ui/ui.h`, `Auto/src/screen/ScreenLVGL.cpp`.
- Build and CI references: `Auto/platformio.ini`, `app_flutter/pubspec.yaml`, `.github/workflows/build.yml`.

## Source Evidence

- `Auto/platformio.ini`: primary firmware target is `esp32-s3-supermini`, using ESP32-S3, ILI9341, 320x240 landscape, LVGL 8.4, TFT_eSPI, RGB565, SPI 27MHz, PSRAM enabled.
- `Auto/src/config/Config.h`: project version is `0.9.9`; physical screen is enabled; refresh target is 50ms.
- `Auto/src/screen/ScreenLVGL.cpp`: active display path maps `NavState` to LVGL controls and uses `ui_init()`.
- `app_flutter/pubspec.yaml`: Flutter app version is `1.0.0+11`; Material You dependencies are present: `dynamic_color`, `google_fonts`.
- `.github/workflows/build.yml`: CI builds ESP32-S3, ESP32-C3, merged firmware images, and Flutter release APK.

## External Design Evidence

- Material 3 official navigation bar page: `https://m3.material.io/components/navigation-bar/overview` confirms NavigationBar is the Material 3 small-device pattern.
- Flutter official Material Design page: `https://docs.flutter.dev/ui/design/material` confirms Material 3 is the current Flutter Material baseline and supports dynamic color, adaptive experiences, accessibility, design tokens, typography, color, and elevation.
- Material 3 official search results for color roles and surfaces were cross-checked through `m3.material.io`; direct page fetches timed out, so implementation keeps Flutter `ColorScheme` semantic roles as the executable source of truth.
- UI/UX skill checklist used for implementation review: 48dp touch target, semantic color tokens, readable text, stable interaction states, compact operational UI, high contrast for critical status.

## Design Decisions

- Flutter app remains a Material 3 operational tool. The UI favors dense status visibility, predictable bottom navigation, semantic color roles, and stable controls.
- Flutter status colors are centralized through `ColorScheme` usage where possible; raw color use is reduced to traffic-domain colors and warning states.
- Flutter cards and buttons use restrained radius and lower elevation to reduce decorative weight and improve scan speed.
- ESP32 screen prioritizes immediate driving tasks: next maneuver, remaining segment distance, current speed, lane hint, speed limit, connection state, route summary.
- ESP32 layout stays within 320x240, uses 4px/8px spacing rhythm, avoids small decorative elements, and uses high-contrast RGB565-safe colors.
- ESP32 text uses existing LVGL Montserrat fonts and custom arrow fonts. No new font dependency is introduced.

## Reproducibility

Run firmware builds:

```bash
cd /workspace/Auto
pio run -e esp32-s3-supermini
pio run -e esp32-c3-supermini
```

Run Flutter checks/build:

```bash
cd /workspace/app_flutter
flutter pub get
flutter analyze
flutter build apk --release
```

Check GitHub Actions:

```bash
gh run list --workflow build.yml --limit 5
gh run watch <run-id>
```

## Verification Result

- `pio run -e esp32-s3-supermini`: passed after version bump to `0.9.10`. RAM 29.0%, Flash 42.1%.
- `pio run -e esp32-c3-supermini`: passed after version bump. RAM 11.4%, Flash 41.3%.
- `/tmp/opencode/flutter/bin/flutter analyze`: passed with no issues.
- `/tmp/opencode/flutter/bin/flutter build apk --release`: blocked locally because `ANDROID_HOME` is not configured and no Android SDK is installed in this workspace. The GitHub workflow installs Android SDK via `android-actions/setup-android@v3`.
- Public GitHub Actions API shows latest available `main` workflow run #279 completed successfully for commit `8116c3a`. This workspace has unpushed changes, so a new run for this revision does not exist yet.

## Versioning Rule

- Firmware `PROJECT_VERSION` and Flutter `pubspec.yaml` version were incremented after local firmware build and Flutter analysis succeeded.
- GitHub Actions monitoring requires a pushed commit or an existing workflow run. Without explicit push approval, only existing runs are inspected.
