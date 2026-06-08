AT_COMPILE_TIME :: true;

#if AT_COMPILE_TIME {
  #run,stallable {
    set_build_options_dc(.{do_output=false});
    path_to_clay_source = tprint("%/clay_source/clay.h", #filepath);
    if !build_library() || !generate_bindings() {
      compiler_set_workspace_status(.FAILED);
    }
  }
}

build_library :: (target := OS) -> bool {
  extra: [..] string;
  array_add(*extra, "-DCLAY_IMPLEMENTATION");

  if target == .WINDOWS {
    array_add(*extra, "-Tc");
  } else {
    array_add(*extra, "-x", "c");
  }
  success := build_cpp_static_lib("clay", path_to_clay_source, extra = extra);

  return success;
}

generate_bindings :: () -> bool {
  opts: Generate_Bindings_Options;
  {
    using opts;

    array_add(*libraries, .{filename="clay"});
    array_add(*source_files, path_to_clay_source);

    header = HANDWRITTEN_MACRO_API_BINDINGS;

    generate_compile_time_struct_checks = false;
  }

  return generate_bindings(opts, "clay_bindings.jai");
}

#scope_file

path_to_clay_source: string;

// NOTE PascalCase consistent with rest of API
HANDWRITTEN_MACRO_API_BINDINGS :: #string EOF

// Public Macro API ------------------------

Clay_BorderOutside :: inline (width: u16) -> Clay_BorderWidth {
  return {width, width, width, width, 0};
}

Clay_BorderAll :: inline (width: u16) -> Clay_BorderWidth {
  return {width, width, width, width, width};
}

Clay_CornerRadiusAll :: inline (radius: float) -> Clay_CornerRadius {
  return {radius, radius, radius, radius};
}

Clay_PaddingAll :: inline (padding: u16) -> Clay_Padding {
  return {padding, padding, padding, padding};
}

Clay_SizingFit :: inline (minMax := Clay_SizingMinMax.{}) -> Clay_SizingAxis {
  return {size = {minMax = minMax}, type = .FIT};
}

Clay_SizingGrow :: inline (minMax := Clay_SizingMinMax.{}) -> Clay_SizingAxis {
  return {size = {minMax = minMax}, type = .GROW};
}

Clay_SizingFixed :: inline (fixedSize: float) -> Clay_SizingAxis {
  return {size = {minMax = {fixedSize, fixedSize}}, type = .FIXED};
}

Clay_SizingPercent :: inline (percentOfParent: float) -> Clay_SizingAxis {
  return {size = {percent = percentOfParent}, type = .PERCENT};
}

Clay_Id :: inline (label: string, index: u32 = 0) -> Clay_ElementId {
  return Clay__HashString(label, index);
}

Clay_IdLocal :: inline (label: string, index: u32 = 0) -> Clay_ElementId {
  return Clay__HashStringWithOffset(label, index, Clay_GetOpenElementId());
}

Clay_MakeString :: inline (str: string) -> Clay_String {
  return {chars = str.data, length = cast(u32)str.len};
}

/* NOTE
    Compared to the C usage of element macros:
    CLAY(CLAY_ID("Outer"), { .layout = { .padding = CLAY_PADDING_ALL(16) } }) {
      // Child elements here
    }

    We use `if` statements in Jai to scope child elements and obtain the same effect:
    if Clay_UI(Clay_Id("outer"), { layout = { padding = Clay_PaddingAll(16) } }) {
      // Child elements here
    }
*/

Clay_UI :: (element := Clay_ElementDeclaration.{}) #expand -> bool {
  Clay__OpenElement();
  Clay__ConfigureOpenElement(element);
  `defer Clay__CloseElement();

  return true;
}

Clay_UI :: (id: Clay_ElementId, element := Clay_ElementDeclaration.{}) #expand -> bool {
  Clay__OpenElementWidthId(id);
  Clay__ConfigureOpenElement(element);
  `defer Clay__CloseElement();

  return true;
}

Clay_Text :: inline ($$text: string, config := Clay_TextElementConfig.{}) {
  wrapped := Clay_MakeString(text);
  #if is_constant(text) then wrapped.isStaticallyAllocated = true;
  Clay__OpenTextElement(wrapped, config);
}

EOF

#import "Basic";
#import "Bindings_Generator";
#import "BuildCpp";
#import "Compiler";