# nkblimit 0.1.1 検証結果 — 2026-10-14

数字が読みにくい、LUFSがない、音圧が上がりにくいという使用時の問題を修正した開発版。
JUCE 8.0.15、AGPL-3.0-only、公開パラメーターID／保存スキーマは0.1.0から維持。

## 変更

- 820×500の実JUCE UI。入力／出力を縦型メーター、LUFS-S／TP／GRを大きな独立表示にした。
- 数字は5 Hz、バーは30 Hz。ピーク／GRを1秒保持し、緩やかに下降する。
- 音声ブロック間の最大値をatomic CAS/exchangeで保持し、短いピークを表示更新前に失わない。
- 最終ステレオ出力のK-weighted／ゲートなし3秒窓からLUFS-Sを算出。最初の3秒は取得中表示。
- 旧TP安全処理の絶対寄与和が位相相殺を捨て、余分に減衰させていた。符号付き128-tap/8x予測、
  64-sampleの先読みアタック、32ms release、実測で選んだ0.5dBマージンへ変更した。
- リセットボタンにもホストのオートメーション開始／終了通知を付けた。

LUFSの計測窓に追加の平滑化は行わない。表示更新を読みやすくした設計であり、完全なEBU Mode認証を主張しない。
TP表示は8x/32-tapの推定器。検証には別の16x/96-tapを用いる。

## 自動検証

Windows x64、MSVC19.51、SDK10.0.26100、CMake4.4.3、Ninja、Releaseでビルド。
再開後に全6テストを再実行し、すべて合格。ログはverification-0.1.1/に保存。

| 試験 | 確認内容 |
|---|---|
| DSPTests | 480ケース、全5レート／float・double／4x・8x／Ceiling3値、約230万チェック |
| LoudnessTests | 3,339チェック、独立48k係数参照、全5レート、3秒窓、左右重み、リセット、無音、不正入力 |
| MeterBallisticsTests | 1秒ホールド、5Hzの数字更新でも短いピーク保持、下降速度、リセット |
| TruePeakGuardTests | 768ケース、独立16x/96-tapで最大Ceiling差−0.014dB、処理／reset時の割当なし |
| ProcessorTests | 保存復元、float/double、出力LUFS校正、区間ピーク、GUI、リセット |
| VST3HostTests | 実VST3のロード／音声処理／保存復元／アンロード、全5レートと両精度 |

DSPTestsの最大Ceiling差は−0.418dB。Guardの追加20x/128-tap監査384ケースも目標内。
pluginval v1.0.4、Strictness5、seed42、指定5レート／1・64・127・512・2048ブロックはSUCCESS。
pluginval GUI試験はskip。GUIは実JUCEの画像生成と目視確認を実施した。
pluginval stderrに同梱LV2テスト資源の一時パス警告があるが、VST3試験のexitは0。

## 提供された実楽曲の全編比較

PCM24／48kHz／Stereo、238.6667秒のWAVを変更せずに読み込んだ。
旧0.1.0と修正0.1.1を別々に実VST3ホストで処理。両者とも8x、Ceiling−0.5dBTP。
最終出力全編と末尾を独立16x/96-tapで測定し、すべての出力が目標内だった。

| Input Gain | 旧LUFS-S中央値 | 新LUFS-S中央値 | 改善 | 新LUFS-S最大 | 新True Peak |
|---|---:|---:|---:|---:|---:|
| +16dB | −13.204 | −8.410 | +4.794LU | −7.403 | −0.982dBTP |
| +24dB | −13.237 | −8.558 | +4.678LU | −7.495 | −0.982dBTP |

中央値は完成した3秒窓を約10Hzで読んだ統計で、Integrated LUFSではない。
この音源では+16dBで既に強い制限に達し、+24dBでさらに音圧は上がらなかった。
Adaptive Releaseによる復帰量の違いもあり、高いInput Gainほど必ず音圧が増すとは主張しない。
Loud／Punchの設計と、強い制限下での音圧・トランジェントの両立は次の開発課題。

190–220秒の旧版／新版30秒WAVと、K-weighted平均パワーを合わせた新版も別のoutputs/audio-comparison/へ保存。
音量一致版は新版を4.842dB減衰したもので、主観的な音質やOzoneとの同等性を検証したものではない。
ユーザー音声はソースリポジトリ／ソースZIPに含めない。元WAVのSHA-256は読み込み前後で一致。

## 遅延と残る確認

| fs | 4x/8x共通の報告遅延 |
|---|---:|
| 44.1kHz | 367 samples |
| 48kHz | 376 samples |
| 88.2kHz | 477 samples |
| 96kHz | 496 samples |
| 192kHz | 736 samples |

独立計測で検証した有限の信号集合であり、全入力の数学的保証やBS.2217／EBU規格認証ではない。
旧版がCubaseに認識されたことはユーザー画像で確認。新版の実Cubaseでのオートメーション、
保存復元、書き出し／実時間一致、長時間・複数インスタンス動作は未完了。
Ozoneとの音量一致の直接比較、実楽曲の主観的試聴評価、エイリアシング／CPU持続試験も残る。
Punch、Loud、Clipper、Integrated LUFS、GR履歴は未実装。GitHubへのpushは行わない。
