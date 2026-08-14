# UI Toolkit・uGUI・Text・戦闘HUD

> 対象: Unity 6。UI ToolkitとuGUIは併用できるが、入力、sorting、解像度、data flowの所有者を明確にすること。

## 1. UIの責務

UIはGameplay Stateを表示し、Player Commandを入力層へ返します。

```text
Gameplay Domain
├─ HP / Energy
├─ Current Character
├─ Target
├─ Cooldown
└─ Settings
      ↓ immutable View Model / typed events
UI Presenter
      ↓
uGUI / UI Toolkit View
      ↑
UI Command
      ↑
Input / Event System
```

UI Button callbackでPlayer HPを直接書き換えたり、HUDが毎frameSceneを検索したりしません。

## 2. Unityの三つのUI System

### uGUI

GameObject/Componentベースのruntime UI。Canvas、RectTransform、Image、Button、Layout Group、EventSystemを使います。World Space UI、custom Material/Shader、Scene統合に強いです。

### UI Toolkit

retained-modeのUI。Visual Tree、UXML、USS、C#、Panel Settings、UI Documentを使います。大量のmenu、style共有、Editor Tool、runtime screen UIに向きます。

### IMGUI

`OnGUI`によるimmediate-mode UI。Editor Toolや簡易debugには使えますが、一般のruntime UIには推奨されません。

## 3. 選択基準

Unity 6公式比較では、runtimeの推奨用途が機能別に異なります。必要機能を確認します。

### uGUI向き

- World Space HP bar。
- custom Shader/Material。
- 既存UI Asset。
- Scene ViewでGameObjectとして編集。
- TextMeshPro中心。

### UI Toolkit向き

- 大規模menu。
- 設定画面。
- Inventory/List。
- 共通style/theme。
- Editor Toolとruntimeで知識共有。

同じ画面内で混ぜるとFocus/Input/sortingが複雑になります。画面単位でSystemを選び、境界を文書化します。

## 4. Retained Mode

UI hierarchyを保持し、値・style・layoutが変化したときに更新します。

```text
Visual Tree
├─ Header
├─ Content
│  ├─ CharacterList
│  └─ DetailPanel
└─ Footer
```

毎frame全UIを作り直さず、Elementを生成・bind・unbind・再利用します。

## 5. uGUIの基本構造

```text
Canvas
├─ CanvasScaler
├─ GraphicRaycaster
└─ SafeAreaRoot
   ├─ HUD
   ├─ PauseMenu
   └─ ModalLayer

EventSystem
└─ InputSystemUIInputModule
```

Canvas配下のGraphicが変化すると、Canvas rebuild/batch再構築が発生し得ます。巨大Canvas一つへ静的・毎frame変化UIを全部置きません。

## 6. Canvas Render Mode

### Screen Space - Overlay

Cameraを介さず画面上へ描画。一般HUDに簡単です。

### Screen Space - Camera

指定Cameraを使い、plane distanceやCamera設定と関係します。Camera stacking/sortingを確認します。

### World Space

World内の面として表示。Enemy HP、interaction panel等。距離、scale、occlusion、raycastを処理します。

## 7. Canvas分割

```text
StaticHudCanvas
├─ frame/background

DynamicHudCanvas
├─ HP
├─ energy
└─ cooldown

PopupCanvas
├─ damage numbers
└─ notifications
```

更新頻度やsorting単位で分けます。Canvasを細かくしすぎるとdraw/batch overheadが増えるため、Profilerでbalanceを取ります。

## 8. RectTransform

RectTransformはAnchor、Pivot、Anchored Position、Size Deltaで親矩形に対する配置を表します。

- Anchor Min/Max: 親内の基準範囲。
- Pivot: 自身の回転/scale/position基準。
- Anchored Position。
- Size Delta。
- Offset Min/Max。

Anchorがstretchか固定かでInspector fieldの意味が変わります。値を丸暗記せず親矩形から計算します。

## 9. Anchor

```text
Top-left fixed:
anchorMin = anchorMax = (0, 1)

Full stretch:
anchorMin = (0, 0)
anchorMax = (1, 1)
```

HP barを固定pixel位置だけで置くとaspect ratioでずれます。画面端基準、安全領域基準、中央構図基準を選びます。

## 10. Canvas Scaler

Screen Space UIのscale方法:

- Constant Pixel Size。
- Scale With Screen Size。
- Constant Physical Size。

`Scale With Screen Size`ではReference ResolutionとScreen Match Modeを使います。

```text
Reference: 1920 × 1080
Actual:    2560 × 1080（ultrawide）
Match Width/Heightの値でscale基準が変わる
```

Referenceと同じaspectだけで確認せず、16:9、16:10、21:9、4:3、mobile portrait/landscape等をtestします。

## 11. Safe Area

Mobile notch、rounded corner、console overscan等で重要UIが切れないよう`Screen.safeArea`を使います。

```csharp
using UnityEngine;

public sealed class SafeAreaFitter : MonoBehaviour
{
    [SerializeField] private RectTransform target;
    private Rect lastSafeArea;
    private Vector2Int lastScreenSize;

    private void Update()
    {
        Rect safeArea = Screen.safeArea;
        Vector2Int screenSize = new(Screen.width, Screen.height);

        if (safeArea == lastSafeArea && screenSize == lastScreenSize)
        {
            return;
        }

        Apply(safeArea, screenSize);
        lastSafeArea = safeArea;
        lastScreenSize = screenSize;
    }

    private void Apply(Rect area, Vector2Int screenSize)
    {
        Vector2 min = area.position;
        Vector2 max = area.position + area.size;

        min.x /= screenSize.x;
        min.y /= screenSize.y;
        max.x /= screenSize.x;
        max.y /= screenSize.y;

        target.anchorMin = min;
        target.anchorMax = max;
        target.offsetMin = Vector2.zero;
        target.offsetMax = Vector2.zero;
    }
}
```

screen size 0やEditor Device Simulator、orientation変更をtestします。

## 12. Layout System

uGUI:

- Horizontal/Vertical/Grid Layout Group。
- Content Size Fitter。
- Layout Element。
- Aspect Ratio Fitter。

Layoutは親子のpreferred/min/flexible sizeから計算します。Content Size FitterとLayout Groupを循環させるとlayoutが不安定になります。

## 13. Layout Rebuild

Text、child追加、size変更等でLayout dirtyとなり、rebuildされます。毎frame大量に変更するとCPU costになります。

対策:

- 値が変わったときだけ更新。
- 静的Canvasと分離。
- Layout Groupを深くnestしすぎない。
- Popupをpool。
- `LayoutRebuilder.ForceRebuildLayoutImmediate`を常用しない。
- ProfilerのUI/Layout markerを確認。

## 14. Graphic Raycaster

Canvas上のGraphicへpointer raycastします。

- Raycast Targetが不要なImage/TextはOff。
- blocker/canvas group。
- sorting。
- multiple canvas。
- World Spaceの場合のCamera。

背景Imageまで全てRaycast Targetにするとcandidate数が増え、buttonを遮ることがあります。

## 15. EventSystem

uGUIのpointer、submit、navigation、selectionを管理します。Sceneに複数EventSystemを誤って置くと競合します。

Input System使用時:

- InputSystemUIInputModule。
- UI Action Map。
- Point/Click/Scroll/Navigate/Submit/Cancel。
- Multiplayer UI。

Gameplay ActionとUI Actionを同時に有効にして決定buttonで攻撃しないよう、Game Flowがmapを切り替えます。

## 16. Focus

Gamepad/Keyboard UIでは現在選択Elementが必要です。

```text
Menu Open
 → previous Gameplay focusを保存
 → default buttonへfocus
 → navigation
 → modal openならfocus trap
 → modal closeで元focus復元
 → menu closeでGameplayへ
```

Element disable/destroy時にfocusがnullになる場合のfallbackを用意します。

## 17. Navigation

uGUI SelectableのAutomatic/Explicit navigationを使えます。複雑layoutでAutomaticは意図しない斜め移動を選ぶことがあります。

- Grid/ListはExplicitまたは規則生成。
- disabled itemをskip。
- wrapするか。
- scroll positionをfocusへ追従。
- languageでbutton幅が変わってもnavigationを維持。

## 18. UI Toolkitの構造

```text
UIDocument (GameObject Component)
├─ VisualTreeAsset (UXML)
├─ PanelSettings
└─ rootVisualElement
    ├─ VisualElement
    ├─ Label
    ├─ Button
    └─ ListView
```

`VisualElement`はMonoBehaviourではなくGameObjectへattachしません。

## 19. UXML

UI構造をmarkup Assetで定義します。

```xml
<ui:UXML xmlns:ui="UnityEngine.UIElements">
    <ui:VisualElement name="hud" class="hud">
        <ui:Label name="character-name" class="hud__name" />
        <ui:ProgressBar name="hp-bar" class="hud__hp" />
    </ui:VisualElement>
</ui:UXML>
```

構造をUXML、styleをUSS、動作/bindingをC#へ分けます。小さい動的ElementはC#生成でも構いません。

## 20. USS

CSSに似たstyle sheetです。

```css
.hud {
    position: absolute;
    left: 24px;
    bottom: 24px;
    flex-direction: column;
}

.hud__name {
    font-size: 24px;
    color: white;
}

.hud--danger .hud__hp {
    background-color: rgb(180, 20, 20);
}
```

- type selector。
- name selector。
- class selector。
- pseudo-class。
- specificity。
- inheritance。

inline styleを乱用せずclass切替で状態を表します。

## 21. Flexbox Layout

UI ToolkitはFlexbox系layoutを使います。

- flex-direction。
- justify-content。
- align-items。
- flex-grow/shrink。
- width/height/min/max。
- margin/padding。
- position relative/absolute。

absolute positioningだけで全画面を作ると多解像度対応が難しくなります。flow layoutとabsolute overlayを使い分けます。

## 22. UI Documentの寿命

```csharp
using UnityEngine;
using UnityEngine.UIElements;

[RequireComponent(typeof(UIDocument))]
public sealed class PauseMenuView : MonoBehaviour
{
    private Button resumeButton;

    private void OnEnable()
    {
        VisualElement root =
            GetComponent<UIDocument>().rootVisualElement;

        resumeButton = root.Q<Button>("resume-button");
        resumeButton.clicked += OnResumeClicked;
    }

    private void OnDisable()
    {
        if (resumeButton != null)
        {
            resumeButton.clicked -= OnResumeClicked;
        }
    }

    private void OnResumeClicked()
    {
        // Game FlowへResume commandを送る。
    }
}
```

root再生成/Panel attach-detach、enable/disableに応じてElement referenceとcallbackを管理します。

## 23. Query

`Q<T>("name")`等でElementを検索できますが、毎framehierarchy queryしません。初期bind時にcacheします。

name string typoはcompileで検出されないため:

- 定数化。
- required element validator。
- startup時fail fast。
- generated binding等の検討。

## 24. UI Toolkit Event

Eventはtargetへdispatchされ、trickle-down/bubble-up phaseを持ちます。

```text
Root
 ↓ TrickleDown
Parent
 ↓
Target
 ↑ BubbleUp
Parent
 ↑
Root
```

`StopPropagation`を乱用すると親の共通handlerが動かなくなります。Default actionとcallbackの違いも確認します。

## 25. Pointer capture

Drag/Slider等ではpointerをElementへcaptureし、pointerが外へ出ても操作を継続します。終了・cancel・detachでreleaseします。

複数pointer/touchではpointer IDごとに状態を持ちます。

## 26. UI Toolkit Focus

- focusable。
- tabIndex。
- FocusController。
- navigation event。
- focus/blur event。

Modalを開いたらbackground Elementを操作不能にし、focusをModal内部へ閉じ込めます。閉じたら呼出元へ戻します。

## 27. uGUIとUI Toolkit併用

Unity 6では同時利用可能です。ただし:

- input eventの優先。
- pointerが片方を貫通するか。
- sorting order。
- focus owner。
- modal。
- UI Action Map。

を統合するUI Flowが必要です。

例:

```text
uGUI: World-space HP / combat reticle
UI Toolkit: Pause / settings / inventory
```

Pause Menu表示中はuGUI gameplay raycastとActionを止めます。

## 28. Presenter

```csharp
public readonly struct PlayerHudModel
{
    public PlayerHudModel(
        string displayName,
        int currentHp,
        int maxHp,
        float energy01)
    {
        DisplayName = displayName;
        CurrentHp = currentHp;
        MaxHp = maxHp;
        Energy01 = energy01;
    }

    public string DisplayName { get; }
    public int CurrentHp { get; }
    public int MaxHp { get; }
    public float Energy01 { get; }
}

public interface IPlayerHudView
{
    void Render(in PlayerHudModel model);
    void SetVisible(bool visible);
}
```

PresenterがDomain StateをView Modelへ変換し、uGUI/UI Toolkitのどちらでも同じinterfaceを実装できます。

## 29. PushとPolling

悪い例:

```csharp
private void Update()
{
    hpText.text = player.CurrentHp.ToString();
}
```

値が変わったときだけ通知:

```text
Damage確定
 → HP State変更
 → HpChanged event
 → PresenterがView Model更新
 → Viewが必要部分だけ描画
```

event登録解除、初期snapshot、Scene寿命を管理します。高頻度値はfixed interval/差分更新も検討します。

## 30. HP Bar

現在HPと表示HPを分けるとdamageの減少演出を作れます。

```csharp
private float targetHp01;
private float displayedHp01;

private void Update()
{
    displayedHp01 = Mathf.MoveTowards(
        displayedHp01,
        targetHp01,
        hpAnimationSpeed * Time.unscaledDeltaTime);

    hpFill.fillAmount = displayedHp01;
}
```

Gameplay HPは即時確定し、UIだけ遅れて追従します。Pause/Hit Stop中に進めるか時計を明記します。

## 31. Character交代HUD

```text
Party HUD
├─ Slot 0: portrait / HP / cooldown
├─ Slot 1
└─ Slot 2

Current Character Changed
 → active marker移動
 → main HP sourceをrebind
 → skill icons差替え
 → old subscriptions解除
```

旧Character eventの解除漏れで、非active Characterのdamageがmain barへ反映されないようgeneration/owner IDを検証します。

## 32. Skill Cooldown

表示:

- radial fill。
- remaining seconds。
- disabled tint。
- input glyph。
- resource不足。
- lock状態。

Cooldownの真実はCombat Stateです。UIが`remaining -= deltaTime`して独自に確定するとズレます。Domain deadline/tickから表示値を計算します。

## 33. Lock-on UI

World TargetをScreen座標へ変換します。

```csharp
Vector3 screen = gameplayCamera.WorldToScreenPoint(targetPosition);
bool isBehind = screen.z <= 0.0f;
```

- behind camera。
- off-screen clamp。
- Safe Area。
- Camera viewport。
- dynamic resolution。
- target破棄。
- occlusion。

を処理します。Camera Lock-on StateがTargetを所有し、UIは表示だけ行います。

## 34. Off-screen Indicator

Screen centerからtarget方向を求め、Safe Area内edgeへclampします。behind-cameraでは方向を反転/角度補正する必要があります。

Indicatorに距離、上下、高低差をどう表すか決めます。すべての敵を表示するとnoiseになるため危険攻撃/Current Target等へ限定します。

## 35. Damage Number

毎hitでInstantiate/DestroyするとGCとCanvas rebuildが増えます。Poolします。

```text
Damage Event
 → Popup Pool.Get
 → data bind
 → world/screen position
 → animation
 → Pool.Release
```

同時hitをまとめる、表示上限、重要度、距離、画面外を規則化します。Damage NumberはGameplay処理を逆参照しません。

## 36. World Space UI

Enemy HP等:

- Camera facing/billboard。
- distance scale。
- occlusion。
- render order/depth。
- update frequency。
- pooling。
- visibility culling。

全EnemyにCanvas一つずつはcostになる場合があります。中央Screen Space Canvasへmarkerをまとめる方式も比較します。

## 37. TextMeshPro

TextMeshProはSDF等を使う高品質Text systemです。

- TMP_Text。
- TextMeshProUGUI。
- Font Asset。
- Material Preset。
- Sprite Asset。
- Style Sheet。
- fallback。
- rich text。

Package/Unity Versionで統合状況を確認します。

## 38. SDF

Signed Distance Fieldはglyph edgeまでの距離情報をTextureへ保存し、scaleしても比較的滑らかなedgeを作ります。

Material property:

- Face。
- Outline。
- Underlay。
- Dilate。
- Softness。

強いOutline/Glowはglyph atlas samplingとoverdraw costを増やします。全Textへ豪華presetを使いません。

## 39. Font Asset

- Static Atlas: 事前にglyphを生成。Buildで安定。
- Dynamic Atlas: runtimeにglyphを追加可能。Font file参照、memory、platform、atlas growthを考慮。

必要文字:

- Latin。
- 日本語。
- 中国語。
- 韓国語。
- Cyrillic等。
- 数字/記号。

CJK全文字を一枚へ詰めず、使用文字set、dynamic、fallback、複数atlasを設計します。

## 40. Fallback Font

主Fontにglyphが無い場合fallbackを検索します。

```text
Primary UI Font
 → Japanese Fallback
 → CJK Common Fallback
 → Symbol/Emoji Fallback
```

fallback chainが長いとlookupと管理が増えます。missing glyphをBuild前にLocalization table全件で検査します。

## 41. Text更新とAllocation

```csharp
// 文字列補間は更新頻度次第でallocationになる。
timerText.text = $"{remaining:F1}";
```

TMPの`SetText` overload等を使える場合があります。

```csharp
timerText.SetText("{0:0.0}", remaining);
```

値が変わった時だけ更新し、毎frame全Textを書き直しません。ProfilerでGC Allocを確認します。

## 42. Localization

文字列長は言語で大きく変わります。

- text expansion。
- line wrap。
- font fallback。
- RTL。
- plural。
- gender/context。
- date/number format。
- controller glyph。

固定widthに合わせてfont sizeを極端に縮めず、layoutが伸縮する設計にします。

## 43. Auto Size

TextMeshPro Auto Sizeは領域へ収めますが、広いmin/max rangeや大量Textで計算costが増えます。

- 見出しは固定size。
- 翻訳が長いbuttonだけ限定。
- min sizeで読めるか。
- layout rebuildとの相互作用。

## 44. Rich Text

`<color>`、`<sprite>`等で装飾できます。外部/user入力をそのままrich textとして表示するとtag解釈されるためescape/無効化を検討します。

Localization文字列へmarkupを入れる場合、translator向け規約とvalidatorを用意します。

## 45. Input Glyph

現在Control SchemeとBindingに応じて表示を更新します。

```text
Gameplay Action "Dodge"
 → effective binding
 → device layout/control path
 → Glyph Database
 → Sprite/Text
```

key名をhardcodeせずrebind後に更新します。複数binding、keyboard layout、gamepad種類を考慮します。

## 46. Modal

Modal表示中:

- backgroundを操作不能。
- gameplay入力停止。
- focusをModal内へ。
- Cancelの意味。
- 連打による二重open防止。
- animation中input policy。

Modal stackをUI Flow一つが管理します。各WindowがTime.timeScaleを直接変更しません。

## 47. Screen Stack

```text
HUD
 → Pause
   → Settings
     → Confirm Dialog
```

push/pop:

- enter/exit animation。
- focus save/restore。
- input map。
- time scale request token。
- async load cancellation。
- duplicate prevention。

Sceneを跨ぐUIならpersistent UI Scene/ServiceとScreen Assetを組み合わせます。

## 48. Async UI

Addressables/Network/Saveを待つ場合:

```text
Screen request generation N
 → loading表示
 → async
 → generation確認
 → still visible?
 → result bind
```

Screenを閉じた後に古いresultがElementへ書き込まないようCancellationTokenとgenerationを使います。

## 49. Loading UI

`AsyncOperation.progress`等をそのまま0～100%表示するとactivation段階や複数loadで不自然になります。

- phase別weight。
- minimum display time。
- progress smoothing。
- cancel。
- error/retry。
- accessibility。

本当の進捗と演出上のdisplayed progressを分けますが、嘘のまま停止しないようtimeout/errorを示します。

## 50. Pooling

Pool対象:

- Damage popup。
- Item slot。
- Enemy marker。
- Notification。
- List cell。

reuse時にreset:

- text。
- sprite。
- material/property。
- event listener。
- animation。
- cancellation。
- focus。
- accessibility metadata。

前回listenerを残すと一clickで複数処理が走ります。

## 51. ListView virtualization

UI Toolkitの`ListView`等は見えるcellだけ生成・再利用できます。

概念:

```csharp
listView.makeItem = () => new Label();
listView.bindItem = (element, index) =>
{
    ((Label)element).text = items[index].DisplayName;
};
```

`bindItem`でcallbackを追加し続けず、`unbindItem`/`destroyItem`やcell固有stateを正しく処理します。

## 52. Accessibility

- font size。
- contrast。
- color-blind safe。
- subtitle。
- input remap。
- hold/toggle。
- screen shake。
- motion blur。
- UI scale。
- audio cueとvisual cue。
- focus visibility。

HP危険を赤色だけでなく形、animation、soundでも示します。

## 53. UI Animation

- Animator。
- Animation Clip。
- Tween library。
- USS transition。
- custom time interpolation。

画面数が多いUIでAnimator Controllerを各Elementへ置くと管理/性能が重くなる場合があります。単純fade/translateは軽量なtransitionで十分です。

clock:

- Menuはunscaled。
- Combat popupは仕様次第。
- Cutscene subtitleはTimeline。

## 54. UI Audio

Focus move、submit、cancel、error、open/closeをtyped UI Audio Eventへします。各ButtonへAudioSourceを持たせず中央Serviceでvoice limitとvolume groupを管理します。

同じframeにFocusが多数移動して音が連打されないようdebounceします。

## 55. Sorting

uGUI:

- Canvas sorting layer/order。
- Override Sorting。
- Camera depth/stack。

UI Toolkit:

- PanelSettings sorting order。
- UIDocument order。
- tree order/z-index的style。

uGUI/UI Toolkit混在時の最終render順を実機で確認します。ModalがWorld Space UIの後ろになる事故をtestします。

## 56. MaskとClipping

uGUI:

- Mask。
- RectMask2D。
- Stencil。

UI Toolkit:

- overflow hidden等。

深いMask nesting、soft mask、transparent overlapはfill rate/Stencil costを増やします。Scroll Viewのclip範囲外Elementが本当に描画されていないかFrame Debuggerで確認します。

## 57. UI Material

uGUIのcustom Shaderでglow、distortion、radial cooldown等を作れます。

注意:

- Canvas batching。
- Material instance増加。
- Stencil property。
- Mask compatibility。
- premultiplied alpha。
- HDR/Bloom。
- Camera stacking。

MaterialPropertyBlockは一般のRendererと同じ感覚でuGUI Graphicへ使えるとは限りません。UI systemのMaterial管理を確認します。

## 58. Sprite Atlas

UI SpriteをAtlasへまとめtexture切替を減らせます。

- packing tag/Atlas Asset。
- variant scale。
- tight packing。
- rotation。
- padding。
- mipmap。
- Addressables bundle duplication。

異なるAtlasを交互に使うhierarchyはbatchが分かれます。Frame Debuggerで確認します。

## 59. UI Performance

CPU:

- Layout。
- Canvas rebuild。
- Event dispatch。
- Text generation。
- Instantiate/Destroy。
- hierarchy query。
- binding。

GPU:

- overdraw。
- transparent。
- mask。
- material/texture switch。
- high resolution。
- blur。

最適化:

1. Profilerでmarker確認。
2. 値変更時だけ更新。
3. Pool/virtualize。
4. Canvas分割。
5. Raycast Target削減。
6. overdraw/material削減。

## 60. Debug

- UI Toolkit Debugger。
- Event Debugger。
- Panel/Visual Tree。
- uGUI Scene/Hierarchy。
- Frame Debugger。
- Profiler UI/Layout/GC。
- selected Focus表示。
- active Screen Stack。
- current Action Map。
- Safe Area overlay。
- localization pseudo language。

```text
Buttonが反応しない
 → active?
 → raycast?
 → blocker?
 → EventSystem/Input Module?
 → Action Map?
 → focus?
 → callback registered?
 → duplicate/old screen?
```

## 61. よくある不具合

- HUDが毎frameSceneからPlayerを検索する。
- uGUI/UI Toolkitを理由なく同画面で混在。
- Canvasを巨大一枚にしてHP変化で全体rebuild。
- Canvasを細分化しすぎdrawが増える。
- 全Image/TextでRaycast Targetを有効。
- Content Size FitterとLayoutを循環。
- Force Rebuildを毎frame呼ぶ。
- Gameplay/UI Action Mapが同時に反応。
- Modal外へfocusが逃げる。
- UI Toolkit callbackを解除せず二重発火。
- Q queryを毎frame実行。
- Input値とDomain Stateで別々にCooldownを進める。
- Character交代後も旧HP eventを購読。
- Damage Popupを毎hit生成破棄。
- Font fallback不足で豆腐文字。
- Auto Sizeで文字が読めないほど縮む。
- Screen.safeAreaを初回だけ計算。
- async完了後に閉じたScreenへ書き込む。

## 62. Test Matrix

| 観点 | Test |
|---|---|
| Resolution | 720p、1080p、4K、ultrawide、4:3 |
| Device | notch、safe area、orientation |
| Input | Mouse、Keyboard、Gamepad、Touch |
| Focus | open/close、modal、disable、list scroll |
| Language | 日本語、英語、長い翻訳、RTL対象 |
| Text | missing glyph、dynamic atlas、rich text |
| Combat | damage連打、交代、lock-on、pause |
| Async | close中完了、error、retry、Scene unload |
| Performance | popup大量、enemy marker大量、4K |
| Accessibility | UI scale、contrast、color、motion off |

## 63. 設計チェックリスト

- 画面ごとにuGUI/UI Toolkitを選んだ理由があるか。
- UIがDomain Stateを所有していないか。
- Presenter/View Model境界があるか。
- 値変更時だけUIを更新するか。
- Canvas/Panel分割は更新頻度基準か。
- Safe Areaと全aspect ratioをtestしたか。
- Gameplay/UI入力をFlowが切り替えるか。
- Modalのfocus trap/restoreがあるか。
- Character交代で旧eventを解除するか。
- Localization全文字のFont/Fallbackを検証したか。
- Popup/Listをpool/virtualizeするか。
- async resultへgeneration/cancelがあるか。
- UI scale、Shake、Blur等の設定があるか。
- Profiler/Frame DebuggerでCPU/GPU両方を測ったか。

## 公式資料

- [Unity Manual: User interfaces](https://docs.unity3d.com/6000.0/Documentation/Manual/UIToolkits.html)
- [Unity Manual: Comparison of UI systems](https://docs.unity3d.com/6000.0/Documentation/Manual/UI-system-compare.html)
- [Unity Manual: UI Toolkit](https://docs.unity3d.com/6000.0/Documentation/Manual/UIElements.html)
- [Unity Manual: Runtime UI events and input](https://docs.unity3d.com/6000.0/Documentation/Manual/UIE-Runtime-Event-System.html)
- [Unity Manual: Render runtime UI](https://docs.unity3d.com/6000.0/Documentation/Manual/UIE-render-runtime-ui.html)
- [Unity Manual: Transition from uGUI to UI Toolkit](https://docs.unity3d.com/6000.0/Documentation/Manual/UIE-Transitioning-From-UGUI.html)
- [Unity UI package manual](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/index.html)
- [Unity Manual: Canvas](https://docs.unity3d.com/6000.0/Documentation/Manual/UICanvas.html)
- [Unity Manual: Canvas Scaler](https://docs.unity3d.com/6000.0/Documentation/Manual/script-CanvasScaler.html)
- [TextMeshPro package documentation](https://docs.unity3d.com/Packages/com.unity.textmeshpro@3.2/manual/index.html)

