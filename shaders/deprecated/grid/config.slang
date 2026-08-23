#ifndef GRID_CONFIG_GLSL_
#define GRID_CONFIG_GLSL_

static const float grid_size = 500.0;
static const float grid_cell_size = 1.0;

#define GRID_BLACK 0

#if GRID_BLACK
static const float4 grid_color_thick = float4(0.1, 0.1, 0.1, 1.0);
static const float4 grid_color_thin = float4(0.2, 0.2, 0.2, 1.0);
#else
static const float4 grid_color_thick = float4(1.0, 1.0, 1.0, 1.0);
static const float4 grid_color_thin = float4(0.5, 0.5, 0.5, 1.0);
#endif

static const float grid_min_pixels_between_cells = 2.0;

#endif //GRID_CONFIG_GLSL_
