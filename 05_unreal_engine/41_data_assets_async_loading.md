# 41 Data Asset、Soft Reference、非同期ロード

## 1. Dataと実行処理を分ける

```cpp
UCLASS(BlueprintType)
class UCharacterDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly)
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly)
    TSoftObjectPtr<USkeletalMesh> Mesh;

    UPROPERTY(EditDefaultsOnly)
    TSoftClassPtr<AActionCharacter> CharacterClass;

    UPROPERTY(EditDefaultsOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> AttackMontages;
};
```

Data Assetは調整値とAsset参照、Runtime Componentは現在状態と処理を持ちます。

## 2. Hard ReferenceとSoft Reference

- Hard：参照元をLoadすると依存AssetもLoad対象になりやすい。
- Soft：Asset Pathを保持し、必要時にLoadする。

全Character Dataが全Mesh、全Montage、全NiagaraをHard参照すると、Menuを開いただけで大量Memoryを使う可能性があります。

Soft ReferenceはLoad済み保証ではありません。`Get()`がnullなら非同期Loadが必要です。

## 3. Primary Asset

Asset ManagerはPrimary Asset IDでAssetを発見・Load／Unloadできます。

```text
PrimaryAssetType: Character
PrimaryAssetName: Hero_A
PrimaryAssetId:   Character:Hero_A
```

Project SettingsのPrimary Asset Types to ScanへClassとDirectoryを設定します。

## 4. Asset Bundle

同じCharacterでもMenu用Iconと戦闘用Mesh／Animationを別Bundleにできます。

```cpp
UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "UI"))
TSoftObjectPtr<UTexture2D> Portrait;

UPROPERTY(EditDefaultsOnly, meta = (AssetBundles = "Gameplay"))
TSoftObjectPtr<USkeletalMesh> Mesh;
```

編成画面ではUI Bundle、出撃前にGameplay BundleをLoadします。

## 5. 非同期Load

```cpp
TSharedPtr<FStreamableHandle> LoadHandle =
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        CharacterDefinition->Mesh.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [this, RequestId]()
        {
            if (!IsCurrentRequest(RequestId))
            {
                return;
            }

            USkeletalMesh* LoadedMesh = CharacterDefinition->Mesh.Get();
            ApplyLoadedMesh(LoadedMesh);
        }));
```

実際の捕捉対象の寿命を確認します。Handleを保持しない場合のLoad維持、Cancel、Unload条件も設計します。

## 6. 古い完了結果を捨てる

PlayerがHero Aを選択→Load中にHero Bへ変更した場合、Aの完了Callbackで表示を戻してはいけません。Request ID、選択ID、Owner有効性を完了時に再検証します。

## 7. PreloadとLoading Screen

戦闘開始後に最初の攻撃Montageを同期LoadするとHitchになります。

- 編成確定時に使用CharacterをPreload。
- Stage Load中に共通VFX／UIをLoad。
- Boss Phase前に次Phase Assetを準備。
- Memory Budgetを超えるAssetは退場後にUnload。

## 8. Asset Audit

Reference Viewer、Size Map、Asset AuditでHard参照Chain、Disk Size、Memory、Chunkを確認します。Soft化だけではCook対象から外れる場合があるため、Primary Asset Rule／LabelでPackaging対象を保証します。

## 9. Data Validation

- Primary Asset ID重複。
- 必須Soft Reference未設定。
- MontageとSkeleton不一致。
- Ability Tag／Action Tag欠落。
- Bundle名誤り。
- Editor専用AssetをRuntime Dataが参照。

## 参考

- [Asset Management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine)
