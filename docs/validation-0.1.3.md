# NKB LIMIT 0.1.3 検証結果 — 2026-10-14

## 変更と判断

Notionの次重点とユーザーの追加要望に沿い、ポンピングの軽減、Threshold／Ceiling中心のUI、
UI表記「NKB LIMIT」を実装した。0.1.2の音圧・Ceiling追従と既存Cubase設定の互換性を維持する。

DSPはAdaptive Releaseの信号エネルギー検出を25 msから50 msへ変更した。
100 msの保持ピーク、6–200 msのピーク復帰、持続音の15/90 ms、先読みの予測ランプ、
2段のTrue Peakガード、4x/8x FIRは変更していない。処理品質や再構成タップを下げる変更はない。
信号状態の短い変化にゲイン制御が過度に追従することを抑えるための変更で、
単に入力を減らしたり、出力を平均化して数字だけ改善する処理ではない。

候補選定は音量を揃えた実音源、持続右997 Hz＋周期左キック、純音・混合音で行った。
予測attackだけを強める案、conditional 12 ms release、長い保持ピーク125/150 msなどは
改善の不足や別条件でのAM／オンセット保持の回帰により採用しなかった。
50 ms検出は短い揺れを減らす一方、一部の遅い揺れと強い周期制限の復帰には代償が残る。
全条件でポンピングを解消したとは扱わない。

## 実音源の全編比較

ユーザー指定の `カタフラクト_mix_2026-09-25.wav`、PCM24 stereo 48 kHz、238.6667秒を使用。
0.1.2と0.1.3を実VST3ホストで8x、Ceiling −0.5 dBTP、double、512 samplesで全編・末尾まで処理した。
最終TPは表示値ではなく独立16x/96-tapで測定。完成した3秒窓のLUFS-Sを約10 Hzで読んだ統計で、
Integrated LUFSではない。元ファイルのハッシュは不変。

| Input Gain／UI Threshold | 0.1.2 LUFS-S中央値 | 0.1.3 LUFS-S中央値 | 0.1.3 LUFS-S最大 | 0.1.3 独立TP |
|---|---:|---:|---:|---:|
| +7 dB／−7.5 dB | −8.862 | −8.804 | −6.597 | −0.551 dBTP |
| +16 dB／−16.5 dB | −6.270 | −6.247 | −5.619 | −0.546 dBTP |

音圧の上昇は0.058／0.022 LUで小さい。今回の目的は0.1.2比の音圧増加ではなく、音量の揺れを抑えること。
60–90、120–150、190–220秒の3区間×2ゲインを同じK加重平均へ揃えて比較した。
全6組で原音比50 msゲイン変動のp90−p10幅が約0.092–0.380 dB減少し、400 ms幅も減少した。
残りの整数サンプルの時間ずれは全組0。

代償として、原音から選定した20オンセットの先頭10 msエネルギー中央値は追加で
約0.009–0.169 dB小さくなった。最大の190–220秒／+16 dBでは原音比−1.838→−2.007 dB。
今回はトランジェント保持の改善とは主張しない。Ozone同等の音質・アタック保持も未達のまま。
独立192-tap/32-phaseの抜粋TPは+7で−0.554、+16で−0.550 dBTP付近で、旧版と同等。

## 周期的なポンピングの分離測定

自作30秒音源は右に連続997 Hz、左に周期キックと必要に応じ80 Hz持続低音を配置した。
120／180 BPM、低音あり／なし、Input Gain +7／+16の全8組を両VST3で処理。
右チャンネルの変動幅とキック周期の側帯波AMは全体の音量合わせに依存しない。

| Full VST3の条件 | 0.1.2 50 ms持続音変動幅 | 0.1.3 50 ms持続音変動幅 |
|---|---:|---:|
| 120 BPM／+7 dB／低音なし | 3.527 dB | 1.575 dB |
| 120 BPM／+7 dB／低音あり | 4.193 dB | 1.455 dB |
| 180 BPM／+7 dB／低音なし | 3.313 dB | 1.091 dB |
| 180 BPM／+7 dB／低音あり | 3.351 dB | 1.963 dB |
| 120 BPM／+16 dB／低音なし | 11.268 dB | 8.208 dB |
| 120 BPM／+16 dB／低音あり | 11.362 dB | 6.473 dB |
| 180 BPM／+16 dB／低音なし | 10.621 dB | 6.252 dB |
| 180 BPM／+16 dB／低音あり | 10.355 dB | 6.214 dB |

全8組で50 ms窓のp95−p5幅とキック周期AMが減少した。
ただし400 ms窓では、+7／180 BPM／低音ありで0.333→0.707 dB、
+16／120 BPM／低音なしで1.098→1.232 dBに増える条件がある。
主段単体の強い周期キック（120 BPM／+16／低音なし）は、120 msのバースト終了後に
前回ヒット前のゲイン−0.5 dBへ戻る時間が約89.6→249.5 msとなる。
一方、単発60 msキックの0.5 dB以内への復帰は43.4→38.8 msへ短縮。
これらは別条件の測定であり、ひとつの復帰性能値として混ぜない。
短い揺れの減少だけを聴感上の全面的な改善と解釈しない。

## UIと互換性

860×560の独自JUCE画面。上段に大きなLUFS-S／TP／GR、中央にThreshold／Ceilingの縦フェーダー、
左右に入力／出力ピークメーターを置き、ヘッダーをNKB LIMITへ変更した。
数値5 Hz、バー30 Hz、ピーク／GRの1秒保持は維持。

Thresholdは既存Input Gainを `Threshold = Ceiling − Input Gain` で表示する。
Ceiling −0.5、Threshold −7.5はInput Gain +7 dBに相当する。Thresholdを下げると音圧が上がる。
Ceiling変更時はInput Gainを維持し、Threshold表示が追従する。
VST3 ID、3パラメーターのID／ホスト上の名前、保存スキーマ、プラグインのホスト名nkblimitは維持。
詳細は [UI設計・互換性](ui-threshold-design.md)。

数値変換と単位付きテキスト解析、ホスト通知、ドラッグの開始／複数変更／終了、
編集画面をドラッグ中に閉じた場合のジェスチャ終了、Ceiling操作時のInput Gain不変を検証した。
実際のLabel編集欄のフォーカス／ReturnやCubaseの実マウス操作・HiDPIは別途実機確認が必要。
実コンポーネントを2倍の画像へ描画し、見切れ・重なりを目視確認した。

## 自動検証と保存

MSVC 19.51／VS2026 Insiders、SDK 10.0.26100.0、CMake 4.4.3、Ninja、Release、JUCE 8.0.15固定。
全9テスト合格、22.00秒：DSPTests、LoudnessTests、MeterBallisticsTests、TruePeakGuardTests、
LimiterBehaviourTests、PumpingBehaviourTests、CeilingTrackingTests、ProcessorTests、VST3HostTests。
新規PumpingBehaviourTestsは44.1/48/96/384 kHzのdoubleと48 kHzのfloat、計40組で、sample ceiling、50/400 ms揺れ、AM、
周期的な復帰を検証。既存の低域歪み、ドライブ単調性、単発復帰、RT割当、TP行列の条件は緩めていない。

pluginval v1.0.4 Strictness 5、seed 42、5サンプルレート、block 1/64/127/512/2048でSUCCESS／exit 0。
GUI試験はskip。既存の同梱LV2テスト資源警告とVST3の成功結果を区別している。

検証ログ・JSON・CSVはdocs/verification-0.1.3、独立品質監査は
[quality-validation-0.1.3.md](quality-validation-0.1.3.md)。
試聴用30秒WAVと音量一致A/Bはoutputs/audio-comparison-0.1.3へ保存した。
元音源をソースリポジトリやソースZIPへ含めない。

遅延は0.1.2から変更なし：44.1／48／88.2／96／192 kHzで558／567／668／687／927 samples、4x/8x共通。
検証済み0.1.3を `C:\Program Files\Common Files\VST3\nkblimit.vst3` へ配置した。
ビルド／配布用／配置先のモジュールとメタデータのハッシュ一致を確認し、配置先からの
実VST3ロード・処理・保存復元・アンロードも5レート／float・doubleで合格した。
モジュールSHA-256は `F9C82507829425A93C1FA9FDF6E6DEB0D9B8FC27939902F5B840E7DF9C9DF677`。
更新後はCubaseを起動し直してロードする。
48 kHzは11.8125 ms。Cubase内の長時間再生・複数インスタンス・書き出し一致、正式なメーター規格適合は未完了。
Punch、Loud、Clipper、Integrated LUFS、GR履歴は未実装。GitHubへのpushは行っていない。
