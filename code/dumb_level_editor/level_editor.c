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
Get rid of all the magic numbers!
*/ 

#define MAX_EDGES 4096

/* @note: 
I'm kind-of experimenting with a new way of storage and memory layout than I usually do things here (while procrastinating my math final)
 Instead of using a linked-list or a large edge structure with every piece of data which can possibly pertain to the edge,
 I like storing it in an array with every edge stored continuously. It feels clean, trivially looping through either
 the points or edges in a simple for loop. Preventing fragmentation has been pretty easy too. The `next_free_edge` field indexes
 the edge that was most recently freed, and if `next_free_edge` > -1 next time we free an edge, we tuck this old index in the x value of
 p0 of this edge and overwrite it. Thus creating a chain through the array to get our memory back. This means though, that all extreneous 
edge data must be stored in a parallel array. Whether this is the most optimal way to do things in my situation, time will tell, but it has
been a fun outside-the-box thinking exercise.
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
    int next_free_edge;
} Edge_Array;

typedef struct Edge_Data {
    b32 passthrough;
    String8 texture_name;
} Edge_Data;

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
    //- Init
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Dumb Level Editor");
    SetExitKey(0);
    
    Camera2D cam = {0};
    cam.zoom = 10.f;
    cam.offset = (Vector2){GetRenderWidth()/2, GetRenderHeight()/2};
    
    Arena *arena = arena_alloc();
    Edge_Array edges = {0};
    edges.e = arena_pushn(arena, Edge, MAX_EDGES);
    edges.next_free_edge = -1;
    Edge_Data *edge_data = arena_pushn(arena, Edge_Data, MAX_EDGES);
    
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
        if (IsKeyPressed(KEY_ESCAPE)) {
            active_point = Vector2_Inf;
        }
        
        if (IsKeyPressed(KEY_P)) {
            for (int i = 0; i < edges.count; ++i) {
                Edge *e = &edges.e[i];
                if (CheckCollisionPointLine(mouse_world_pos, e->p0, e->p1, 1.5f)) {
                    edge_data[i].passthrough = !edge_data[i].passthrough;
                    break;
                }
            }
        }
        
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
                        if (edges.next_free_edge > -1)
                            e->p0.x = edges.next_free_edge;
                        edges.next_free_edge = i;
                        break;
                    } 
                }
            } else { 
                // Add line
                if (active_point.y == INFINITY) { 
                    active_point = cursor;
                } else {
                    Vector2 new_point = cursor;
                    Edge *new_edge;
                    int new_index;
                    if (edges.next_free_edge > -1) {
                        new_index = edges.next_free_edge;
                        new_edge = &edges.e[new_index];
                        edges.next_free_edge = new_edge->p0.x == INFINITY ? -1 : new_edge->p0.x;
                    } else {
                        assert(edges.count != MAX_EDGES);
                        new_index = edges.count++;
                        new_edge = &edges.e[new_index];
                    }
                    *new_edge = (Edge){active_point, new_point};
                    memory_zero(&edge_data[new_index], sizeof(Edge_Data));
                    active_point = Vector2_Inf;
                }
            }
        }
        
        //- Render
        BeginDrawing();
        {
            // Background
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
            draw_grid(cam, ideal_grid_spacing);
            
            // HUD
            DrawFPS(0,0);
            DrawText(TextFormat("[%f, %f]", cursor.x, cursor.y), GetMouseX() + 15, GetMouseY(), 20, BLACK);
            
            // World
            BeginMode2D(cam);
            {
                if (active_point.x != INFINITY) {
                    DrawLineEx(active_point, cursor, 5/cam.zoom, GOLD);
                    DrawCircleV(active_point, 5/cam.zoom, GRAY);
                }
                for (int i = 0; i < edges.count; ++i) {
                    DrawLineEx(edges.e[i].p0, edges.e[i].p1, 5/cam.zoom, edge_data[i].passthrough ? GREEN : RED);
                    DrawCircleV(edges.e[i].p0, 5/cam.zoom, GRAY);
                    DrawCircleV(edges.e[i].p1, 5/cam.zoom, GRAY);
                }
                
                DrawCircle(0, 0, 10/cam.zoom, BLACK);  
                DrawCircleV(cursor, 8/cam.zoom, DARKBLUE);
            }
            EndMode2D();
            
        }
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}