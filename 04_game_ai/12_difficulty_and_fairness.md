# 難易度・公平性・適応

難易度は敵HP/Damage倍率だけではありません。情報量、反応猶予、同時圧力、Recovery、Aim精度を調整します。

## 調整軸

- Telegraph時間。
- Reaction Delay。
- 同時Attack Token数。
- Attack選択の賢さ。
- Aim Error。
- Cooldown/Recovery。
- 移動速度。
- HP/Damage。
- Parry/Dodge Window。
- Resource Drop。

HPだけを増やすと戦闘が長くなるだけです。

## Reaction Delay

AIが知覚した同tickに完璧な回避を行わず、知覚→判断→Action開始へDelayを持ちます。高難易度でもAnimation StartupとCharacter制約を無視しません。

## 情報制限

AIがPlayer入力を直接読んでCounterすると不公平です。Animation、Attack State、過去行動などAIが観測可能な情報だけ使います。内部情報を使う場合は演出上納得できる能力として設計します。

## Aim Error

Projectile Aimへ角度/位置誤差と追従速度を設けます。単純RandomだけでなくPlayer速度予測の誤差、発射前Lock時間を調整します。

## Dynamic Difficulty

連続死亡等に応じて密かに数値を変える方式は、学習の一貫性や信頼を損なう場合があります。利用するなら変化範囲、保存、解除、説明を設計します。明示的Assist設定を優先できる場合があります。

## Pressure Relief

Player低HP・連続被弾時に、Directorが次強攻撃までの間隔を少し広げられます。敵が露骨に何もしないのでなく、位置調整や威嚇として表現します。

## Accessibility

- Game Speed調整。
- Parry/Dodge受付拡大。
- Damage軽減。
- Target Lock補助。
- Telegraph強調。
- Button操作代替。

単独設定にし、他の能力を不要に下げません。

## Fairness検証

- Camera外攻撃。
- 重なって見えないTelegraph。
- 回避不能な同時Pattern。
- Spawn直後Hit。
- 入力受付とAnimation表示のずれ。
- Frame RateによるWindow差。

録画、Input/Decision Log、固定Seedで再現します。

## Metric

Damage原因、回避成功、Attack使用率、戦闘時間、Death位置を収集できます。個人情報と同意を考慮し、数値だけで楽しさを決めずPlaytest観察と組み合わせます。
