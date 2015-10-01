
#pragma once
#include "pebble.h"

// -----------------------------------------------------------------------------
// Fixed point graphics context.
// -----------------------------------------------------------------------------

#define CHECK(obj) checkObject(obj, #obj)
bool checkObject(void* obj, const char* objname);

typedef int32_t fixed_t;
struct FFont;
typedef struct FFont FFont;

// Defines the fixed point conversions
#define FIXED_POINT_SHIFT 4
#define FIXED_POINT_SCALE 16
#define INT_TO_FIXED(a) ((a) * FIXED_POINT_SCALE)
#define FIXED_TO_INT(a) ((a) / FIXED_POINT_SCALE)
#define FIXED_MULTIPLY(a, b) (((a) * (b)) / FIXED_POINT_SCALE)
#define FIX1 FIXED_POINT_SCALE
#define FRAC12 = (F1/2)

typedef struct FPoint {
    fixed_t x;
    fixed_t y;
} FPoint;
#define FPoint(x, y) ((FPoint){(x), (y)})
#define FPointI(x, y) ((FPoint){INT_TO_FIXED(x), INT_TO_FIXED(y)})
#define FPointZero FPoint(0, 0)
#define FPointOne FPoint(1, 1)

static inline bool fpoint_equal(const FPoint* const a, const FPoint* const b) {
	return a->x == b->x && a->y == b->y;
}

static inline FPoint g2fpoint(GPoint gpoint) {
	return FPoint(INT_TO_FIXED(gpoint.x), INT_TO_FIXED(gpoint.y));
}

static inline GPoint f2gpoint(FPoint fpoint) {
	return GPoint(FIXED_TO_INT(fpoint.x), FIXED_TO_INT(fpoint.y));
}

static inline FPoint fpoint_add(FPoint a, FPoint b) {
	return (FPoint){a.x + b.x, a.y + b.y};
}

static inline GPoint gpoint_add(GPoint a, GPoint b) {
	return (GPoint){a.x + b.x, a.y + b.y};
}

typedef struct FSize {
    fixed_t w;
    fixed_t h;
} FSize;

typedef struct FRect {
    FPoint origin;
    FSize size;
} FRect;

typedef struct FContext {
	GContext* gctx;
	GBitmap* flagBuffer;
	FPoint min;
	FPoint max;
    FPoint cp;
    FPoint offset;
    FPoint scale_from;
	FPoint scale_to;
    fixed_t rotation;
	fixed_t subpixel_adjust;

    GColor fillColor;
	int16_t colorBias;
} FContext;

void fctx_set_fill_color(FContext* fctx, GColor c);
void fctx_set_color_bias(FContext* fctx, int16_t bias);
void fctx_set_offset(FContext* fctx, FPoint offset);
void fctx_set_scale(FContext* fctx, FPoint scale_from, FPoint scale_to);
void fctx_set_rotation(FContext* fctx, uint32_t rotation);

void fctx_move_to   (FContext* fctx, FPoint p);
void fctx_line_to   (FContext* fctx, FPoint p);
void fctx_curve_to  (FContext* fctx, FPoint cp0, FPoint cp1, FPoint p);
void fctx_draw_path (FContext* fctx, FPoint* points, uint32_t num_points);

typedef void (*fctx_init_context_func)(FContext* fctx, GContext* gctx);
typedef void (*fctx_begin_fill_func)(FContext* fctx);
typedef void (*fctx_plot_edge_func)(FContext* fctx, FPoint* a, FPoint* b);
typedef void (*fctx_plot_circle_func)(FContext* fctx, const FPoint* c, fixed_t r);
typedef void (*fctx_end_fill_func)(FContext* fctx);
typedef void (*fctx_deinit_context_func)(FContext* fctx);

extern fctx_init_context_func fctx_init_context;
extern fctx_begin_fill_func fctx_begin_fill;
extern fctx_plot_edge_func fctx_plot_edge;
extern fctx_plot_circle_func fctx_plot_circle;
extern fctx_end_fill_func fctx_end_fill;
extern fctx_deinit_context_func fctx_deinit_context;

// -----------------------------------------------------------------------------
// Compiled SVG path drawing.
// -----------------------------------------------------------------------------

typedef int16_t fixed16_t;
typedef struct __attribute__((__packed__)) FPathDrawCommand {
	uint16_t code;
	fixed16_t params[];
} FPathDrawCommand;

void fctx_draw_commands(FContext* fctx, FPoint advance, void* path_data, uint16_t length);

// -----------------------------------------------------------------------------
// Text drawing.
// -----------------------------------------------------------------------------

typedef enum {
	FTextAnchorBaseline = 0,
	FTextAnchorMiddle,
	FTextAnchorTop,
	FTextAnchorBottom
} FTextAnchor;

void fctx_set_text_size(FContext* fctx, FFont* font, int16_t pixels);
void fctx_draw_string(FContext* fctx, const char* text, FFont* font, GTextAlignment alignment, FTextAnchor anchor);
