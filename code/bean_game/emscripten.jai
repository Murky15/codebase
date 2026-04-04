// TODO: This should probably be generated based on emscripten.h, but for our purposes right now this is fine.

// NOTE: Browser Execution Environment
em_callback_func :: #type () #c_call;
em_arg_callback_func :: #type (*void) #c_call;

emscripten_set_main_loop :: (func: em_callback_func, fps: int, simulate_infinite_loop: bool) #foreign;
emscripten_set_main_loop_arg :: (func: em_arg_callback_func, arg: *void, fps: int, simulate_infinite_loop: bool) #foreign;

emscripten_set_main_loop_expected_blockers :: (num: int) #foreign;
emscripten_push_main_loop_blocker :: (func: em_arg_callback_func, arg: *void) #foreign;
emscripten_push_uncounted_main_loop_blocker :: (func: em_arg_callback_func, arg: *void) #foreign;

emscripten_pause\_main_loop :: () #foreign;
emscripten_resume_main_loop :: () #foreign;
emscripten_cancel_main_loop :: () #foreign;

emscripten_set_main_loop_timing :: (mode: int, value: int) -> int #foreign;
emscripten_get_main_loop_timing :: (mode: *int, value: *int) #foreign;

emscripten_exit_with_live_runtime :: () #foreign;
emscripten_hide_mouse :: () #foreign;

emscripten_sleep :: (ms: u32) #foreign;