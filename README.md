# SCP-55 Dock

Roland SCP-55 PC Card音源をPC Cardスロットなしで使用するための専用アダプターです。
USB-MIDIまたはDIN MIDIを受信し、基板のLINE OUTから音声を出力します。
RP2354Bを使用。Rev.A基板は73 × 115 mm、4層、設計板厚1.6 mmです。

## 公開ファイル

- [回路図PDF](output/pdf/scp55-reva.pdf)
- [製造用ガーバーZIP（PTH/NPTHドリル入り）](output/manufacturing/scp55-reva-73x115-gerbers.zip)
- [演奏用ファームウェアUF2](output/firmware/usb-midi-bench/scp55-usb-midi-bench.uf2)
- [初回確認用・受動診断UF2](output/firmware/passive/scp55-passive.uf2)
- [ファームウェアのソース・ビルド手順](firmware/README.md)
- [ファイルのSHA-256](SHA256SUMS)

## 書き込み・使い方

1. USBを外す。初回の基板確認ではSCP-55も外す。
2. 基板のBOOTボタンを押しながらUSB接続し、表示されたBOOTSELドライブへUF2をコピーする。
3. 初回は受動診断版で基板電圧・カード検出を確認する。受動診断版では演奏できない。
4. USBを外してSCP-55を挿入し、演奏用UF2を書き込む。給電時にカードを自動起動する。
5. USBの出力先「SCP-55 USB MIDI」または基板のDIN MIDI INへMIDIを送信する。
   LINE OUTをアンプ／スピーカーへ接続し、音量は接続先で調整する。

USBとDINは常時待受で、入力切り替えは不要。音声は44.1 kHz / 16-bitステレオです。
BOOTを約2秒長押しするとカード電源OFF、離して短く押すとONになります。
これはカード電源の操作で、マイコン・USB・DACには引き続き給電されます。

## 確認済みの範囲

実機でUSB/DIN経由の演奏、ステレオ出力、BOOTによるカード電源操作を確認済み。
2台目でもUSB演奏・左右分離を確認しています。全体音量のUIはありません。
付属UF2はADC基準3320 mV・誤差補償なしの実機試験設定です。
別個体では電圧を実測してソースからビルドしてください。
SCP-55専用設計です。製造時は回収品J1コネクターのピン対応を確認してください。

回路図とガーバーは2026-09-30版、演奏用UF2は2026-10-19ビルドです。
KiCad編集データ、筐体、開発ログ、SCP-55のROMデータは含みません。
Roland公式製品・公式ファームウェアではありません。

## ライセンス

本プロジェクト独自部分の再利用ライセンスは未設定です。
第三者由来コードの著作権・ライセンス表記は各ソース内に保持しています。
Raspberry Pi由来のBOOTSEL処理等のBSD条件は[こちら](licenses/pico-sdk-BSD-3-Clause.txt)を参照してください。
