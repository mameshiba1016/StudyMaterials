# 05 Unreal Engine

この章では、標準C++をUnreal Engine（以下UE）のゲームコードへつなげます。単なるAPI一覧ではなく、エディタ、Unreal Build Tool（UBT）、Unreal Header Tool（UHT）、リフレクション、`UObject`、Actor、Componentなどが裏でどう協力しているかを理解することが目的です。

UE固有の仕組みは、標準C++の代用品ではありません。標準C++のコンパイラ、リンカ、所有権、RAIIを土台に、エディタ連携、シリアライズ、GC、ネットワーク複製などを追加した層です。両者を区別して学びます。

## ノート一覧

1. [UE C++の全体像と標準C++との違い](01_ue_cpp_overview.md)
2. [プロジェクト、モジュール、UBT、UHT](02_project_modules_ubt_uht.md)
3. [リフレクションと主要マクロ](03_reflection_macros.md)
4. [`UObject`の寿命、GC、参照方法](04_uobject_lifetime_gc.md)
5. [UEの命名規則、型、文字列、コンテナ](05_types_strings_containers.md)
6. [Actorの役割、生成、座標、破棄](06_actor_spawn_transform_destroy.md)
7. [Component設計とアタッチ階層](07_components_and_attachment.md)
8. [ActorとComponentのライフサイクル](08_actor_component_lifecycle.md)
9. [PawnとCharacterの設計](09_pawn_and_character.md)
10. [Controller、Possess、操作主体の分離](10_controller_and_possession.md)
11. [GameMode、GameState、PlayerState](11_game_mode_state_player_state.md)
12. [GameInstanceとSubsystem](12_game_instance_and_subsystems.md)
13. [Enhanced Inputの構成と内部の流れ](13_enhanced_input.md)
14. [高速アクション向け入力バッファとCommand設計](14_action_input_buffer.md)
15. [Transform、座標空間、回転](15_transform_coordinate_rotation.md)
16. [CollisionのChannel、Object Type、Response](16_collision_channels_responses.md)
17. [Line Trace、Shape Sweep、Overlap](17_traces_sweeps_overlaps.md)
18. [Chaos物理、Force、Impulse、Ragdoll](18_chaos_physics.md)
19. [Character Movementと特殊移動](19_character_movement.md)
20. [アクションカメラとPlayerCameraManager](20_action_camera.md)
21. [ロックオンシステムの設計](21_target_lock_system.md)
22. [Animation Blueprintと更新設計](22_animation_blueprint.md)
23. [State Machine、Blend Space、Layer](23_animation_state_blend_layer.md)
24. [Animation Montage、Slot、Notify](24_montage_slots_notifies.md)
25. [Root MotionとMotion Warping](25_root_motion_warping.md)
26. [データ駆動コンボとキャンセル](26_combo_cancel_system.md)
27. [ダメージ、Hit判定、リアクション](27_damage_hit_reaction.md)
28. [回避、無敵、ガード、パリィ](28_dodge_guard_parry.md)
29. [Hit Stop、演出通知、キャラクター交代](29_hit_stop_switching.md)
30. [AIControllerとAI Perception](30_ai_controller_perception.md)
31. [BlackboardとBehavior Tree](31_blackboard_behavior_tree.md)
32. [NavigationとEQS](32_navigation_eqs.md)
33. [Combat Directorと複数敵制御](33_combat_director.md)
34. [Gameplay Ability Systemの全体像](34_gas_overview.md)
35. [Gameplay TagとASCの所有設計](35_gameplay_tags_asc.md)
36. [Attribute SetとGameplay Effect](36_attributes_effects.md)
37. [Gameplay Ability、Ability Task、Gameplay Cue](37_abilities_tasks_cues.md)

## 今後追加する主題

Actor／Component／Pawn／Character、Controller、GameMode系、Subsystem、Enhanced Input、座標とTransform、Collision、Character Movement、カメラとターゲットロック、Animation Blueprint、Montage、Notify、コンボとキャンセル、回避・パリィ、キャラクター交代、AI Controller、Behavior Tree、EQS、Gameplay Ability System、VFX・Niagara、サウンド、UI、SaveGame、非同期ロード、Data Asset、Replication、プロファイリング、最適化、パッケージングを順番に分冊します。

> バージョンでAPIやエディタ画面が変わる部分は、使用中のUEバージョンに対応するEpic Games公式ドキュメントも確認してください。本章では、長期間通用する設計原則と内部構造を中心に説明します。
