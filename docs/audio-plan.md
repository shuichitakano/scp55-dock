# PCMCIA capture → I2S LINE OUT の実装計画

## 2026-10-19 実基板での進展

Line Output Loopbackの6144サンプル取得に成功。無音時RMS 0.082 LSBに対し、
MIDI A4/A5で約1.27/1.47 LSB、FFTに約440/880 Hzの成分を確認。
44100 Hzへの8倍補間とPCM5102A向けI2S DMA送信も完了した。基板LINE OUTにつないだスピーカーで、ユーザーが低音→高音の聴取を確認。
この録音試験に続き、`live-bench` で16-bit / 11025 Hz mono取得→44.1 kHz左右同音出力を連続化。
12秒試験に続き60秒の連続演奏で661425サンプル、SER・DMA停止・バッファ不足/溢れがすべて0。ユーザーが途切れないメロディーを聴取確認。演奏中の停止と給電・バス無効化も実機確認済み。
2本のDMAを256フレーム単位で交互に使い、バッファ量に応じてPIO分周を±2000 ppm以内で追従する。
続いてUSB-MIDI版を左右独立の11025 Hz / 16-bit stereo取得に拡張し、LINE OUTまでの左→右の分離をユーザーが聴取確認。その後、採用レートを22.05 kHz stereoへ上げ、31-tap半帯域FIRで44.1 kHzへ変換。その後44.1 kHz取り込み・補間なし出力へ更新し、42.070秒のUSB演奏でエラー0件。
ログは [初回LINE OUT](../output/firmware/audio-bench/line-out-first.txt)、[60秒連続演奏](../output/firmware/live-bench/live-60s.txt)、[停止確認](../output/firmware/live-bench/live-stop.txt)。

## 当初の資料調査

SCP-55 上の CS4231A PIO **再生**は既存プロジェクトで実機確認済み。Codec 自体は PIO capture と ADC の Line Output Loopback を持つが、SCP-55 の PCMCIA 経由 capture と Sound Canvas mix の取り込みは未検証。[実機解析](research.md)、[CS4231A datasheet](https://dosdays.co.uk/media/crystal/CS4231A_datasheet.pdf)。

実験コード `firmware/audio_capture.c` は 5.512 kHz / unsigned 8-bit / mono に限定し、最初に Line、次に Aux1、最後に Line Output Loopback を選ぶ。R2=`base+6` の CRDY を確認して R3=`base+7` を読む。波形が変化するだけでは成功としない。既知の入力周波数との一致、カード音声端子で観測した Sound Canvas 発音との時間的相関、無音時の底雑音と発音時のレベル差を確認する。

## 44.1 kHz へ拡張する際の数字

| フォーマット | PCMCIA 8-bit 読出し量 | 平均 byte 間隔 |
|---|---:|---:|
| 5.512 kHz / mono / 8-bit | 5,512 B/s | 約181 µs |
| 11.025 kHz / stereo / 16-bit | 44,100 B/s | 約22.68 µs |
| 22.05 kHz / stereo / 16-bit | 88,200 B/s | 約11.34 µs |
| 44.1 kHz / stereo / 16-bit | 176,400 B/s | 約5.67 µs |

44.1 kHzでは1ステレオフレームの期限が約22.68 µs。R2 statusを1回、R3 dataを4回読む。旧実装は各読み出しで1 µsを5回待つため、待ち時間だけで25 µsとなり期限を超えた。現在はstreaming専用SIO経路で各段階を250 ns以上へ短縮し、MIDIのstatus読出し・data書込みも同じ経路を使用する。規格のIORD/IOWR幅165 ns以上、アドレスsetup 70 ns以上、write data setup/hold 60/30 ns以上に対して余裕を設け、WAIT#を確認してから取得する。[PC Card Standard 8.0 §4.11.4–5](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)。現物の波形測定は未実施で、実機の連続転送・エラー統計による検証。

## リングバッファとクロック差

SCP-55 の ADC sampling clock と RP2350 が生成する I2S BCK/LRCK は別の発振器に由来する。両者を「44.1 kHz」に設定しても、発振子誤差でリングバッファの占有率は長時間では必ず片側へ寄る。この問題はメモリ容量だけでは解消しない。capture と I2S の sample count、ring fill level、underrun/overrun を計測し、I2S PIO divider の微調整または適応 resampling で長期の平均速度を合わせる設計とする。

短時間の最初の実験では card の PIO capture を RAM に蓄積して PC へ診断データとして出す、または UART/USB 経由で統計だけ出す。Sound Canvas output が capture に含まれると確認してから I2S 出力を接続する。[PCM5102A](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf) の BCK PLL を使う 3-wire I2S なら外部 MCLK は不要。

## 実験ごとの記録

1. 各 source の I0/I1 と I8/I9 readback、R2 CRDY/SER 回数。
2. 連続 sample 数、期待 sample 数、最長 byte 読出し時間、WAIT# 延長回数。
3. 無音・1 kHz 入力・MIDI note 発音時の PCM 平均/ピーク/RMS、FFT の主ピーク。
4. I2S を起動した場合は 1 分ごとの ring fill、underrun、overrun。占有率が単調増減するかを確認。

測定値から初めて「PCMCIA だけで音声を取得できる」「ライン出力へ安定して出せる」の判定を行う。
