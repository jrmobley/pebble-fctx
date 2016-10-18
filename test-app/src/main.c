
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/fpath.h>
#include <pebble-fctx/ffont.h>

// --------------------------------------------------------------------------
// Types and global variables.
// --------------------------------------------------------------------------

#define RESMEM 1

Window* g_window;
Layer* g_layer;
#if RESMEM
void* g_resource_memory;
#endif
FFont* g_font;
FPath* g_body;
FPath* g_hour;
FPath* g_minute;
struct tm g_local_time;

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
    int16_t outer_radius = bounds.size.w / 2 - BEZEL_INSET;
    int16_t pip_size = 6;
    int32_t minute_angle = g_local_time.tm_min * TRIG_MAX_ANGLE / 60;
    int32_t hour_angle = (g_local_time.tm_hour % 12) * TRIG_MAX_ANGLE / 12
                       +  g_local_time.tm_min        * TRIG_MAX_ANGLE / (12 * 60);
    char date_string[3];
    strftime(date_string, sizeof date_string, "%d", &g_local_time);

    FContext fctx;
    fctx_init_context(&fctx, ctx);
    fctx_set_color_bias(&fctx, 0);
    fctx_set_fill_color(&fctx, GColorBlack);

    /* Draw the pips. */
    fixed_t bar_length = INT_TO_FIXED(pip_size);
    fixed_t dot_radius = INT_TO_FIXED(pip_size - 4) / 2;
    fixed_t pips_radius = INT_TO_FIXED(outer_radius) - INT_TO_FIXED(pip_size) / 2;
    fctx_begin_fill(&fctx);
    fctx_set_pivot(&fctx, FPoint(0, pips_radius));
    fctx_set_offset(&fctx, center);
    for (int m = 0; m < 60; ++m) {
        int32_t angle = m * TRIG_MAX_ANGLE / 60;
        if (0 == m % 5) {
            fixed_t pipw = (m % 15 == 0) ? INT_TO_FIXED(2) : INT_TO_FIXED(1);
            fctx_set_rotation(&fctx, angle);
            fctx_move_to(&fctx, FPoint(-pipw, -bar_length / 2));
            fctx_line_to(&fctx, FPoint( pipw, -bar_length / 2));
            fctx_line_to(&fctx, FPoint( pipw,  bar_length / 2));
            fctx_line_to(&fctx, FPoint(-pipw,  bar_length / 2));
            fctx_close_path(&fctx);
        } else {
            FPoint p = clockToCartesian(center, pips_radius, angle);
            fctx_plot_circle(&fctx, &p, dot_radius);
        }
    }
    fctx_end_fill(&fctx);

    /* Set up for drawing the hands. */
    int16_t from_size = 90;
    int16_t to_size = outer_radius - pip_size;
    fctx_set_scale(&fctx, FPoint(from_size, from_size), FPoint(to_size, to_size));
    fctx_set_pivot(&fctx, FPointI(90, 90));
    fctx_set_offset(&fctx, center);

    /* Draw the hour hand. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, GColorDarkGray);
    fctx_set_rotation(&fctx, hour_angle);
    fctx_draw_commands(&fctx, FPointZero, g_hour->data, g_hour->size);
    fctx_end_fill(&fctx);

    /* Draw the minute hand. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, GColorBlack);
    fctx_set_rotation(&fctx, minute_angle);
    fctx_draw_commands(&fctx, FPointZero, g_minute->data, g_minute->size);
    fctx_end_fill(&fctx);

    /* Draw the body. */
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, GColorBlack);
    fctx_set_rotation(&fctx, 0);
    fctx_draw_commands(&fctx, FPointZero, g_body->data, g_body->size);
    fctx_end_fill(&fctx);

    /* Draw the date. */
    FPoint date_pos;
    date_pos.x = center.x + INT_TO_FIXED( 5) * to_size / from_size;
    date_pos.y = center.y + INT_TO_FIXED(48) * to_size / from_size;
    fctx_begin_fill(&fctx);
    fctx_set_text_em_height(&fctx, g_font, 30 * to_size / from_size);
    fctx_set_fill_color(&fctx, GColorWhite);
    fctx_set_pivot(&fctx, FPointZero);
    fctx_set_offset(&fctx, date_pos);
    fctx_set_rotation(&fctx, -5 * TRIG_MAX_ANGLE / (2*360));
    fctx_draw_string(&fctx, date_string, g_font, GTextAlignmentCenter, FTextAnchorBaseline);
    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx);
}

// --------------------------------------------------------------------------
// System event handlers.
// --------------------------------------------------------------------------

void on_tick_timer(struct tm* tick_time, TimeUnits units_changed) {
    g_local_time = *tick_time;
    layer_mark_dirty(g_layer);
}

// --------------------------------------------------------------------------
// Initialization and teardown.
// --------------------------------------------------------------------------

static void init() {

#if RESMEM
    size_t font_size = resource_size(resource_get_handle(RESOURCE_ID_NARROW_FFONT));
    size_t body_size = sizeof(FPath) + resource_size(resource_get_handle(RESOURCE_ID_BODY_FPATH));
    size_t hour_size = sizeof(FPath) + resource_size(resource_get_handle(RESOURCE_ID_HOUR_FPATH));
    size_t minute_size = sizeof(FPath) + resource_size(resource_get_handle(RESOURCE_ID_MINUTE_FPATH));
    size_t resource_size = font_size + body_size + hour_size + minute_size;
    void* g_resource_memory = malloc(resource_size);
    void* resptr = g_resource_memory;

    g_font = ffont_load_from_resource_into_buffer(RESOURCE_ID_NARROW_FFONT, resptr);
    resptr += font_size;

    g_body = fpath_load_from_resource_into_buffer(RESOURCE_ID_BODY_FPATH, resptr);
    resptr += body_size;

    g_hour = fpath_load_from_resource_into_buffer(RESOURCE_ID_HOUR_FPATH, resptr);
    resptr += hour_size;

    g_minute = fpath_load_from_resource_into_buffer(RESOURCE_ID_MINUTE_FPATH, resptr);
    resptr += minute_size;
#else
    g_font = ffont_create_from_resource(RESOURCE_ID_NARROW_FFONT);
    g_body = fpath_create_from_resource(RESOURCE_ID_BODY_FPATH);
    g_hour = fpath_create_from_resource(RESOURCE_ID_HOUR_FPATH);
    g_minute = fpath_create_from_resource(RESOURCE_ID_MINUTE_FPATH);
#endif

    g_window = window_create();
    window_set_background_color(g_window, GColorWhite);
    window_stack_push(g_window, true);
    Layer* window_layer = window_get_root_layer(g_window);
    GRect window_frame = layer_get_frame(window_layer);

    g_layer = layer_create(window_frame);
    layer_set_update_proc(g_layer, &on_layer_update);
    layer_add_child(window_layer, g_layer);

    time_t now = time(NULL);
    g_local_time = *localtime(&now);
    tick_timer_service_subscribe(MINUTE_UNIT, &on_tick_timer);
}

static void deinit() {
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    window_destroy(g_window);
    layer_destroy(g_layer);
#if RESMEM
    free(g_resource_memory);
#else
    fpath_destroy(g_minute);
    fpath_destroy(g_hour);
    fpath_destroy(g_body);
    ffont_destroy(g_font);
#endif
}

// --------------------------------------------------------------------------
// The main event loop.
// --------------------------------------------------------------------------

int main() {
    init();
    app_event_loop();
    deinit();
}
