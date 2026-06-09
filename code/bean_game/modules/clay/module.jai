// NOTE: Handwritten macro API

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

Clay_UI :: (element := Clay_ElementDeclaration.{}) -> bool #expand {
  Clay__OpenElement();
  Clay__ConfigureOpenElement(element);
  `defer Clay__CloseElement();

  return true;
}

Clay_UI :: (id: Clay_ElementId, element := Clay_ElementDeclaration.{}) -> bool #expand {
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

#load "clay_bindings.jai";