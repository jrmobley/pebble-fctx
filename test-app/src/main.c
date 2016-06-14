
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>

// --------------------------------------------------------------------------
// Types and global variables.
// --------------------------------------------------------------------------

enum Palette {
    BEZEL_COLOR,
    FACE_COLOR,
    RING_COLOR,
    MINUTE_TEXT_COLOR,
    MINUTE_HAND_COLOR,
    HOUR_TEXT_COLOR,
    PALETTE_SIZE
};

Window* g_window;
Layer* g_layer;
FFont* g_font;
struct tm g_local_time;
GColor g_palette[PALETTE_SIZE];

static const char* kHourString[12] = {
    "TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE",
    "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN",
};

#if defined(PBL_ROUND)
#define BEZEL_INSET 6
#else
#define BEZEL_INSET 2
#endif

// --------------------------------------------------------------------------
// Utility functions.
// --------------------------------------------------------------------------

static inline FPoint clockToCartesian(FPoint center, fixed_t radius, int32_t angle) {
    FPoint pt;
    int32_t c = cos_lookup(angle);
    int32_t s = sin_lookup(angle);
    pt.x = center.x + s * radius / TRIG_MAX_RATIO;
    pt.y = center.y - c * radius / TRIG_MAX_RATIO;
    return pt;
}

// --------------------------------------------------------------------------
// The main drawing function.
// --------------------------------------------------------------------------

void on_layer_update(Layer* layer, GContext* ctx) {

    GRect bounds = layer_get_bounds(layer);
    FPoint center = FPointI(bounds.size.w / 2, bounds.size.h / 2);
    fixed_t safe_radius = INT_TO_FIXED(bounds.size.w / 2 - BEZEL_INSET);

    int minute_text_size = 12;
    int hour_text_size = 14;
    fixed_t minute_text_radius = safe_radius;
    fixed_t ring_outer_radius = minute_text_radius - INT_TO_FIXED(minute_text_size + 0);
    fixed_t ring_inner_radius = ring_outer_radius - INT_TO_FIXED(1);
    fixed_t minute_hand_radius = ring_inner_radius - INT_TO_FIXED(1);

    const char* hour_string = kHourString[g_local_time.tm_hour % 12];
    char minute_string[3];
    int32_t minute_angle = g_local_time.tm_min * TRIG_MAX_ANGLE / 60;

    FContext fctx;
    fctx_init_context(&fctx, ctx);
    fctx_set_color_bias(&fctx, 0);

    /* Draw the minute marks. */

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g_palette[MINUTE_TEXT_COLOR]);
    fctx_set_text_size(&fctx, g_font, minute_text_size);
    for (int m = 0; m < 60; m += 5) {
        snprintf(minute_string, sizeof minute_string, "%02d", m);
        int32_t minute_angle = m * TRIG_MAX_ANGLE / 60;
        int32_t text_rotation;
        FTextAnchor text_anchor;
        if (m > 15 && m < 45) {
            text_rotation = minute_angle + TRIG_MAX_ANGLE / 2;
            text_anchor = FTextAnchorBaseline;
        } else {
            text_rotation = minute_angle;
            text_anchor = FTextAnchorTop;
        }
        FPoint p = clockToCartesian(center, minute_text_radius, minute_angle);
        fctx_set_rotation(&fctx, text_rotation);
        fctx_set_offset(&fctx, p);
        fctx_draw_string(&fctx, minute_string, g_font, GTextAlignmentCenter, text_anchor);
    }
    fctx_end_fill(&fctx);

    /* Draw a thin ring. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g_palette[RING_COLOR]);
    fctx_plot_circle(&fctx, &center, ring_outer_radius);
    fctx_plot_circle(&fctx, &center, ring_inner_radius);
    fctx_end_fill(&fctx);

    /* Draw the minute hand. */

    fixed_t hand_size = INT_TO_FIXED(7);
    fixed_t ctrl = hand_size * 3 / 4;

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g_palette[MINUTE_HAND_COLOR]);
    fctx_set_offset(&fctx, center);
    fctx_set_scale(&fctx, FPointOne, FPointOne);
    fctx_set_rotation(&fctx, minute_angle);
    fctx_move_to (&fctx, FPoint(0, - minute_hand_radius));
    fctx_curve_to(&fctx, FPoint(0, - minute_hand_radius),
                         FPoint(hand_size, 1 * hand_size - minute_hand_radius),
                         FPoint(hand_size, 3 * hand_size - minute_hand_radius));
    fctx_line_to (&fctx, FPoint(hand_size, 0));
    fctx_curve_to(&fctx, FPoint(hand_size, ctrl),
                         FPoint(ctrl, hand_size),
                         FPoint(0, hand_size));
    fctx_curve_to(&fctx, FPoint(-ctrl, hand_size),
                         FPoint(-hand_size, ctrl),
                         FPoint(-hand_size, 0));
    fctx_line_to (&fctx, FPoint(-hand_size, 3 * hand_size - minute_hand_radius));
    fctx_curve_to(&fctx, FPoint(-hand_size, 1 * hand_size - minute_hand_radius),
                         FPoint(0, - minute_hand_radius),
                         FPoint(0, - minute_hand_radius));
    fctx_end_fill(&fctx);

    /* Draw the hour string onto the minute hand. */

    fixed_t text_margin = INT_TO_FIXED(2);
    fixed_t anchor_radius = text_margin;
    FPoint anchor_point = clockToCartesian(center, anchor_radius, minute_angle);
    int32_t text_rotation;
    GTextAlignment text_align;
    if (g_local_time.tm_min < 30) {
        text_rotation = minute_angle - TRIG_MAX_ANGLE / 4;
        text_align = GTextAlignmentLeft;
    } else {
        text_rotation = minute_angle + TRIG_MAX_ANGLE / 4;
        text_align = GTextAlignmentRight;
    }

    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, g_palette[HOUR_TEXT_COLOR]);
    fctx_set_offset(&fctx, anchor_point);
    fctx_set_rotation(&fctx, text_rotation);
    fctx_set_text_size(&fctx, g_font, hour_text_size);
    fctx_draw_string(&fctx, hour_string, g_font, text_align, FTextAnchorMiddle);
    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx);
}

// --------------------------------------------------------------------------
// System event handlers.
// --------------------------------------------------------------------------


void on_battery_state(BatteryChargeState charge) {

    if (charge.is_charging) {
        g_palette[RING_COLOR] = PBL_IF_COLOR_ELSE(GColorElectricBlue, GColorWhite);
    } else if (charge.charge_percent <= 20) {
        g_palette[RING_COLOR] = PBL_IF_COLOR_ELSE(GColorOrange, GColorDarkGray);
    } else if (charge.charge_percent <= 50) {
        g_palette[RING_COLOR] = PBL_IF_COLOR_ELSE(GColorYellow, GColorLightGray);
    } else {
        g_palette[RING_COLOR] = PBL_IF_COLOR_ELSE(GColorScreaminGreen, GColorWhite);
    }

    layer_mark_dirty(g_layer);
}

void on_tick_timer(struct tm* tick_time, TimeUnits units_changed) {
    g_local_time = *tick_time;
    layer_mark_dirty(g_layer);
}

// --------------------------------------------------------------------------
// Initialization and teardown.
// --------------------------------------------------------------------------

static void init() {

    setlocale(LC_ALL, "");

    g_palette[      BEZEL_COLOR] = GColorWhite;
    g_palette[       FACE_COLOR] = GColorBlack;
    g_palette[MINUTE_TEXT_COLOR] = GColorWhite;
    g_palette[MINUTE_HAND_COLOR] = GColorWhite;
    g_palette[  HOUR_TEXT_COLOR] = PBL_IF_COLOR_ELSE(GColorBlack, GColorBlack);

    g_font = ffont_create_from_resource(RESOURCE_ID_DIN_CONDENSED_FFONT);
    ffont_debug_log(g_font, APP_LOG_LEVEL_DEBUG);

    g_window = window_create();
    window_set_background_color(g_window, g_palette[FACE_COLOR]);
    window_stack_push(g_window, true);
    Layer* window_layer = window_get_root_layer(g_window);
    GRect window_frame = layer_get_frame(window_layer);

    g_layer = layer_create(window_frame);
    layer_set_update_proc(g_layer, &on_layer_update);
    layer_add_child(window_layer, g_layer);

    time_t now = time(NULL);
    g_local_time = *localtime(&now);
    on_battery_state(battery_state_service_peek());

    tick_timer_service_subscribe(MINUTE_UNIT, &on_tick_timer);

    battery_state_service_subscribe(&on_battery_state);
}

static void deinit() {
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    window_destroy(g_window);
    layer_destroy(g_layer);
	ffont_destroy(g_font);
}

// --------------------------------------------------------------------------
// The main event loop.
// --------------------------------------------------------------------------

int main() {
    init();
    app_event_loop();
    deinit();
}
