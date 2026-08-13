# Prefab・Nested Prefab・Prefab Variant

## 1. Prefabの役割

Prefab Assetは再利用可能なGameObject構成の設計図です。Scene上のPrefab InstanceはAssetとの接続を持ち、共通変更とinstance固有overrideを分離できます。

```text
CharacterBase.prefab
├─ CharacterMotor
├─ Combatant
├─ Animator
├─ HurtboxRoot
└─ ViewRoot
```

Prefabは「class」、instanceは「object」に少し似ていますが完全に同じではありません。serialized asset、override、Editor操作、Nested Prefabという独自規則があります。

## 2. Instance Override

Scene上のinstanceで位置、Component値、追加/削除したGameObject等が基底と違えばoverrideになります。

- `Revert`: overrideを捨ててPrefab側へ戻す。
- `Apply`: overrideをPrefab Asset側へ反映する。

Applyは多数のScene instanceへ波及し得ます。実行時に偶然変化した値や、Scene専用参照を誤ってApplyしないよう、Overrides画面で差分と適用先を確認します。

## 3. Prefab Variant

Variantは別Prefabをbaseとして継承し、差分だけをoverrideするPrefab Assetです。

```text
CharacterBase.prefab
├─ Character_Sword.prefab (Variant: mesh, animator, attack data)
├─ Character_Gun.prefab   (Variant: mesh, animator, attack data)
└─ Character_Heavy.prefab (Variant: mesh, animator, attack data)
```

共通構造はbaseに置き、Character固有の見た目や設定をVariantへ置きます。Variantは別Variantをbaseにもできますが、継承段数が深いほど「値がどこから来たか」が追いにくくなります。

## 4. 継承とcompositionの使い分け

すべてを巨大なCharacter Prefab階層に詰めないでください。

- 構造が全Characterで本当に共通: base Prefab。
- Characterごとの差分: Variant override。
- 独立再利用できる装備、VFX socket、UI: Nested Prefab。
- 数値やanimation clip集合: ScriptableObject等のdata asset。
- runtimeだけの状態: Componentのruntime field。AssetへApplyしない。

## 5. Nested Prefab

Prefabの中に別Prefab instanceを含める構造です。例えばCharacterにWeapon Prefabを入れても、Weaponは自身のPrefab Assetとの接続を維持できます。

```text
Character_Sword Variant
└─ WeaponSocket
   └─ Sword_A Prefab instance
      ├─ Mesh
      ├─ Trail
      └─ Hitbox
```

外側Prefabと内側Prefabの両方にoverride候補があるため、「どのAssetへApplyするか」を明示します。Sword全体へ反映すべき修正をCharacter側だけのoverrideにすると、別CharacterのSwordには反映されません。

## 6. Prefab AssetからScene Objectを参照しない

Project内のPrefab Assetは、特定SceneのCameraやManagerへ恒久参照する設計に向きません。生成時に依存を注入します。

```csharp
using UnityEngine;

public sealed class CharacterRuntime : MonoBehaviour
{
    private Camera gameplayCamera;

    // Factory/InstallerがScene生成後に呼ぶ。
    public void Initialize(Camera camera)
    {
        gameplayCamera = camera;
    }
}
```

Inspector参照は便利ですが、「Asset間の固定参照」「同じPrefab内部の参照」「Sceneで注入する参照」を区別します。

## 7. Prefab ModeとScene Context

Prefab ModeではAsset内容を隔離して編集できます。Scene内の環境を見ながら編集するContext機能もありますが、見えているScene objectが保存可能なPrefab依存とは限りません。Asset単独で開いても壊れないことを確認します。

## 8. Unpackの意味

UnpackするとPrefab接続を切り、通常のGameObject階層へ変えます。Unpack CompletelyはNestedな接続にも影響し得ます。接続を失うと共通修正の伝播やoverride追跡ができません。「編集しにくいから」と常用せず、接続を切る設計理由がある場合に限定します。

## 9. 高速アクション向け構成例

```text
CharacterDefinition.asset (ScriptableObject・静的data)
├─ status / skill IDs / animation set / VFX keys

CharacterBase.prefab (共通runtime構造)
├─ CharacterFacade
├─ Motor
├─ CombatStateMachine
├─ AnimationDriver
├─ HurtboxRoot
└─ ViewRoot

Character_A.prefab (Variant)
├─ CharacterDefinition参照をoverride
├─ Animator Controllerをoverride
└─ Weapon_A (Nested Prefab)
```

Prefabは依存の組立、ScriptableObjectは共有する静的定義、runtime instanceは現在HP・combo・cooldownを持つ、と責務を分けます。ScriptableObject Assetへ現在HPを書き戻すと、複数instanceが状態を共有する事故になります。

## 10. Override事故を防ぐ規約

- root名、必須Component、socket階層を規約化する。
- Variantで消してはいけないComponentをtest/validatorで検査する。
- Scene instanceに大量overrideが付いたら、Variant化または構成見直しを行う。
- runtime値をInspector Debug表示する場合、Prefab Apply対象と混同しない。
- Model Prefabを直接複雑化せず、wrapper Prefabを検討する。
- GUID/meta fileをVersion Controlに含め、Asset移動をOS Explorerだけで行わない。

## 11. Review時に読む順序

1. Scene instanceのOverrides一覧。
2. 対象Variant自身のoverride。
3. base Prefab。
4. Nested Prefabとそのoverride適用先。
5. 参照しているdata asset。

値だけでなく「どの層が所有する差分か」を読みます。

## 公式資料

- [Unity Manual: Introduction to prefabs](https://docs.unity3d.com/6000.0/Documentation/Manual/prefabs-introduction.html)
- [Unity Manual: Prefab Variants](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabVariants.html)
- [Unity Manual: Nested Prefabs](https://docs.unity3d.com/6000.0/Documentation/Manual/NestedPrefabs.html)
- [Unity Manual: Overriding prefab instance data](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html)

