# pebble-fctx

This is a graphics library for the Pebble smart watch.

Provides subpixel accurate, anti-aliased rendering of filled shapes.  Supports circles, line segments and bezier segments, SVG paths, and SVG fonts.

### Notes and caveats

The library uses an even-odd fill rule.

Only filled shapes are supported.  So, to create a line, you would need to draw a thin box.  And to draw a ring, you would plot a pair concentric circles.

Clipping is supported for AA and BW rendering, *except* that it does not produce correct results in BW rendering mode on circular displays.

### Mode selection (color platforms only)
    void fctx_enable_aa(bool enable);
    bool fctx_is_aa_enabled();

By default, color platforms will use the anti-aliased (AA) rendering path, but the 1-bit (BW) rendering path is available as an option.  Make this selection *before* calling `fctx_init_context`.  Note that clipping does not work properly in BW mode with circular frame buffers.

### Initialization and cleanup
    void fctx_init_context(FContext* fctx, GContext* gctx);
    void fctx_deinit_context(FContext* fctx);

Initialize an FContext for rendering by providing a GContext to render to.  An internal buffer will be allocated of the same dimensions as the GContext.  This buffer will be one byte per pixel on basalt with anti-aliasing enabled.  On aplite, or with anti-aliasing disabled, the buffer will be just one bit per pixel.
Deinitialize the FContext when drawing is complete.

### Drawing procedure
    void fctx_begin_fill(FContext* fctx);
    void fctx_end_fill(FContext* fctx);

To draw a filled shape, call `fctx_begin_fill` then call any number of plotting or drawing functions.  Finally, call `fctx_end_fill`.  At this point, the accumulated shape will be rendered to the GContext.

### Color
    void fctx_set_fill_color(FContext* fctx, GColor c);
    void fctx_set_color_bias(FContext* fctx, int16_t bias);

The current color and bias are applied when `fctx_end_fill` is called.  The bias value is applied as an adjustment to the 'pixel coverage' value in the anti-aliasing calculations.  Meaningful values are -8 to +8, though positive values are not really useful in practice.  Negative values are effectively an opacity setting.  -8 would be completely transparent.

### Transform
    void fctx_set_offset(FContext* fctx, FPoint offset);
    void fctx_set_scale(FContext* fctx, FPoint scale_from, FPoint scale_to);
    void fctx_set_rotation(FContext* fctx, uint32_t rotation);

The current transform state is applied at the time a `draw` function is called.  Transform components are applied in the following order:  scale, rotate, then offset.

### Primitive plotting
    void fctx_plot_edge(FContext* fctx, FPoint* a, FPoint* b);
    void fctx_plot_circle(FContext* fctx, const FPoint* c, fixed_t r);

The plotting functions are the lowest level drawing functions.  They do not apply the current transform state to the coordinates.

### Path drawing
    void fctx_draw_path(FContext* fctx, FPoint* points, uint32_t num_points);

Path (i.e. polygon) drawing respects the current transform state.  It draws an array of points as a closed polygon, automatically connecting the last and first points.

### Stateful drawing
    void fctx_move_to(FContext* fctx, FPoint p);
    void fctx_line_to(FContext* fctx, FPoint p);
    void fctx_curve_to(FContext* fctx, FPoint cp0, FPoint cp1, FPoint p);
    void fctx_close_path(FContext* fctx);

The stateful draw commands respect the current transform state.  `fctx_curve_to` draws cubic spline (bezier) segments.  The shape is *not* automatically closed.

### Compiled SVG path drawing
    FPath* fpath_create_from_resource(uint32_t resource_id);
    void fpath_destroy(FPath* fpath);
    void fctx_draw_commands(FContext* fctx, FPoint advance, void* path_data, uint16_t length);

The `advance` parameter is an offset that is applied before the regular transform state is applied.
Compiled path resources are built by the [fctx-compiler](#resource-compiler) tool.

### Text drawing
    void fctx_set_text_size(FContext* fctx, FFont* font, int16_t pixels);
    void fctx_draw_string(FContext* fctx, const char* text, FFont* font, GTextAlignment alignment, FTextAnchor anchor);

The `fctx_set_text_size` function is a convenience method that calls `fctx_set_scale` with values to achieve a specific text size (in pixels).

### Fonts
    FFont* ffont_create_from_resource(uint32_t resource_id);
    void ffont_destroy(FFont* font);

The font resources are built by the [fctx-compiler](#resource-compiler) tool.

## Resource Compiler

The `fctx-compiler` tool is provided for the compilation of SVG data files into a binary format for use with the pebble-fctx drawing library.

    ./node_modules/.bin/fctx-compiler <svg file>

Supports the extraction of SVG font definitions and individual paths.

[FontForge](https://fontforge.github.io/en-US/) is recommended for preparing
SVG fonts.

A single SVG input file can generate multiple output files.  Each supported resource in the input is written as an output file into the resources directory.  Each output file is named with the id of the element from the input.

For example, if the input file `resources.svg` contains, within the `<defs>` element, a `<font>` element with `id="digits"` and a `<path>` element `id="icon"`.
Then the command

    ./node_modules/.bin/fctx-compiler resources.svg`

will output two files: `resources/digits.ffont` and `resources/icon.fpath`.
