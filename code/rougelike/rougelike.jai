WINDOW_WIDTH :: 1280;
WINDOW_HEIGHT :: 720;

main :: () {
  hwnd := create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Rougelike");

  running := true;
  while running {
    update_window_events();

    for events_this_frame {
      if it.type == {
        case .QUIT; running = false;
      }
    }

    /*
    clear_color := float.[1, 1, 1, 1];
    ID3D11DeviceContext_ClearRenderTargetView(device_ctx, render_target, *clear_color);
    ID3D11DeviceContext_ClearDepthStencilView(device_ctx, depth_stencil_view, xx D3D11_CLEAR_FLAG.DEPTH, 1, 0);
    ID3D11DeviceContext_DrawIndexed(device_ctx, indices.count, 0, 0);
    IDXGISwapChain_Present(swap_chain, 0, 0);
    */
  }
}

#load "renderer.jai";

#import "Basic";
#import "Window_Creation";
#import "Math";
#import "Windows";
#import "d3d11";
#import "d3d_compiler";
#import "dxgi";
#import "Input";