# nkblimit 0.1.2 検証結果 — 2026-10-14

## 修正と結果

0.1.1の長い減衰保持と、Ceilingより常に0.5 dB低くする最終ガードを修正した。
主段はピーク／持続音のdBエンベロープと信号のcrestに基づく復帰へ変更。
ガードは実際の処理後波形を再検査する2段のステレオリンク処理へ変更した。
メーターの数値を書き換えてCeilingに合わせる処理ではない。

提供された音源の全編を実VST3で処理した。+16 dB時のLUFS-S中央値は
−8.410から−6.270へ約2.14 LU上がり、独立True Peakは−0.982から−0.546 dBTPへ改善した。
同じ静音増幅量でOzone 9 Elementsの画像設定とも直接比較した。
中央値は近いが、最大3秒窓と音の変化には差がある。主観的な音質の同等性は主張しない。

詳しい設計と単体測定は [主段の修正](envelope-revision.md) と
[Ceiling追従](ceiling-tracking.md) を参照。

## ビルドと自動検証

Windows x64、MSVC 19.51 / VS2026 Insiders、SDK 10.0.26100.0、CMake 4.4.3、
Release / Ninja。JUCE 8.0.15の固定コミットは変更していない。
8テストすべて合格（18.04秒）。公開パラメーター比較ホストの修正後もVST3HostTests合格。

| テスト | 主な確認 |
|---|---|
| DSPTests | 各サンプルレート／精度／OS、Ceiling、遅延、切替、非有限入力、RT割当 |
| LoudnessTests | K加重、3秒窓、リセット、規定係数とレート変換、RT割当 |
| MeterBallisticsTests | 数値更新、1秒ホールド、減衰、リセット |
| TruePeakGuardTests | 768のfloat/double敵対入力、独立16x/96再構成、リンク、遅延、RT割当 |
| LimiterBehaviourTests | 先読み、復帰、入力増幅時の単調性、低域歪み、非有限入力、RT割当 |
| CeilingTrackingTests | 62の独立20x/128ケース、定常余裕、バースト、SIMD/scalar一致、GR時間整合 |
| ProcessorTests | 3パラメーター、保存復元、ホストジェスチャー、LUFS受渡し、RT処理 |
| VST3HostTests | 実VST3スキャン／ロード／保存復元／処理／アンロード、5レートと両精度 |

pluginval v1.0.4、Strictness 5、seed 42、44.1/48/88.2/96/192 kHz、
block size 1/64/127/512/2048でSUCCESS、exit 0。GUI試験はskip。
stderrの同梱LV2テスト資源警告とは別に、VST3試験の成功を確認した。
今回のUI操作部は0.1.1から変更していない。
記録はverification-0.1.2/へ保存。

ガード単体768ケースの最大Ceiling超過は、独立16x/96で+0.079303 dB、
別の20x/128で+0.063997 dB。事前の許容値+0.1 dB以内。
定常波のCeilingとの差は最大約0.060 dB。
これは有限の検証集合と再構成カーネルに対する結果であり、全入力の数学的保証や規格認証ではない。

## 元音源と0.1.1からの比較

ユーザー許可の `カタフラクト_mix_2026-09-25.wav` を変更せず読み込んだ。
PCM24、stereo、48 kHz、11,456,001 frames、238.6667秒。
元ファイルのSHA-256は
`CA0F1B8F88EBB496CA1292F38AF9BFB24F507C337240C51ACE9811ABE90A72A2`。
音声をソースリポジトリやソースZIPへ含めない。

両VST3とも8x、Ceiling −0.5 dBTP、double、512 samplesで全編と末尾を処理。
True Peakはプラグインの表示ではなく独立16x/96-tap Kaiser beta=9で測定。
LUFS-Sは完成した3秒窓を約10 Hzで読む統計であり、Integrated LUFSではない。

| Input Gain | 0.1.1中央値 | 0.1.2中央値 | 改善 | 0.1.2最大 | 0.1.2 True Peak |
|---|---:|---:|---:|---:|---:|
| +16 dB | −8.410 | −6.270 | +2.141 LU | −5.635 | −0.546 dBTP |
| +24 dB | −8.558 | −6.169 | +2.389 LU | −5.628 | −0.542 dBTP |

+24 dBが+16 dBより静かになる逆転はこの音源では解消したが、追加の音圧は約0.10 LUに留まる。
この強い制限域で入力をさらに増やすことが音質改善になるとは言えない。
190–220秒の比較WAVを別のoutputs/audio-comparison-0.1.2/へ保存した。
原音との音量一致版は新版+16 dBを8.159517 dB下げ、同じ区間のungated K加重平均パワーに揃えた。
この30秒平均の値は全曲LUFS-S中央値と区別する。

## Ozoneの画像設定との直接比較

インストール済みOzone 9 Elements 9.13.0を新規ホストインスタンスで処理。
公開VST3パラメーターだけを読み書きし、商用コード／アルゴリズム／素材は取得していない。
ユーザーのCubaseプロジェクトやOzoneインスタンスは操作していない。

画像に合わせてMaximizer IRC II、Threshold −7.50 dB、Ceiling −0.50 dB、
Character 2.00、True Peak ON、Stereo Unlink 0、Transient Emphasis OFF。
EQ／Imagerをバイパスし、入出力ゲイン0、Global／Maximizer bypass OFF。
設定後の公開表示の往復一致を確認した。

ThresholdとInput Gainの数字が同じ増幅量になるとは仮定しなかった。
静かな997 Hz正弦波で実増幅を測ると、Ozoneは約+7.002 dBだったため、
nkblimitを+7.00 dB（実測+6.993 dB）に設定した。差は約0.009 dB。
表示の小数点丸めとフィルター通過帯域差を含む。
両者ともfloat、48 kHz、512 samples、全編と末尾を処理した。

| 実VST3 | LUFS-S中央値 | LUFS-S最大 | 独立True Peak |
|---|---:|---:|---:|
| Ozone画像設定 | −9.024 | −5.998 | −0.427 dBTP |
| nkblimit 0.1.2、Input Gain +7 dB / 8x | −8.862 | −6.644 | −0.551 dBTP |

中央値では新版が0.162 LU高いが、最も大きい3秒窓ではOzoneより0.647 LU低い。
この結果から音質、全曲の印象、他の音源での性能同等性を結論しない。
Ozoneは設定の初回処理後に報告遅延が784から3790 samplesへ変化した。
静音校正後に再取得した最終値で末尾を処理し、抜粋を時間整合した。
nkblimitは567 samplesで変わらない。初回遅延値のまま処理した予備比較は採用しない。
190–220秒の出力と音量一致版をoutputs/ozone-comparison-0.1.2/へ保存する。
音量合わせでは必要な側のみ減衰し、追加リミッティングはしない。
この30秒区間はOzoneのK加重平均が新版より0.182 LU高く、全曲中央値とは異なる。
Ozoneだけ0.181704 dB下げたA/Bの再読込後の差は約5.3e−10 dB。
両者の時間ずれは広域相互相関で0 samplesを確認した。

原音から選定した20箇所のオンセットについて、音量一致後の10 msエネルギーの
原音に対する中央値はOzone −0.625 dB、新版 −1.761 dBだった。
新版のアタックが約1.14 dB余分に減衰している指標であり、潰れ感がOzone並みに解消したとは言えない。
一方、旧版からの強い制限域では、ピーク後の長い音量落ち込みを減らす指標を確認した。
音色の変化と残留歪みも含め、[独立音質監査](quality-validation-0.1.2.md) に
改善と残る差を区別して記録した。これらは主観的な優劣のスコアではない。

再現用ホストは `VST3HostTests --music <reference VST3> <nkblimit bundle>`。
`NKBLIMIT_TEST_AUDIO`で元WAV、`NKBLIMIT_TEST_PRECISION=float`、
`NKBLIMIT_TEST_DRIVES=7.5`、`NKBLIMIT_TEST_START_SECONDS=190`、
`NKBLIMIT_TEST_RENDER_DIR`で音声保存先を指定する。
旧版比較はreferenceに0.1.1 bundle、precision=double、drives=16,24を指定。

## 遅延と制限事項

| fs | 4x/8x共通の報告遅延 |
|---|---:|
| 44.1 kHz | 558 samples |
| 48 kHz | 567 samples（11.8125 ms） |
| 88.2 kHz | 668 samples |
| 96 kHz | 687 samples |
| 192 kHz | 927 samples |

検証済み0.1.2を `C:\Program Files\Common Files\VST3\nkblimit.vst3` へ配置し、
ビルドとのモジュール／メタデータのハッシュ一致を確認した。
配置先から実VST3をロードして5レート／両精度の処理・保存復元・アンロードも合格。
モジュールSHA-256は `FC6032A875AE27474E03806D77448407647961864E7BED4DBB41FB5C36C5BB72`。
Cubaseは新しい遅延値を受け取る必要がある。更新後に再起動してロードする。
今回Cubase内での長時間再生／複数インスタンス／書き出しと実時間の一致は未検証。
実音源の音量一致WAVと時間／周波数分析は、主観的な潰れ感の試聴評価を代替しない。
高速復帰と強い制限では音色や残留歪みのトレードオフがある。
Punch、Loud、Clipper、Integrated LUFS、GR履歴は未実装。GitHubへのpushはしていない。
