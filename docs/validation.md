# 検証結果 — 2026-10-14

これは初期版0.1.0の記録です。最新の結果は [0.1.1の検証資料](validation-0.1.1.md) を参照してください。

今回の成果は **Phase 1 開発版 0.1.0**。Windows x64 VST3の生成と自動ホスト検証まで実施。
Cubaseでの実操作は未検証のため、Phase 1の完了条件をすべて満たしたとは扱わない。

## 実装済み

- C++20 / CMake、JUCE 8.0.15をコミット固定、AGPL-3.0-only。
- Stereo / float・double、Input Gain、Ceiling、4x/8x FIRオーバーサンプリング。
- Transparentの2.5ms先読み、ステレオ共通ゲイン、独立したAdaptive Release。
- ダウンサンプリング／切替後の保守的なTrue Peak安全処理。
- パラメーターのホスト通知、保存復元、平滑化、固定遅延でのOSクロスフェード。
- JUCE実コンポーネントの操作画面、入力／出力ピーク、TP推定、GR制御値。
- README、AGENTS、DSP設計、ライセンス資料、Cubase検証手順。

## 確認した結果

環境：Windows x64 / Visual Studio 2026 Insiders / MSVC 19.51.36257.0 /
Windows SDK 10.0.26100.0 / CMake 4.4.3 / Ninja / Release。

| 検証 | 結果と対象 |
|---|---|
| Release VST3ビルド | 成功、実バンドルとmoduleinfo.jsonを生成 |
| DSPTests | 合格、480ケースの信号行列＋追加試験、約222万の数値チェック |
| ProcessorTests | 合格、float/double、保存復元、OS切替の遅延、GUI生成破棄、不正状態入力 |
| VST3HostTests | 合格、生成VST3を別プロセスのJUCE headlessホストでスキャン／ロード／音声処理／アンロード |
| pluginval v1.0.4 | Strictness 5、exit 0 / SUCCESS。GUI試験はskip、seed=42 |
| GUI描画 | JUCE自身で2x画像を生成し、表示の切れ・重なりを目視確認 |

信号行列：44.1 / 48 / 88.2 / 96 / 192 kHz × float/double × 4x/8x ×
Ceiling -3/-0.5/0 dBTP × 無音、DC、インパルス、997Hz正弦波、0.46fs高域正弦波、
スイープ、固定seedノイズ、大ピーク。入力ゲイン+12dB、左右レベル比0.37。
ホストブロックは1/7/64/127/513/2048を変化させ、準備容量超過も確認。

独立測定は **16x / 96タップKaiser窓sinc**。最終出力と末尾ゼロ詰めを測定し、
最大のCeiling差は **約 -0.286 dB**。試験した480ケースはすべて
Ceiling +0.1dBの目標以内。これを全入力の数学的保証や規格認証とは解釈しない。

追加試験：NaN/Infと極端な有限値、サンプルレート再準備、ブロック分割の出力一致、
OS／Ceiling／Gainの連続変更、低レベルインパルスの遅延、TP推定器校正、
低レベル正弦波の振幅／位相、DSP処理とresetでのoperator new呼び出しゼロ。
malloc・OS内部割当を網羅するフックではなく、リアルタイム安全性の完全証明ではない。

997Hz・48kHzの低レベル正弦波で、整数遅延補償後の相対RMS誤差は
4xで約0.035%、8xで約0.080%。JUCEの整数遅延用Thiran補償は周波数依存の
残留位相誤差を持つ。単一周波数の結果であり、帯域全体の透明性は未証明。

pluginvalは指定5サンプルレートと1/64/127/512/2048ブロックで実行。
GUIは別のProcessorTestsと描画確認で検証した範囲に限定する。
pluginvalのstderrには同梱LV2テスト資源の一時パス警告があり、VST3試験結果はSUCCESS。
Steinberg公式validatorは未実行。

| サンプルレート | 報告遅延（samples、4x/8x共通） |
|---|---:|
| 44.1 kHz | 207 |
| 48 kHz | 216 |
| 88.2 kHz | 317 |
| 96 kHz | 336 |
| 192 kHz | 576 |

短時間のDSP測定では最遅ケースのRTFは約0.25だったが、実行環境や短い計測区間に
依存する。CubaseのCPU表示や長時間負荷保証ではない。両OS経路を常時処理する。

## 残課題

1. Cubase 13で認識、オートメーション、保存復元、遅延補償、実時間／オフライン整合、長時間安定性。
2. BS.2217等の正式なメーター試験と、独立した認証済みTrue Peakメーターでの比較。
3. 保守的な安全処理の余分な減衰・ゲイン変調、プリリンギング、ポンピングを実楽曲で評価。
4. エイリアシングの定量試験、帯域全体の伝達特性、持続CPUベンチマーク。
5. Punch / Loud / Clipper / LUFS / GR履歴 / 最終UIは将来フェーズ。未実装の操作項目は公開していない。
6. 初期Ceiling平滑化は20ms時定数。設定変更直後に新しい最終値を即時保証しない。

日本語MSVCでCMakeが/showIncludesを誤って解釈し、増分ビルドに古いヘッダー定義が
残る問題を検出した。`NKBLIMIT_MSVC_INCLUDE_PREFIX`を設定し、ヘッダー依存関係を修正。
関連コードを再ビルドして全テストを再実行した。READMEに環境用の設定を記載。

GitHubへのpush、公開設定変更、VST3のシステム領域へのインストールは行っていない。
