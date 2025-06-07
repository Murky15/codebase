#include <stdio.h>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#define BASE_TYPES_ESSENTIAL_ONLY
#define BASE_MEMORY_MINIMAL
#include "base/include.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "base/memory.c"

/* @todo: 
Split intersecting lines 
  Shade completed sectors
Line deletion
Cancel line adding
*/ 

typedef struct Edge {
    union {
        struct { Vector2 p0, p1; };
        Vector2 p[2];
    };
} Edge;

typedef struct Edge_Array {
    Edge *e;
    int count;
    int free_edge;
} Edge_Array;

read_only Vector2 Vector2_Inf = (Vector2){INFINITY, INFINITY};

void
draw_grid (Camera2D cam, int spacing) {
    Vector2 origin = GetWorldToScreen2D((Vector2){0,0}, cam);
    f32 cam_spacing = spacing*cam.zoom;
    int render_width = GetRenderWidth();
    int render_height = GetRenderHeight();
    
    int num_columns = render_width/cam_spacing;
    int onside_columns = (render_width-origin.x)/cam_spacing;
    int num_rows = render_height/cam_spacing;
    int onside_rows = (render_height-origin.y)/cam_spacing;
    for (f32 x = onside_columns-num_columns; x <= onside_columns; x++) {
        f32 px = x*cam_spacing + origin.x;
        DrawLine(px, 0, px, render_height, BLACK);
    }
    for (f32 y = onside_rows-num_rows; y <= onside_rows; y++) {
        f32 py = y*cam_spacing + origin.y;
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
    cam.zoom = 10.f;
    cam.offset = (Vector2){GetRenderWidth()/2, GetRenderHeight()/2};
    
    Arena *edge_arena = arena_alloc();
    Edge_Array edges = {0};
    edges.e = arena_pushn(edge_arena, Edge, 0);
    edges.free_edge = -1;
    
    Vector2 active_point = Vector2_Inf;
    Vector2 cursor = {0};
    
    int ideal_grid_spacing = 10;
    while (!WindowShouldClose())
    {
        //- Update
        Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), cam); 
        
        // Camera
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f/cam.zoom);
            cam.target = Vector2Add(cam.target, delta);
        }
        
        f32 wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            cam.offset = GetMousePosition();
            cam.target = mouse_world_pos;
            f32 scale = 0.2f*wheel;
            cam.zoom = Clamp(expf(logf(cam.zoom)+scale), 2.f, 64);
        }
        
        // Editor 
        
        if (IsKeyDown(KEY_LEFT_SHIFT)) { // Snap to grid
            Vector2 norm_cursor = Vector2Scale(mouse_world_pos, 1.f/ideal_grid_spacing);
            norm_cursor.x = roundf(norm_cursor.x);
            norm_cursor.y = roundf(norm_cursor.y);
            cursor = Vector2Scale(norm_cursor, ideal_grid_spacing);
        } else {
            b32 snapped = false;
            Vector2 *points = edges.e[0].p;
            int num_points = edges.count*2;
            for (int i = 0; i < num_points; ++i) {
                Vector2 p = points[i];
                if (p.y != INFINITY && CheckCollisionPointCircle(mouse_world_pos, p, 30.f/cam.zoom)) {
                    cursor = p;
                    snapped = true;
                    break;
                }
            }
            if (!snapped)
                cursor = mouse_world_pos;
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (IsKeyDown(KEY_LEFT_CONTROL)) {
                // Remove line
                for (int i = 0; i < edges.count; ++i) {
                    Edge *e = &edges.e[i];
                    if (CheckCollisionPointLine(mouse_world_pos, e->p0, e->p1, 1)) {
                        *e = (Edge){Vector2_Inf, Vector2_Inf};
                        if (edges.free_edge > -1)
                            e->p0.x = edges.free_edge;
                        edges.free_edge = i;
                        break;
                    } 
                }
            } else { 
                // Add line
                if (active_point.x == INFINITY) { 
                    active_point = cursor;
                } else {
                    Vector2 new_point = cursor;
                    Edge *new_edge;
                    if (edges.free_edge > -1) {
                        new_edge = &edges.e[edges.free_edge];
                        // CHECK HERE
                        edges.free_edge = new_edge->p0.x;
                    } else {
                        new_edge = arena_pushn(edge_arena, Edge, 1);
                    }
                    *new_edge = (Edge){active_point, new_point};
                    edges.count++;
                    active_point = Vector2_Inf;
                }
            }
        }
        
        //- Render
        BeginDrawing();
        {
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
            draw_grid(cam, ideal_grid_spacing);
            DrawFPS(0,0);
            DrawText(TextFormat("[%f, %f]", cursor.x, cursor.y), GetMouseX() + 15, GetMouseY(), 20, BLACK);
            
            BeginMode2D(cam);
            {
                if (active_point.x != INFINITY)
                    DrawLineEx(active_point, cursor, 0.5f, GREEN);
                for (int i = 0; i < edges.count; ++i) {
                    DrawLineEx(edges.e[i].p0, edges.e[i].p1, 0.5f, RED);
                    DrawCircleV(edges.e[i].p0, 0.5f, GRAY);
                    DrawCircleV(edges.e[i].p1, 0.5f, GRAY);
                }
                
                DrawCircle(0, 0, 10/cam.zoom, BLACK); // Origin 
                DrawCircleV(cursor, 8/cam.zoom, DARKBLUE);
            }
            EndMode2D();
            
        }
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}