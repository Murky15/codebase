// TODO: This should probably be generated based on emscripten.h, but for our purposes right now this is fine.

// NOTE: Browser Execution Environment
em_callback_func :: #type () #c_call;
em_arg_callback_func :: #type (*void) #c_call;

emscripten_set_main_loop :: (func: em_callback_func, fps: s32, simulate_infinite_loop: bool) #foreign;
emscripten_set_main_loop_arg :: (func: em_arg_callback_func, arg: *void, fps: s32, simulate_infinite_loop: bool) #foreign;

emscripten_set_main_loop_expected_blockers :: (num: s32) #foreign;
emscripten_push_main_loop_blocker :: (func: em_arg_callback_func, arg: *void) #foreign;
emscripten_push_uncounted_main_loop_blocker :: (func: em_arg_callback_func, arg: *void) #foreign;

emscripten_pause\_main_loop :: () #foreign;
emscripten_resume_main_loop :: () #foreign;
emscripten_cancel_main_loop :: () #foreign;

emscripten_set_main_loop_timing :: (mode: s32, value: s32) -> s32 #foreign;
emscripten_get_main_loop_timing :: (mode: *s32, value: *s32) #foreign;

emscripten_exit_with_live_runtime :: () #foreign;
emscripten_hide_mouse :: () #foreign;

emscripten_sleep :: (ms: u32) #foreign;

emscripten_get_preloaded_image_data :: (path: *u8, w: *s32, h: *s32) -> *u8 #foreign;
