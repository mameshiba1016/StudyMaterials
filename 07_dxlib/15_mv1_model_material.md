# 第15章 MV1 Model・Material

MV1 ModelはMeshだけではなく、Frame階層、Material、Texture、Animation、場合によっては物理情報をまとめたResourceです。本章では基礎DataとInstance状態を分離し、同一Characterを効率よく複製・描画します。

## 1. Modelの構成

```text
Model
├─ Frame hierarchy（Bone/Node）
├─ Mesh
│  └─ Triangle/Vertex
├─ Material
│  └─ Texture・色・反射特性
├─ Animation
└─ Physics metadata（形式による）
```

## 2. Load

```cpp
const int model = MV1LoadModel("assets/models/player.mv1");
if (model == -1)
{
    // Path、形式、依存Texture、文字Code、Memoryを調査。
    return -1;
}

MV1DrawModel(model);
MV1DeleteModel(model);
```

Load失敗Handleを他のMV1関数へ渡しません。

## 3. Model Handle

Model Handleも整数ですがGraph/Sound Handleとは別型です。

```cpp
struct ModelTag;
using ModelHandle = RawHandle<ModelTag>;

struct ModelDeleter final
{
    using Handle = ModelHandle;
    static void Destroy(Handle h) noexcept
    {
        if (h.IsValid()) MV1DeleteModel(h.Value());
    }
};

using UniqueModel = UniqueHandle<ModelDeleter>;
```

## 4. DX終了前の破棄

公式には`DxLib_End`で残存Modelも削除されますが、個別RAIIと全体自動削除を寿命上混在させません。Application Scope内でModelを先に破棄し、所有状態を明確にします。

## 5. 基礎DataとInstance

同じ見た目の敵を10体出すために同じFileを10回Loadすると、Load時間とMemoryが増えます。基礎Modelを一度Loadし、Instanceを複製します。

## 6. Duplicate

```cpp
const int source = MV1LoadModel("assets/models/enemy.mv1");
const int instance = MV1DuplicateModel(source);
if (source == -1 || instance == -1)
{
    // 作成済みHandleだけ安全に破棄する。
}
```

`MV1DuplicateModel`は公式仕様上、同じ基礎Dataを使用するModel Handleを作ります。返されたHandleにも個別の削除責任があります。

## 7. Prototype Cache

```cpp
struct ModelPrototype final
{
    UniqueModel source{};
    std::string normalizedPath{};
    std::unordered_map<std::string, int> frameIndices{};
};

struct ModelInstance final
{
    UniqueModel model{};
    std::shared_ptr<const ModelPrototype> prototype{};
};
```

Prototypeが基礎HandleとMetadataを所有し、Instanceが複製Handleを所有します。

## 8. Duplicate失敗のFactory

```cpp
[[nodiscard]] std::optional<ModelInstance> CreateInstance(
    std::shared_ptr<const ModelPrototype> prototype)
{
    const int raw = MV1DuplicateModel(prototype->source.Get().Value());
    if (raw == -1) return std::nullopt;

    return ModelInstance{
        UniqueModel{ModelHandle{raw}},
        std::move(prototype)
    };
}
```

## 9. 毎Frame複製しない

複製はSpawn時、削除はDespawn時に行います。描画のたびに作成・削除する必要はありません。Object Poolは計測で必要な場合に導入します。

## 10. Transform設定

```cpp
MV1SetPosition(handle, ToDx(transform.position));
MV1SetRotationXYZ(handle, VGet(pitch, yaw, roll));
MV1SetScale(handle, ToDx(transform.scale));
```

角度はRadianです。Euler順とModelの前方軸をAsset規約として固定します。

## 11. Matrix設定

```cpp
MV1SetMatrix(handle, worldMatrix);
```

公式仕様では単位行列以外を設定すると、以後PositionやScale等の設定が無視され、設定MatrixだけでLocal→World変換されます。TRS APIとMatrix APIを同じInstanceで混在させません。

## 12. Transform Mode

```cpp
enum class ModelTransformMode { TrsParameters, ExplicitMatrix };
```

Renderer AdapterがModeごとに一つの経路だけを適用します。

## 13. Asset座標規約

- Up軸は何か。
- Forward軸は+Zか-Zか。
- 左手/右手系。
- 1 Unitは何mか。
- Root位置は足元か原点か。
- Rotation適用済みか。

Import時に統一し、Game中の補正Matrixを乱立させません。

## 14. Scale

Modelごとに`0.01`等の補正を入れるより、DCC Export設定とImport規約を統一します。非一様ScaleはNormalとCollider、物理へ影響します。

## 15. Real-time PhysicsとScale

公式資料ではリアルタイム物理演算時の`MV1SetScale`に注意があり、正常な結果を得られない旨が示されています。使用形式と物理Modeの制約を確認し、Asset側Scaleを整えます。

## 16. Draw

```cpp
if (MV1DrawModel(handle) == -1)
{
    // Handle、Camera、Render State、Model状態をLogする。
}
```

Drawは状態を読む処理にし、Animation時間やCombatを進めません。

## 17. Frame

FrameはModel内の階層Nodeです。Bone、Socket、Mesh配置Nodeなどを含みます。Frame IndexはModelごとの番号で、別Modelへ流用しません。

## 18. Frame数と名前

```cpp
const int count = MV1GetFrameNum(handle);
if (count == -1) return;

for (int i = 0; i < count; ++i)
{
    const char* name = MV1GetFrameName(handle, i);
    // NULLならError。Library所有文字列を勝手に解放しない。
}
```

## 19. Frame検索

```cpp
const int rightHand = MV1SearchFrame(handle, "RightHand");
if (rightHand == -2)
{
    // 未発見。Asset契約違反またはOptional Frame。
}
else if (rightHand == -1)
{
    // API Error。
}
```

同名が複数ある場合は若い番号が返る公式契約なので、名前を一意にします。

## 20. Index Cache

Frame名検索を毎Frame行わず、Prototype検証時に必要Frameを検索してIndexを保存します。Hot Reload後はIndexが変わり得るため再構築します。

## 21. Frame親子

`MV1GetFrameParent`、`MV1GetFrameChildNum`、`MV1GetFrameChild`で階層を調べられます。親なしとErrorの戻り値を区別します。

## 22. Local Matrix

`MV1GetFrameLocalMatrix`はFrameの変換Matrix、`MV1GetFrameLocalWorldMatrix`はFrame LocalからWorldへのMatrixです。名前の似たAPIを混同しません。

## 23. Frame World Position

```cpp
const VECTOR handWorld = MV1GetFramePosition(handle, rightHand);
```

Weapon、VFX、Hitbox、頭上UIのAttachment位置に使えます。Animation適用後の取得時点を更新順で固定します。

## 24. Attachment

```cpp
struct ModelSocket final
{
    std::string frameName{};
    int cachedFrameIndex = -1;
    MATRIX localOffset = MGetIdent();
};
```

Bone MatrixへWeaponのLocal Offsetを合成します。行列規約と積順を既知値でTestします。

## 25. Frame Override

`MV1SetFrameUserLocalMatrix`で指定Frameの変換を上書きできます。公式仕様ではResetまで優先され、Animation Keyよりも設定Matrixが優先されます。

## 26. Overrideの解除

一時的なLook-atやBone補正を終えたら対応Reset APIで解除します。解除忘れはAnimationが動かない原因になります。

## 27. Procedural Bone

頭・上半身のAim、足IK、Weapon RecoilなどをAnimation結果へ加える場合、元Pose取得→補正→描画の順を決めます。Overrideが絶対値か差分かを明確にします。

## 28. Mesh

MeshはVertex/Indexの集合で、通常一つ以上のMaterialを参照します。Model全体より細かい表示・描画単位です。

## 29. Mesh情報

```cpp
const int meshCount = MV1GetMeshNum(handle);
for (int mesh = 0; mesh < meshCount; ++mesh)
{
    const int triangleCount = MV1GetMeshTriangleNum(handle, mesh);
    const int material = MV1GetMeshMaterial(handle, mesh);
}
```

戻り値Errorを確認し、開発時MetricsへTriangle数を表示します。

## 30. Mesh Visibility

```cpp
MV1SetMeshVisible(handle, helmetMeshIndex, FALSE);
```

装備差分、破壊部位、LOD表示へ使えます。Mesh IndexはAsset更新で変わり得るため、検証Dataから解決します。

## 31. Frame DrawとMesh Draw

`MV1DrawFrame`、`MV1DrawMesh`で部分描画できます。全体Drawとの重複描画を避けます。部分描画が実際に性能改善するか計測します。

## 32. Material

Materialは面の見え方を定義します。

- Diffuse/Base Color。
- Specular。
- Emissive。
- Opacity/Blend。
- TextureとSampling。
- Lighting有無。

使用Model形式とDX変換で対応範囲が異なります。

## 33. Mesh Color Scale

MeshごとのDiffuse、Specular、Emissive Scale APIがあります。Damage Flash、選択表示、発光強調に使えますが、元値へ戻す必要があります。

## 34. Model Instance状態

複製InstanceごとにTransform、Animation、Visibility、Color Scale等の可変状態を持てます。どの情報が基礎Data共有で、どれがInstance固有かを公式仕様で確認します。

## 35. Texture

Model Materialが参照するTexture PathはModel Fileからの相対Pathの場合があります。Package配置と作業Directoryを検証します。欠落TextureをLoad成功扱いで見落とさないようAsset検証を行います。

## 36. Sampling Filter

`MV1SetTextureSampleFilterMode`等でTexture Samplingを変更できます。Pixel風ならNearest、一般3DならLinear/Anisotropicなど用途に合わせます。Texture番号の意味を確認します。

## 37. Texture Address

Wrap、Clamp、MirrorはUV範囲外の読み方です。意図しないSeamや繰り返しはMaterial設定とUVを確認します。

## 38. UV Transform

Frame Texture Address TransformでUV移動・拡大等を行えるAPIがあります。流れるTextureや簡易Effectに使えます。状態Resetを忘れません。

## 39. Alpha TestとBlend

- Alpha Test/Cutout: 閾値で破棄。順序問題が少なく輪郭が硬い。
- Alpha Blend: 滑らかな半透明。奥から手前の描画順が必要。

髪、草、ガラスで使い分けます。

## 40. 半透明描画Mode

`MV1SetSemiTransDrawMode`で半透明要素を含む部分の描画Modeを設定できます。不透明部を先に、半透明部を後に描く方式はDepthと順序の問題を減らします。

## 41. 半透明Modelの順序

Model単位の距離Sortだけでは、Model内部の面順まで完全には解決できません。髪や衣装はMesh分割、Material設計、Cutout利用を検討します。

## 42. Opacity

Frame単位のOpacity設定APIがあります。Fade、Ghost表現に利用できますが、半透明描画順とShadowへの影響を確認します。

## 43. Render State漏洩

Model描画前に設定したBlend、Shader、Light、Fog、Semi-transparent Modeが次のModelへ影響する場合があります。Pass開始時に既知状態へ戻します。

## 44. Original Shader

`MV1SetUseOrigShader`等で独自Shader利用へ進めます。ModelのVertex Layout、Skinning、Material Constantを理解せずに切り替えると表示が崩れます。第19章で扱います。

## 45. Bounds

ModelのLocal BoundsをLoad時に取得・保存し、World TransformでCulling用Boundsへ変換します。Animationで手足がBounds外へ出ないよう余裕またはAnimation Boundsを使います。

## 46. Frustum Culling

World Bounding Sphere/AABBをCamera Frustumと判定し、外なら`MV1DrawModel`を省略します。描画しないこととAnimation・AI更新を止めることは別です。

## 47. Occlusion

Frustum内でも壁の裏で完全に見えないObjectがあります。Occlusion Query等は複雑で遅延もあるため、まずRoom/Portal、距離、手動Zoneなど安価な方法を検討します。

## 48. LOD

Camera距離または画面占有率に応じて低Detail Modelへ切り替えます。

```cpp
enum class LodLevel { High, Medium, Low, Culled };
```

距離境界へHysteresisを設け、行き来によるちらつきを防ぎます。

## 49. Screen Size基準LOD

距離だけではFOVやObject Sizeを反映できません。Projected BoundsのPixel Sizeを使うと画面上の見え方に合います。

## 50. LOD切替

瞬間切替、Crossfade、Dither Fadeがあります。半透明Crossfadeは描画数が一時的に2倍になり、順序問題も増えます。

## 51. Animation共有とLOD

LOD Model間でSkeleton名・階層を揃えるとAnimation状態を移しやすくなります。Bone削減LODではAttachmentに必要なBoneを残します。

## 52. Instancingとの違い

`MV1DuplicateModel`は基礎Data共有ですが、必ずGPU Instanced Drawになるとは限りません。大量ObjectのDraw Call数はProfilerで確認します。

## 53. Draw Command

```cpp
struct ModelDrawCommand final
{
    int modelHandle = -1;
    MATRIX world = MGetIdent();
    float cameraDistance = 0.0F;
    bool semiTransparent = false;
    DrawLayer layer = DrawLayer::Character;
};
```

Command実行までHandleを生存させます。Resource共有参照やFrame寿命を使います。

## 54. Sorting

- 不透明: State/Materialをまとめやすい。
- 半透明: 原則Cameraから遠い順。
- UI/Billboard: LayerとDepth契約。

正しい見た目を壊してまでMaterial順へ並べません。

## 55. UpdateとDraw

```text
Gameplay Transform更新
→ Animation Pose更新
→ Procedural Bone補正
→ Attachment/Collider同期
→ Bounds更新
→ Culling
→ Draw Command生成
→ 描画
```

BoneからColliderを取る時点を固定します。

## 56. Root TransformとRoot Motion

Model RootのAnimation移動とGameplay Character位置を二重に適用しないよう、どちらを正にするか決めます。Root Motionの詳細は次章で扱います。

## 57. Physics順序

公式資料ではModelのリアルタイム物理結果がPosition、Rotation、Animation時間設定で無効になる場合があり、それらの姿勢決定後に物理計算するよう注意されています。使用機能の更新順を公式契約へ合わせます。

## 58. Asset Validation

- 必須Frame名が一意に存在。
- Mesh/Material数が想定範囲。
- Textureが全て解決。
- Scale、Up、Forwardが規約通り。
- Boundsが正。
- Triangle/Bone数がBudget内。
- LOD間SkeletonとMaterial対応が整合。

## 59. Model Inspection Tool

Frame tree、Mesh→Material、Triangle数、Texture、Bounds、半透明状態を画面へ一覧表示します。Frameを選ぶとBasisと名前をWorldへ描画します。

## 60. Gizmo

```cpp
void DrawFrameAxis(int handle, int frameIndex, float length)
{
    const MATRIX world = MV1GetFrameLocalWorldMatrix(handle, frameIndex);
    const VECTOR origin = VTransform(VGet(0,0,0), world);
    const VECTOR x = VTransform(VGet(length,0,0), world);
    const VECTOR y = VTransform(VGet(0,length,0), world);
    const VECTOR z = VTransform(VGet(0,0,length), world);

    DrawLine3D(origin, x, GetColor(255,64,64));
    DrawLine3D(origin, y, GetColor(64,255,64));
    DrawLine3D(origin, z, GetColor(64,128,255));
}
```

非一様Scaleがある場合、軸長も変化します。

## 61. Hot Reload

新Prototypeを別HandleへLoad・検証し、成功後に交換します。既存Instanceを新Prototypeから再作成する場合、Transform、Animation State、装備、Visibilityを移行します。

## 62. Error Context

```text
event=ModelLoadFailed
assetId=enemy_basic
path=assets/models/enemy.mv1
scene=Battle
requiredFrame=RightHand
stage=FrameValidation
```

Load成功でもFrame検証失敗をResource失敗として扱えます。

## 63. よくある不具合

- Positionが効かない: 非単位`MV1SetMatrix`が有効。
- Weaponがずれる: Local/World Matrix、積順、更新時点が違う。
- AnimationしないBone: User Local MatrixをResetしていない。
- 同じ敵でMemory増加: 毎回LoadしDuplicateしていない。
- 半透明が破綻: 不透明/半透明PassとSort不足。
- Modelだけ逆向き: Asset Forward軸不一致。
- Physicsが崩れる: Scaleまたは更新順が公式制約と不一致。

## 64. Metrics

- Prototype/Instance数。
- Model Load/Duplicate時間。
- Visible/Culled数。
- Mesh、Triangle、Material数。
- Model Draw Call相当数。
- 半透明Model数。
- LOD別個数。
- Model/Texture推定Memory。

## 65. Test

- Duplicate失敗時にSourceを壊さない。
- Model Handleを一度だけ削除。
- 必須Frame未発見`-2`をErrorと区別。
- Frame CacheをHot Reloadで再構築。
- TRS ModeとMatrix Modeを混在させない。
- Culling境界とLOD Hysteresis。
- Attachmentの既知Matrix結果。

## 66. 設計チェックリスト

- [ ] Model Handleを型付きRAIIで所有する。
- [ ] 同じAssetはLoad一回＋Duplicateする。
- [ ] PrototypeとInstanceを分離した。
- [ ] TRSとMatrix設定を混在させない。
- [ ] AssetのUp/Forward/Scaleを統一した。
- [ ] Frame未発見とAPI Errorを区別した。
- [ ] Frame IndexをPrototypeでCacheした。
- [ ] User Frame Matrixを確実にResetする。
- [ ] 半透明を別順序で描く。
- [ ] Bounds、Frustum、LODを計測する。
- [ ] PhysicsのScale・更新順制約を確認した。

## 67. 理解確認問題

1. `MV1DuplicateModel`を使う利点は何か。
2. Duplicate Handleを削除する責任は誰にあるか。
3. 非単位`MV1SetMatrix`後にPositionが効かない理由は何か。
4. `MV1SearchFrame`の`-2`と`-1`の違いは何か。
5. Frame Local MatrixとLocal→World Matrixの違いは何か。
6. User Local MatrixがAnimationより優先される影響は何か。
7. Alpha Blend Modelを後で描く理由は何か。
8. DistanceだけのLODに不足する情報は何か。
9. DuplicateがGPU Instancingを保証しないのはなぜか。
10. Hot Reload時にFrame Indexを再検索する理由は何か。

## 68. 実践課題

1. Model用型付きRAIIを作る。
2. Prototype CacheとDuplicate Factoryを作る。
3. Frame tree/mesh/material Inspectorを作る。
4. RightHand SocketへWeapon Modelを付ける。
5. Mesh Visibilityで装備を切り替える。
6. 不透明・半透明の二Pass描画を作る。
7. Frustum Cullingと3段階LODを作る。
8. Frame BasisとBoundsをGizmo表示する。
9. Asset ValidationをScene開始前に実行する。
10. Hot Reload時にInstance状態を移行する。

## 69. 公式資料

- [DXライブラリ 3D関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [MV1 Model読込・複製・Transform](https://dxlib.xsrv.jp/function/dxfunc_3d_model_0.html)
- [MV1 Frame関数](https://dxlib.xsrv.jp/function/dxfunc_3d_model_2.html)
- [MV1 Mesh・Material関数](https://dxlib.xsrv.jp/function/dxfunc_3d_model_3.html)

Model形式ごとの対応機能、基礎Data共有範囲、Frame/Material戻り値、半透明Mode、物理制約を利用中バージョンの公式資料で確認してください。

## 70. 次章への接続

次章ではMV1 Animation・Blendを扱い、Attach Handle、再生時間、Loop、Crossfade、Layer、Bone Mask、Root Motion、Animation EventをModel Instanceへ統合します。
