# 03 3Dゲーム開発

3D数学、CPU側のSceneデータ、GPUレンダリング、Animation、Physics、3Dアクションを段階的に学びます。座標系や行列規約はAPI・Engineで異なるため、暗記ではなく変換の契約を確認します。

## ノート

1. [`01_3d_coordinate_systems.md`](01_3d_coordinate_systems.md)
2. [`02_vectors_and_geometry.md`](02_vectors_and_geometry.md)
3. [`03_matrices_and_transforms.md`](03_matrices_and_transforms.md)
4. [`04_quaternions.md`](04_quaternions.md)
5. [`05_camera_and_projection.md`](05_camera_and_projection.md)
6. [`06_meshes_and_vertex_data.md`](06_meshes_and_vertex_data.md)
7. [`07_gpu_rendering_pipeline.md`](07_gpu_rendering_pipeline.md)
8. [`08_shaders_and_materials.md`](08_shaders_and_materials.md)
9. [`09_textures_and_sampling.md`](09_textures_and_sampling.md)
10. [`10_lighting.md`](10_lighting.md)
11. [`11_shadows.md`](11_shadows.md)
12. [`12_post_processing.md`](12_post_processing.md)
13. [`13_skeletons_and_skinning.md`](13_skeletons_and_skinning.md)
14. [`14_animation_clips_and_blending.md`](14_animation_clips_and_blending.md)
15. [`15_animation_state_machines.md`](15_animation_state_machines.md)
16. [`16_inverse_kinematics.md`](16_inverse_kinematics.md)
17. [`17_3d_collision_and_queries.md`](17_3d_collision_and_queries.md)
18. [`18_rigid_body_physics.md`](18_rigid_body_physics.md)

今後、Spatial Partition、3D Character Controller、3D Camera、Action Combat、Optimizationを追加します。

## 数式・擬似APIについて

行列の記憶配置、Vectorを左右どちらから掛けるか、左手・右手座標、Clip Spaceの範囲はLibraryごとに異なります。本ノートの式を利用APIへ移す際は、そのAPIの規約を必ず確認します。`Renderer`等は原理説明用の擬似APIです。
