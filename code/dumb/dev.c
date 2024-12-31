// I have no clue what I'm doing
// Right now I'm just trying to get something down
// Prototyping time!
// This system is very slow but I'm not gonna worry about performance just yet.

// I need a break from this

#include <GL/gl.h>
#include "third_party/microui/microui.h"
#define STB_TRUETYPE_IMPLEMENTATION 
#include "third_party/stb/ttf.h"

typedef struct Font_Info {
    u8 *bitmap;
    u64 width, height;
    stbtt_bakedchar glyphs[96];
} Font_Info;

global struct {
    Font_Info font;
    mu_Context *mu;
} dev_context;


function int
dev_get_text_width (mu_Font font, const char *str, int len) {
    int w = 0;
    
    for (int i = 0; i < cstr_length(str); ++i) {
        u8 c = str[i];
        if ((c & 0xc0) == 0x80) { continue; }
        if (c >= 32 && c <= 127) {
            stbtt_bakedchar dim = dev_context.font.glyphs[c];
            w += dim.xadvance;
        }
    }
    
    return w;
}

function int
dev_get_text_height (mu_Font font) {
    return 18;
}


function void
dev_tools_init (Arena *arena) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    String8 font_buffer = os_read_file(scratch.arena, str8_lit("W:/assets/dumb/fonts/Geo/Geo-Regular.ttf"), false);
    Font_Info font = {0};
    font.width = 1024; // assume this fits
    font.height = 1024;
    font.bitmap = arena_pushn(arena, u8, font.width * font.height);
    if (stbtt_BakeFontBitmap(font_buffer.str, 0, 32, font.bitmap, font.width, font.height, 32, 96, font.glyphs) != 0) {
        dev_context.font = font;
        mu_Context *mu = arena_pushn(arena, mu_Context, 1);
        mu_init(mu);
        mu->text_width = dev_get_text_width;
        mu->text_height = dev_get_text_height;
        dev_context.mu = mu;
    } else {
        OutputDebugString("Unable to parse font for developer tools!\n");
    }
    
    release_scratch(scratch);
}

function void
dev_renderer_init (void) {
    
}

function void
dev_renderer_calibrate (int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

function void
dev_ui_text_impl (const char *text, mu_Vec2 pos, mu_Color color) {
    glColor4b(color.r, color.g, color.b, color.a);
    glRasterPos2i(pos.x, pos.y);
    for (int i = 0; i < cstr_length(text); ++i) {
        u8 c = text[i];
        if ((c & 0xc0) == 0x80) { continue; }
        if (c >= 32 && c <= 127) {
            stbtt_bakedchar dim = dev_context.font.glyphs[c];
            glBitmap(dim.xadvance, 18, dim.x0, dim.y0, dim.xadvance, 0, dev_context.font.bitmap);
            int w = dim.xadvance;
            int h = 18;
            //glRecti(pos.x, pos.y, pos.x + w, pos.y + h);
        }
    }
}


function void
dev_ui_rect_impl (mu_Rect rect, mu_Color color) {
    glColor4b(color.r, color.g, color.b, color.a);
    glRecti(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
}


function void
dev_ui_icon_impl () {
    
}


function void
dev_ui_clip_impl () {
    
}

function void
dev_ui_render (void) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    
    mu_Command *cmd = 0;
    while (mu_next_command(dev_context.mu, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: dev_ui_text_impl(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: dev_ui_rect_impl(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: dev_ui_icon_impl(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: dev_ui_clip_impl(cmd->clip.rect); break;
        }
    }
}

function void
dev_ui_frame (void) {
    mu_begin(dev_context.mu);
    /* do window */
    if (mu_begin_window(dev_context.mu, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container *win = mu_get_current_container(dev_context.mu);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);
        
        /* window info */
        if (mu_header(dev_context.mu, "Window Info")) {
            mu_Container *win = mu_get_current_container(dev_context.mu);
            char buf[64];
            mu_layout_row(dev_context.mu, 2, (int[]) { 54, -1 }, 0);
            mu_label(dev_context.mu,"Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(dev_context.mu, buf);
            mu_label(dev_context.mu, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(dev_context.mu, buf);
        }
        
        /* labels + buttons */
        if (mu_header_ex(dev_context.mu, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(dev_context.mu, 3, (int[]) { 86, -110, -1 }, 0);
            mu_label(dev_context.mu, "Test buttons 1:");
            mu_button(dev_context.mu, "Button 1");
            mu_button(dev_context.mu, "Button 2");
            mu_label(dev_context.mu, "Test buttons 2:");
            mu_button(dev_context.mu, "Button 3");
            if (mu_button(dev_context.mu, "Popup")) { mu_open_popup(dev_context.mu, "Test Popup"); }
            if (mu_begin_popup(dev_context.mu, "Test Popup")) {
                mu_button(dev_context.mu, "Hello");
                mu_button(dev_context.mu, "World");
                mu_end_popup(dev_context.mu);
            }
        }
        
        /* tree */
        if (mu_header_ex(dev_context.mu, "Tree and Text", MU_OPT_EXPANDED)) {
            mu_layout_row(dev_context.mu, 2, (int[]) { 140, -1 }, 0);
            mu_layout_begin_column(dev_context.mu);
            if (mu_begin_treenode(dev_context.mu, "Test 1")) {
                if (mu_begin_treenode(dev_context.mu, "Test 1a")) {
                    mu_label(dev_context.mu, "Hello");
                    mu_label(dev_context.mu, "world");
                    mu_end_treenode(dev_context.mu);
                }
                if (mu_begin_treenode(dev_context.mu, "Test 1b")) {
                    mu_button(dev_context.mu, "Button 1");
                    mu_button(dev_context.mu, "Button 2");
                    mu_end_treenode(dev_context.mu);
                }
                mu_end_treenode(dev_context.mu);
            }
            if (mu_begin_treenode(dev_context.mu, "Test 2")) {
                mu_layout_row(dev_context.mu, 2, (int[]) { 54, 54 }, 0);
                mu_button(dev_context.mu, "Button 3");
                mu_button(dev_context.mu, "Button 4");
                mu_button(dev_context.mu, "Button 5");
                mu_button(dev_context.mu, "Button 6");
                mu_end_treenode(dev_context.mu);
            }
            if (mu_begin_treenode(dev_context.mu, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(dev_context.mu, "Checkbox 1", &checks[0]);
                mu_checkbox(dev_context.mu, "Checkbox 2", &checks[1]);
                mu_checkbox(dev_context.mu, "Checkbox 3", &checks[2]);
                mu_end_treenode(dev_context.mu);
            }
            mu_layout_end_column(dev_context.mu);
            
            mu_layout_begin_column(dev_context.mu);
            mu_layout_row(dev_context.mu, 1, (int[]) { -1 }, 0);
            mu_text(dev_context.mu, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                    "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                    "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(dev_context.mu);
        }
        
        mu_end_window(dev_context.mu);
    }
    mu_end(dev_context.mu);
}