#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_Screen1;

extern lv_obj_t *ui_RoadNameLabel;
extern lv_obj_t *ui_LaneContainer;
extern lv_obj_t *ui_TurnArrow;
extern lv_obj_t *ui_DistanceLabel;
extern lv_obj_t *ui_SpeedLabel;
extern lv_obj_t *ui_SpeedUnitLabel;
extern lv_obj_t *ui_LimitSign;
extern lv_obj_t *ui_LimitLabel;
extern lv_obj_t *ui_CameraIcon;

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif
