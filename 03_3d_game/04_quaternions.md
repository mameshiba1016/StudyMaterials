# クォータニオン・3D回転

Quaternionは3D回転を安定して合成・補間する表現です。四成分を持ちますが、単なる4D Vectorではありません。

```cpp
struct Quaternion
{
    float x{};
    float y{};
    float z{};
    float w{1.0F};
};
```

回転を表すには通常Unit Quaternionを使います。

## Axis-Angleから作る

正規化軸`n`と角度`θ`から概念的に次です。

```text
q.xyz = n * sin(θ / 2)
q.w   = cos(θ / 2)
```

角度はRadianです。軸がゼロならIdentity等へ処理します。

## Vector回転

純Quaternion`v=(vx,vy,vz,0)`として次で回転できます。

```text
v' = q * v * inverse(q)
```

実装ではCrossを使う最適化式が一般的です。Library関数を使いConventionを統一します。

## 合成順序

Quaternion乗算も交換できません。

```text
qCombined = qParent * qLocal
```

がどの順で作用するかはLibrary ConventionとVector側で確認します。テストとしてBasis Vectorを回転して期待方向を見ると安全です。

## InverseとConjugate

Unit QuaternionではInverseがConjugate`(-x,-y,-z,w)`です。正規化されていない場合は長さ二乗で割る必要があります。誤差で徐々にUnit長から外れるため、適切に再正規化します。

## `q`と`-q`

Quaternion `q`と全成分を反転した`-q`は同じRotationを表します。Interpolation前にDotが負なら片方を反転し、短い経路を選びます。

## NlerpとSlerp

- Nlerp：線形補間して正規化。高速、一定角速度ではない。
- Slerp：球面線形補間。ほぼ一定角速度、やや高コスト。

```cpp
Quaternion Nlerp(Quaternion a, Quaternion b, float t)
{
    if (Dot(a, b) < 0.0F)
    {
        b = -b;
    }
    return Normalize(a * (1.0F - t) + b * t);
}
```

Animationの大量BoneではNlerpが使われることもあります。

## Euler Angle

Yaw/Pitch/RollはEditor表示や入力に分かりやすい一方、回転順依存とGimbal Lockがあります。

```text
Yaw then Pitch then Roll
```

と別順では結果が違います。Eulerを毎フレームQuaternionへ変換して回し続ける場合、角度Wrap、Pitch Clamp、Conventionを管理します。

## Gimbal Lock

Euler表現で二つの回転軸が一致し自由度が失われる現象です。Quaternionは回転表現・合成で回避できますが、Eulerへ変換して編集すれば表示上の不連続は残ります。

## Look Rotation

ForwardとUpから正規直交Basisを作りMatrix→Quaternionへ変換します。ForwardとUpが平行なら失敗するため代替Upを選びます。TargetとPositionが同じ場合も前回Rotationを維持します。

## From-To Rotation

一方向を別方向へ回すQuaternionです。二方向が反対向きの場合、回転軸が一意でないため適切な直交軸を選びます。

## Character用途

Camera-relative移動方向へCharacterを向けます。

```cpp
if (LengthSquared(moveDirection) > epsilon)
{
    Quaternion target{LookRotation(moveDirection, worldUp)};
    rotation = Slerp(rotation, target, rotationFactor);
}
```

FPS非依存の`rotationFactor = 1-exp(-sharpness*dt)`を使います。瞬間方向転換が必要な戦闘中は状態ごとに速度を変えます。

## Quaternionを直接編集しない

`x/y/z`をEuler角のように変更してはいけません。Axis-Angle、Euler変換、乗算、LookRotation等の意味あるOperationを使います。
