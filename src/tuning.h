#ifndef EL_TUNING_H
#define EL_TUNING_H

/* Shared by C and GLSL. Use preprocessor expressions valid in both. */

/* Native matrix and front panel geometry in design pixels. */
#define PANEL_W 320
#define PANEL_H 240
#define CASE_W 1120
#define CASE_H 872
#define GLASS_W 896
#define GLASS_H 672
#define GLASS_X ((CASE_W - GLASS_W) / 2)
#define GLASS_Y 86
#define INK_X 100
#define INK_W (CASE_W - 2 * INK_X)
#define INK_SCALE 2
#define CASE_MARGIN_X 34
#define CASE_MARGIN_Y 21
#define BEZEL_LIP_X 26.0
#define BEZEL_LIP_Y 21.0

/* Faceplate controls. The last button is aligned with the right bezel. */
#define BUTTON_COUNT 7
#define BUTTON_W 74
#define BUTTON_H 34
#define BUTTON_GAP 8
#define BUTTON_STEP (BUTTON_W + BUTTON_GAP)
#define BUTTON_RIGHT_MARGIN 118
#define BUTTON_X (CASE_W - BUTTON_RIGHT_MARGIN - BUTTON_W - (BUTTON_COUNT - 1) * BUTTON_STEP)
#define BUTTON_Y (CASE_H - 76)
#define BUTTON_KEY_X 8
#define BUTTON_LABEL_X 25
#define BUTTON_TEXT_Y 12
#define PILOT_X (GLASS_X + 6)
#define PILOT_Y (BUTTON_Y + 13)

/* Rates in Hz and time constants in seconds. */
#define PANEL_HZ 72
#define VIDEO_HZ 30
#define MAX_RENDER_HZ 120
#define MAX_FRAME_STEP 0.10
#define SLOW_RATE 0.15
#define WAKE_MODE 2
#define REWRITE_MODE 1
#define WAKE_DELAY 0.068
#define WAKE_SWEEP 0.115
#define CLEAR_SWEEP 0.092
#define WRITE_DELAY 0.043
#define WRITE_SWEEP 0.122
#define TRANSITION_SETTLE 0.22
#define POWER_RISE 0.036
#define POWER_FALL 0.012
#define LOAD_RESPONSE 0.043
#define PIXEL_RISE 0.00075
#define PIXEL_FALL 0.0048
#define PIXEL_TAIL_GAIN 0.32
#define MEMORY_CHARGE 0.58
#define MEMORY_RELEASE 3.3
#define MAX_LEVEL 255
#define HISTORY_DECAY_BIAS_CODES 0.51

/* Brightness, dot aperture and cover glass. */
#define DEFAULT_BRIGHTNESS 1.00
#define MIN_BRIGHTNESS 0.20
#define MAX_BRIGHTNESS 1.25
#define BRIGHTNESS_STEP 0.10
#define DOT_HALF_WIDTH 0.428
#define DOT_HALF_HEIGHT 0.440
#define DOT_VARIATION 0.104
#define COLUMN_VARIATION 0.041
#define ROW_VARIATION 0.026
#define GLASS_SCATTER_PITCH 1.35
#define DOT_APERTURE_GAIN 0.94
#define DOT_INTERPOLATED_GAIN 0.048
#define GLASS_SCATTER_GAIN 0.012
#define SOFT_INTERPOLATED_GAIN 0.76
#define SOFT_SCATTER_GAIN 0.039
#define LOAD_SAG 0.20
#define PULSE_RIPPLE 0.024
#define POLARITY_RIPPLE 0.009
#define BASE_EXPOSURE 1.63
#define BLOOM_EXPOSURE 2.35
#define BASE_AMBER_RGB 1.0, 0.270, 0.031
#define BLOOM_AMBER_RGB 1.0, 0.335, 0.036
#define DISPLAY_GAMMA 2.2

/* Near and wide optical scatter for the two viewing modes. */
#define BLOOM_DOWNSAMPLE 2
/* Kernel radius and standard deviations in downsampled texels. */
#define BLOOM_RADIUS 6
#define BLOOM_NEAR_SIGMA 0.90
#define BLOOM_WIDE_SIGMA 2.20
#define DARK_FIELD_SOFTEN 0.79
#define ROLLING_FIELD_SOFTEN 0.17
#define DARK_FIELD_BLOOM_NEAR 0.255
#define DARK_FIELD_BLOOM_WIDE 0.112
#define ROLLING_FIELD_BLOOM_NEAR 0.180
#define ROLLING_FIELD_BLOOM_WIDE 0.075

/* Apparent sweep for the filmed look, in fractions of the picture height. */
#define BAND_PERIOD 14.4
#define BAND_START 1.22
#define BAND_TRAVEL 1.44
#define BAND_PHASE 0.30
#define BAND_WIDTH 0.102
#define BAND_WIDTH_SWING 0.056
#define BAND_WIDTH_RATE 0.46
#define BAND_DEPTH 0.51
#define BAND_EDGE_IN 0.68
#define BAND_EDGE_OUT 1.24
#define RESIDUAL_SOFT 0.10
#define RESIDUAL_DARK 0.18

/* Steady uneven amber background in the filmed look. */
#define IDLE_LIGHT 0.111
#define IDLE_SIDE_GLOW 0.177
#define IDLE_UPPER_SHADE 0.050
#define IDLE_CORNER_GLOW 0.051
#define IDLE_COARSE_VARIATION 0.043
#define IDLE_FINE_VARIATION 0.013

#endif
