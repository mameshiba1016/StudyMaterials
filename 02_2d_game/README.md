# 02 2Dゲーム開発

標準C++で身につけた知識を、リアルタイム2Dゲームのシステムへ接続します。特定ライブラリの関数暗記ではなく、ゲームループ、時間、入力、座標、描画、衝突、状態管理の原理を理解します。

## ノート

1. [`01_2d_game_architecture.md`](01_2d_game_architecture.md)
2. [`02_game_loop.md`](02_game_loop.md)
3. [`03_time_and_frame_control.md`](03_time_and_frame_control.md)
4. [`04_input_system.md`](04_input_system.md)
5. [`05_2d_coordinates_and_vectors.md`](05_2d_coordinates_and_vectors.md)
6. [`06_sprite_rendering.md`](06_sprite_rendering.md)
7. [`07_2d_camera.md`](07_2d_camera.md)
8. [`08_sprite_animation.md`](08_sprite_animation.md)
9. [`09_collision_shapes.md`](09_collision_shapes.md)
10. [`10_collision_detection_and_resolution.md`](10_collision_detection_and_resolution.md)
11. [`11_character_movement_and_2d_physics.md`](11_character_movement_and_2d_physics.md)
12. [`12_tilemaps.md`](12_tilemaps.md)
13. [`13_scene_management.md`](13_scene_management.md)
14. [`14_ui_system.md`](14_ui_system.md)
15. [`15_audio_system.md`](15_audio_system.md)
16. [`16_particles_and_game_feel.md`](16_particles_and_game_feel.md)
17. [`17_action_combat_states.md`](17_action_combat_states.md)
18. [`18_combos_cancels_and_input_buffer.md`](18_combos_cancels_and_input_buffer.md)
19. [`19_damage_invincibility_and_knockback.md`](19_damage_invincibility_and_knockback.md)
20. [`20_enemy_and_boss_design.md`](20_enemy_and_boss_design.md)
21. [`21_save_data_and_settings.md`](21_save_data_and_settings.md)
22. [`22_2d_optimization.md`](22_2d_optimization.md)
23. [`23_completion_checklist.md`](23_completion_checklist.md)

これで2Dゲーム開発編の初稿は完成です。実装例は`90_examples`へ別々の`.cpp`として追加します。

## 擬似APIについて

コード中の`Window`、`Renderer`、`Texture`、`InputBackend`などは原理説明用の型です。そのままコンパイルできる特定ライブラリのAPIではありません。使用例編では、選定したライブラリへ具体的に接続します。
