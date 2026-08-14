# DXライブラリ：Lighting・Shadow

この章では、3Dモデルがなぜその明るさに見えるのか、影がどの空間で何を比較して作られるのかを学びます。API名だけでなく、光源・法線・材質・シャドウマップ・描画状態の関係を理解することが目的です。

> 本章のAPI契約はDXライブラリ公式リファレンスを基準にしています。利用中のバージョンでも再確認してください。

## 1. 光と材質

```text
最終色 ≒ 環境光 + 拡散反射 + 鏡面反射 + 自己発光
```

- 光源は方向・位置・色・強度を持つ。
- 面は位置と法線を持つ。
- 材質は光をどの色・強さで返すかを決める。
- カメラ方向は鏡面反射へ影響する。
- 影は「光源からその点が見えるか」を弱める係数になる。

同じ赤い材質でも青い光を当てると、返せる赤成分が少ないため暗く見えます。

## 2. 法線と正規化

法線 `N` は面の正面を示すベクトルです。光へ向かう単位ベクトルを `L` とし、内積で光に対する向きを測ります。

```cpp
VECTOR normal = VNorm(surfaceNormal); // 長さ1にして角度だけを表せるようにする。
VECTOR toLight = VNorm(lightPosition - worldPosition); // 点から光源への方向。
float nDotL = VDot(normal, toLight);
float diffuseFactor = std::max(0.0f, nDotL); // 裏側から来る光は0にする。
```

非一様スケールを法線へそのまま適用すると向きが崩れます。一般にはワールド行列の逆転置行列で変換し、再正規化します。

## 3. Ambient：環境光

環境光は全方向から回り込む光の簡略表現です。暗部を完全な黒にしない一方、強すぎると立体感が消えます。

```cpp
// RGBを暗めから調整する。Alphaは1.0とする。
SetGlobalAmbientLight(GetColorF(0.08f, 0.09f, 0.12f, 1.0f));
```

## 4. Diffuse：拡散反射

Lambert拡散反射は `max(0, N・L)` を用います。

```text
diffuse = lightColor × materialDiffuse × max(0, dot(N, L))
```

面が光へ正対するほど明るく、横を向くほど暗くなります。必要なら距離減衰と影係数も掛けます。

## 5. Specular：鏡面反射

Blinn-Phongでは光方向 `L` と視線方向 `V` の中間ベクトル `H` を使います。

```cpp
VECTOR viewDirection = VNorm(cameraPosition - worldPosition);
VECTOR halfVector = VNorm(toLight + viewDirection);
float nDotH = std::max(0.0f, VDot(normal, halfVector));
float specularFactor = std::pow(nDotH, shininess); // 大きいほど小さく鋭い光沢。
```

自己発光色はライトがなくても明るく見える成分ですが、通常は設定しただけで周囲を照らしません。

## 6. Directional Light：平行光源

太陽のように、シーン全域へ同じ方向から届く光を近似します。位置ではなく方向が重要で、通常は距離減衰を持ちません。

```cpp
int lightHandle = CreateDirLightHandle(VGet(-0.4f, -1.0f, 0.2f));
if (lightHandle == -1)
{
    throw std::runtime_error("Directional light creation failed");
}
SetLightDifColorHandle(GetColorF(1.0f, 0.95f, 0.86f, 1.0f), lightHandle);
SetLightSpcColorHandle(GetColorF(0.8f, 0.8f, 0.8f, 1.0f), lightHandle);
SetLightEnableHandle(lightHandle, TRUE);
```

## 7. Point Light：点光源

電球のように一点から全方向へ広がり、位置・到達距離・距離減衰を持ちます。

```cpp
int lightHandle = CreatePointLightHandle(
    VGet(0.0f, 2.0f, 0.0f), // ワールド位置。
    8.0f,                    // 有効距離。必要以上に広げない。
    0.0f, 0.08f, 0.02f      // 定数・一次・二次減衰係数。
);
```

概念上は `1 / (a0 + a1*d + a2*d*d)` のように距離 `d` で弱まります。

## 8. Spot Light：スポット光源

位置・方向・内外円錐角・距離減衰を持ちます。角度はラジアンで指定します。

```cpp
int lightHandle = CreateSpotLightHandle(
    VGet(0.0f, 3.0f, -2.0f), // 位置。
    VGet(0.0f, -0.6f, 1.0f), // 照射方向。
    DX_PI_F / 8.0f,           // 内側角度。
    DX_PI_F / 5.0f,           // 外側角度。
    12.0f,                    // 有効距離。
    0.0f, 0.05f, 0.015f      // 距離減衰。
);
```

## 9. 色・強度・色空間

設計上は色と強度を分けると調整意図が明確になります。

```cpp
struct LightColor final
{
    float red{1.0f};
    float green{1.0f};
    float blue{1.0f};
    float intensity{1.0f};
};
```

物理的な計算はリニア色空間で行い、表示時にsRGBへ変換するのが基本です。ガンマ空間で加算すると明暗が不自然になります。

## 10. 材質とライトの切り分け

モデルが暗いときは次を確認します。

1. 頂点法線が正しいか。
2. 面の表裏とカリングが合っているか。
3. Diffuse・Specular・Emissive値が適切か。
4. テクスチャの色空間が正しいか。
5. ライトの有効状態、方向、位置、範囲が正しいか。
6. 影判定が全ピクセルを遮蔽していないか。

## 11. 同時ライト数という重要な契約

DXライブラリでは、標準ライトを含め同時に有効にできるライトは最大3個です。4個以上を有効にすると、どれが描画へ使われるかは不定です。

カメラや対象への寄与を評価し、上位だけを有効にします。境界で毎フレーム入れ替わると明滅するため、前フレーム採用ライトへヒステリシス加点を与えます。

```cpp
struct LightCandidate final
{
    int handle{-1};
    float distanceToTarget{};
    float authoredPriority{};
    bool affectsMainCharacter{};
};

float CalculateLightScore(const LightCandidate& light)
{
    const float characterBonus = light.affectsMainCharacter ? 1000.0f : 0.0f;
    return characterBonus + light.authoredPriority * 100.0f - light.distanceToTarget;
}
```

## 12. ライトハンドルのRAII

ハンドルはDXライブラリ内部資源を参照する整数です。所有者を一つに決めて解放します。

```cpp
class UniqueLight final
{
public:
    explicit UniqueLight(int handle = -1) noexcept : handle_(handle) {}
    ~UniqueLight() { Reset(); }

    UniqueLight(const UniqueLight&) = delete;
    UniqueLight& operator=(const UniqueLight&) = delete;

    UniqueLight(UniqueLight&& other) noexcept
        : handle_(std::exchange(other.handle_, -1)) {}

    UniqueLight& operator=(UniqueLight&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = std::exchange(other.handle_, -1);
        }
        return *this;
    }

    [[nodiscard]] int Get() const noexcept { return handle_; }

    void Reset() noexcept
    {
        if (handle_ != -1)
        {
            DeleteLightHandle(handle_); // 内部ライトを一度だけ解放する。
            handle_ = -1;              // 二重削除を防ぐ。
        }
    }

private:
    int handle_{-1};
};
```

`DeleteLightHandleAll` と個別RAIIを無計画に併用してはいけません。

## 13. Shadow Mapの原理

まず光源視点から最も近い面の深度をテクスチャへ保存し、通常描画時に描画点の光源空間深度と比較します。

```text
現在点の深度 <= 保存深度 + bias：光が届く
現在点の深度 >  保存深度 + bias：別の面に遮られている
```

- 影を落とす物体をCasterという。
- 影を受ける物体をReceiverという。
- 影生成と通常描画は別の描画パスである。

## 14. シャドウマップ生成と所有権

`MakeShadowMap` の幅・高さは2の累乗で指定します。高解像度ほど輪郭は細かくなりますが、VRAM・生成描画・参照コストが増えます。

```cpp
class UniqueShadowMap final
{
public:
    explicit UniqueShadowMap(int size) : handle_(MakeShadowMap(size, size))
    {
        if (handle_ == -1)
        {
            throw std::runtime_error("Shadow map creation failed");
        }
    }

    ~UniqueShadowMap()
    {
        if (handle_ != -1) DeleteShadowMap(handle_);
    }

    UniqueShadowMap(const UniqueShadowMap&) = delete;
    UniqueShadowMap& operator=(const UniqueShadowMap&) = delete;
    [[nodiscard]] int Get() const noexcept { return handle_; }

private:
    int handle_{-1};
};
```

公式仕様ではこの機能にShader Model 2.0以上が必要です。

## 15. 公式手順に沿った影描画

```cpp
void RenderWithShadowMap(int shadowMapHandle)
{
    // 1. 影生成開始前に光方向と対象範囲を設定する。
    SetShadowMapLightDirection(shadowMapHandle, VGet(-0.4f, -1.0f, 0.2f));
    SetShadowMapDrawArea(
        shadowMapHandle,
        VGet(-20.0f, -5.0f, -20.0f),
        VGet( 20.0f, 15.0f, 20.0f));

    // 2. 描画先をシャドウマップへ切り替える。
    // この期間はSetDrawScreenの描画先より優先される。
    ShadowMap_DrawSetup(shadowMapHandle);
    DrawStageShadowCasters();
    DrawCharacterShadowCasters();
    ShadowMap_DrawEnd();

    // 3. スロット0へ割り当て、影を受ける通常モデルを描く。
    SetUseShadowMap(0, shadowMapHandle);
    DrawStageReceivers();
    DrawCharacters();

    // 4. 後続パスへ状態を漏らさない。
    SetUseShadowMap(0, -1);
}
```

`ShadowMap_DrawSetup` から `ShadowMap_DrawEnd` まで、描画範囲・光方向・使用マップなどの影設定や取得関数は呼べません。必ず開始前に設定します。

## 16. 影生成スコープのRAII

途中returnや例外でも `ShadowMap_DrawEnd` を保証します。

```cpp
class ShadowDrawScope final
{
public:
    explicit ShadowDrawScope(int handle)
    {
        if (ShadowMap_DrawSetup(handle) == -1)
            throw std::runtime_error("ShadowMap_DrawSetup failed");
        active_ = true;
    }

    ~ShadowDrawScope()
    {
        if (active_) ShadowMap_DrawEnd();
    }

    ShadowDrawScope(const ShadowDrawScope&) = delete;
    ShadowDrawScope& operator=(const ShadowDrawScope&) = delete;

private:
    bool active_{false};
};
```

## 17. Draw Areaとテクセル密度

同じ1024×1024でも、20m四方と200m四方では1テクセルが担当する世界空間の幅が10倍違います。範囲はCasterとReceiverを含む最小限へ絞ります。

カメラ追従で範囲が微小移動すると影が揺れます。光空間で範囲中心をシャドウマップのテクセル単位へ丸める安定化が有効です。

## 18. Depth Bias

深度の有限精度で受け面が自身を遮蔽する縞模様をShadow Acneと呼びます。

```cpp
// 小さな値から調整する。大きすぎると影が物体から浮く。
SetShadowMapAdjustDepth(shadowMapHandle, 0.0005f);
```

Biasが大きすぎて影が接地点から離れる現象はPeter Panningです。解像度・範囲・光角度も合わせて調整します。独自シェーダーでは法線方向へずらすNormal Biasや、傾斜で増やすSlope Biasも検討します。

## 19. PCFと柔らかい影

1回の深度比較では輪郭が硬く、テクセル形状が目立ちます。周辺の複数テクセルを比較し、結果を平均するPCFで滑らかにできます。

```text
visibility = 周辺サンプルの「光が届く」結果の平均
shadowFactor = 1 - visibility
```

サンプル数と品質は上がりますが、テクスチャ参照コストも増えます。

## 20. Cascaded Shadow Maps

近距離から遠距離まで1枚で覆うと近くの密度が不足します。視錐台を近・中・遠へ分け、各区間にマップを割り当てる方法がCSMです。

```cpp
SetUseShadowMap(0, nearShadowMap);
SetUseShadowMap(1, middleShadowMap);
SetUseShadowMap(2, farShadowMap);
DrawWorld();
SetUseShadowMap(0, -1);
SetUseShadowMap(1, -1);
SetUseShadowMap(2, -1);
```

DXライブラリの使用スロット0～2を利用できます。分割距離は線形と対数分割を混ぜ、境界をブレンドし、各範囲をテクセルへスナップします。

## 21. CasterとReceiverの選別

- 遠い小物はCasterから外す。
- 影を落とさないVFXやUIを除外する。
- 葉や金網はAlpha Testで切り抜いた輪郭を反映する。
- 半透明の影は専用近似を検討する。
- 画面外でも画面内へ影を落とすCasterは残す。

通常描画と影描画で別の描画リストを作ります。

## 22. アニメーションモデルの影

キャラクターは現在のボーン姿勢で影パスにも描きます。影と通常描画で別々に時間を進めず、同じ確定済み姿勢を参照します。

```text
Update animation once
  -> build final bone palette once
  -> shadow pass uses palette
  -> main pass uses same palette
```

## 23. 接地影と簡易影

足元の接触感を補うため、薄い楕円のBlob Shadowを使えます。レイキャストで接地点・法線・高さを求め、浮いているほど透明にします。段差や壁を無視する弱点があるため、主影の代用品ではなく補助です。

## 24. 更新頻度とキャッシュ

- 主役の近距離影は毎フレーム更新する。
- 静的背景は結果を再利用する。
- 遠距離カスケードは数フレームに一度更新する。
- ライトもCasterも動かなければ更新しない。
- 複数マップの更新をフレームへ分散する。

品質設定には解像度だけでなく、枚数・距離・更新頻度・Caster数を含めます。

## 25. 複数ライトの影予算

影付きライトごとにCasterの再描画が必要です。主方向光、主役へ寄与する光、画面占有率の高い光、演出上重要な光の順に予算を割り当てます。影なしライトと簡易影も組み合わせます。

## 26. Render Stateを漏らさない

ライト、シャドウマップ、描画先、ブレンド、カリングは描画状態です。各パスが必要状態を設定し、終了時に解除します。「前のパスが設定したはず」という暗黙依存を作りません。

## 27. デバッグ可視化

- 平行光の方向を線で描く。
- 点光源の範囲を球で描く。
- スポット光の円錐を描く。
- 頂点法線を線で描く。
- Draw Areaをボックスで描く。
- `TestDrawShadowMap` で深度内容を表示する。
- Caster、Receiver、カスケードを色分けする。

マップが真っ白・真っ黒なら、方向、範囲、Caster描画、深度精度を順に疑います。

## 28. 計測項目

- 影パスのCPU・GPU時間。
- Caster数・Draw Call数・三角形数。
- シャドウマップ合計VRAM。
- ライト候補数と採用数。
- 更新した影と再利用した影の枚数。
- PCFサンプル数とスキニング回数。

平均だけでなく最大値と負荷が跳ねたフレームも調べます。

## 29. よくある不具合

### モデルが暗い

影を一度無効化して明るくなるなら影側、変わらなければライト・法線・材質・色空間側を調べます。

### 影が出ない

- SetupとEndの間にCasterを描いていない。
- 通常描画前に `SetUseShadowMap` していない。
- CasterかReceiverがDraw Area外にある。
- 光方向が逆、Biasが大きすぎる、描画リストから漏れている。

### 影が揺れる

- 範囲が光空間のテクセル境界を毎フレーム跨いでいる。
- カスケードが境界で往復している。
- 影と通常パスのアニメーション時刻が違う。

### 影が欠ける

Receiverだけでなく、光方向上流にあるCasterもDraw Areaへ含めます。

## 30. 設定のデータ化

```cpp
struct DirectionalLightDefinition final
{
    VECTOR direction{VGet(0.0f, -1.0f, 0.0f)};
    COLOR_F diffuse{GetColorF(1.0f, 1.0f, 1.0f, 1.0f)};
    bool castsShadow{true};
    int shadowResolution{2048}; // 読込時に2の累乗か検査する。
    float shadowBias{0.0005f};
};
```

方向がゼロでないか、範囲が正しいかも検証します。

## 31. ゲーム側と描画側の分離

```text
Gameplay/VFX -> LightRequestを登録
LightingSystem -> 可視性・距離・優先度を評価し最大3個へ選抜
Renderer -> DXライブラリのハンドルへ反映
```

ゲーム側をAPIの同時ライト数へ直接依存させません。Update完了後に描画スナップショットを作り、全描画パスが同じ姿勢とリストを読みます。

## 32. テスト

- ライトスコア順、最大3個、同点時の決定順。
- 解像度が2の累乗か。
- CSM分割距離が単調増加するか。
- Draw Areaが必要Boundsを含むか。
- 品質設定から影枚数と更新間隔が正しく出るか。
- 無効ハンドルを二重削除しないか。
- 固定カメラ・時刻による基準画像比較。

## 33. 実装チェックリスト

- [ ] ライト方向を正規化した。
- [ ] 標準ライト込みで有効ライトが3個以下である。
- [ ] ハンドル生成失敗を検査した。
- [ ] 所有者が資源を一度だけ削除する。
- [ ] マップ寸法が2の累乗である。
- [ ] 影設定をSetupより前に行う。
- [ ] 必ずEndへ到達する。
- [ ] 使用後に各スロットを `-1` で解除する。
- [ ] Draw Areaを必要最小限にした。
- [ ] Biasを複数の角度・距離で確認した。
- [ ] 動的モデルの姿勢を両パスで共有した。
- [ ] ライト範囲とマップを可視化できる。
- [ ] CPU・GPU・VRAMを計測した。

## 34. 練習課題

1. 平行光の方向を回転し、Diffuse変化を観察する。
2. 点光源の減衰係数を一つずつ変える。
3. スポット光の内外角度を可視化する。
4. 512、1024、2048のマップ品質と負荷を比較する。
5. Draw Areaを広げ、密度低下を観察する。
6. Biasを動かし、AcneとPeter Panningを確認する。
7. 近・中・遠の3カスケードを色分けする。
8. ライト候補10個から重要な3個を安定選抜する。
9. 静的影をキャッシュしGPU時間を比較する。
10. `TestDrawShadowMap` をデバッグ画面へ組み込む。

## 35. 理解確認

1. 法線を正規化する理由は何ですか。
2. Ambientが強すぎると立体感が消える理由は何ですか。
3. Point Lightに距離減衰が必要な理由は何ですか。
4. 4個以上のライトを有効にすべきでない理由は何ですか。
5. 光源視点の深度を保存する理由は何ですか。
6. Draw Areaを広げると影が粗くなる理由は何ですか。
7. Biasが小さい場合と大きい場合の症状は何ですか。
8. CSMが近距離品質を改善する理由は何ですか。
9. 両パスで同じボーン姿勢を使う理由は何ですか。
10. 描画状態の解除忘れが後続パスを壊す理由は何ですか。

## 36. 到達点

- 法線、Diffuse、Specular、材質の関係を説明できる。
- 3種類のライトを使い分けられる。
- 最大3個の有効ライトを決定的に選べる。
- 影の生成・使用・解除を正しい順で行える。
- Acne、Peter Panning、揺れ、粗さを切り分けられる。
- CSM、選別、更新頻度で品質と負荷を設計できる。
- RAIIと可視化で壊れにくい描画処理を作れる。

## 37. 公式リファレンス

- [3Dライト関係関数](https://dxlib.xsrv.jp/function/dxfunc_3d_light.html)
- [3Dシャドウマップ関係関数](https://dxlib.xsrv.jp/function/dxfunc_3d_shadow.html)
- [3D関係関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)

戻り値、引数の向き、呼び出せる期間、対応環境を必ず公式資料で再確認してください。
