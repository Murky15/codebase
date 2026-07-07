// NOTE: Only desktop page to save time
// TODO: Would it be possible to write a Jai plugin that allows me to write incomplete struct literals without member names like in C?
// TODO: Also warn on unused variables

FONT_ID_BODY_16 :: 0;
FONT_ID_TITLE_56 :: 1;
FONT_ID_BODY_24 :: 2;
FONT_ID_BODY_36 :: 3;
FONT_ID_TITLE_36 :: 4;
FONT_ID_MONOSPACE_24 :: 5;

COLOR_LIGHT :: Clay_Color.{244, 235, 230, 255};
COLOR_LIGHT_HOVER :: Clay_Color.{224, 215, 210, 255};
COLOR_RED :: Clay_Color.{168, 66, 28, 255};
COLOR_RED_HOVER :: Clay_Color.{148, 46, 8, 255};
COLOR_ORANGE :: Clay_Color.{225, 138, 50, 255};
COLOR_BLUE :: Clay_Color.{111, 173, 162, 255};

// Colors for top stripe
COLOR_TOP_BORDER_1 :: Clay_Color.{168, 66, 28, 255};
COLOR_TOP_BORDER_2 :: Clay_Color.{223, 110, 44, 255};
COLOR_TOP_BORDER_3 :: Clay_Color.{225, 138, 50, 255};
COLOR_TOP_BORDER_4 :: Clay_Color.{236, 189, 80, 255};
COLOR_TOP_BORDER_5 :: Clay_Color.{240, 213, 137, 255};

COLOR_BLOB_BORDER_1 :: Clay_Color.{168, 66, 28, 255};
COLOR_BLOB_BORDER_2 :: Clay_Color.{203, 100, 44, 255};
COLOR_BLOB_BORDER_3 :: Clay_Color.{225, 138, 50, 255};
COLOR_BLOB_BORDER_4 :: Clay_Color.{236, 159, 70, 255};
COLOR_BLOB_BORDER_5 :: Clay_Color.{240, 189, 100, 255};

HEADER_TEXT_CONFIG :: Clay_TextElementConfig.{fontId = 2, fontSize = 24, textColor = {61, 26, 5, 255}};
BLOB\ _TEXT_CONFIG :: Clay_TextElementConfig.{fontId = 2, fontSize = 30, textColor = {244, 235, 230, 255}};

Custom_Html_Data :: struct {
  link: Clay_String;
  cursor_pointer: bool;
  disable_pointer_events: bool;
}

window_width := 1024.0;
window_height := 768.0;

frame_allocate_custom_data :: (data: Custom_Html_Data) -> *Custom_Html_Data {
  result := New(Custom_Html_Data,, temp);
  result.* = data;

  return result;
}

frame_allocate_string :: (str: Clay_String) -> *Clay_String {
  allocated := New(Clay_String,, temp);
  allocated.* = str;

  return allocated;
}

landing_page_blob :: (index: u32, font_size: u16, color: Clay_Color, text: string, image_url: string) {
  if Clay_PushUI(Clay_Id("HeroBlob", index), {
    layout = {sizing = {width = Clay_SizingGrow({max = 480})}, padding = Clay_PaddingAll(16), childGap = 16, childAlignment = {y = .CENTER}},
    border = {color = color, width = {2, 2, 2, 2, 0}},
    cornerRadius = Clay_CornerRadiusAll(10) }) {

    Clay_UI(Clay_Id("CheckImage", index), {
      layout = {sizing = {width = Clay_SizingFixed(32)}},
      aspectRatio = {1},
      image = {imageData = frame_allocate_string(Clay_MakeString(image_url))}});

    Clay_Text(text, {fontSize = font_size, fontId = FONT_ID_BODY_24, textColor = color});
  }
}

landing_page_desktop :: () {
  Clay_PushUI(Clay_Id("LandingPage1Desktop"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFit({min = window_height - 70})}, childAlignment = {y = .CENTER}, padding = {50, 50, 0, 0}}});
  Clay_PushUI(Clay_Id("LandingPage1"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingGrow()}, childAlignment = {y = .CENTER}, padding = Clay_PaddingAll(32), childGap = 32}, border = {width = {left = 2, right = 2}, color = COLOR_RED}});
  if Clay_PushUI(Clay_Id("LeftText"), {layout = {sizing = {width = Clay_SizingPercent(0.55)}, layoutDirection = .TOP_TO_BOTTOM, childGap = 8}}){
    Clay_Text("Clay is a flex-box style UI auto layout library in C, with declarative syntax and microsecond performance.", {fontSize = 56, fontId = FONT_ID_TITLE_56, textColor = COLOR_RED});
    Clay_UI(Clay_Id("LandingPageSpacer"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFixed(32)}}});
    Clay_Text("Clay is laying out this webpage right now!", {fontSize = 36, fontId = FONT_ID_TITLE_36, textColor = COLOR_ORANGE});
  }
  if Clay_PushUI(Clay_Id("HeroImageOuter"), {layout = {layoutDirection = .TOP_TO_BOTTOM, sizing = {width = Clay_SizingPercent(0.45)}, childAlignment = {x = .CENTER}, childGap = 16}}) {
    landing_page_blob(1, 32, COLOR_BLOB_BORDER_5, "High performance", "/clay/images/check_5.png");
    landing_page_blob(2, 32, COLOR_BLOB_BORDER_4, "Flexbox-style responsive layout", "/clay/images/check_4.png");
    landing_page_blob(3, 32, COLOR_BLOB_BORDER_3, "Declarative syntax", "/clay/images/check_3.png");
    landing_page_blob(4, 32, COLOR_BLOB_BORDER_2, "Single .h file for C/C++", "/clay/images/check_2.png");
    landing_page_blob(5, 32, COLOR_BLOB_BORDER_1, "Compile to 15kb .wasm", "/clay/images/check_1.png");
  }
}

feature_blocks_desktop :: () {
  Clay_PushUI(Clay_Id("FeatureBlocksOuter"), {layout = {sizing = {width = Clay_SizingGrow()}}});
  Clay_PushUI(Clay_Id("FeatureBlocksInner"), {layout = {sizing = {width = Clay_SizingGrow()}, childAlignment = {y = .CENTER}}, border = {width = {betweenChildren = 2}, color = COLOR_RED}});
  text_config := Clay_TextElementConfig.{fontSize = 24, fontId = FONT_ID_BODY_24, textColor = COLOR_RED};
  if Clay_PushUI(Clay_Id("HFileBoxOuter"), {layout = {layoutDirection = .TOP_TO_BOTTOM, sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {y = .CENTER}, padding = {50, 50, 32, 32}, childGap = 8}}) {
    if Clay_PushUI(Clay_Id("HFileIncludeOuter"), {layout = {padding = {8, 4, 0, 0}}, backgroundColor = COLOR_RED, cornerRadius = Clay_CornerRadiusAll(8)}) {
      Clay_Text("#include clay.h", {fontSize = 24, fontId = FONT_ID_BODY_24, textColor = COLOR_LIGHT});
    }
    Clay_Text("~2000 lines of C99.", text_config);
    Clay_Text("Zero dependencies, including no C standard library.", text_config);
  }
  if Clay_PushUI(Clay_Id("BringYourOwnRendererOuter"), {layout = {layoutDirection = .TOP_TO_BOTTOM, sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {y = .CENTER}, padding = {50, 50, 32, 32}, childGap = 8}}) {
    Clay_Text("Render agnostic.", {fontId = FONT_ID_BODY_24, fontSize = 24, textColor = COLOR_ORANGE});
    Clay_Text("Layout with clay, then render with Raylib, WebGL Canvas or even as HTML.", text_config);
    Clay_Text("Flexible output for easy compositing in your custom engine or environment.", text_config);
  }
}

declarative_syntax_page_desktop :: () {
  Clay_PushUI(Clay_Id("SyntaxPageDesktop"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFit({min = window_height - 50})}, childAlignment = {y = .CENTER}, padding = {50, 50, 0, 0}}});
  Clay_PushUI(Clay_Id("SyntaxPage"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingGrow()}, childAlignment = {y = .CENTER}, padding = Clay_PaddingAll(32), childGap = 32}, border = {width = {left = 2, right = 2}, color = COLOR_RED}});
  if Clay_PushUI(Clay_Id("SyntaxPageLeftText"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, layoutDirection = .TOP_TO_BOTTOM, childGap = 8}}) {
    Clay_Text("Declarative Syntax", {fontSize = 52, fontId = FONT_ID_TITLE_56, textColor = COLOR_RED});
    Clay_UI(Clay_Id("SyntaxSpacer"), {layout = {sizing = {width = Clay_SizingGrow({max = 16})}}});
    text_config := Clay_TextElementConfig.{fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_RED};
    Clay_Text("Flexible and readable declarative syntax with nested UI element hierarchies.", text_config);
    Clay_Text("Mix elements with standard C code like loops, conditionals and functions.", text_config);
    Clay_Text("Create your own library of re-usable components from UI primitives like text, images and rectangles.", text_config);
  }
  Clay_PushUI(Clay_Id("SyntaxPageRightImage"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {x = .CENTER}}});
  Clay_UI(Clay_Id("SyntaxPageRightImageInner"), {layout = {sizing = {width = Clay_SizingGrow({max = 568})}}, aspectRatio = {1136.0 / 1194.0}, image = {imageData = frame_allocate_string(Clay_MakeString("/clay/images/declarative.png"))}});
}

color_lerp :: (a: Clay_Color, b: Clay_Color, amount: float) -> Clay_Color {
  result := lerp(cast,force(Vector4) a, cast,force(Vector4) b, amount);
  return cast,force(Clay_Color) result;
}

LOREM_IPSUM_TEXT :: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

high_performance_page_desktop :: (lerp_value: float) {
  Clay_PushUI(Clay_Id("PerformanceCounter"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFit({min = window_height - 50})}, childAlignment = {y = .CENTER}, padding = {82, 82, 32, 32}, childGap = 64}, backgroundColor = COLOR_RED});
  if Clay_PushUI(Clay_Id("PerformanceLeftText"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, layoutDirection = .TOP_TO_BOTTOM, childGap = 8}}) {
    Clay_Text("High Performance", {fontSize = 52, fontId = FONT_ID_TITLE_56, textColor = COLOR_LIGHT});
    Clay_UI(Clay_Id("PerformanceSpacer"), {layout = {sizing = {width = Clay_SizingGrow({max = 16})}}});
    Clay_Text("Fast enough to recompute your entire UI every frame.", {fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_LIGHT});
    Clay_Text("Small memory footprint (3.5mb default) with static allocation & reuse. No malloc / free.", {fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_LIGHT});
    Clay_Text("Simplify animations and reactive UI design by avoiding the standard performance hacks.", {fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_LIGHT});
  }
  Clay_PushUI(Clay_Id("PerformanceRightImageOuter"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {x = .CENTER}}});
  Clay_PushUI({layout = {sizing = {Clay_SizingGrow(), Clay_SizingFixed(400)}}, border = {width = {2, 2, 2, 2, 0}, color = COLOR_LIGHT}});
  if Clay_PushUI(Clay_Id("AnimationDemoContainerLeft"), {layout = {sizing = {Clay_SizingPercent(0.3 + 0.4 * lerp_value), Clay_SizingGrow()}, childAlignment = {y = .CENTER}, padding = Clay_PaddingAll(32)}, backgroundColor = color_lerp(COLOR_RED, COLOR_ORANGE, lerp_value)}) {
    Clay_Text(LOREM_IPSUM_TEXT, {fontSize = 24, fontId = FONT_ID_TITLE_56, textColor = COLOR_LIGHT});
  }
  if Clay_PushUI(Clay_Id("AnimationDemoContainerRight"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingGrow()}, childAlignment = {y = .CENTER}, padding = Clay_PaddingAll(32)}, backgroundColor = color_lerp(COLOR_ORANGE, COLOR_RED, lerp_value)}) {
    Clay_Text(LOREM_IPSUM_TEXT, {fontSize = 24, fontId = FONT_ID_TITLE_56, textColor = COLOR_LIGHT});
  }
}

renderer_button_active :: (text: string) {
  Clay_PushUI({
    layout = {sizing = {width = Clay_SizingFixed(300)}, padding = Clay_PaddingAll(16)},
    backgroundColor = ifx Clay_Hovered() then COLOR_RED_HOVER else COLOR_RED,
    cornerRadius = Clay_CornerRadiusAll(10),
    userData = frame_allocate_custom_data({disable_pointer_events = true, cursor_pointer = true}),
  });
  Clay_Text(text, {fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_LIGHT});
}

renderer_button_inactive :: (text: string, renderer_idx: u32) {
  Clay_PushUI({
    layout = {sizing = {width = Clay_SizingFixed(300)}, padding = Clay_PaddingAll(16)},
    border = {width = {2, 2, 2, 2, 0}, color = COLOR_RED},
    backgroundColor = ifx Clay_Hovered() then COLOR_LIGHT_HOVER else COLOR_LIGHT,
    cornerRadius = Clay_CornerRadiusAll(10),
    userData = frame_allocate_custom_data({disable_pointer_events = true, cursor_pointer = true}),
  });
}

renderer_page_desktop :: () {
  Clay_PushUI(Clay_Id("RendererPageDesktop"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFit({min = window_height - 50})}, childAlignment = {y = .CENTER}, padding = {50, 50, 0, 0}}});
  Clay_PushUI(Clay_Id("RendererPage"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingGrow()}, childAlignment = {y = .CENTER}, padding = Clay_PaddingAll(32), childGap = 32}, border = {width = {left = 2, right = 2}, color = COLOR_RED}});
  if Clay_PushUI(Clay_Id("RendererLeftText"), {layout = {sizing = {width = Clay_SizingPercent(0.5)}, layoutDirection = .TOP_TO_BOTTOM, childGap = 8}}) {
    Clay_Text("Renderer & Platform Agnostic", {fontSize = 52, fontId = FONT_ID_TITLE_56, textColor = COLOR_RED});
    Clay_UI(Clay_Id("RendererSpacerLeft"), {layout = {sizing = {width = Clay_SizingGrow({max = 16})}}});
    text_config := Clay_TextElementConfig.{fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_RED};
    Clay_Text("Clay outputs a sorted array of primitive render commands, such as RECTANGLE, TEXT or IMAGE.", text_config);
    Clay_Text("Write your own renderer in a few hundred lines of code, or use the provided examples for Raylib, WebGL canvas and more.", text_config);
    Clay_Text("There's even an HTML renderer - you're looking at it right now!", text_config);
  }
  if Clay_PushUI(Clay_Id("RendererRightText"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {x = .CENTER}, layoutDirection = .TOP_TO_BOTTOM, childGap = 16}}) {
    Clay_Text("Try changing renderer!", {fontSize = 36, fontId = FONT_ID_BODY_36, textColor = COLOR_ORANGE});
    Clay_UI(Clay_Id("RendererSpacerRight"), {layout = {sizing = {width = Clay_SizingGrow({max = 32})}}});
    renderer_button_active("HTML Renderer");
    renderer_button_inactive("Canvas Renderer", 1);
  }
}

debugger_page_desktop :: () {
  Clay_PushUI(Clay_Id("DebuggerDesktop"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFit({min = window_height - 50})}, childAlignment = {y = .CENTER}, padding = {82, 82, 32, 32}, childGap = 64}, backgroundColor = COLOR_RED});
  if Clay_PushUI(Clay_Id("DebuggerLeftText"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, layoutDirection = .TOP_TO_BOTTOM, childGap = 8}}) {
    Clay_Text("Integrated Debug Tools", {fontSize = 52, fontId = FONT_ID_TITLE_56, textColor = COLOR_LIGHT});
    Clay_UI(Clay_Id("DebuggerSpacer"), {layout = {sizing = {width = Clay_SizingGrow({max = 16})}}});
    text_config := Clay_TextElementConfig.{fontSize = 28, fontId = FONT_ID_BODY_36, textColor = COLOR_LIGHT};
    Clay_Text("Clay includes built in \"Chrome Inspector\"-style debug tooling.", text_config);
    Clay_Text("View your layout hierarchy and config in real time.", text_config);
    Clay_UI(Clay_Id("DebuggerPageSpacer"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFixed(32)}}});
    Clay_Text("Press the \"d\" key to try it out now!", {fontSize = 32, fontId = FONT_ID_TITLE_36, textColor = COLOR_ORANGE});
  }
  if Clay_PushUI(Clay_Id("DebuggerRightImageOuter"), {layout = {sizing = {width = Clay_SizingPercent(0.50)}, childAlignment = {x = .CENTER}}}) {
    Clay_UI(Clay_Id("DebuggerPageRightImageInner"), {layout = {sizing = {width = Clay_SizingGrow({max = 558})}}, aspectRatio = {1620.0 / 1474.0}, image = {imageData = frame_allocate_string(Clay_MakeString("/clay/images/debugger.png"))}});
  }
}

animation_lerp_value := 1.0;

create_layout :: (mobile_screen: bool, lerp_value: float) -> Clay_RenderCommandArray {
  Clay_BeginLayout();
  if Clay_PushUI(Clay_Id("OuterContainer"), {layout = {layoutDirection = .TOP_TO_BOTTOM, sizing = {Clay_SizingGrow(), Clay_SizingGrow()}}, backgroundColor = COLOR_LIGHT}) {
    if Clay_PushUI(Clay_Id("Header"), {layout = {sizing = {Clay_SizingGrow(), Clay_SizingFixed(50)}, childAlignment = {y = .CENTER}, childGap = 16, padding = {32, 32, 0, 0}}}) {
      Clay_Text("Clay", HEADER_TEXT_CONFIG);
      Clay_UI(Clay_Id("Spacer"), {layout = {sizing = {width = Clay_SizingGrow()}}});
      if !mobile_screen {
        if Clay_PushUI(Clay_Id("LinkExamplesOuter"), {layout = {padding = {8,8,0,0}}}) {
          Clay_Text("Examples", {
            userData = frame_allocate_custom_data({
              link = Clay_MakeString("https://github.com/nicbarker/clay/tree/main/examples"),
            }),
            fontId = FONT_ID_BODY_24, fontSize = 24, textColor = {61, 26, 5, 255}}
          );
        }
        if Clay_PushUI(Clay_Id("LinkDocsOuter"), {layout = {padding = {8,8,0,0}}}) {
          Clay_Text("Docs", {
            userData = frame_allocate_custom_data({link = Clay_MakeString("https://github.com/nicbarker/clay/blob/main/README.md")}),
            fontId = FONT_ID_BODY_24, fontSize = 24, textColor = {61, 26, 5, 255}}
          );
        }
      }
      if Clay_PushUI({
        layout = {padding = {16, 16, 6, 6}},
        backgroundColor = ifx Clay_Hovered() then COLOR_LIGHT_HOVER else COLOR_LIGHT,
        border = {width = {2, 2, 2, 2, 0}, color = COLOR_RED},
        cornerRadius = Clay_CornerRadiusAll(10),
        userData = frame_allocate_custom_data({link = Clay_MakeString("https://discord.gg/b4FTWkxdvT")})
      }) {
        Clay_Text("Discord", {
          userData = frame_allocate_custom_data({disable_pointer_events = true}),
          fontId = FONT_ID_BODY_24, fontSize = 24, textColor = {61, 26, 5, 255}}
        );
      }
      if Clay_PushUI({
        layout = {padding = {16, 16, 6, 6}},
        backgroundColor = ifx Clay_Hovered() then COLOR_LIGHT_HOVER else COLOR_LIGHT,
        border = {width = {2, 2, 2, 2, 0}, color = COLOR_RED},
        cornerRadius = Clay_CornerRadiusAll(10),
        userData = frame_allocate_custom_data({link = Clay_MakeString("https://github.com/nicbarker/clay")})
      }) {
        Clay_Text("Github", {
          userData = frame_allocate_custom_data({disable_pointer_events = true}),
          fontId = FONT_ID_BODY_24, fontSize = 24, textColor = {61, 26, 5, 255}}
        );
      }
    }
    top_border_config := Clay_LayoutConfig.{sizing = {Clay_SizingGrow(), Clay_SizingFixed(4)}};
    Clay_UI(Clay_Id("TopBorder1"), { layout = top_border_config, backgroundColor = COLOR_TOP_BORDER_5 });
    Clay_UI(Clay_Id("TopBorder2"), { layout = top_border_config, backgroundColor = COLOR_TOP_BORDER_4 });
    Clay_UI(Clay_Id("TopBorder3"), { layout = top_border_config, backgroundColor = COLOR_TOP_BORDER_3 });
    Clay_UI(Clay_Id("TopBorder4"), { layout = top_border_config, backgroundColor = COLOR_TOP_BORDER_2 });
    Clay_UI(Clay_Id("TopBorder5"), { layout = top_border_config, backgroundColor = COLOR_TOP_BORDER_1 });
    if Clay_PushUI(Clay_Id("OuterScrollContainer"), {
      layout = {sizing = {Clay_SizingGrow(), Clay_SizingGrow()}, layoutDirection = .TOP_TO_BOTTOM},
      clip = {vertical = true, childOffset = Clay_GetScrollOffset()},
      border = {width = {betweenChildren = 2}, color = COLOR_RED}
    }) {
      if mobile_screen {
        assert(false, "Mobile platforms are unsupported for this Jai demo!");
      } else {
        landing_page_desktop();
        feature_blocks_desktop();
        declarative_syntax_page_desktop();
        high_performance_page_desktop(lerp_value);
        renderer_page_desktop();
        debugger_page_desktop();
      }
    }
  }

  // NOTE: There is some extra code here regarding the canvas renderer which I am not including

  return Clay_EndLayout(0);
}

debug_mode_enabled := false;
#program_export "UpdateDrawFrame"
update_draw_frame :: (width: float, height: float, mouse_wheel_x: float, mouse_wheel_y: float, mouse_position_x: float, mouse_position_y: float, is_touch_down: bool, is_mouse_down: bool, arrow_key_down_pressed_this_frame: bool, arrow_key_up_pressed_this_frame: bool, d_key_pressed_this_frame: bool, delta_time: float) -> Clay_RenderCommandArray #c_call {
  push_context,defer_pop; // TODO: This is horrible. How to store in Javascript?
  window_width = width;
  window_height = height;
  Clay_SetLayoutDimensions({width, height});

  animation_lerp_value += delta_time;
  if animation_lerp_value > 1 animation_lerp_value -= 2;

  if d_key_pressed_this_frame {
    debug_mode_enabled = !debug_mode_enabled;
    Clay_SetDebugModeEnabled(debug_mode_enabled);
  }

  Clay_SetPointerState({mouse_position_x, mouse_position_y}, is_mouse_down || is_touch_down);

  Clay_UpdateScrollContainers(is_touch_down, {mouse_wheel_x, mouse_wheel_y}, delta_time);

  return create_layout(false, ifx animation_lerp_value < 0 then animation_lerp_value + 1 else 1 - animation_lerp_value);
}

#import "clay";

#import "Basic";
#import "Math";