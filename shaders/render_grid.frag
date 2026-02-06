#version 450 core

uniform usampler2D u_grid;      // RGBA8UI pixel grid
uniform sampler1D  u_palette;   // Material color palette
uniform vec2 u_camera_pos;      // Camera center in WORLD coordinates
uniform vec2 u_screen_size;     // Window size in screen pixels
uniform vec2 u_grid_size;       // Texture size
uniform float u_zoom;           // Zoom factor

in vec2 v_uv;
out vec4 frag_color;

void main() {
    // Compute world-space pixel position of this fragment
    vec2 visible_world_size = u_screen_size / u_zoom;
    vec2 world_pixel = u_camera_pos - visible_world_size * 0.5 + v_uv * visible_world_size;

    // Bounds check — outside the grid is background color
    if (world_pixel.x < 0.0 || world_pixel.x >= u_grid_size.x ||
        world_pixel.y < 0.0 || world_pixel.y >= u_grid_size.y) {
        frag_color = vec4(0.05, 0.05, 0.08, 1.0);
        return;
    }

    // Direct UV mapping (world coords = texture coords)
    vec2 grid_uv = world_pixel / u_grid_size;
    grid_uv = clamp(grid_uv, vec2(0.001), vec2(0.999));

    // Read material ID from the red channel
    uint mat_id = texture(u_grid, grid_uv).r;

    // Look up color from palette
    float palette_coord = (float(mat_id) + 0.5) / 256.0;
    frag_color = texture(u_palette, palette_coord);
}
