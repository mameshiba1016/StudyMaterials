# 08 DirectX

DXライブラリ編の後、Windows、DirectX Graphics Infrastructure（DXGI）、Direct3D、HLSL、GPUの関係を低水準から学ぶ章です。まずDirectX 11で描画Pipelineの構成要素を理解し、その後DirectX 12で明示的なResource状態、Descriptor、Command Queue、Fence、複数Frame管理を扱います。

## DirectX 11編

1. [全体像・Win32・COM・描画Pipeline](01_directx11_overview_win32_com_pipeline.md)
2. [Visual Studio・Windows SDK・Project設定](02_visual_studio_windows_sdk_project.md)
3. [Win32 Window・Message Loop](03_win32_window_message_loop.md)
4. [HRESULT・COM・ComPtr・Debug Layer](04_hresult_com_comptr_debug_layer.md)
5. [DXGI Factory・Adapter・Output](05_dxgi_factory_adapter_output.md)
6. [Device・Feature Level・Device Context](06_device_feature_level_context.md)
7. [Swap Chain・Flip Model・Present](07_swap_chain_flip_model_present.md)
8. [Back Buffer・Render Target View](08_back_buffer_render_target_view.md)
9. [Depth Stencil・Viewport・Resize](09_depth_stencil_viewport_resize.md)
10. [HLSL・Shader Compile・Reflection](10_hlsl_shader_compile_reflection.md)
11. [Vertex Shader・Pixel Shader](11_vertex_shader_pixel_shader.md)
12. [Vertex Buffer・Input Layout](12_vertex_buffer_input_layout.md)
13. [Index Buffer・Primitive Topology](13_index_buffer_primitive_topology.md)
14. [Constant Buffer・Alignment・更新](14_constant_buffer_alignment_update.md)
15. [Texture・WIC・Shader Resource View](15_texture_wic_shader_resource_view.md)
16. [Sampler・UV・Mip Map](16_sampler_uv_mipmap.md)
17. [Rasterizer・Cull・Scissor](17_rasterizer_cull_scissor.md)
18. [Blend・Alpha・Render Target](18_blend_alpha_render_target.md)
19. [Depth Stencil State](19_depth_stencil_state.md)
20. [DirectXMath・座標・行列](20_directxmath_coordinates_matrices.md)
21. [Camera・Projection](21_camera_projection.md)
22. [Lighting・Normal・Material](22_lighting_normal_material.md)
23. [Model・Mesh・Animation](23_model_mesh_animation.md)
24. [Shadow Mapping](24_shadow_mapping.md)
25. [Render to Texture・Post Process](25_render_to_texture_post_process.md)
26. [Instancing・Batch・Culling](26_instancing_batch_culling.md)
27. [Compute Shader・UAV](27_compute_shader_uav.md)
28. [Multithread・Deferred Context](28_multithread_deferred_context.md)
29. [Device Lost・Resize・Fullscreen](29_device_lost_resize_fullscreen.md)
30. [GPU Debug・PIX・Profiler](30_gpu_debug_pix_profiler.md)
31. [Renderer Architecture・Frame Graph](31_renderer_architecture_frame_graph.md)
32. [DirectX 11総合3D戦闘描画](32_directx11_integrated_3d_action_rendering.md)

DirectX 11編は全32章を追加済みです。個別APIを暗記するだけでなく、第32章で高速3D戦闘Sceneの一Frameへ統合して復習できます。

## DirectX 12編予定

1. [DirectX 11との違い・明示的API・全体構造](33_directx12_overview_explicit_api.md)
2. [Windows SDK・Debug Layer・Factory・Adapter・Device](34_directx12_sdk_debug_factory_adapter_device.md)
3. [Command Queue・Allocator・Command List](35_directx12_command_queue_allocator_list.md)
4. [Fence・Event・Frame Resource](36_directx12_fence_event_frame_resource.md)
5. [Swap Chain・RTV・Present](37_directx12_swap_chain_rtv_present.md)
6. [Descriptor Heap・Handle・Allocator](38_directx12_descriptor_heap_handle_allocator.md)
7. [Root Signature・Resource Binding](39_directx12_root_signature_resource_binding.md)
8. [Pipeline State Object・Shader](40_directx12_pso_shader.md)
9. [Resource・Heap・Upload](41_directx12_resource_heap_upload.md)
10. [Resource Barrier・State Tracking](42_directx12_resource_barrier_state_tracking.md)
11. [Texture・Mip・Copy](43_directx12_texture_mip_copy.md)

## 今後制作するDirectX 12ノート

12. Depth・Blend・Rasterizer
13. Model・Material・Animation
14. Compute・UAV・Indirect
15. Multithread Command Recording
16. Multiple Queue・Async Compute
17. GPU Memory・Transient Resource
18. Device Removed・DRED・PIX
19. Frame Graph統合
20. DirectX 12総合3D戦闘描画

DirectX 11で各Pipeline StageとResource Bindingを可視化してからDirectX 12へ進むことで、「何を明示的に管理するようになったか」を比較できます。
