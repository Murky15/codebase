#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#define base_function extern
#define BASE_IMPLEMENTATION
#include "base.h"

#include "runtime_shader_compiler.h"

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

global struct {
  SDL_Window *window;
  SDL_GPUDevice *gpu;
  SDL_GPUGraphicsPipeline *pipeline;

  SDL_GPUBuffer *test_vertices;

  Arena *perm;
} game_state;

function SDL_GPUShader*
create_shader (SDL_GPUDevice *gpu, Str8 file_path, Str8 entry_point, SDL_GPUShaderStage stage) {
  SDL_GPUShader *result = 0;

  Str8 profile = {0};
  if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
    profile = Str8Lit("vs_6_0");
  } else if (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
    profile = Str8Lit("ps_6_0");
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Invalid shader stage");
    return 0;
  }

  ScratchBlock(0,0) {
    Str8_List messages = {0};
    Compiled_Shader_Data source = compile_shader_from_file(
      scratch.arena,
      file_path,
      entry_point,
      profile,
      &messages
    );
    for EachInList(messages) SDL_Log("Shader Compilation (%.*s): %.*s\n", Str8Expand(entry_point), Str8Expand(it->string));
    if (source.data) {
      SDL_GPUShaderCreateInfo info = {0};
      info.code_size = source.count;
      info.code = source.data;
      info.entrypoint = str8_to_cstr(scratch.arena, entry_point);
      info.format = source.format;
      info.stage = stage;
      result = SDL_CreateGPUShader(gpu, &info);
    }
  }

  return result;
}

function SDL_AppResult
SDL_AppInit (void **appstate, int argc, char **argv) {
  Arena *perm = arena_alloc_default();

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  SDL_Window *window = SDL_CreateWindow("Binghamton: The Horror Game", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

  Str8 default_shader_source     = Str8Lit("W:/code/BTHG/shader_default.hlsl");
  SDL_GPUShader *vertex_shader   = create_shader(gpu, default_shader_source, Str8Lit("vs_main"), SDL_GPU_SHADERSTAGE_VERTEX);
  SDL_GPUShader *fragment_shader = create_shader(gpu, default_shader_source, Str8Lit("ps_main"), SDL_GPU_SHADERSTAGE_FRAGMENT);

  Vec3 vbuffer_data[] = {
    V3(-0.5f, -0.5f, 0.f),
    V3( 0.f,   0.5f, 0.f),
    V3( 0.5f, -0.5f, 0.f)
  };
  SDL_GPUTransferBufferCreateInfo vbtinfo = {0};
  vbtinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  vbtinfo.size = sizeof(vbuffer_data);
  SDL_GPUTransferBuffer *vbtransfer = SDL_CreateGPUTransferBuffer(gpu, &vbtinfo);
  void *vb_mapped = SDL_MapGPUTransferBuffer(gpu, vbtransfer, false);
  {
    memcpy(vb_mapped, vbuffer_data, sizeof(vbuffer_data));
  }
  SDL_UnmapGPUTransferBuffer(gpu, vbtransfer);
  SDL_GPUBufferCreateInfo vbinfo = {0};
  vbinfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  vbinfo.size = sizeof(vbuffer_data);
  SDL_GPUBuffer *vbuffer = SDL_CreateGPUBuffer(gpu, &vbinfo);
  SDL_GPUCommandBuffer *copy_commands = SDL_AcquireGPUCommandBuffer(gpu);
  {
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(copy_commands);
    {
      SDL_UploadToGPUBuffer(copy_pass, &(SDL_GPUTransferBufferLocation){vbtransfer, 0}, &(SDL_GPUBufferRegion){vbuffer, 0, sizeof(vbuffer_data)}, false);
    }
    SDL_EndGPUCopyPass(copy_pass);
  }
  SDL_SubmitGPUCommandBuffer(copy_commands);
  SDL_GPUVertexBufferDescription vbdesc = {0};
  vbdesc.slot = 0;
  vbdesc.pitch = sizeof(Vec3);
  vbdesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  SDL_GPUVertexAttribute vapos = {0};
  vapos.location = 0; // TODO: Verify
  vapos.buffer_slot = 0;
  vapos.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  vapos.offset = 0;
  SDL_GPUVertexInputState vin = {0};
  vin.vertex_buffer_descriptions = &vbdesc;
  vin.num_vertex_buffers = 1;
  vin.vertex_attributes = &vapos;
  vin.num_vertex_attributes = 1;

  SDL_GPURasterizerState rsstate = {0};
  rsstate.fill_mode = SDL_GPU_FILLMODE_FILL;
  rsstate.cull_mode = SDL_GPU_CULLMODE_BACK;
  rsstate.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
  rsstate.enable_depth_clip = false; // TODO: Change to true after initial setup is verified

  SDL_GPUColorTargetDescription color_target = {0};
  color_target.format = SDL_GetGPUSwapchainTextureFormat(gpu, window);

  SDL_GPUGraphicsPipelineCreateInfo gfx_pipeline_info = {0};
  gfx_pipeline_info.vertex_shader = vertex_shader;
  gfx_pipeline_info.fragment_shader = fragment_shader;
  gfx_pipeline_info.vertex_input_state = vin;
  gfx_pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  gfx_pipeline_info.rasterizer_state = rsstate;
  /* TODO
  gfx_pipeline_info.multisample_state = ;
  gfx_pipeline_info.depth_stencil_state = ;
  */
  gfx_pipeline_info.target_info.color_target_descriptions = &color_target;
  gfx_pipeline_info.target_info.num_color_targets = 1;
  SDL_GPUGraphicsPipeline *gfx_pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &gfx_pipeline_info);
  if (gfx_pipeline == NULL) {
    return SDL_APP_FAILURE;
  }

  game_state.perm = perm;
  game_state.window = window;
  game_state.gpu = gpu;
  game_state.pipeline = gfx_pipeline;

  game_state.test_vertices = vbuffer;

  return SDL_APP_CONTINUE;
}

function SDL_AppResult
SDL_AppIterate (void *appstate) {
  SDL_GPUCommandBuffer *render_commands = SDL_AcquireGPUCommandBuffer(game_state.gpu);
  {
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
    {
      SDL_BindGPUGraphicsPipeline(render_pass, game_state.pipeline);
      SDL_SetGPUViewport(render_pass, &(SDL_GPUViewport){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, -1, 1});
      SDL_BindGPUVertexBuffers(render_pass, 0, &(SDL_GPUBufferBinding){game_state.test_vertices, 0}, 1);

      SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
    }
    SDL_EndGPURenderPass(render_pass);
  }
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
