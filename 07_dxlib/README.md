# 07 DXライブラリ

Unity編の完了後、C++基礎をDXライブラリ上の実ゲームへ統合する章です。DXライブラリの簡潔なAPIを使いつつ、その下にあるWindows message pump、resource handle、back buffer、GPU pipelineを意識し、次のDirectX編へ接続します。

## ノート一覧

1. [DXライブラリの全体像・初期化・Game Loop](01_overview_initialization_game_loop.md)
2. [Project設定・文字コード・Path](02_project_encoding_paths.md)
3. [Delta Time・Fixed Step・Frame制御](03_delta_time_fixed_step_frame_control.md)
4. [Keyboard・Mouse・Gamepad・Action Mapping](04_keyboard_mouse_gamepad.md)
5. [2D座標・図形・文字描画](05_2d_coordinates_shapes_text.md)
6. [Texture・Handle・2D Sprite](06_texture_handle_2d_sprite.md)
7. [Blend・Alpha・Render Target](07_blend_alpha_render_target.md)
8. [Sound・Music・Voice管理](08_sound_music_voice_management.md)
9. [2D Collision・Spatial Query](09_2d_collision_spatial_query.md)
10. [Scene・Application State](10_scene_application_state.md)
11. [Resource Cache・RAII](11_resource_cache_raii.md)
12. [Debug Log・Gizmo・Error処理](12_debug_log_gizmo_error_handling.md)
13. [3D数学・座標・行列](13_3d_math_coordinates_matrices.md)
14. [3D Camera・Projection](14_3d_camera_projection.md)
15. [MV1 Model・Material](15_mv1_model_material.md)
16. [MV1 Animation・Blend](16_mv1_animation_blend.md)
17. [3D Collision・Physics設計](17_3d_collision_physics_design.md)
18. [Lighting・Shadow](18_lighting_shadow.md)
19. [Shader・Constant・Render State](19_shader_constant_render_state.md)
20. [UI・HUD・Font](20_ui_hud_font.md)
21. [Save・Settings・File I/O](21_save_settings_file_io.md)
22. [Profiler・Memory・最適化](22_profiler_memory_optimization.md)
23. [Character Controller](23_character_controller.md)

## 今後制作するノート

24. Action Camera・Target Lock
25. Combat State・入力Buffer
26. Combo・Cancel
27. Dodge・Guard・Parry
28. Hit・Damage・Reaction
29. Hit Stop・VFX・Audio演出
30. Enemy AI・Navigation
31. Combat Director・複数敵
32. Boss・Phase・部位
33. Character交代・Support
34. 3D戦闘Action統合
35. Architecture・Test・完成確認表

DXライブラリのGlobal関数を各所へ直書きせず、Application、Scene、Renderer、Input、Resource、Combatといった責任へ分離し、後のDirectX学習につながる構造を作ります。
