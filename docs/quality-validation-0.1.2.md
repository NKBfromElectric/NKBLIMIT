# 0.1.2 音量一致・時間軸・音質プロキシ検証

2026-10-14。元WAVは読み取り専用で使用し、SHA256の前後一致を確認しました。
数値は音の変化を比較する補助です。主観的な音質、歪みの聴感、Ozoneとの音質同等性を証明するものではありません。

## 実曲・旧版との比較

元曲の190–220秒を、Input Gain +16 dB、Ceiling −0.5 dBTP、8xで処理した30秒のVST3レンダーを比較しました。
遅延補償後の波形で整数サンプルの位置一致を確認しています。

| 30秒抜粋の測定 | 0.1.1 | 0.1.2 |
|---|---:|---:|
| ゲートなしK加重平均・LUFS相当 | −8.0052 | −6.0387 |
| Crest factor | 10.1128 dB | 8.6116 dB |
| 同K加重平均での原音との差分RMS／原音RMS | −14.2962 dB | −14.6493 dB |
| 原音と出力の50 ms RMSエンベロープ相関 | 0.5134 | 0.6572 |
| 原音で選定した20オンセット・先頭10 msの相対RMS中央値 | −2.4982 dB | −1.8381 dB |
| 上記オンセット直後50 msの相対RMS中央値 | −2.4860 dB | −0.0223 dB |

相対RMSと差分RMSは、両音源を同じゲートなしK加重平均に揃えてから計算しています。
原音で強い10 msのエネルギー増加を選び、120 ms以上離れた20イベントを使用しました。
直後の50 msが回復したことは、持続するダッキングが減った根拠になります。
Crest factorが下がったことも併記しており、強い制限で潰れる感覚が解消したとは断言しません。

+24 dB入力では、同じ抜粋のK加重平均は−5.9887 LUFS相当となり、+16 dBからの増加は0.0500 LUでした。
入力をさらに上げても、この区間の平均音量増加は小さく、同音量での差分RMSとエンベロープ相関は+16 dBより悪化しました。
同音量での粗い帯域パワーも変化しています。0.1.2 +16 dBは原音比で20–60 Hzが−0.694 dB、8–20 kHzが+0.743 dBです。

## Ozone 9 Elementsとの直接比較

実機に入っているVST3を、公開されたホストパラメータだけで設定・処理しました。
設定はユーザーの画像に合わせてIRC II、Threshold −7.50 dB、Ceiling −0.50 dB、Character 2、True Peakオン、Stereo Unlink 0、Transient Shapingオフです。
EQとImagerはバイパスし、入力・出力の追加ゲインは0 dBです。商用プラグインのコードやアセットは参照していません。

静音997 Hz入力による実ゲイン校正はOzone +7.00195 dB、NKBLIMITのInput Gain +7設定が+6.99306 dBで、差は約0.009 dBでした。
Thresholdの数値だけからInput Gainの同等性を仮定していません。

Ozoneのレイテンシーは設定反映前784サンプル、校正・再prepare後3790サンプルでした。
最終レンダーは3790で補償し、NKBLIMITは567で補償しています。
原音に対する±10000サンプルの相互相関確認で、両最終WAVの残り整数遅延は0サンプルでした。
旧784で切り出した中間レンダーの計測値と音量一致ファイルは最終版で上書きしています。

| 190–220秒・30秒抜粋 | Ozone画像設定 | NKBLIMIT 0.1.2 +7 dB |
|---|---:|---:|
| ゲートなしK加重平均・LUFS相当 | −7.55442 | −7.73612 |
| Crest factor | 10.1595 dB | 10.2895 dB |
| 同K加重平均での原音との差分RMS／原音RMS | −12.2781 dB | −22.3043 dB |
| 原音で選定した20オンセット・先頭10 msの相対RMS中央値 | −0.62541 dB | −1.76140 dB |
| 上記オンセット直後50 msの相対RMS中央値 | +0.18578 dB | −0.08367 dB |

この抜粋はOzoneの平均が0.18170 LU高く、選定オンセットの先頭10 msもOzoneが1.136 dB多く保持していました。
全曲のLUFS-S中央値が近くても、局所的な音圧・オンセットの挙動は同等ではありません。
NKBLIMITの差分RMSが小さいことは、Ozoneより良い音を意味しません。
Ozone並みのトランジェント保持と、ユーザーが感じる潰れ感の改善は、今後の調整・同音量試聴が必要です。

## 独立True Peakクロスチェック

NumPyの独立FFT畳み込みで、UI相当32タップ・8相Blackman、96タップ・16相Kaiser、192タップ・32相Kaiserの有限FIR再構成を比較しました。
以下は30秒抜粋の値です。規格適合認証や全入力の数学的上限を意味しません。

| 音源 | UI相当 dBTP | 96タップ・16相 dBTP | 192タップ・32相 dBTP |
|---|---:|---:|---:|
| 0.1.1 +16 dB | −0.991527 | −0.990291 | −0.990274 |
| 0.1.2 +16 dB | −0.542481 | −0.550220 | −0.549715 |
| 0.1.2 +24 dB | −0.534018 | −0.550189 | −0.549739 |
| 0.1.2 +7 dB | −0.551586 | −0.553765 | −0.553805 |
| Ozone画像設定 | −0.507444 | −0.485102 | −0.476327 |

旧版の−1.0 dB付近はUIの大きな誤差ではなく、実出力に入っていた0.5 dBの追加マージンでした。

## 分離DSPプローブ

主リミッターだけを384 kHzで動かした補助検証です。VST3全体やJUCE再構成フィルターの計測とは区別します。
60 msの減衰60 Hzキックを静かな997 Hz背景音に重ねた場合、キック終了後にゲインが0.5 dB以内へ戻る時間は447.09 → 43.42 msでした。
60 Hz定常音で2／6／10 dB制限したTHD+Nは0.0956／0.1566／0.1449% → 0.0843／0.1353／0.1366%です。
997 Hzは数値誤差に近い領域でした。

単一正弦波だけで判断しないため、二音も比較しました。
60+62 Hzの残差／キャリアRMSは改善しましたが、60+997 Hzでは0.340／0.410／0.396% → 0.355／0.444／0.452%と少し増えています。
速い回復と定常混合音の変調にはトレードオフが残ります。

## 同音量の試聴ファイル

PCM24へ保存してから再読み込みし、K加重平均の一致と入力ハッシュの不変を検証しました。
音量合わせは大きい方を下げて行っています。これはIntegrated LUFSではありません。

- 原音と新版+16 dB: [原音190–220秒](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/audio-comparison-0.1.2/original-190to220sec.wav) ／ [新版・原音と同音量](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/audio-comparison-0.1.2/updated-gain16dB-matched-to-original-30sec.wav)。新版を8.15951695 dB減衰し、保存後の差は−1.8×10⁻⁹ dBでした。
- Ozoneと同等実ゲインの新版: [Ozone・同音量](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/ozone-comparison-0.1.2/ozone-threshold-7p5dB-level-matched-30sec.wav) ／ [NKBLIMIT +7・同音量](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/ozone-comparison-0.1.2/nkblimit-gain7dB-level-matched-30sec.wav)。Ozoneを0.18170352 dB減衰し、保存後の差は5.3×10⁻¹⁰ dBでした。

生データは[実曲品質プロキシJSON](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/NKBLIMIT/docs/verification-0.1.2/quality-audit.json)と[Ozone品質プロキシJSON](C:/Users/nkbys/Documents/Codex/2026-10-14/files-pasted-by-the-user-nkblimit/outputs/NKBLIMIT/docs/verification-0.1.2/ozone-quality-audit.json)に保存しています。
