# 02 Unity Project、Asset、Package、Assembly

## 1. Project構造

```text
Project/
├─ Assets/          Game固有AssetとScript
├─ Packages/        Package ManifestとLock
├─ ProjectSettings/ Project全体設定
├─ Library/         Import済みCache（再生成可能）
├─ Temp/            一時生成物
├─ Logs/            Editor Log等
└─ UserSettings/    利用者ローカル設定
```

Version管理では通常`Assets`、`Packages`、`ProjectSettings`を対象にし、`Library`等の生成物を除外します。Project方針に対応するUnity用`.gitignore`を使います。

## 2. Assetと.meta

UnityはAssetをGUIDで識別し、GUIDやImport設定を`.meta`へ保存します。Assetだけ移動・コピーして`.meta`を失うと参照切れや別GUID化が起きます。

File Explorerだけで不用意に移動するよりEditor内で移動し、Version管理ではAssetと`.meta`を一緒に扱います。

## 3. Import Pipeline

```text
Source Asset（png、fbx、wav）
  ↓ Importer + Settings
Library内のPlatform向けArtifact
  ↓
Scene／Prefab／Runtimeから参照
```

元FileとRuntimeで使われる形式は同一ではありません。Texture Compression、Mesh Read/Write、Animation Import等がMemoryとBuild Sizeへ影響します。

## 4. Package Manager

`Packages/manifest.json`は直接依存、`packages-lock.json`は解決された依存関係を記録します。Input System、Cinemachine、Addressables、Burst等はPackage VersionでAPIが変わり得ます。

教材では使用Versionを確認し、PackageのSamplesや公式Manualを対応Versionで参照します。

## 5. Assembly

asmdefなしの多くのGame Scriptは既定の`Assembly-CSharp.dll`へCompileされます。Projectが大きくなると変更のたびに広範囲を再Compileし、依存方向も曖昧になります。

```text
Game.Core          Pure C#、共通Data
Game.Combat        Combat Domain
Game.UnityRuntime  MonoBehaviour Adapter
Game.UI            UI
Game.Editor        Editor専用Tool
Game.Tests         Test
```

## 6. asmdef

Assembly Definition Assetを含むFolder配下のScriptは、そのAssemblyへCompileされます。子Folderに別asmdefがあればそこで境界が変わります。

```json
{
  "name": "Game.Combat",
  "references": ["Game.Core"],
  "autoReferenced": true
}
```

Compile順を数値指定するのではなく、参照Graphから順序が決まります。循環参照は設計を見直します。

## 7. Editor Assembly

`UnityEditor` APIを使うScriptはEditor専用Assemblyへ置き、Runtime Buildへ混入させません。Folder名`Editor`とasmdefのPlatform設定を理解します。

## 8. Namespace

Namespaceは名前衝突を防ぎますがAssembly境界そのものではありません。別Assemblyでも同じNamespaceを使え、同じAssemblyでも複数Namespaceを持てます。

## 9. Dependency方針

```text
UI → Application → Domain
Unity Adapter ─────→ Domain
Editor Tools → Runtime/Data
Domain ─X→ Unity UI / Editor
```

Pure Domainが`MonoBehaviour`やSceneを知らない方向へするとTestと再利用が容易です。

## 参考

- [Unity 6：アセンブリの概要](https://docs.unity3d.com/ja/6000.0/Manual/assembly-definitions-intro.html)
