// NOTE: I have no idea how we're gonna make this compile for WASM. But there's a first time for everything right?

window: WT.Window_Type;

theme: Overall_Theme;
current_theme := cast(s32) Default_Themes.Default;

#if OS != .WASM {
  main :: () {
    ctx := init();
    push_context,defer_pop ctx;

    window_width, window_height := 1280, 720;
    window = create_window(window_width, window_height, "Bean Game Website");
    Simp.set_render_target(window);

    quit := false;
    last_time := seconds_since_init();
    while !quit {
      Input.update_window_events();
      for e: Input.events_this_frame {
        if e.type == {
          case .QUIT; quit = true;
        }

        process_event(*context, e);
      }

      current_time := seconds_since_init();
      dt := cast(float, current_time - last_time);
      last_time = current_time;

      bg_col := theme.background_color;
      Simp.clear_render_target(bg_col.x, bg_col.y, bg_col.z, 1);

      x, y, width, height := get_dimensions(window, true);
      collect_render_commands(*context, width, height, cast(float) current_time, dt);

      Simp.swap_buffers(window);

      reset_temporary_storage();
    }
  }
}

#program_export
init :: () -> *#Context #c_call {
  custom_ctx: *#Context;
  push_context {
    custom_ctx = New(#Context);
    initializer_of(#Context)(custom_ctx);
  }
  push_context,defer_pop custom_ctx;

  #if OS == .WASM {
    window = true;

    web_draw_procs: Draw_Procs;
    web_draw_procs.texture_load_from_memory   = texture_load_from_memory;
    web_draw_procs.set_scissor                = set_scissor;
    web_draw_procs.clear_scissor              = clear_scissor;
    web_draw_procs.set_shader_for_color       = set_shader_for_color;
    web_draw_procs.set_shader_for_images      = set_shader_for_images;
    web_draw_procs.prepare_text               = prepare_text;
    web_draw_procs.draw_prepared_text         = draw_prepared_text;
    web_draw_procs.get_mouse_pointer_position = get_mouse_pointer_position;
    //web_draw_procs.set_pointer_image          = set_pointer_image;
    web_draw_procs.get_font_at_size           = get_font_at_size;
    web_draw_procs.immediate_triangle         = immediate_triangle;
    web_draw_procs.immediate_quad             = immediate_quad;
    web_draw_procs.immediate_flush            = immediate_flush;
    ui_init(procs = *web_draw_procs);
  } else {
    ui_init();
  }

  theme_proc := default_theme_procs[current_theme];
  theme = theme_proc();
  set_default_theme(theme);

  return *context;
}

#program_export
process_event :: (ctx: *#Context, e: Input.Event) #c_call {
  push_context,defer_pop ctx;
  getrect_handle_event(e);
}

#program_export
collect_render_commands :: (ctx: *#Context, width: s32, height: s32, current_time: float, dt: float) #c_call {
  push_context,defer_pop ctx;
  ui_per_frame_update(window, width, height, current_time);

  button_theme := theme.button_theme;
  button_theme.label_theme.alignment = .Left;
  //button_theme.font = main_font;
  r := get_rect(100, 100, 200, 100);
  button(r, "Test button", *button_theme);
}

#import "Basic";
#import "GetRect"()(Type_Indicator = GetRect_Type_Indicator);
#import "Math";
#import "String";

Input :: #import "Input"; // NOTE: Needed for Event

#if OS == .WASM {
  WT :: #import "Window_Type";

  JS_Handle :: s32;
  Web_Font :: struct {
    handle          : JS_Handle;
    // NOTE: Required fields (strange).
    em_width        : s32;
    character_height: s32;
  }

  GetRect_Type_Indicator :: struct {
    Window_Type  :: WT.Window_Type;
    Font         :: Web_Font;
    Font_Effects :: JS_Handle;
    Texture      :: JS_Handle;
  }

  #program_export
  basic_alloc :: (ctx: *#Context, size: s32) -> *void #c_call {
    push_context ctx {
      return alloc(size);
    }
  }

  // NOTE: Generate WASM stubs for `Draw_Procs`
  // TODO: Eventually this should just be inlined in this file once the kinks are worked out
  #insert -> string {
    Compiler :: #import "Compiler"; // For `get_type`
    builder: String_Builder;
    draw_procs := cast(*Type_Info_Struct) type_info(Draw_Procs);
    for member: draw_procs.members {
      if member.name == "set_pointer_image" continue;

      proc := cast(*Type_Info_Procedure) member.type;
      proc_argument_header_list: [..] string;
      proc_return\ _header_list: [..] string;
      parameter_name_list      : [..] string;
      return\  _name_list      : [..] string;

      for proc.argument_types {
        param_name := sprint("param%", it_index);
        array_add(*parameter_name_list, param_name);
        array_add(*proc_argument_header_list, sprint("%: %", param_name, Compiler.get_type(it)));
      }
      proc_argument_header := join(..proc_argument_header_list, separator = ", ");
      parameter_names := join(..parameter_name_list, separator = ", ");

      for proc.return_types {
        array_add(*proc_return_header_list, sprint("%", Compiler.get_type(it)));
      }
      proc_return_header := join(..proc_return_header_list, separator = ", ");
      if proc_return_header.count > 0 proc_return_header = sprint("-> % ", proc_return_header);

      // NOTE: JS doesn't support multiple return values, so we package them into a struct and return that
      return_struct_members: [..] string;
      return_names: string;
      if proc.return_types.count > 1 {
        for proc_return_header_list {
          return_name := sprint("ret%", it_index);
          array_add(*return_name_list, return_name);
          array_add(*return_struct_members, sprint("%: %", return_name, it));
        }
        return_names = join(..return_name_list, separator = ", ");
      }

      print_to_builder(*builder, "% :: (%) %{\n", member.name, proc_argument_header, proc_return_header);
      if proc.return_types.count > 1 {
        print_to_builder(*builder, "\tComposite_Return :: struct { %}\n", join(..return_struct_members, separator = "; ", after_last = true));
        print_to_builder(*builder, "\tweb_% :: (%) -> Composite_Return #foreign;\n", member.name, proc_argument_header, proc_return_header);
        print_to_builder(*builder, "\tusing returns := web_%(%);\n", member.name, parameter_names);
        print_to_builder(*builder, "\treturn %;\n", return_names);
      } else {
        print_to_builder(*builder, "\tweb_% :: (%) %#foreign;\n", member.name, proc_argument_header, proc_return_header);
        return_statement := ifx proc.return_types.count == 1 then "return " else "";
        print_to_builder(*builder, "\t%web_%(%);\n", return_statement, member.name, parameter_names);
      }
      print_to_builder(*builder, "}\n\n");
    }

    return builder_to_string(*builder);
  }
} else {
  GetRect_Type_Indicator :: void;

  #import "Window_Creation";
  Simp  :: #import "Simp";
}