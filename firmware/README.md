# ファームウェア

RP2354B用。演奏版はUSB/DIN MIDI同時待受、44.1 kHz / 16-bitステレオ、unity gain、BOOT電源操作に対応。
ソースは `rp2354b_diag/` とこのディレクトリのCファイルです。

## ビルド

使用実績: Pico SDK 2.3.0、Arm GNUツールチェーン、CMake、picotool 2.x。
Pico SDKは必要なサブモジュール（TinyUSB等）を取得して別途用意します。
リポジトリルートで実行してください。

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICOTOOL=/path/to/picotool
sh firmware/build-rp2354b.sh passive
# 実測したADC基準電圧(mV)を設定。3320は配布UF2の実機試験値。
SCP55_ADC_REFERENCE_MV=3320 sh firmware/build-rp2354b.sh usb-midi-bench
```

成果物は `output/firmware/<profile>/` に生成されます。
`usb-midi-bench` は誤差補償を省いた試験プロファイルです。
ADC基準電圧を未設定にするとカード給電は許可されません。

## 検証

```sh
sh firmware/run-host-tests.sh
```

ホスト上で電源状態、カード識別、バス、MIDIマージ、音声処理、ボタン、GPIO対応を検査します。
実機の動作確認とは別です。
