#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#define base_function extern
#define BASE_IMPLEMENTATION
#include "base.h"

#include "runtime_shader_compiler.h"

global struct {
  SDL_Window *window;
  SDL_GPUDevice *gpu;

  Arena *perm;
} game_state;

function SDL_AppResult
SDL_AppInit (void **appstate, int argc, char **argv) {
  Arena *perm = arena_alloc_default();

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  SDL_Window *window = SDL_CreateWindow("Binghamton: The Horror Game", 1280, 720, SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUDevice *gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
  if (gpu == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create GPU device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not claim window for GPU device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  Str8 driver = Str8CStr(SDL_GetGPUDeviceDriver(gpu));
  bool vulkan = str8_match(driver, Str8Lit("vulkan"), 0);
  Assert(!vulkan, "This GPU driver is not supported yet");
  SDL_Log("GPU device created with %.*s driver\n", Str8Expand(driver));

  SDL_GPUShader *vertex_shader = 0;
  ScratchBlock(&perm,1) {
    Str8 entry_point = Str8Lit("vs_main");
    Str8_List vs_compilation_messages = {0};
    Compiled_Shader_Data vs_source = compile_shader_from_file(
      scratch.arena,
      Str8Lit("W:/code/BTHG/shader_default.hlsl"),
      entry_point,
      Str8Lit("vs_6_1"),
      &vs_compilation_messages
    );
    for EachInList(vs_compilation_messages) SDL_Log("VS Compilation: %.*s\n", Str8Expand(it->string));
    if (vs_source.data) {
      SDL_GPUShaderCreateInfo vsinfo = {0};
      vsinfo.code_size = vs_source.count;
      vsinfo.code = vs_source.data;
      vsinfo.entrypoint = str8_to_cstr(scratch.arena, entry_point);
      vsinfo.format = vs_source.format;
      vsinfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
      vertex_shader = SDL_CreateGPUShader(gpu, &vsinfo);
    }
  }

  SDL_GPUGraphicsPipelineCreateInfo gfx_pipeline_info = {0};
  /*
  gfx_pipeline_info.vertex_shader = ;
  gfx_pipeline_info.fragment_shader = ;
  gfx_pipeline_info.vertex_input_state = ;
  gfx_pipeline_info.primitive_type = ;
  gfx_pipeline_info.rasterizer_state = ;
  gfx_pipeline_info.multisample_state = ;
  gfx_pipeline_info.depth_stencil_state = ;
  gfx_pipeline_info.target_info = ;
  SDL_GPUGraphicsPipeline *gfx_pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &gfx_pipeline_info);
  if (gfx_pipeline == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create graphics pipeline: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  */

  Vec2 vbuffer_data[] = {
    V2(0, 0),
    V2(0.5f, 1),
    V2(1, 0)
  };
  SDL_GPUTransferBufferCreateInfo vbtinfo = {0};
  vbtinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  vbtinfo.size = sizeof(vbuffer_data);
  SDL_GPUTransferBuffer *vbtransfer = SDL_CreateGPUTransferBuffer(gpu, &vbtinfo);
  void *vb_mapped = SDL_MapGPUTransferBuffer(gpu, vbtransfer, false);
  memcpy(vb_mapped, vbuffer_data, sizeof(vbuffer_data));
  SDL_UnmapGPUTransferBuffer(gpu, vbtransfer);

  SDL_GPUBufferCreateInfo vbinfo = {0};
  vbinfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  vbinfo.size = sizeof(vbuffer_data);
  SDL_GPUBuffer *vbuffer = SDL_CreateGPUBuffer(gpu, &vbinfo);

  SDL_GPUCommandBuffer *copy_commands = SDL_AcquireGPUCommandBuffer(gpu);
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(copy_commands);

  SDL_UploadToGPUBuffer(copy_pass, &(SDL_GPUTransferBufferLocation){vbtransfer, 0}, &(SDL_GPUBufferRegion){vbuffer, 0, sizeof(vbuffer_data)}, false);

  SDL_EndGPUCopyPass(copy_pass);
  SDL_SubmitGPUCommandBuffer(copy_commands);


  game_state.perm = perm;
  game_state.window = window;
  game_state.gpu = gpu;

  return SDL_APP_CONTINUE;
}

function SDL_AppResult
SDL_AppIterate (void *appstate) {
  SDL_GPUCommandBuffer *render_commands = SDL_AcquireGPUCommandBuffer(game_state.gpu);

  SDL_GPUTexture *swapchain_texture = NULL;
  u32 back_buffer_width = 0, back_buffer_height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(render_commands, game_state.window, &swapchain_texture, &back_buffer_width, &back_buffer_height)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire swapchain texture: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  if (swapchain_texture == NULL) return SDL_APP_CONTINUE;

  SDL_GPUColorTargetInfo back_buffer = {0};
  back_buffer.texture = swapchain_texture;
  back_buffer.clear_color = (SDL_FColor){1.f, 1.f, 1.f, 1.f};
  back_buffer.load_op = SDL_GPU_LOADOP_CLEAR;
  back_buffer.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(render_commands, &back_buffer, 1, NULL);

  //SDL_BindGPUGraphicsPipeline(render_pass, );

  SDL_EndGPURenderPass(render_pass);

  SDL_SubmitGPUCommandBuffer(render_commands);
  return SDL_APP_CONTINUE;
}

function SDL_AppResult
SDL_AppEvent (void *appstate, SDL_Event *e) {
  switch (e->type) {
    case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

void
SDL_AppQuit (void *appstate, SDL_AppResult result) {

}
