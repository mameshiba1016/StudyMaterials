# DXライブラリ：Shader・Constant・Render State

この章では、GPUへどの処理を実行させ、CPU側の値をどう渡し、描画状態をどう管理するかを学びます。シェーダーは特殊効果だけの機能ではありません。頂点を画面へ運び、各ピクセルの色を決める描画処理の中核です。

> API仕様はDXライブラリ公式リファレンスを基準にしています。Direct3D 9用とDirect3D 11用の仕組みを混同しないでください。

## 1. CPUとGPUの役割

```text
CPU
  -> 描画物、シェーダー、定数、テクスチャ、描画状態を選ぶ
  -> Draw Callを発行する
GPU
  -> 頂点を処理する
  -> 三角形をラスタライズする
  -> ピクセル候補を処理する
  -> Depth・Blend判定を経てRender Targetへ書く
```

CPUがピクセルを一個ずつ塗るのではなく、GPUが大量の頂点・ピクセルを並列処理します。

## 2. グラフィックスパイプライン

基本的な流れは次のとおりです。

1. Vertex Bufferから頂点属性を読む。
2. Vertex Shaderで座標・法線・UVなどを変換する。
3. 三角形を組み立てる。
4. 画面を覆うピクセル候補を生成する。
5. Pixel Shaderで各候補の色を計算する。
6. Depth Testで前後関係を判定する。
7. Blendで既存色と新しい色を合成する。
8. Render Targetと必要ならDepth Bufferへ書く。

処理順を理解すると「シェーダーの色は正しいのに表示されない」原因を切り分けられます。

## 3. Vertex Shader

Vertex Shaderは頂点ごとに実行されます。最重要の仕事はローカル座標をクリップ座標へ変換することです。

```hlsl
cbuffer PerObject : register(b0)
{
    float4x4 worldViewProjection;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    // HLSL側の行列規約とC++側の転置有無を必ず統一する。
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);
    output.uv = input.uv;
    return output;
}
```

## 4. Pixel Shader

Pixel Shaderはラスタライズされたピクセル候補ごとに実行されます。

```hlsl
Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

cbuffer PerMaterial : register(b1)
{
    float4 tintColor;
};

float4 main(VSOutput input) : SV_TARGET
{
    float4 textureColor = colorTexture.Sample(colorSampler, input.uv);
    return textureColor * tintColor;
}
```

透明度を返しても、Blend Stateが無効なら背景と混ざりません。シェーダー出力と固定機能の状態は別です。

## 5. Semanticは入出力の契約

`POSITION`、`NORMAL`、`TEXCOORD0`、`SV_POSITION`、`SV_TARGET` はデータの意味をGPUへ伝えます。C++が送る頂点形式とHLSL入力のSemantic・型・順序が一致しなければ、座標やUVが壊れます。

DXライブラリの `DrawPolygon3DToShader` などは入力頂点形式が決められています。公式リファレンスに記載された `VERTEX3DSHADER` 等のレイアウトにHLSL側を合わせます。

## 6. シェーダーソースとバイナリ

`LoadVertexShader` と `LoadPixelShader` はHLSLソースを直接読みません。DXライブラリ付属の `Tool/ShaderCompiler` 等で事前コンパイルしたバイナリを読み込みます。

```cpp
const int vertexShader = LoadVertexShader("Data/Shader/BasicVS.vso");
const int pixelShader  = LoadPixelShader("Data/Shader/BasicPS.pso");

if (vertexShader == -1 || pixelShader == -1)
{
    // パス、対象Direct3D版、コンパイル結果をログへ残す。
    throw std::runtime_error("Shader binary loading failed");
}
```

Direct3D 9用とDirect3D 11用のバイナリは別物です。

## 7. Direct3D版の指定と確認

`SetUseDirect3DVersion` は `DxLib_Init` より前に呼ばなければ効果がありません。また、環境が指定版に対応しなければ別版や非Direct3Dへフォールバックする可能性があります。

```cpp
SetUseDirect3DVersion(DX_DIRECT3D_11); // 必ず初期化前。

if (DxLib_Init() == -1)
{
    return -1;
}

// 要求値ではなく、実際に採用された版を初期化後に検査する。
if (GetUseDirect3DVersion() != DX_DIRECT3D_11)
{
    DxLib_End();
    return -1;
}
```

`GetValidShaderVersion()` が0ならプログラマブルシェーダーは使用できません。返り値はシェーダーモデル版を100倍した値です。

## 8. Shader HandleのRAII

同時に保持できるシェーダー数には限りがあります。不要になったら `DeleteShader` で解放します。

```cpp
class UniqueShader final
{
public:
    explicit UniqueShader(int handle = -1) noexcept : handle_(handle) {}
    ~UniqueShader() { Reset(); }

    UniqueShader(const UniqueShader&) = delete;
    UniqueShader& operator=(const UniqueShader&) = delete;

    UniqueShader(UniqueShader&& other) noexcept
        : handle_(std::exchange(other.handle_, -1)) {}

    UniqueShader& operator=(UniqueShader&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = std::exchange(other.handle_, -1);
        }
        return *this;
    }

    void Reset() noexcept
    {
        if (handle_ != -1)
        {
            DeleteShader(handle_);
            handle_ = -1;
        }
    }

    [[nodiscard]] int Get() const noexcept { return handle_; }

private:
    int handle_{-1};
};
```

`InitShader` による全削除と個別RAIIを無計画に併用しません。

## 9. シェーダーを描画へ設定する

```cpp
SetUseVertexShader(vertexShaderHandle);
SetUsePixelShader(pixelShaderHandle);
SetUseTextureToShader(0, textureHandle); // HLSL側の対応スロットへ設定。

DrawPolygonIndexed3DToShader(
    vertices,
    vertexCount,
    indices,
    triangleCount);
```

シェーダー、テクスチャ、定数、Render Stateをすべて設定してからDraw Callを発行します。

## 10. Shader Constantとは何か

フレームごと・物体ごと・材質ごとに変化する小さな値をGPUへ渡します。

- View・Projection行列。
- World行列。
- カメラ位置、時間、画面サイズ。
- ライト方向・色。
- 材質色、粗さ、発光強度。
- エフェクトの進行率。

頂点列のような大容量データと、頻繁に渡す小さな定数を分けます。

## 11. Direct3D 9の定数レジスタ

`SetVSConstF`、`SetVSConstFMtx`、`SetPSConstF` などはDirect3D 9用であり、Direct3D 11では効果がありません。

```cpp
FLOAT4 color{};
color.x = 1.0f;
color.y = 0.5f;
color.z = 0.2f;
color.w = 1.0f;

SetPSConstF(0, color); // HLSLのregister(c0)へ対応。

// 使用後はDXライブラリ側の上書き状態を無効化する。
ResetPSConstF(0, 1);
```

Vertex ShaderのFLOAT4レジスタ番号は0～255です。ただしDXライブラリ内部利用レジスタとの衝突を公式資料で確認します。

## 12. Direct3D 11のConstant Buffer

Direct3D 11では定数バッファを作成し、CPU側領域へ書き、更新を反映し、対象Shader Stageのスロットへ設定します。

```cpp
struct alignas(16) PerFrameConstants final
{
    FLOAT4 cameraPosition;
    FLOAT4 lightDirection;
    FLOAT4 lightColor;
};

int constantBuffer = CreateShaderConstantBuffer(sizeof(PerFrameConstants));
if (constantBuffer == -1)
{
    throw std::runtime_error("Constant buffer creation failed");
}

auto* data = static_cast<PerFrameConstants*>(
    GetBufferShaderConstantBuffer(constantBuffer));

if (data == nullptr)
{
    throw std::runtime_error("Constant buffer mapping failed");
}

data->cameraPosition = cameraPosition;
data->lightDirection = lightDirection;
data->lightColor = lightColor;

// CPU側の変更をGPUから利用できる状態へ反映する。
if (UpdateShaderConstantBuffer(constantBuffer) == -1)
{
    throw std::runtime_error("Constant buffer update failed");
}

// HLSLのregister(b0)、Vertex Shaderへ対応させる。
SetShaderConstantBuffer(constantBuffer, DX_SHADERTYPE_VERTEX, 0);
```

## 13. Constant Bufferの寿命

`SetShaderConstantBuffer` の直後に削除してはいけません。描画時にバッファが存在する必要があります。シェーダーやMaterialの読み込み時に作成し、そのシェーダーを使い終えるまで保持します。

```cpp
class UniqueShaderConstantBuffer final
{
public:
    explicit UniqueShaderConstantBuffer(std::size_t byteSize)
        : handle_(CreateShaderConstantBuffer(static_cast<int>(byteSize)))
    {
        if (handle_ == -1)
            throw std::runtime_error("Constant buffer creation failed");
    }

    ~UniqueShaderConstantBuffer()
    {
        if (handle_ != -1) DeleteShaderConstantBuffer(handle_);
    }

    UniqueShaderConstantBuffer(const UniqueShaderConstantBuffer&) = delete;
    UniqueShaderConstantBuffer& operator=(const UniqueShaderConstantBuffer&) = delete;

    [[nodiscard]] int Get() const noexcept { return handle_; }

private:
    int handle_{-1};
};
```

## 14. 16-byte Packing

HLSLのConstant Bufferは基本的に16byte単位のレジスタへ詰められます。C++の自然な構造体配置と一致するとは限りません。

```hlsl
cbuffer MaterialData : register(b1)
{
    float4 baseColor; // 16byte。
    float roughness;  // 次の16byte領域へ配置。
    float metallic;
    float emission;
    float padding;    // 4要素を揃える。
};
```

```cpp
struct alignas(16) MaterialConstants final
{
    FLOAT4 baseColor;
    float roughness{};
    float metallic{};
    float emission{};
    float padding{};
};

static_assert(sizeof(MaterialConstants) % 16 == 0);
```

`bool`、`float3`、配列、行列、入れ子構造体は特に配置ミスが起きやすいため、明示的な型とPaddingを使います。

## 15. 行列の転置問題

C++とHLSLで行ベクトル・列ベクトル、row-major・column-majorの規約が違うと、平行移動が消える、回転が逆になるなどの不具合が出ます。

次をプロジェクトで一つに固定します。

- ベクトルを行列の左と右のどちらから掛けるか。
- HLSLをrow-majorとしてコンパイルするか。
- CPU側で転置して渡すか。
- `mul(vector, matrix)` と `mul(matrix, vector)` のどちらを使うか。

単位行列だけでは転置ミスを検出できないため、回転・非一様スケール・平行移動を含む行列でテストします。

## 16. 更新頻度別の定数設計

```text
PerFrame    : View、Projection、Camera、Time
PerPass     : Light、Shadow、Fog
PerMaterial : Color、Roughness、Texture flags
PerObject   : World、Object ID、Animation parameters
```

変化頻度の違う値を一つの巨大バッファへ詰めると、一部変更のたびに全体を更新します。頻度別に分割し、同じ値の再送を減らします。

## 17. TextureとSampler

Textureは画像データ、SamplerはUVが画素間・範囲外へ来たときの読み方です。

- Nearest：最寄りの画素。ドット絵向け。
- Bilinear：周辺4画素を補間。
- Trilinear：Mip間も補間。
- Wrap：UVを繰り返す。
- Clamp：端の色を伸ばす。
- Border：範囲外へ境界色を返す。

`SetUseTextureToShader(slot, handle)` のSlotとHLSL側のTexture登録番号を一致させます。使用後に不要なTextureを `-1` で解除し、前のDraw Callへの依存を防ぎます。

## 18. Render Target

Render TargetはPixel Shader出力を書き込む画像です。画面だけでなく `MakeScreen` で作った描画可能画像へ出力し、後段のシェーダーで入力Textureとして利用できます。

```text
Scene -> HDR Render Target
HDR Target -> Bloom extraction
Bloom -> Blur
Scene + Bloom -> Tone mapping
Tone mapped result -> Back Buffer
```

同じ画像を入力Textureと出力Render Targetへ同時設定するFeedback Loopを作ってはいけません。必要ならPing-Pong用に2枚を交互使用します。

## 19. Multiple Render Targets

`SetRenderTargetToShader` で複数出力先を扱える場合、Pixel Shaderから複数の情報を同時出力できます。

```hlsl
struct PSOutput
{
    float4 color  : SV_TARGET0;
    float4 normal : SV_TARGET1;
};
```

出力数・形式・対応環境を検査し、使用後は各スロットを `-1` へ戻します。

## 20. Depth TestとDepth Write

この二つは別の状態です。

- Depth Test：保存済み深度と比較し、色を書けるか決める。
- Depth Write：描画に通った新しい深度をバッファへ保存する。

```cpp
// 不透明物：比較も書込みも有効。
SetUseZBuffer3D(TRUE);
SetWriteZBuffer3D(TRUE);

DrawOpaqueGeometry();

// 半透明物：既存の不透明物とは比較するが、通常は深度を書かない。
SetUseZBuffer3D(TRUE);
SetWriteZBuffer3D(FALSE);
DrawSortedTransparentGeometry();
```

公式仕様上、これらは `DrawPolygon3D` 等の3D図形に対する設定で、MV1モデルには専用設定があります。

## 21. 透明描画の順序

半透明物をDepth Write有効で先に描くと、奥の半透明物が消えます。一般的な順序は次のとおりです。

1. 不透明物を手前・奥の順序に依存せず描く。
2. Alpha Test/Cutoutを描く。
3. 半透明物をカメラから遠い順に描く。
4. 加算エフェクトを用途に応じて描く。
5. UIを描く。

完全な透明順序は三角形同士で循環する場合があり、単純なObject Sortだけでは解けません。

## 22. Blend State

Blendは概念的に次の計算を行います。

```text
result = sourceColor × sourceFactor + destinationColor × destinationFactor
```

- Alpha Blend：通常の半透明。
- Add：炎、光、火花などの加算。
- Multiply：暗くする表現。
- No Blend：不透明。

```cpp
SetDrawBlendMode(DX_BLENDMODE_ADD, 180);
DrawEffect();
SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255); // 必ず既知状態へ戻す。
```

Premultiplied AlphaかStraight Alphaかで正しいBlend係数が異なります。

## 23. Culling State

Back-face Cullingはカメラへ裏を向けた三角形を描かず負荷を減らします。頂点の時計回り・反時計回りというWinding規約が逆だと、表面が消えます。

両面材質でCullingを切ると描画量が増え、裏面の法線やライティングも考慮が必要です。状態を無効にして隠すのではなく、モデルのWinding・座標変換の符号を調べます。

## 24. RasterizerとWireframe

Rasterizer Stateは塗りつぶし、カリング、Depth Biasなどを制御します。Wireframeは三角形分割、破綻、過剰な細分化を見るデバッグに有効です。製品表現としての輪郭線は、Wireframeとは別の手法を使います。

## 25. 描画状態はグローバルな隠れ入力

関数引数が同じでも、直前のBlend・Depth・Culling・Shader・Textureによって結果が変わります。この「隠れ入力」が描画不具合を難しくします。

```cpp
struct PipelineState final
{
    int vertexShader{-1};
    int pixelShader{-1};
    bool depthTest{true};
    bool depthWrite{true};
    int blendMode{DX_BLENDMODE_NOBLEND};
    int cullMode{};
};
```

描画前に必要状態を宣言的に適用し、前の描画に依存しない設計へ近づけます。

## 26. State Cache

同じ状態を何度もAPIへ設定するとCPU負荷が増えます。現在値をキャッシュし、変化したときだけ反映します。

```cpp
void Renderer::SetPixelShader(int requestedHandle)
{
    if (currentPixelShader_ == requestedHandle)
        return; // GPU状態が同じなら冗長な呼び出しを省く。

    SetUsePixelShader(requestedHandle);
    currentPixelShader_ = requestedHandle;
}
```

外部コードがキャッシュを通さず直接APIを呼ぶと実状態とキャッシュがずれます。描画APIの入口をRendererへ集約します。

## 27. MaterialとShader Variation

MaterialはShader、Texture、定数、Blend、Culling等の組合せです。

```cpp
struct Material final
{
    int vertexShader{-1};
    int pixelShader{-1};
    std::array<int, 4> textures{-1, -1, -1, -1};
    FLOAT4 baseColor{};
    bool transparent{};
    bool doubleSided{};
};
```

機能ごとにShaderを無制限に増やすと組合せ爆発が起きます。静的分岐、動的分岐、データ駆動のどれを使うか、実測して選びます。

## 28. Render QueueとSort Key

不透明物はShader・Material・Textureが近い順へ並べると状態変更を減らせます。透明物は奥から手前という順序が優先です。

```text
Opaque sort key:
[pass][shader][material][texture][depth]

Transparent sort key:
[pass][reverse depth][material]
```

64bit整数へ各フィールドを詰めると高速に比較できますが、ビット幅と最大値を明文化します。

## 29. Shader Hot Reload

開発中にシェーダーを再コンパイル・再読込できると調整が速くなります。

1. ファイル変更を検知する。
2. 別ハンドルへ新バイナリを読む。
3. 読込成功後だけ参照を交換する。
4. 古いハンドルを安全な時点で削除する。
5. 失敗時は旧Shaderを維持し、コンパイルログを表示する。

描画中のハンドルを先に削除してはいけません。

## 30. エラーフォールバック

Shader読込失敗時に何も描かないと、Asset欠落かShader不具合か分かりません。鮮やかなマゼンタ等のError Materialを表示し、ファイル名・Direct3D版・Material名をログへ残します。

## 31. Post Effectの基本

Post EffectはシーンをRender Targetへ描き、その画像を全画面三角形・四角形で処理します。

```hlsl
float4 main(VSOutput input) : SV_TARGET
{
    float3 color = sceneTexture.Sample(sceneSampler, input.uv).rgb;
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    return float4(lerp(color, luminance.xxx, grayscaleAmount), 1.0f);
}
```

複数効果を無計画に別パス化すると帯域負荷が増えます。解像度、Format、パス統合、Half Resolutionを検討します。

## 32. Action表現への応用

- Hit Flash：Material定数で一時的に白・赤へ寄せる。
- Dissolve：Noise Textureと閾値で消滅境界を作る。
- Rim Light：法線と視線の内積で輪郭を強調する。
- Afterimage：過去姿勢を透明・加算で描く。
- Screen Distortion：背景Render TargetのUVをずらす。
- Color Grading：場面・必殺演出の色調を変える。
- Outline：裏面拡張または画面空間Edge Detectionを使う。

演出中もDepth・Blend・Cullingを明示し、状態漏れを防ぎます。

## 33. Debug Visualization

- UVをRGBとして表示する。
- World Normalを0～1へ変換して表示する。
- Depthを線形化して表示する。
- Texture Slotごとに単独表示する。
- Constant値を画面へ表示する。
- Draw CallごとにMaterial ID色を出す。
- Overdrawを加算表示する。
- Render Targetを一覧表示する。

最終色だけでなく、中間値を直接見るのがShaderデバッグの基本です。

## 34. よくある不具合：何も描かれない

```text
Shader handle valid?
  -> Direct3D版は一致？
  -> 頂点形式とSemanticは一致？
  -> クリップ座標のwは正常？
  -> Cullingで全消去？
  -> Depth Testで全不合格？
  -> Render Targetは正しい？
  -> Pixel Shader出力は有限値？
```

最小Shaderで固定色を返し、Vertex段・Pixel段・固定状態を一つずつ戻します。

## 35. よくある不具合：定数がずれる

- HLSLとC++の構造体順序が違う。
- 16byte Packingを無視した。
- Constant BufferのSlotが違う。
- VertexとPixelのStage指定が違う。
- CPU側で書いただけでUpdateしていない。
- 行列の転置規約が違う。
- バッファを描画前に削除した。

`static_assert`、明示Padding、Slot定数、デバッグ色で切り分けます。

## 36. よくある不具合：透明がおかしい

- Depth Writeが有効なまま。
- 奥から手前へSortしていない。
- Alpha方式とBlend係数が不一致。
- TextureのAlphaまたは色空間が違う。
- 不透明パスと透明パスが混ざっている。
- 状態復元を忘れ、後続描画へBlendが残った。

## 37. パフォーマンス計測

- Draw Call数。
- Shader・Texture・Blend切替回数。
- Constant Buffer更新回数と転送byte数。
- Render Target切替回数。
- 各PassのCPU・GPU時間。
- Pixel ShaderのTexture Sample数。
- Overdrawと透明描画面積。
- Full/Half/Quarter Resolutionの差。

Shaderコードの行数だけで軽さを判断せず、対象GPUで計測します。

## 38. テスト可能な部分

- MaterialからPipelineStateへの変換。
- Sort Keyの順序とビット範囲。
- C++ Constant構造体のサイズ・Alignment。
- Direct3D版ごとの正しいShaderパス選択。
- 無効HandleのFallback選択。
- Pass終了時の既知Stateへの復元。
- Render Graphで同一画像を同時入出力しない検査。

固定入力による画面比較テストでは、UV、法線、Depth、最終色を別々に保存すると原因を追いやすくなります。

## 39. 実装チェックリスト

- [ ] Direct3D版指定を `DxLib_Init` 前に行った。
- [ ] 初期化後に実際のDirect3D版を確認した。
- [ ] 対応版のコンパイル済みShaderを読み込んだ。
- [ ] Shader Handleの生成失敗と寿命を管理した。
- [ ] 頂点形式とHLSL Semanticを一致させた。
- [ ] Direct3D 9と11のConstant APIを混同していない。
- [ ] HLSL/C++構造体を16byte規則へ合わせた。
- [ ] 行列の掛け方・転置規約を統一した。
- [ ] Buffer書換後にUpdateした。
- [ ] Shader StageとSlotを一致させた。
- [ ] TextureとRender Targetの同時入出力を避けた。
- [ ] Depth TestとDepth Writeを別々に設定した。
- [ ] 透明物を適切にSortした。
- [ ] Blend・Culling・Shader・Texture状態を後続へ漏らしていない。
- [ ] Draw Callと状態変更を計測した。
- [ ] Error Materialと中間値表示を用意した。

## 40. 練習課題

1. UVを色として表示するVertex/Pixel Shaderを作る。
2. Constantで色を時間変化させる。
3. 行列を渡して三角形を回転させる。
4. D3D11 Constant Bufferへ複数のFLOAT4を渡す。
5. Depth TestとWriteの4通りを比較する。
6. Alpha・Add・MultiplyのBlend結果を比較する。
7. CullingとWindingを切り替えるデバッグ画面を作る。
8. Render Targetを使ったグレースケール効果を作る。
9. Ping-Pong Bufferで2Pass Blurを作る。
10. Shader/Material順Sort前後の状態変更数を測る。
11. Error MaterialとHot Reloadを実装する。
12. Hit Flash、Rim Light、Dissolveから一つを実装する。

## 41. 理解確認

1. Vertex ShaderとPixel Shaderの役割は何ですか。
2. Semanticが一致しないと何が起きますか。
3. Shaderソースを直接Loadできない理由は何ですか。
4. Direct3D版を初期化後にも確認する理由は何ですか。
5. D3D9の定数レジスタとD3D11のConstant Bufferの違いは何ですか。
6. 16byte Packingが必要な理由は何ですか。
7. Depth TestとDepth Writeの違いは何ですか。
8. 半透明物を奥から描く理由は何ですか。
9. State Cacheを迂回してAPIを呼ぶと何が壊れますか。
10. Render Targetを同時に入力と出力へ使えない理由は何ですか。

## 42. この章の到達点

- GPU Pipelineと各Shader Stageを説明できる。
- コンパイル済みShaderを安全に読み込み、所有できる。
- D3D9/D3D11に応じて正しいConstant方式を選べる。
- HLSLとC++のLayout・行列規約を一致させられる。
- Depth・Blend・Culling・Texture・Render Targetを明示管理できる。
- Material、Render Queue、State Cacheで状態変更を整理できる。
- 中間値可視化と計測でShader不具合を切り分けられる。
- Post Effectと戦闘演出へ基礎技術を応用できる。

## 43. 公式・関連資料

- [DXライブラリ：プログラマブルシェーダー関係関数](https://dxlib.xsrv.jp/function/dxfunc_3d_shader.html)
- [DXライブラリ：3D描画関係関数](https://dxlib.xsrv.jp/function/dxfunc_3d_draw.html)
- [DXライブラリ：3D関係関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ公式掲示板：DirectX 11の定数バッファ](https://dxlib.xsrv.jp/cgi/patiobbs/patio.cgi?mode=view&no=4032)
- [DXライブラリ：オリジナルシェーダーによる3Dモデル描画](https://dxlib.xsrv.jp/program/dxprogram_3DModelShaderBase.html)

戻り値、対応Direct3D版、Slot、頂点形式、DXライブラリ内部定数との衝突を公式資料で再確認してください。
