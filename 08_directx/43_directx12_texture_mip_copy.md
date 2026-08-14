# DirectX 12 第11章：Texture・Mip・Copy

この章では、画像DataをGPU TextureへUploadしてShaderから読むまでを学びます。WIC/DDS、Texture Description、Subresource、Row/Slice Pitch、GetCopyableFootprints、CopyTextureRegion、Mip、SRV、Array/Cube/BC Format、Readbackを扱います。

## 1. Texture Upload全体

```text
Image File
 -> CPU Decode/Format変換
 -> Texture Resource作成(COPY_DEST)
 -> Copyable Footprint計算
 -> Upload BufferへRow Copy
 -> CopyTextureRegion
 -> Shader Resource StateへTransition
 -> SRV作成
 -> Fence完了後Upload Buffer再利用
```

## 2. CPU ImageとGPU Texture

CPU ImageはPixel配列、GPU TextureはHardware向けLayoutを持つResourceです。同じMemory配置と仮定しません。

## 3. Image Metadata

```cpp
struct ImageMetadata
{
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArraySize;
    uint32_t mipLevels;
    DXGI_FORMAT format;
    bool isCube;
};
```

## 4. Subresource Data

```cpp
struct ImageSubresource
{
    const std::byte* data;
    size_t rowPitch;
    size_t slicePitch;
};
```

Source DataのPitchを保持します。

## 5. WIC

Windows Imaging ComponentでPNG/JPEG/BMP/TIFF等をDecodeできます。Runtime Texture読込みやTool用途に使えます。

## 6. WIC Factory

COM初期化と`IWICImagingFactory`生成が必要です。ThreadのCOM Apartment方針を決めます。

## 7. WIC Decoder Flow

```text
Decoder -> Frame -> Format Converter -> CPU Pixel Buffer
```

Source FormatをGPUで扱いやすいRGBA等へ変換します。

## 8. JPEGのAlpha

JPEGに通常Alphaはありません。Decode後Alphaを1へ補うFormat変換等を行います。

## 9. Color Management

ICC Profile、Gamma、EXIF Orientation等をどう扱うかAsset Pipelineで決めます。Decodeできたことと正しい色は別です。

## 10. DDS

DirectDraw SurfaceはMip、Array、Cube、BC圧縮、GPU Formatを保持しやすく、Game Runtime向けTexture Containerとして有用です。

## 11. DDSの利点

OfflineでMip/圧縮を生成し、Runtime Decode/変換Costを減らせます。

## 12. DirectXTex

MicrosoftのTexture Processing LibraryでDDS/WIC/TGA/HDR読込み、変換、Mip、圧縮等を行えます。Tool/Runtimeで用途を分けます。

## 13. DirectXTK12 Loader

`WICTextureLoader`、`DDSTextureLoader`、`ResourceUploadBatch`等のHelperがあります。内部処理を理解した上で利用できます。

## 14. Source AssetとCooked Asset

```text
Source : PNG/TGA/EXR等、編集向け
Cooked : DDS/Platform Format、Mip/圧縮済み
```

製品Runtimeで毎回Source変換しないPipelineを検討します。

## 15. Texture2D Resource Desc

```cpp
D3D12_RESOURCE_DESC textureDesc{};
textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
textureDesc.Alignment = 0;
textureDesc.Width = metadata.width;
textureDesc.Height = metadata.height;
textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.depthOrArraySize);
textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
textureDesc.Format = metadata.format;
textureDesc.SampleDesc = { 1, 0 };
textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
```

## 16. Width Type

Widthは64-bit Field、Height等は別Typeです。Cast前にFormat/Device LimitとOverflowを検証します。

## 17. Texture Format

RGBA8、16-bit Float、BC1/3/5/7等、用途・品質・Memory・Sampling Supportから選びます。

## 18. UNORM

整数Bitを0～1へ正規化してShaderへ渡します。Color/Mask等で使います。

## 19. SRGB

Color TextureをSampling時にLinearへ変換するSRGB Viewを使えます。Normal/Metallic/Roughness等のData Textureに使いません。

## 20. Typeless Resource

ResourceをTypeless Formatで作り、用途ごとに互換Typed Viewを作る設計があります。Format Familyの互換規則を守ります。

## 21. HDR Format

R16G16B16A16_FLOAT等は広いRangeを保持できます。Source Decode、Bandwidth、Filter、Output Tone Mapを考慮します。

## 22. Block Compression

BC Formatは4x4 Block単位等で圧縮します。Size、Row Pitch、Copy Box AlignmentをPixel Formatとは別に計算します。

## 23. BC1

低MemoryのColor用途です。Alpha/品質要件に応じて他Formatを選びます。

## 24. BC3

明示Alpha Channelを持つColor Texture用途の一例です。

## 25. BC5

2 Channel Normal Map等に向きます。ShaderでZを再構築する設計があります。

## 26. BC7

高品質Color/Alphaを保持できますがOffline Compression Cost等を考慮します。

## 27. Format Support

SRV、Filter、RTV、UAV、Typed Load/Store等のSupportを`CheckFeatureSupport`で確認します。

## 28. Mip Level数

```cpp
uint32_t FullMipCount(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    while (width > 1 || height > 1)
    {
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
        ++levels;
    }
    return levels;
}
```

## 29. Mip Dimension

各Levelは前Levelの約半分で最低1です。奇数Sizeの扱いを統一します。

## 30. Texture Array

同Size/Format/Mip構成のTexture Sliceを一Resourceにまとめます。

## 31. Cube Texture

6 FaceをArray Sliceとして持ち、Cube SRVでSamplingします。Face順序と座標方向をAsset Pipelineで統一します。

## 32. Cube Array

6 Slice単位で複数Cubeを持ちます。Array SizeとSRV Num Cubesを正しく設定します。

## 33. Texture3D

Depthを持つVolume Textureです。Array TextureとSubresource計算が異なります。

## 34. Subresource数

2D ArrayではMip Levels×Array Size×Plane Countです。3D TextureではMipごとがSubresourceでDepth Sliceを別Subresourceとは扱いません。

## 35. D3D12CalcSubresource

```cpp
const UINT subresource = D3D12CalcSubresource(
    mipSlice,
    arraySlice,
    planeSlice,
    mipLevels,
    arraySize);
```

## 36. Plane Count

Depth/Stencil、YUV等のMulti-plane FormatではPlaneを考慮します。Format Infoから取得します。

## 37. Default Texture作成

DEFAULT Heap、初期State COPY_DESTでCommitted/Placed Resourceを作ります。

## 38. Upload Size

TextureはSourceの`width*height*bytes`だけでUpload Sizeを決めません。Copyable FootprintをDeviceに問い合わせます。

## 39. GetCopyableFootprints

```cpp
std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
std::vector<UINT> rowCounts(subresourceCount);
std::vector<UINT64> rowSizes(subresourceCount);
UINT64 totalBytes = 0;

device->GetCopyableFootprints(
    &textureDesc,
    0,
    subresourceCount,
    baseOffset,
    layouts.data(),
    rowCounts.data(),
    rowSizes.data(),
    &totalBytes);
```

## 40. Placed Footprint

Upload Buffer内のOffsetと、Copy用Format/Width/Height/Depth/RowPitchを表します。

## 41. Row Pitch Alignment

Texture Copy用Row Pitchは`D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`、通常256 byte Alignmentを守ります。

## 42. Placement Alignment

Subresource Footprint Offsetは`D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT`、通常512 byte Alignmentを守ります。

## 43. Row SizeとRow Pitch

```text
Row Size  : 実Pixel/Block DataのByte数
Row Pitch : 次Rowまでの間隔、Alignment Padding込み
```

## 44. Source Row Pitch

Decoder/Asset Data側のRow間隔です。GPU Footprint Row Pitchとは異なる場合があります。

## 45. Slice Pitch

一Depth Slice/2D Surfaceぶんの間隔です。3D Texture Uploadで必要です。

## 46. Upload Buffer作成

`totalBytes`以上のUPLOAD Heap Bufferを作りPersistent Mapします。

## 47. Base Offset

共有Upload Pageの一部へFootprintを配置する場合、Base OffsetをAlignmentへ揃えます。

## 48. Row Copy

```cpp
for (UINT z = 0; z < layout.Footprint.Depth; ++z)
{
    const std::byte* sourceSlice =
        source.data + z * source.slicePitch;

    std::byte* destinationSlice =
        mapped + layout.Offset +
        z * layout.Footprint.RowPitch * rowCount;

    for (UINT row = 0; row < rowCount; ++row)
    {
        std::memcpy(
            destinationSlice + row * layout.Footprint.RowPitch,
            sourceSlice + row * source.rowPitch,
            static_cast<size_t>(rowSize));
    }
}
```

## 49. 一括memcpyが危険な理由

Destination Row Padding、Source Pitch、Depth Slice、BC Block Layoutがあるためです。

## 50. Padding初期化

CopyされないRow Paddingを読むことは通常ありませんが、Debug再現性/Tool要件に応じてUpload Rangeを0初期化できます。

## 51. Source Capacity検証

`rowPitch >= rowSize`、`slicePitch`、Data Size、Subresource CountをCopy前に検証します。

## 52. Destination Location

```cpp
D3D12_TEXTURE_COPY_LOCATION destination{};
destination.pResource = texture;
destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
destination.SubresourceIndex = subresourceIndex;
```

## 53. Source Location

```cpp
D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
sourceLocation.pResource = uploadBuffer;
sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
sourceLocation.PlacedFootprint = layouts[subresourceIndex];
```

## 54. CopyTextureRegion

```cpp
commandList->CopyTextureRegion(
    &destination,
    0, 0, 0,
    &sourceLocation,
    nullptr);
```

Source Box `nullptr`でFootprint全体をCopyします。

## 55. Copy State

Destination TextureはCOPY_DEST、Upload BufferはGENERIC_READ/COPY_SOURCE互換で使います。

## 56. Subresource Loop

全Mip/Array/PlaneのLocationを作り、対応Source DataをCopyします。

## 57. Copy Box

部分領域Copyでは`D3D12_BOX`をSourceへ指定します。座標/Block Alignment/範囲を検証します。

## 58. Destination Offset

Copy先x/y/z OffsetとFormat/Block制約を守ります。

## 59. Texture間Copy

Format/Dimension/Sample等の互換性を守り、双方をCOPY_SOURCE/COPY_DESTへTransitionします。

## 60. CopyResource

Resource全体が互換な場合に一括Copyできます。部分/Footprint UploadではCopyTextureRegionを使います。

## 61. Copy後Transition

TextureをCOPY_DESTからPIXEL_SHADER_RESOURCE/NON_PIXEL_SHADER_RESOURCE等へ遷移します。

## 62. Read State組合せ

Graphics/Compute両方で読むなら必要Read State Bitを組み合わせるかState Tracker Policyを使います。

## 63. Upload Fence

Copy Queue/Direct QueueのSignal Valueを保存し、GPU Copy完了までUpload Rangeを保持します。

## 64. Asset Ready

Copy完了とConsumer Queue Wait/Transitionが済むまでTextureをReadyとして公開しないか、Default Textureを使います。

## 65. Async Upload

CPU Decode、Upload Memory Copy、Copy Command、GPU CompletionをTask/Ticketとして分離します。

## 66. Publish順序

新TextureとSRVを作りUpload完了後にHandle Mappingを切替え、旧Texture/DescriptorをFence後に解放します。

## 67. SRV Description

```cpp
D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
srv.Format = viewFormat;
srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srv.Texture2D.MostDetailedMip = 0;
srv.Texture2D.MipLevels = mipLevels;
srv.Texture2D.PlaneSlice = 0;
srv.Texture2D.ResourceMinLODClamp = 0.0f;
```

## 68. CreateShaderResourceView

```cpp
device->CreateShaderResourceView(
    texture,
    &srv,
    destinationCpuHandle);
```

Descriptor SlotとResource Lifetimeを別管理します。

## 69. Texture Array SRV

View DimensionをTEXTURE2DARRAYにし、Most Detailed Mip、Mip Levels、First Array Slice、Array Sizeを指定します。

## 70. Cube SRV

View DimensionをTEXTURECUBEにします。Resource Array Sizeは6 Faceを含む必要があります。

## 71. Cube Array SRV

First2DArrayFaceとNumCubesを指定します。

## 72. Texture3D SRV

View DimensionをTEXTURE3DにしMip範囲を指定します。

## 73. MSAA SRV

TEXTURE2DMS/ARRAY Dimensionを使います。通常TextureとMip Field構造が異なります。

## 74. Most Detailed Mip

Streaming/LOD制限で見せるMip範囲の先頭を制御できます。

## 75. ResourceMinLODClamp

Sampling可能な最小LODを制限できます。Streaming設計と組み合わせます。

## 76. Mipの目的

小さく表示されるTextureを適切な解像度でSamplingし、Alias、Cache/Bandwidth負荷を抑えます。

## 77. D3D12に自動GenerateMipsはない

D3D11 Contextのような単純自動APIへ依存せず、Offline、Compute、Render Pass等で生成します。

## 78. Offline Mip

Asset Cook時に品質の高いFilter、Normal/Alpha補正、BC圧縮まで行いDDSへ保存できます。

## 79. Runtime Compute Mip

Mip NをSRVで読みMip N+1をUAVへ書くCompute Passを繰り返します。

## 80. UAV対応Resource

Runtime Compute Mip生成にはALLOW_UNORDERED_ACCESS Flag、Format Support、Mip別UAV Descriptorが必要です。

## 81. Mip State

入力MipをSRV、出力MipをUAVへSubresource単位でTransitionします。

## 82. UAV Barrier

次Dispatchが前Mip結果へ依存する場合、Transition/UAV Barrierで順序を保証します。

## 83. Odd Size

奇数Dimension DownsampleのSample座標/Weightを正しく扱います。

## 84. SRGB Mip

Color TextureはLinear空間でFilterし、保存/表示Encodingを正しく扱います。

## 85. Normal Map Mip

Average後にNormalを再正規化する等、通常Colorと異なる処理が必要です。

## 86. Alpha Coverage

Alpha Test TextureはMipでCoverageが変わりSilhouetteが消えることがあります。Coverage-preserving Mipを検討します。

## 87. Roughness Mip

Specular Alias対策等、Material Dataに応じたFilterをOffline Pipelineで検討します。

## 88. Streaming Mip

低解像Mipを先にUploadし、高解像Mipを後から追加する設計があります。Resource/Tiled Resource方式を選びます。

## 89. Reserved/Tiled Texture

Mip/TileのPhysical Memory Mappingを変更し、大Textureの一部だけResidentにできます。

## 90. Mip Residency Feedback

Sampler Feedback等を利用できるHardwareでは必要Mip/RegionのStreaming判断に使えます。

## 91. Texture Cache Key

PathだけでなくSRGB/Linear、Format、Mip Policy、Compression、Usage、VersionをKeyへ含めます。

## 92. Same File Different View

同一ResourceへLinear/SRGB互換Viewを作る設計が可能な場合があります。Typeless/Format Familyを正しく使います。

## 93. Texture Handle

Materialは生Resource/SRV Handleでなく世代付きTexture Handle/Descriptor Indexを持ちます。

## 94. Default Textures

White、Black、Flat Normal、Default ORM、Error Checkerを起動時に作ります。

## 95. Error Texture

Decode/Upload失敗時に明確なChecker Textureを表示し、Path/ErrorをLogします。

## 96. Readback Footprint

TextureをREADBACK BufferへCopyする際も`GetCopyableFootprints`でRow Pitch/Layoutを取得します。

## 97. Screenshot Flow

```text
Texture -> COPY_SOURCE
Readback Buffer -> COPY_DEST
CopyTextureRegion
Fence Wait
Map
Row Pitchを除いてImage Encoderへ渡す
```

## 98. Readback Row Copy

GPU Row PitchのPaddingを除き、Encoderが期待する密なRowへCopyします。

## 99. HDR Screenshot

Float/HDR ResourceをEXR等へ保存するか、Tone Map後SDRをPNGへ保存するかを明示します。

## 100. Texture Update

部分更新でもUpload FootprintとCopy Boxを使います。GPU使用中Resourceへの書込み順をBarrier/Fenceで管理します。

## 101. Dynamic Texture Ring

Video/Streaming等で複数Texture/Upload SlotをRing化し、GPU使用中内容を上書きしません。

## 102. Render Target Texture

ALLOW_RENDER_TARGET、Optimized Clear、RTV/SRV Descriptor、State Transitionを用意します。

## 103. Depth Texture

Typeless Resource＋DSV/SRV ViewのFormat設計、Plane、Depth Stateを扱います。次章で詳しく扱います。

## 104. UAV Texture

ALLOW_UNORDERED_ACCESS Flag、Typed UAV Support、UAV Descriptor、Barrierを管理します。

## 105. Texture Memory統計

Format、Dimension、Mip、Array、Allocation Size、Resident Mip、Upload Byte、Decode/Upload時間を記録します。

## 106. PIX

Subresource、Mip、Format、SRV、Copy Footprint、Resource State、内容を確認します。

## 107. Debug Name

Asset名、Format、Size、Mip、SRGB、Array/CubeをResource/Registryへ記録します。

## 108. Device Lost

Source/Cooked Asset MetadataからTexture、Upload、SRVを再生成します。

## 109. Hot Reload

新Texture Upload完了後にHandleを切替え、旧Resource/Descriptorを最終使用Fence後に解放します。

## 110. Unit Test

Mip Count/Dimension、Subresource Index、Pitch、Footprint範囲、BC Size、SRGB Policy、Cache Key、OverflowをTestします。

## 111. Integration Test

RGBA、SRGB、Mip、BC、Array、Cube TextureをUpload/Samplingし期待画像を比較します。

## 112. Stress Test

多数Texture、Async Decode/Copy、Upload Page不足、Mip Streaming、Hot Reload、Device Lostを組み合わせます。

## 113. よくある失敗：一括memcpy

Source/Destination Row Pitchが違うためRowが崩れます。RowごとにRow SizeだけCopyします。

## 114. よくある失敗：Source SizeだけUpload確保

GPU Row/Placement Alignment Paddingが足りません。GetCopyableFootprintsのTotal Bytesを使います。

## 115. よくある失敗：SRGB誤用

Normal/RoughnessをSRGB Samplingし数値が変わります。Texture SemanticからColor Spaceを決めます。

## 116. よくある失敗：Copy後State忘れ

COPY_DESTのままShaderが読みます。必要SRV StateへTransitionします。

## 117. よくある失敗：Upload即再利用

GPU Copy完了前に別Texture Dataで上書きします。Fence付きUpload Ringを使います。

## 118. よくある失敗：Mip/Array順序違い

CPU Subresource配列とD3D12のIndex計算がずれます。CalcSubresourceとMetadataを使います。

## 119. よくある失敗：BCをPixel Byte計算

Width×BytesPerPixelでRow Sizeを出します。4x4 Block数とBlock Byteで計算します。

## 120. 実装Checklist

- [ ] Source Image Metadata/Subresourceを表現する。
- [ ] WIC/DDS/Cooked Assetの用途を区別する。
- [ ] Texture Desc/Format/Mip/Arrayを検証する。
- [ ] Subresource Count/Indexを正しく計算する。
- [ ] GetCopyableFootprintsでUpload Layoutを得る。
- [ ] Row/SliceごとにDataをCopyする。
- [ ] CopyTextureRegionとState Transitionを記録する。
- [ ] Upload RangeをFence完了まで保持する。
- [ ] Dimensionに合うSRVを作る。
- [ ] SRGB/LinearをTexture Semanticから選ぶ。
- [ ] Mip FilterをColor/Normal/Alpha用途別に扱う。
- [ ] ReadbackでRow Paddingを除去する。

## 121. 理解確認問題

1. CPU ImageとGPU Texture Layoutの違いを説明してください。
2. Row Size、Source Row Pitch、GPU Row Pitchの違いを説明してください。
3. GetCopyableFootprintsが必要な理由を説明してください。
4. Texture UploadをRowごとにCopyする理由を説明してください。
5. Mip/Array/PlaneからSubresourceを作る方法を説明してください。
6. D3D12でMipを生成する三方式を説明してください。
7. SRGB ViewをData Textureへ使えない理由を説明してください。
8. Upload BufferをFenceまで保持する理由を説明してください。

## 122. 章末要点

- Image FileをCPU SubresourceへDecodeし、GPU TextureとはLayoutを分離します。
- GetCopyableFootprintsでOffset、Row Pitch、Row Count、Total Bytesを取得します。
- Upload BufferへSubresource/Depth/Row単位で実Data ByteをCopyします。
- CopyTextureRegion後にShader Read StateへTransitionしSRVを作ります。
- Upload Memory/ResourceをCopy Fence完了まで保持します。
- MipはOffline/Compute/Renderで生成し、Color/Normal/AlphaごとにFilterを変えます。
- SRGB、BC、Array、Cube、3D、ReadbackのFormat/Subresource規則を守ります。

## 123. 公式資料

- [Copying texture data](https://learn.microsoft.com/en-us/windows/win32/direct3d12/copying-texture-data)
- [ID3D12Device::GetCopyableFootprints](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints)
- [ID3D12GraphicsCommandList::CopyTextureRegion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion)
- [D3D12_PLACED_SUBRESOURCE_FOOTPRINT](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_placed_subresource_footprint)
- [D3D12_SUBRESOURCE_FOOTPRINT](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_subresource_footprint)
- [ID3D12Device::CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [DirectXTK12 WICTextureLoader](https://github.com/microsoft/DirectXTK12/wiki/WICTextureLoader)
- [DirectXTK12 DDSTextureLoader](https://github.com/microsoft/DirectXTK12/wiki/DDSTextureLoader)
- [DirectXTK12 ResourceUploadBatch](https://github.com/microsoft/DirectXTK12/wiki/ResourceUploadBatch)

次章では、Depth/Stencil ResourceとView、Blend、Rasterizer、MSAA、Read-only Depth、Stencil Maskを扱います。
