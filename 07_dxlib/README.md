# 07 DXライブラリ

Unity編の完了後、C++基礎をDXライブラリ上の実ゲームへ統合する章です。

予定内容：導入と文字コード、初期化と終了、Game Loop、Delta Time、Keyboard／Mouse／Gamepad、2D描画、Texture、Sound、2D Collision、Scene管理、Resource Cache、3D座標、Camera、Model、MV1 Animation、3D Collision、Lighting、Shader、UI、Save、Debug描画、最適化、Character Controller、Target Lock、Combo／Cancel、Dodge／Parry、Enemy AI、Boss、Character交代、3D戦闘アクション統合。

DXライブラリのGlobal関数を各所へ直書きせず、Application、Scene、Renderer、Input、Resource、Combatといった責任へ分離し、後のDirectX学習につながる構造を作ります。
