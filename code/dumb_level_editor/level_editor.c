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
draw_grid (int columns, int spacing) {
    rlPushMatrix();
    {
        rlTranslatef(0, columns * spacing * 0.25f, 0);
        rlRotatef(90, 1, 0, 0);
        DrawGrid(columns, spacing); 
    }
    rlPopMatrix();
}

int 
main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Dumb Level Editor");
    //SetTargetFPS(60);
    
    Camera2D cam = {0};
    cam.zoom = 1.f;
    cam.offset = (Vector2){GetScreenWidth()/2, GetScreenHeight()/2};
    
    int grid_columns = 100, ideal_grid_spacing = 50;
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
            
            BeginMode2D(cam);
            {
                draw_grid(grid_columns, grid_scale); // @todo: Figure out how to make this stretch infinitely
                DrawCircle(0, 0, 10/cam.zoom, BLACK); // Origin point
            }
            EndMode2D();
        }
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}