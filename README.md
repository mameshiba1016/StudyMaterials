# StudyMaterials

C++、2Dゲーム、3Dゲーム、ゲームAI、Unreal Engine、Unity、DXライブラリ、DirectXを、基礎からゲームクライアント開発レベルまで体系的に学ぶための教材リポジトリです。

## 教材の原則

- 一つの巨大ファイルへ詰め込まず、テーマごとに独立したノートへ分割します。
- 初出の用語は、意味・読み方・必要な理由を説明します。
- コードは行単位のコメントに加え、実行時の処理やメモリ上の意味も説明します。
- 「動いた」で終わらず、未定義動作、性能、保守性、ゲームでの用途まで扱います。
- 解説ノートと実行可能な使用例は分離します。
- Unreal Engine固有コードと標準C++の違いを明記します。
- Unity固有のC#、Engine API、Managed Memory、DOTSの違いを明記します。
- 高速3D戦闘アクションに必要なCharacter制御、Camera、Target Lock、Combo、Cancel、Dodge、Parry、交代・支援、敵AI、VFX、最適化を汎用技術として扱います。
- 特定作品のCharacter、Model、Animation、音楽、UI、名称等は複製せず、ゲームシステムの原理とOriginal実装方法を学びます。

## 分野

| 番号 | ディレクトリ | 内容 |
|---:|---|---|
| 00 | `00_learning_guide` | 読み方、環境、用語、学習方針 |
| 01 | `01_cpp_basics` | C++文法、型、制御構文、関数、メモリ、OOP、STL |
| 02 | `02_2d_game` | ゲームループ、描画、入力、衝突、2Dアクション |
| 03 | `03_3d_game` | 3D数学、カメラ、描画、アニメーション、物理 |
| 04 | `04_game_ai` | FSM、経路探索、Behavior Tree、Utility AI |
| 05 | `05_unreal_engine` | Unreal C++、Gameplay Framework、Animation、GAS |
| 06U | `06_unity` | C#、GameObject、Prefab、Animation、DOTS、Unity製3Dアクション |
| 07D | `07_dxlib` | DXライブラリによる2D／3Dゲーム実装、設計、アクションゲーム統合 |
| 08D | `08_directx` | DirectX、GPU Pipeline、Resource、Shader、描画Engine基盤 |
| 06 | `06_engine_architecture` | ECS、リソース、シーン、イベント、並列処理 |
| 07 | `07_debug_performance` | デバッグ、テスト、CPU/GPU最適化 |
| 90 | `90_examples` | ノートに対応する独立した実行例（順次追加） |

詳しい学習順序は[`ROADMAP.md`](ROADMAP.md)を参照してください。

## 現在の制作段階

C++基礎編、2D編、3D編、ゲームAI編、Unreal Engine編、Unity編、DXライブラリ編、DirectX 11編の初稿が完成し、現在はDirectX 12編を制作しています。全知識ノートの完成後、各項目をそのままBuild・実行できる実習編を[`90_examples`](90_examples/README.md)へ一つずつ追加します。完成済み・未完成を[`PROGRESS.md`](PROGRESS.md)で明示します。
