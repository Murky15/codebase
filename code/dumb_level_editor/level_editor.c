#include <stdio.h>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#define BASE_TYPES_ESSENTIAL_ONLY
#define BASE_MEMORY_STANDALONE
#include "base/context.h"
#include "base/macros.h"
#include "base/types.h"
#include "base/memory.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "base/memory.c"

void
draw_grid_old (int columns, int spacing) {
    rlPushMatrix();
    {
        rlTranslatef(0, columns * spacing * 0.25f, 0);
        rlRotatef(90, 1, 0, 0);
        DrawGrid(columns, spacing); 
    }
    rlPopMatrix();
}

void
draw_grid (Camera2D cam, int spacing) {
    Vector2 origin = GetWorldToScreen2D((Vector2){0,0}, cam);
    int cam_spacing = spacing*cam.zoom;
    int render_width = GetRenderWidth();
    int render_height = GetRenderHeight();
    
    int num_columns = render_width/cam_spacing;
    int onside_columns = (render_width-origin.x)/cam_spacing;
    int num_rows = render_height/cam_spacing;
    int onside_rows = (render_height-origin.y)/cam_spacing;
    for (int x = onside_columns-num_columns; x <= onside_columns; x++) {
        int px = x*cam_spacing + origin.x;
        DrawLine(px, 0, px, render_height, BLACK);
    }
    for (int y = onside_rows-num_rows; y <= onside_rows; y++) {
        int py = y*cam_spacing + origin.y;
        DrawLine(0, py, render_width, py, BLACK);
    }
}

int 
main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Dumb Level Editor");
    //SetTargetFPS(60);
    
    Camera2D cam = {0};
    cam.zoom = 1.f;
    cam.offset = (Vector2){GetRenderWidth()/2, GetRenderHeight()/2};
    
    int ideal_grid_spacing = 32;
    float grid_scale = ideal_grid_spacing;
    while (!WindowShouldClose())
    {
        //- Update
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            
        }
        
        // Camera
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f/cam.zoom);
            cam.target = Vector2Add(cam.target, delta);
        }
        
        Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), cam);
        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            cam.offset = GetMousePosition();
            cam.target = mouse_world_pos;
            float scale = 0.2f*wheel;
            cam.zoom = Clamp(expf(logf(cam.zoom)+scale), 0.125f, 64.0f);
            
            // Scale grid accordingly
            float world_units_per_pixel = 1.f / cam.zoom;
            float target_spacing = ideal_grid_spacing * world_units_per_pixel;
            float log_spacing = log2f(target_spacing);
            float snapped = roundf(log_spacing);
            grid_scale = powf(2.f, snapped);
        }
        
        //- Render
        BeginDrawing();
        {
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
            DrawFPS(0,0);
            DrawText(TextFormat("Scale %f, [%f, %f]", grid_scale, mouse_world_pos.x, mouse_world_pos.y), GetMouseX() + 15, GetMouseY(), 20, BLACK);
            draw_grid(cam, grid_scale);
            
            BeginMode2D(cam);
            {
                DrawCircle(0, 0, 10, BLACK); // Origin point
            }
            EndMode2D();
        }
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}