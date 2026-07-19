//
// This file was auto-generated using the following command:
//
// jai modules/clay/generate.jai
//



// Note: Clay_String is not guaranteed to be null terminated. It may be if created from a literal C string,
// but it is also used to represent slices.
Clay_String :: struct {
    // Set this boolean to true if the char* data underlying this string will live for the entire lifetime of the program.
    // This will automatically be set for strings created with CLAY_STRING, as the macro requires a string literal.
    isStaticallyAllocated: bool;
    length:                s32;

    // The underlying character memory. Note: this will not be copied and will not extend the lifetime of the underlying memory.
    chars:                 *u8;
}

// Clay_StringSlice is used to represent non owning string slices, and includes
// a baseChars field which points to the string this slice is derived from.
Clay_StringSlice :: struct {
    length:    s32;
    chars:     *u8;
    baseChars: *u8; // The source string / char* that this slice was derived from
}

Clay_Context :: struct {}

// Clay_Arena is a memory arena structure that is used by clay to manage its internal allocations.
// Rather than creating it by hand, it's easier to use Clay_CreateArenaWithCapacityAndMemory()
Clay_Arena :: struct {
    nextAllocation: u64;
    capacity:       u64;
    memory:         *u8;
}

Clay_Dimensions :: struct {
    width:  float;
    height: float;
}

Clay_Vector2 :: struct {
    x: float;
    y: float;
}

// Internally clay conventionally represents colors as 0-255, but interpretation is up to the renderer.
Clay_Color :: struct {
    r: float;
    g: float;
    b: float;
    a: float;
}

Clay_BoundingBox :: struct {
    x:      float;
    y:      float;
    width:  float;
    height: float;
}

// Primarily created via the CLAY_ID(), CLAY_IDI(), CLAY_ID_LOCAL() and CLAY_IDI_LOCAL() macros.
// Represents a hashed string ID used for identifying and finding specific clay UI elements, required
// by functions such as Clay_PointerOver() and Clay_GetElementData().
Clay_ElementId :: struct {
    id:       u32; // The resulting hash generated from the other fields.
    offset:   u32; // A numerical offset applied after computing the hash from stringId.
    baseId:   u32; // A base hash value to start from, for example the parent element ID is used when calculating CLAY_ID_LOCAL().
    stringId: Clay_String; // The string id to hash.
}

// A sized array of Clay_ElementId.
Clay_ElementIdArray :: struct {
    capacity:      s32;
    length:        s32;
    internalArray: *Clay_ElementId;
}

// Controls the "radius", or corner rounding of elements, including rectangles, borders and images.
// The rounding is determined by drawing a circle inset into the element corner by (radius, radius) pixels.
Clay_CornerRadius :: struct {
    topLeft:     float;
    topRight:    float;
    bottomLeft:  float;
    bottomRight: float;
}

// Controls the direction in which child elements will be automatically laid out.
Clay_LayoutDirection :: enum s32 {
    LEFT_TO_RIGHT :: 0;

    TOP_TO_BOTTOM :: 1;

    CLAY_LEFT_TO_RIGHT :: LEFT_TO_RIGHT;

    CLAY_TOP_TO_BOTTOM :: TOP_TO_BOTTOM;
}

// Controls the alignment along the x axis (horizontal) of child elements.
Clay_LayoutAlignmentX :: enum s32 {
    LEFT   :: 0;

    RIGHT  :: 1;

    CENTER :: 2;

    CLAY_ALIGN_X_LEFT   :: LEFT;

    CLAY_ALIGN_X_RIGHT  :: RIGHT;

    CLAY_ALIGN_X_CENTER :: CENTER;
}

// Controls the alignment along the y axis (vertical) of child elements.
Clay_LayoutAlignmentY :: enum s32 {
    TOP    :: 0;

    BOTTOM :: 1;

    CENTER :: 2;

    CLAY_ALIGN_Y_TOP    :: TOP;

    CLAY_ALIGN_Y_BOTTOM :: BOTTOM;

    CLAY_ALIGN_Y_CENTER :: CENTER;
}

// Controls how the element takes up space inside its parent container.
Clay__SizingType :: enum s32 {
    FIT     :: 0;

    GROW    :: 1;

    PERCENT :: 2;

    FIXED   :: 3;

    CLAY__SIZING_TYPE_FIT     :: FIT;

    CLAY__SIZING_TYPE_GROW    :: GROW;

    CLAY__SIZING_TYPE_PERCENT :: PERCENT;

    CLAY__SIZING_TYPE_FIXED   :: FIXED;
}

// Controls how child elements are aligned on each axis.
Clay_ChildAlignment :: struct {
    x: Clay_LayoutAlignmentX; // Controls alignment of children along the x axis.
    y: Clay_LayoutAlignmentY; // Controls alignment of children along the y axis.
}

// Controls the minimum and maximum size in pixels that this element is allowed to grow or shrink to,
// overriding sizing types such as FIT or GROW.
Clay_SizingMinMax :: struct {
    min: float; // The smallest final size of the element on this axis will be this value in pixels.
    max: float; // The largest final size of the element on this axis will be this value in pixels.
}

// Controls the sizing of this element along one axis inside its parent container.
Clay_SizingAxis :: struct {
    size: struct {
        minMax: Clay_SizingMinMax; // Controls the minimum and maximum size in pixels that this element is allowed to grow or shrink to, overriding sizing types such as FIT or GROW.

        #overlay (minMax) percent: float; // Expects 0-1 range. Clamps the axis size to a percent of the parent container's axis size minus padding and child gaps.
    };

    type: Clay__SizingType; // Controls how the element takes up space inside its parent container.
}

// Controls the sizing of this element along one axis inside its parent container.
Clay_Sizing :: struct {
    width:  Clay_SizingAxis; // Controls the width sizing of the element, along the x axis.
    height: Clay_SizingAxis; // Controls the height sizing of the element, along the y axis.
}

// Controls "padding" in pixels, which is a gap between the bounding box of this element and where its children
// will be placed.
Clay_Padding :: struct {
    left:   u16;
    right:  u16;
    top:    u16;
    bottom: u16;
}

Clay__Clay_PaddingWrapper :: struct {
    wrapped: Clay_Padding;
}

// Controls various settings that affect the size and position of an element, as well as the sizes and positions
// of any child elements.
Clay_LayoutConfig :: struct {
    sizing:          Clay_Sizing; // Controls the sizing of this element inside it's parent container, including FIT, GROW, PERCENT and FIXED sizing.
    padding:         Clay_Padding; // Controls "padding" in pixels, which is a gap between the bounding box of this element and where its children will be placed.
    childGap:        u16; // Controls the gap in pixels between child elements along the layout axis (horizontal gap for LEFT_TO_RIGHT, vertical gap for TOP_TO_BOTTOM).
    childAlignment:  Clay_ChildAlignment; // Controls how child elements are aligned on each axis.
    layoutDirection: Clay_LayoutDirection; // Controls the direction in which child elements will be automatically laid out.
}

Clay__Clay_LayoutConfigWrapper :: struct {
    wrapped: Clay_LayoutConfig;
}

CLAY_LAYOUT_DEFAULT: Clay_LayoutConfig #elsewhere clay;

// Controls how text "wraps", that is how it is broken into multiple lines when there is insufficient horizontal space.
Clay_TextElementConfigWrapMode :: enum s32 {
    WORDS    :: 0;

    NEWLINES :: 1;

    NONE     :: 2;

    CLAY_TEXT_WRAP_WORDS    :: WORDS;

    CLAY_TEXT_WRAP_NEWLINES :: NEWLINES;

    CLAY_TEXT_WRAP_NONE     :: NONE;
}

// Controls how wrapped lines of text are horizontally aligned within the outer text bounding box.
Clay_TextAlignment :: enum s32 {
    LEFT   :: 0;

    CENTER :: 1;

    RIGHT  :: 2;

    CLAY_TEXT_ALIGN_LEFT   :: LEFT;

    CLAY_TEXT_ALIGN_CENTER :: CENTER;

    CLAY_TEXT_ALIGN_RIGHT  :: RIGHT;
}

// Controls various functionality related to text elements.
Clay_TextElementConfig :: struct {
    // A pointer that will be transparently passed through to the resulting render command.
    userData:      *void;

    // The RGBA color of the font to render, conventionally specified as 0-255.
    textColor:     Clay_Color;

    // An integer transparently passed to Clay_MeasureText to identify the font to use.
    // The debug view will pass fontId = 0 for its internal text.
    fontId:        u16;

    // Controls the size of the font. Handled by the function provided to Clay_MeasureText.
    fontSize:      u16;

    // Controls extra horizontal spacing between characters. Handled by the function provided to Clay_MeasureText.
    letterSpacing: u16;

    // Controls additional vertical space between wrapped lines of text.
    lineHeight:    u16;

    // Controls how text "wraps", that is how it is broken into multiple lines when there is insufficient horizontal space.
    // CLAY_TEXT_WRAP_WORDS (default) breaks on whitespace characters.
    // CLAY_TEXT_WRAP_NEWLINES doesn't break on space characters, only on newlines.
    // CLAY_TEXT_WRAP_NONE disables wrapping entirely.
    wrapMode:      Clay_TextElementConfigWrapMode;

    // Controls how wrapped lines of text are horizontally aligned within the outer text bounding box.
    // CLAY_TEXT_ALIGN_LEFT (default) - Horizontally aligns wrapped lines of text to the left hand side of their bounding box.
    // CLAY_TEXT_ALIGN_CENTER - Horizontally aligns wrapped lines of text to the center of their bounding box.
    // CLAY_TEXT_ALIGN_RIGHT - Horizontally aligns wrapped lines of text to the right hand side of their bounding box.
    textAlignment: Clay_TextAlignment;
}

Clay__Clay_TextElementConfigWrapper :: struct {
    wrapped: Clay_TextElementConfig;
}

// Controls various settings related to aspect ratio scaling element.
Clay_AspectRatioElementConfig :: struct {
    aspectRatio: float; // A float representing the target "Aspect ratio" for an element, which is its final width divided by its final height.
}

Clay__Clay_AspectRatioElementConfigWrapper :: struct {
    wrapped: Clay_AspectRatioElementConfig;
}

// Controls various settings related to image elements.
Clay_ImageElementConfig :: struct {
    imageData: *void; // A transparent pointer used to pass image data through to the renderer.
}

Clay__Clay_ImageElementConfigWrapper :: struct {
    wrapped: Clay_ImageElementConfig;
}

// Controls where a floating element is offset relative to its parent element.
// Note: see https://github.com/user-attachments/assets/b8c6dfaa-c1b1-41a4-be55-013473e4a6ce for a visual explanation.
Clay_FloatingAttachPointType :: enum s32 {
    LEFT_TOP      :: 0;
    LEFT_CENTER   :: 1;
    LEFT_BOTTOM   :: 2;
    CENTER_TOP    :: 3;
    CENTER_CENTER :: 4;
    CENTER_BOTTOM :: 5;
    RIGHT_TOP     :: 6;
    RIGHT_CENTER  :: 7;
    RIGHT_BOTTOM  :: 8;

    CLAY_ATTACH_POINT_LEFT_TOP      :: LEFT_TOP;
    CLAY_ATTACH_POINT_LEFT_CENTER   :: LEFT_CENTER;
    CLAY_ATTACH_POINT_LEFT_BOTTOM   :: LEFT_BOTTOM;
    CLAY_ATTACH_POINT_CENTER_TOP    :: CENTER_TOP;
    CLAY_ATTACH_POINT_CENTER_CENTER :: CENTER_CENTER;
    CLAY_ATTACH_POINT_CENTER_BOTTOM :: CENTER_BOTTOM;
    CLAY_ATTACH_POINT_RIGHT_TOP     :: RIGHT_TOP;
    CLAY_ATTACH_POINT_RIGHT_CENTER  :: RIGHT_CENTER;
    CLAY_ATTACH_POINT_RIGHT_BOTTOM  :: RIGHT_BOTTOM;
}

// Controls where a floating element is offset relative to its parent element.
Clay_FloatingAttachPoints :: struct {
    element: Clay_FloatingAttachPointType; // Controls the origin point on a floating element that attaches to its parent.
    parent:  Clay_FloatingAttachPointType; // Controls the origin point on the parent element that the floating element attaches to.
}

// Controls how mouse pointer events like hover and click are captured or passed through to elements underneath a floating element.
Clay_PointerCaptureMode :: enum s32 {
    CAPTURE     :: 0;

    PASSTHROUGH :: 1;

    CLAY_POINTER_CAPTURE_MODE_CAPTURE     :: CAPTURE;

    CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH :: PASSTHROUGH;
}

// Controls which element a floating element is "attached" to (i.e. relative offset from).
Clay_FloatingAttachToElement :: enum s32 {
    NONE            :: 0;

    PARENT          :: 1;

    ELEMENT_WITH_ID :: 2;

    ROOT            :: 3;

    CLAY_ATTACH_TO_NONE            :: NONE;

    CLAY_ATTACH_TO_PARENT          :: PARENT;

    CLAY_ATTACH_TO_ELEMENT_WITH_ID :: ELEMENT_WITH_ID;

    CLAY_ATTACH_TO_ROOT            :: ROOT;
}

// Controls whether or not a floating element is clipped to the same clipping rectangle as the element it's attached to.
Clay_FloatingClipToElement :: enum s32 {
    NONE            :: 0;

    ATTACHED_PARENT :: 1;

    CLAY_CLIP_TO_NONE            :: NONE;

    CLAY_CLIP_TO_ATTACHED_PARENT :: ATTACHED_PARENT;
}

// Controls various settings related to "floating" elements, which are elements that "float" above other elements, potentially overlapping their boundaries,
// and not affecting the layout of sibling or parent elements.
Clay_FloatingElementConfig :: struct {
    // Offsets this floating element by the provided x,y coordinates from its attachPoints.
    offset:             Clay_Vector2;

    // Expands the boundaries of the outer floating element without affecting its children.
    expand:             Clay_Dimensions;

    // When used in conjunction with .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID, attaches this floating element to the element in the hierarchy with the provided ID.
    // Hint: attach the ID to the other element with .id = CLAY_ID("yourId"), and specify the id the same way, with .parentId = CLAY_ID("yourId").id
    parentId:           u32;

    // Controls the z index of this floating element and all its children. Floating elements are sorted in ascending z order before output.
    // zIndex is also passed to the renderer for all elements contained within this floating element.
    zIndex:             s16;

    // Controls how mouse pointer events like hover and click are captured or passed through to elements underneath / behind a floating element.
    // Enum is of the form CLAY_ATTACH_POINT_foo_bar. See Clay_FloatingAttachPoints for more details.
    // Note: see <img src="https://github.com/user-attachments/assets/b8c6dfaa-c1b1-41a4-be55-013473e4a6ce />
    // and <img src="https://github.com/user-attachments/assets/ebe75e0d-1904-46b0-982d-418f929d1516 /> for a visual explanation.
    attachPoints:       Clay_FloatingAttachPoints;

    // Controls how mouse pointer events like hover and click are captured or passed through to elements underneath a floating element.
    // CLAY_POINTER_CAPTURE_MODE_CAPTURE (default) - "Capture" the pointer event and don't allow events like hover and click to pass through to elements underneath.
    // CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH - Transparently pass through pointer events like hover and click to elements underneath the floating element.
    pointerCaptureMode: Clay_PointerCaptureMode;

    // Controls which element a floating element is "attached" to (i.e. relative offset from).
    // CLAY_ATTACH_TO_NONE (default) - Disables floating for this element.
    // CLAY_ATTACH_TO_PARENT - Attaches this floating element to its parent, positioned based on the .attachPoints and .offset fields.
    // CLAY_ATTACH_TO_ELEMENT_WITH_ID - Attaches this floating element to an element with a specific ID, specified with the .parentId field. positioned based on the .attachPoints and .offset fields.
    // CLAY_ATTACH_TO_ROOT - Attaches this floating element to the root of the layout, which combined with the .offset field provides functionality similar to "absolute positioning".
    attachTo:           Clay_FloatingAttachToElement;

    // Controls whether or not a floating element is clipped to the same clipping rectangle as the element it's attached to.
    // CLAY_CLIP_TO_NONE (default) - The floating element does not inherit clipping.
    // CLAY_CLIP_TO_ATTACHED_PARENT - The floating element is clipped to the same clipping rectangle as the element it's attached to.
    clipTo:             Clay_FloatingClipToElement;
}

Clay__Clay_FloatingElementConfigWrapper :: struct {
    wrapped: Clay_FloatingElementConfig;
}

// Controls various settings related to custom elements.
Clay_CustomElementConfig :: struct {
    // A transparent pointer through which you can pass custom data to the renderer.
    // Generates CUSTOM render commands.
    customData: *void;
}

Clay__Clay_CustomElementConfigWrapper :: struct {
    wrapped: Clay_CustomElementConfig;
}

// Controls the axis on which an element switches to "scrolling", which clips the contents and allows scrolling in that direction.
Clay_ClipElementConfig :: struct {
    horizontal:  bool; // Clip overflowing elements on the X axis.
    vertical:    bool; // Clip overflowing elements on the Y axis.
    childOffset: Clay_Vector2; // Offsets the x,y positions of all child elements. Used primarily for scrolling containers.
}

Clay__Clay_ClipElementConfigWrapper :: struct {
    wrapped: Clay_ClipElementConfig;
}

// Controls the widths of individual element borders.
Clay_BorderWidth :: struct {
    left:            u16;
    right:           u16;
    top:             u16;
    bottom:          u16;

    // Creates borders between each child element, depending on the .layoutDirection.
    // e.g. for LEFT_TO_RIGHT, borders will be vertical lines, and for TOP_TO_BOTTOM borders will be horizontal lines.
    // .betweenChildren borders will result in individual RECTANGLE render commands being generated.
    betweenChildren: u16;
}

// Controls settings related to element borders.
Clay_BorderElementConfig :: struct {
    color: Clay_Color; // Controls the color of all borders with width > 0. Conventionally represented as 0-255, but interpretation is up to the renderer.
    width: Clay_BorderWidth; // Controls the widths of individual borders. At least one of these should be > 0 for a BORDER render command to be generated.
}

Clay__Clay_BorderElementConfigWrapper :: struct {
    wrapped: Clay_BorderElementConfig;
}

Clay_TransitionData :: struct {
    boundingBox:     Clay_BoundingBox;
    backgroundColor: Clay_Color;
    overlayColor:    Clay_Color;
    borderColor:     Clay_Color;
    borderWidth:     Clay_BorderWidth;
}

Clay_TransitionState :: enum s32 {
    IDLE          :: 0;
    ENTERING      :: 1;
    TRANSITIONING :: 2;
    EXITING       :: 3;

    CLAY_TRANSITION_STATE_IDLE          :: IDLE;
    CLAY_TRANSITION_STATE_ENTERING      :: ENTERING;
    CLAY_TRANSITION_STATE_TRANSITIONING :: TRANSITIONING;
    CLAY_TRANSITION_STATE_EXITING       :: EXITING;
}

Clay_TransitionProperty :: enum s32 {
    NONE             :: 0;
    X                :: 1;
    Y                :: 2;
    POSITION         :: 3;
    WIDTH            :: 4;
    HEIGHT           :: 8;
    DIMENSIONS       :: 12;
    BOUNDING_BOX     :: 15;
    BACKGROUND_COLOR :: 16;
    OVERLAY_COLOR    :: 32;
    CORNER_RADIUS    :: 64;
    BORDER_COLOR     :: 128;
    BORDER_WIDTH     :: 256;
    BORDER           :: 384;

    CLAY_TRANSITION_PROPERTY_NONE             :: NONE;
    CLAY_TRANSITION_PROPERTY_X                :: X;
    CLAY_TRANSITION_PROPERTY_Y                :: Y;
    CLAY_TRANSITION_PROPERTY_POSITION         :: POSITION;
    CLAY_TRANSITION_PROPERTY_WIDTH            :: WIDTH;
    CLAY_TRANSITION_PROPERTY_HEIGHT           :: HEIGHT;
    CLAY_TRANSITION_PROPERTY_DIMENSIONS       :: DIMENSIONS;
    CLAY_TRANSITION_PROPERTY_BOUNDING_BOX     :: BOUNDING_BOX;
    CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR :: BACKGROUND_COLOR;
    CLAY_TRANSITION_PROPERTY_OVERLAY_COLOR    :: OVERLAY_COLOR;
    CLAY_TRANSITION_PROPERTY_CORNER_RADIUS    :: CORNER_RADIUS;
    CLAY_TRANSITION_PROPERTY_BORDER_COLOR     :: BORDER_COLOR;
    CLAY_TRANSITION_PROPERTY_BORDER_WIDTH     :: BORDER_WIDTH;
    CLAY_TRANSITION_PROPERTY_BORDER           :: BORDER;
}

Clay_TransitionCallbackArguments :: struct {
    transitionState: Clay_TransitionState;
    initial:         Clay_TransitionData;
    current:         *Clay_TransitionData;
    target:          Clay_TransitionData;
    elapsedTime:     float;
    duration:        float;
    properties:      Clay_TransitionProperty;
}

Clay_TransitionEnterTriggerType :: enum s32 {
    SKIP_ON_FIRST_PARENT_FRAME    :: 0;
    TRIGGER_ON_FIRST_PARENT_FRAME :: 1;

    CLAY_TRANSITION_ENTER_SKIP_ON_FIRST_PARENT_FRAME    :: SKIP_ON_FIRST_PARENT_FRAME;
    CLAY_TRANSITION_ENTER_TRIGGER_ON_FIRST_PARENT_FRAME :: TRIGGER_ON_FIRST_PARENT_FRAME;
}

Clay_TransitionExitTriggerType :: enum s32 {
    SKIP_WHEN_PARENT_EXITS    :: 0;
    TRIGGER_WHEN_PARENT_EXITS :: 1;

    CLAY_TRANSITION_EXIT_SKIP_WHEN_PARENT_EXITS    :: SKIP_WHEN_PARENT_EXITS;
    CLAY_TRANSITION_EXIT_TRIGGER_WHEN_PARENT_EXITS :: TRIGGER_WHEN_PARENT_EXITS;
}

Clay_TransitionInteractionHandlingType :: enum s32 {
    DISABLE_INTERACTIONS_WHILE_TRANSITIONING_POSITION :: 0;
    ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION   :: 1;

    CLAY_TRANSITION_DISABLE_INTERACTIONS_WHILE_TRANSITIONING_POSITION :: DISABLE_INTERACTIONS_WHILE_TRANSITIONING_POSITION;
    CLAY_TRANSITION_ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION   :: ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION;
}

Clay_ExitTransitionSiblingOrdering :: enum s32 {
    UNDERNEATH_SIBLINGS :: 0;
    NATURAL_ORDER       :: 1;
    ABOVE_SIBLINGS      :: 2;

    CLAY_EXIT_TRANSITION_ORDERING_UNDERNEATH_SIBLINGS :: UNDERNEATH_SIBLINGS;
    CLAY_EXIT_TRANSITION_ORDERING_NATURAL_ORDER       :: NATURAL_ORDER;
    CLAY_EXIT_TRANSITION_ORDERING_ABOVE_SIBLINGS      :: ABOVE_SIBLINGS;
}

// Controls settings related to transitions
Clay_TransitionElementConfig :: struct {
    handler:             #type (arguments: Clay_TransitionCallbackArguments) -> bool #c_call;
    duration:            float;
    properties:          Clay_TransitionProperty;
    interactionHandling: Clay_TransitionInteractionHandlingType;
    enter:               struct {
        setInitialState: #type (targetState: Clay_TransitionData, properties: Clay_TransitionProperty) -> Clay_TransitionData #c_call;
        trigger:         Clay_TransitionEnterTriggerType;
    };

    exit:                struct {
        setFinalState:   #type (initialState: Clay_TransitionData, properties: Clay_TransitionProperty) -> Clay_TransitionData #c_call;
        trigger:         Clay_TransitionExitTriggerType;
        siblingOrdering: Clay_ExitTransitionSiblingOrdering;
    };
}

Clay__Clay_TransitionElementConfigWrapper :: struct {
    wrapped: Clay_TransitionElementConfig;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_TEXT
Clay_TextRenderData :: struct {
    // A string slice containing the text to be rendered.
    // Note: this is not guaranteed to be null terminated.
    stringContents: Clay_StringSlice;

    // Conventionally represented as 0-255 for each channel, but interpretation is up to the renderer.
    textColor:      Clay_Color;

    // An integer representing the font to use to render this text, transparently passed through from the text declaration.
    fontId:         u16;
    fontSize:       u16;

    // Specifies the extra whitespace gap in pixels between each character.
    letterSpacing:  u16;

    // The height of the bounding box for this line of text.
    lineHeight:     u16;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE
Clay_RectangleRenderData :: struct {
    // The solid background color to fill this rectangle with. Conventionally represented as 0-255 for each channel, but interpretation is up to the renderer.
    backgroundColor: Clay_Color;

    // Controls the "radius", or corner rounding of elements, including rectangles, borders and images.
    // The rounding is determined by drawing a circle inset into the element corner by (radius, radius) pixels.
    cornerRadius:    Clay_CornerRadius;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE
Clay_ImageRenderData :: struct {
    // The tint color for this image. Note that the default value is 0,0,0,0 and should likely be interpreted
    // as "untinted".
    // Conventionally represented as 0-255 for each channel, but interpretation is up to the renderer.
    backgroundColor: Clay_Color;

    // Controls the "radius", or corner rounding of this image.
    // The rounding is determined by drawing a circle inset into the element corner by (radius, radius) pixels.
    cornerRadius:    Clay_CornerRadius;

    // A pointer transparently passed through from the original element definition, typically used to represent image data.
    imageData:       *void;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM
Clay_CustomRenderData :: struct {
    // Passed through from .backgroundColor in the original element declaration.
    // Conventionally represented as 0-255 for each channel, but interpretation is up to the renderer.
    backgroundColor: Clay_Color;

    // Controls the "radius", or corner rounding of this custom element.
    // The rounding is determined by drawing a circle inset into the element corner by (radius, radius) pixels.
    cornerRadius:    Clay_CornerRadius;

    // A pointer transparently passed through from the original element definition.
    customData:      *void;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START || commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END
Clay_ClipRenderData :: struct {
    horizontal: bool;
    vertical:   bool;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START || commandType == CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END
Clay_OverlayColorRenderData :: struct {
    color: Clay_Color;
}

// Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_BORDER
Clay_BorderRenderData :: struct {
    // Controls a shared color for all this element's borders.
    // Conventionally represented as 0-255 for each channel, but interpretation is up to the renderer.
    color:        Clay_Color;

    // Specifies the "radius", or corner rounding of this border element.
    // The rounding is determined by drawing a circle inset into the element corner by (radius, radius) pixels.
    cornerRadius: Clay_CornerRadius;

    // Controls individual border side widths.
    width:        Clay_BorderWidth;
}

// A struct union containing data specific to this command's .commandType
Clay_RenderData :: struct {
    // largest member, moved to the start of the struct/union to support #overlay. // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_TEXT
    text: Clay_TextRenderData;

    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE
rectangle:    Clay_RectangleRenderData;
    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE
image:        Clay_ImageRenderData;
    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM
custom:       Clay_CustomRenderData;
    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_BORDER
border:       Clay_BorderRenderData;
    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START|END
clip:         Clay_ClipRenderData;
    #overlay (text) // Render command data when commandType == CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START|END
overlayColor: Clay_OverlayColorRenderData;
}

// Data representing the current internal state of a scrolling element.
Clay_ScrollContainerData :: struct {
    // Note: This is a pointer to the real internal scroll position, mutating it may cause a change in final layout.
    // Intended for use with external functionality that modifies scroll position, such as scroll bars or auto scrolling.
    scrollPosition:            *Clay_Vector2;

    // The bounding box of the scroll element.
    scrollContainerDimensions: Clay_Dimensions;

    // The outer dimensions of the inner scroll container content, including the padding of the parent scroll container.
    contentDimensions:         Clay_Dimensions;

    // The config that was originally passed to the clip element.
    config:                    Clay_ClipElementConfig;

    // Indicates whether an actual scroll container matched the provided ID or if the default struct was returned.
    found:                     bool;
}

// Bounding box and other data for a specific UI element.
Clay_ElementData :: struct {
    // The rectangle that encloses this UI element, with the position relative to the root of the layout.
    boundingBox: Clay_BoundingBox;

    // Indicates whether an actual Element matched the provided ID or if the default struct was returned.
    found:       bool;
}

// Used by renderers to determine specific handling for each render command.
Clay_RenderCommandType :: enum s32 {
    NONE                :: 0;

    RECTANGLE           :: 1;

    BORDER              :: 2;

    TEXT                :: 3;

    IMAGE               :: 4;

    SCISSOR_START       :: 5;

    SCISSOR_END         :: 6;

    OVERLAY_COLOR_START :: 7;

    OVERLAY_COLOR_END   :: 8;

    CUSTOM              :: 9;

    CLAY_RENDER_COMMAND_TYPE_NONE                :: NONE;

    CLAY_RENDER_COMMAND_TYPE_RECTANGLE           :: RECTANGLE;

    CLAY_RENDER_COMMAND_TYPE_BORDER              :: BORDER;

    CLAY_RENDER_COMMAND_TYPE_TEXT                :: TEXT;

    CLAY_RENDER_COMMAND_TYPE_IMAGE               :: IMAGE;

    CLAY_RENDER_COMMAND_TYPE_SCISSOR_START       :: SCISSOR_START;

    CLAY_RENDER_COMMAND_TYPE_SCISSOR_END         :: SCISSOR_END;

    CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START :: OVERLAY_COLOR_START;

    CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END   :: OVERLAY_COLOR_END;

    CLAY_RENDER_COMMAND_TYPE_CUSTOM              :: CUSTOM;
}

Clay_RenderCommand :: struct {
    // A rectangular box that fully encloses this UI element, with the position relative to the root of the layout.
    boundingBox: Clay_BoundingBox;

    // A struct union containing data specific to this command's commandType.
    renderData:  Clay_RenderData;

    // A pointer transparently passed through from the original element declaration.
    userData:    *void;

    // The id of this element, transparently passed through from the original element declaration.
    id:          u32;

    // The z order required for drawing this command correctly.
    // Note: the render command array is already sorted in ascending order, and will produce correct results if drawn in naive order.
    // This field is intended for use in batching renderers for improved performance.
    zIndex:      s16;

    // Specifies how to handle rendering of this command.
    // CLAY_RENDER_COMMAND_TYPE_RECTANGLE - The renderer should draw a solid color rectangle.
    // CLAY_RENDER_COMMAND_TYPE_BORDER - The renderer should draw a colored border inset into the bounding box.
    // CLAY_RENDER_COMMAND_TYPE_TEXT - The renderer should draw text.
    // CLAY_RENDER_COMMAND_TYPE_IMAGE - The renderer should draw an image.
    // CLAY_RENDER_COMMAND_TYPE_SCISSOR_START - The renderer should begin clipping all future draw commands, only rendering content that falls within the provided boundingBox.
    // CLAY_RENDER_COMMAND_TYPE_SCISSOR_END - The renderer should finish any previously active clipping, and begin rendering elements in full again.
    // CLAY_RENDER_COMMAND_TYPE_CUSTOM - The renderer should provide a custom implementation for handling this render command based on its .customData
    commandType: Clay_RenderCommandType;
}

// A sized array of render commands.
Clay_RenderCommandArray :: struct {
    // The underlying max capacity of the array, not necessarily all initialized.
    capacity:      s32;

    // The number of initialized elements in this array. Used for loops and iteration.
    length:        s32;

    // A pointer to the first element in the internal array.
    internalArray: *Clay_RenderCommand;
}

// Represents the current state of interaction with clay this frame.
Clay_PointerDataInteractionState :: enum s32 {
    PRESSED_THIS_FRAME  :: 0;

    PRESSED             :: 1;

    RELEASED_THIS_FRAME :: 2;

    RELEASED            :: 3;

    CLAY_POINTER_DATA_PRESSED_THIS_FRAME  :: PRESSED_THIS_FRAME;

    CLAY_POINTER_DATA_PRESSED             :: PRESSED;

    CLAY_POINTER_DATA_RELEASED_THIS_FRAME :: RELEASED_THIS_FRAME;

    CLAY_POINTER_DATA_RELEASED            :: RELEASED;
}

// Information on the current state of pointer interactions this frame.
Clay_PointerData :: struct {
    // The position of the mouse / touch / pointer relative to the root of the layout.
    position: Clay_Vector2;

    // Represents the current state of interaction with clay this frame.
    // CLAY_POINTER_DATA_PRESSED_THIS_FRAME - A left mouse click, or touch occurred this frame.
    // CLAY_POINTER_DATA_PRESSED - The left mouse button click or touch happened at some point in the past, and is still currently held down this frame.
    // CLAY_POINTER_DATA_RELEASED_THIS_FRAME - The left mouse button click or touch was released this frame.
    // CLAY_POINTER_DATA_RELEASED - The left mouse button click or touch is not currently down / was released at some point in the past.
    state:    Clay_PointerDataInteractionState;
}

Clay_ElementDeclaration :: struct {
    // Controls various settings that affect the size and position of an element, as well as the sizes and positions of any child elements.
    layout:          Clay_LayoutConfig;

    // Controls the background color of the resulting element.
    // By convention specified as 0-255, but interpretation is up to the renderer.
    // If no other config is specified, .backgroundColor will generate a RECTANGLE render command, otherwise it will be passed as a property to IMAGE or CUSTOM render commands.
    backgroundColor: Clay_Color;

    // Perform an image editing style "Color Overlay" on this element and all its children, equivalent to
    // glsl mix(elementColor, overlayColor.rgb, overlayColor.a)
    overlayColor:    Clay_Color;

    // Controls the "radius", or corner rounding of elements, including rectangles, borders and images.
    cornerRadius:    Clay_CornerRadius;

    // Controls settings related to aspect ratio scaling.
    aspectRatio:     Clay_AspectRatioElementConfig;

    // Controls settings related to image elements.
    image:           Clay_ImageElementConfig;

    // Controls whether and how an element "floats", which means it layers over the top of other elements in z order, and doesn't affect the position and size of siblings or parent elements.
    // Note: in order to activate floating, .floating.attachTo must be set to something other than the default value.
    floating:        Clay_FloatingElementConfig;

    // Used to create CUSTOM render commands, usually to render element types not supported by Clay.
    custom:          Clay_CustomElementConfig;

    // Controls whether an element should clip its contents, as well as providing child x,y offset configuration for scrolling.
    clip:            Clay_ClipElementConfig;

    // Controls settings related to element borders, and will generate BORDER render commands.
    border:          Clay_BorderElementConfig;
    transition:      Clay_TransitionElementConfig;

    // A pointer that will be transparently passed through to resulting render commands.
    userData:        *void;
}

Clay__Clay_ElementDeclarationWrapper :: struct {
    wrapped: Clay_ElementDeclaration;
}

// Represents the type of error clay encountered while computing layout.
Clay_ErrorType :: enum s32 {
    TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED :: 0;

    ARENA_CAPACITY_EXCEEDED                :: 1;

    ELEMENTS_CAPACITY_EXCEEDED             :: 2;

    TEXT_MEASUREMENT_CAPACITY_EXCEEDED     :: 3;

    DUPLICATE_ID                           :: 4;

    FLOATING_CONTAINER_PARENT_NOT_FOUND    :: 5;

    PERCENTAGE_OVER_1                      :: 6;

    INTERNAL_ERROR                         :: 7;

    UNBALANCED_OPEN_CLOSE                  :: 8;
    HASH_MAP_CAPACITY_EXCEEDED             :: 9;

    CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED :: TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED;

    CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED                :: ARENA_CAPACITY_EXCEEDED;

    CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED             :: ELEMENTS_CAPACITY_EXCEEDED;

    CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED     :: TEXT_MEASUREMENT_CAPACITY_EXCEEDED;

    CLAY_ERROR_TYPE_DUPLICATE_ID                           :: DUPLICATE_ID;

    CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND    :: FLOATING_CONTAINER_PARENT_NOT_FOUND;

    CLAY_ERROR_TYPE_PERCENTAGE_OVER_1                      :: PERCENTAGE_OVER_1;

    CLAY_ERROR_TYPE_INTERNAL_ERROR                         :: INTERNAL_ERROR;

    CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE                  :: UNBALANCED_OPEN_CLOSE;
    CLAY_ERROR_TYPE_HASH_MAP_CAPACITY_EXCEEDED             :: HASH_MAP_CAPACITY_EXCEEDED;
}

// Data to identify the error that clay has encountered.
Clay_ErrorData :: struct {
    // Represents the type of error clay encountered while computing layout.
    // CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED - A text measurement function wasn't provided using Clay_SetMeasureTextFunction(), or the provided function was null.
    // CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED - Clay attempted to allocate its internal data structures but ran out of space. The arena passed to Clay_Initialize was created with a capacity smaller than that required by Clay_MinMemorySize().
    // CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED - Clay ran out of capacity in its internal array for storing elements. This limit can be increased with Clay_SetMaxElementCount().
    // CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED - Clay ran out of capacity in its internal array for storing elements. This limit can be increased with Clay_SetMaxMeasureTextCacheWordCount().
    // CLAY_ERROR_TYPE_DUPLICATE_ID - Two elements were declared with exactly the same ID within one layout.
    // CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND - A floating element was declared using CLAY_ATTACH_TO_ELEMENT_ID and either an invalid .parentId was provided or no element with the provided .parentId was found.
    // CLAY_ERROR_TYPE_PERCENTAGE_OVER_1 - An element was declared that using CLAY_SIZING_PERCENT but the percentage value was over 1. Percentage values are expected to be in the 0-1 range.
    // CLAY_ERROR_TYPE_INTERNAL_ERROR - Clay encountered an internal error. It would be wonderful if you could report this so we can fix it!
    // CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE - Clay__OpenElement was called more times than Clay__CloseElement, so there were still remaining open elements when the layout ended.
    // CLAY_ERROR_TYPE_HASH_MAP_CAPACITY_EXCEEDED - Clay ran out of capacity in its internal hash map for storing element IDs -> elements. This limit can be increased with Clay_SetMaxElementCount().
    errorType: Clay_ErrorType;

    // A string containing human-readable error text that explains the error in more detail.
    errorText: Clay_String;

    // A transparent pointer passed through from when the error handler was first provided.
    userData:  *void;
}

// A wrapper struct around Clay's error handler function.
Clay_ErrorHandler :: struct {
    // A user provided function to call when Clay encounters an error during layout.
    errorHandlerFunction: #type (errorText: Clay_ErrorData) -> void #c_call;

    // A pointer that will be transparently passed through to the error handler when it is called.
    userData:             *void;
}

// Returns the size, in bytes, of the minimum amount of memory Clay requires to operate at its current settings.
Clay_MinMemorySize :: () -> u32 #foreign clay;

// Creates an arena for clay to use for its internal allocations, given a certain capacity in bytes and a pointer to an allocation of at least that size.
// Intended to be used with Clay_MinMemorySize in the following way:
// uint32_t minMemoryRequired = Clay_MinMemorySize();
// Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(minMemoryRequired, malloc(minMemoryRequired));
Clay_CreateArenaWithCapacityAndMemory :: (capacity: u64, memory: *void) -> Clay_Arena #foreign clay;

// Sets the state of the "pointer" (i.e. the mouse or touch) in Clay's internal data. Used for detecting and responding to mouse events in the debug view,
// as well as for Clay_Hovered() and scroll element handling.
Clay_SetPointerState :: (position: Clay_Vector2, pointerDown: bool) -> void #foreign clay;

// Returns the state of the "pointer" (i.e. the mouse or touch) which was set via Clay_SetPointerState().
Clay_GetPointerState :: () -> Clay_PointerData #foreign clay;

// Initialize Clay's internal arena and setup required data before layout can begin. Only needs to be called once.
// - arena can be created using Clay_CreateArenaWithCapacityAndMemory()
// - layoutDimensions are the initial bounding dimensions of the layout (i.e. the screen width and height for a full screen layout)
// - errorHandler is used by Clay to inform you if something has gone wrong in configuration or layout.
Clay_Initialize :: (arena: Clay_Arena, layoutDimensions: Clay_Dimensions, errorHandler: Clay_ErrorHandler) -> *Clay_Context #foreign clay;

// Returns the Context that clay is currently using. Used when using multiple instances of clay simultaneously.
Clay_GetCurrentContext :: () -> *Clay_Context #foreign clay;

// Sets the context that clay will use to compute the layout.
// Used to restore a context saved from Clay_GetCurrentContext when using multiple instances of clay simultaneously.
Clay_SetCurrentContext :: (_context: *Clay_Context) -> void #foreign clay;

// Updates the state of Clay's internal scroll data, updating scroll content positions if scrollDelta is non zero, and progressing momentum scrolling.
// - enableDragScrolling when set to true will enable mobile device like "touch drag" scroll of scroll containers, including momentum scrolling after the touch has ended.
// - scrollDelta is the amount to scroll this frame on each axis in pixels.
// - deltaTime is the time in seconds since the last "frame" (scroll update)
Clay_UpdateScrollContainers :: (enableDragScrolling: bool, scrollDelta: Clay_Vector2, deltaTime: float) -> void #foreign clay;

// Returns the internally stored scroll offset for the currently open element.
// Generally intended for use with clip elements to create scrolling containers.
Clay_GetScrollOffset :: () -> Clay_Vector2 #foreign clay;

// Updates the layout dimensions in response to the window or outer container being resized.
Clay_SetLayoutDimensions :: (dimensions: Clay_Dimensions) -> void #foreign clay;

// Returns the current dimensions set by Clay_SetLayoutDimensions.
Clay_GetLayoutDimensions :: () -> Clay_Dimensions #foreign clay;

// Called before starting any layout declarations.
Clay_BeginLayout :: () -> void #foreign clay;

// Called when all layout declarations are finished.
// Computes the layout and generates and returns the array of render commands to draw.
Clay_EndLayout :: (deltaTime: float) -> Clay_RenderCommandArray #foreign clay;

// Gets the ID of the currently open element, useful for retrieving IDs generated by CLAY_AUTO_ID()
Clay_GetOpenElementId :: () -> u32 #foreign clay;

// Calculates a hash ID from the given idString.
// Generally only used for dynamic strings when CLAY_ID("stringLiteral") can't be used.
Clay_GetElementId :: (idString: Clay_String) -> Clay_ElementId #foreign clay;

// Calculates a hash ID from the given idString and index.
// - index is used to avoid constructing dynamic ID strings in loops.
// Generally only used for dynamic strings when CLAY_IDI("stringLiteral", index) can't be used.
Clay_GetElementIdWithIndex :: (idString: Clay_String, index: u32) -> Clay_ElementId #foreign clay;

// Returns layout data such as the final calculated bounding box for an element with a given ID.
// The returned Clay_ElementData contains a `found` bool that will be true if an element with the provided ID was found.
// This ID can be calculated either with CLAY_ID() for string literal IDs, or Clay_GetElementId for dynamic strings.
Clay_GetElementData :: (id: Clay_ElementId) -> Clay_ElementData #foreign clay;

// Returns true if the pointer position provided by Clay_SetPointerState is within the current element's bounding box.
// Works during element declaration, e.g. CLAY({ .backgroundColor = Clay_Hovered() ? BLUE : RED });
Clay_Hovered :: () -> bool #foreign clay;

// Bind a callback that will be called when the pointer position provided by Clay_SetPointerState is within the current element's bounding box.
// - onHoverFunction is a function pointer to a user defined function.
// - userData is a pointer that will be transparently passed through when the onHoverFunction is called.
Clay_OnHover :: (onHoverFunction: #type (elementId: Clay_ElementId, pointerData: Clay_PointerData, userData: *void) -> void #c_call, userData: *void) -> void #foreign clay;

// An imperative function that returns true if the pointer position provided by Clay_SetPointerState is within the element with the provided ID's bounding box.
// This ID can be calculated either with CLAY_ID() for string literal IDs, or Clay_GetElementId for dynamic strings.
Clay_PointerOver :: (elementId: Clay_ElementId) -> bool #foreign clay;

// Returns the array of element IDs that the pointer is currently over.
Clay_GetPointerOverIds :: () -> Clay_ElementIdArray #foreign clay;

// Returns data representing the state of the scrolling element with the provided ID.
// The returned Clay_ScrollContainerData contains a `found` bool that will be true if a scroll element was found with the provided ID.
// An imperative function that returns true if the pointer position provided by Clay_SetPointerState is within the element with the provided ID's bounding box.
// This ID can be calculated either with CLAY_ID() for string literal IDs, or Clay_GetElementId for dynamic strings.
Clay_GetScrollContainerData :: (id: Clay_ElementId) -> Clay_ScrollContainerData #foreign clay;

// Binds a callback function that Clay will call to determine the dimensions of a given string slice.
// - measureTextFunction is a user provided function that adheres to the interface Clay_Dimensions (Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);
// - userData is a pointer that will be transparently passed through when the measureTextFunction is called.
Clay_SetMeasureTextFunction :: (measureTextFunction: #type (text: Clay_StringSlice, config: *Clay_TextElementConfig, userData: *void) -> Clay_Dimensions #c_call, userData: *void) -> void #foreign clay;

// Experimental - Used in cases where Clay needs to integrate with a system that manages its own scrolling containers externally.
// Please reach out if you plan to use this function, as it may be subject to change.
Clay_SetQueryScrollOffsetFunction :: (queryScrollOffsetFunction: #type (elementId: u32, userData: *void) -> Clay_Vector2 #c_call, userData: *void) -> void #foreign clay;

// A bounds-checked "get" function for the Clay_RenderCommandArray returned from Clay_EndLayout().
Clay_RenderCommandArray_Get :: (array: *Clay_RenderCommandArray, index: s32) -> *Clay_RenderCommand #foreign clay;

// Enables and disables Clay's internal debug tools.
// This state is retained and does not need to be set each frame.
Clay_SetDebugModeEnabled :: (enabled: bool) -> void #foreign clay;

// Returns true if Clay's internal debug tools are currently enabled.
Clay_IsDebugModeEnabled :: () -> bool #foreign clay;

// Enables and disables visibility culling. By default, Clay will not generate render commands for elements whose bounding box is entirely outside the screen.
Clay_SetCullingEnabled :: (enabled: bool) -> void #foreign clay;

// Returns the maximum number of UI elements supported by Clay's current configuration.
Clay_GetMaxElementCount :: () -> s32 #foreign clay;

// Modifies the maximum number of UI elements supported by Clay's current configuration.
// This may require reallocating additional memory, and re-calling Clay_Initialize();
Clay_SetMaxElementCount :: (maxElementCount: s32) -> void #foreign clay;

// Returns the maximum number of measured "words" (whitespace seperated runs of characters) that Clay can store in its internal text measurement cache.
Clay_GetMaxMeasureTextCacheWordCount :: () -> s32 #foreign clay;

// Modifies the maximum number of measured "words" (whitespace seperated runs of characters) that Clay can store in its internal text measurement cache.
// This may require reallocating additional memory, and re-calling Clay_Initialize();
Clay_SetMaxMeasureTextCacheWordCount :: (maxMeasureTextCacheWordCount: s32) -> void #foreign clay;

// Resets Clay's internal text measurement cache. Useful if font mappings have changed or fonts have been reloaded.
Clay_ResetMeasureTextCache :: () -> void #foreign clay;

// A built in transition function that uses the "Ease Out" curve
Clay_EaseOut :: (arguments: Clay_TransitionCallbackArguments) -> bool #foreign clay;

// Internal API functions required by macros ----------------------
Clay__OpenElement :: () -> void #foreign clay;
Clay__OpenElementWithId :: (elementId: Clay_ElementId) -> void #foreign clay;
Clay__ConfigureOpenElement :: (config: Clay_ElementDeclaration) -> void #foreign clay;
Clay__ConfigureOpenElementPtr :: (config: *Clay_ElementDeclaration) -> void #foreign clay;
Clay__CloseElement :: () -> void #foreign clay;
Clay__HashString :: (key: Clay_String, seed: u32) -> Clay_ElementId #foreign clay;
Clay__HashStringWithOffset :: (key: Clay_String, offset: u32, seed: u32) -> Clay_ElementId #foreign clay;
Clay__OpenTextElement :: (text: Clay_String, textConfig: Clay_TextElementConfig) -> void #foreign clay;

Clay__debugViewHighlightColor: Clay_Color #elsewhere clay;
Clay__debugViewWidth: u32 #elsewhere clay;

#scope_file

#if OS == .WINDOWS {
  clay :: #library,no_dll "windows/clay";
} else #if OS == .LINUX {
  clay :: #library,no_dll "linux/clay";
} else #if OS == .MACOS {
  clay :: #library,no_dll "macos/clay";
} else #if OS == .WASM {
  clay :: #library,no_dll "wasm/clay";
} else {
  #assert false;
}
