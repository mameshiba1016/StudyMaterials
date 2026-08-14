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

## 今後制作するDirectX 11ノート

22. Lighting・Normal・Material
23. Model・Mesh・Animation
24. Shadow Mapping
25. Render to Texture・Post Process
26. Instancing・Batch・Culling
27. Compute Shader・UAV
28. Multithread・Deferred Context
29. Device Lost・Resize・Fullscreen
30. GPU Debug・PIX・Profiler
31. Renderer Architecture・Frame Graph
32. DirectX 11総合3D戦闘描画

## DirectX 12編予定

DirectX 11編の後、Device、Command Queue、Command Allocator、Command List、Descriptor Heap、Root Signature、Pipeline State Object、Resource Barrier、Upload、Fence、Frame Resource、Async Compute、GPU Memory、Renderer統合の順に進みます。

DirectX 11で各Pipeline StageとResource Bindingを可視化してからDirectX 12へ進むことで、「何を明示的に管理するようになったか」を比較できます。
