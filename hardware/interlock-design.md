# SCP-55専用版：電源・バス許可（2026-09-30）

現行回路は **16 IC、電源／検出／許可回路は6 IC**。旧26 IC版は `simplification/before/` に保存。
汎用PC Cardホストではなく、5 VのRoland SCP-55専用。

## 残した回路

|部品|役割|
|---|---|
|U7 TPS25200|電流制限付きカード電源スイッチ。FAULTでラッチ解除|
|U17 SN74LVC2G17|CD1/CD2入力バッファ。R15/R16は常時3.3 Vへの10 kΩ|
|U15 SN74LVC04A|CDの極性変換、バス /OE 生成。未使用入力はGND|
|U12 SN74LVC08A|CD、停止要求、FAULTをAND合成。最後のゲートでバス許可|
|U9 SN74LVC1G74|抜去時に消える電源ラッチ。接触復帰だけでは再始動しない|
|U21 SN74LVC1G00|バス許可とDATA_ENABLEのNANDでデータ /OE|

```
CD_BOTH_LOW = CD1_LOW & CD2_LOW
POWER_STAGE_1 = CD_BOTH_LOW & MASTER_CLEAR_N
POWER_CLR_N = POWER_STAGE_1 & FAULT_N
U9: D=3V3, CLK=POWER_ARM_CLK, /CLR=POWER_CLR_N, /PRE=3V3
POWER_EN = U9.Q
BUS_ENABLE = POWER_EN & MCU_BUS_REQUEST
BUS_OE_N = !BUS_ENABLE
DATA_OE_N = !(BUS_ENABLE & DATA_ENABLE)
```

CDのどちらかが開くとU9は非同期クリアされ、U7のENと全バッファの許可が落ちる。
CDが戻っても新しいCLK立上りがない限り電源を再投入しない。
起動時はGPIO25、39、40、44を外部抵抗でLに固定。電源投入／再起動時はソフトもLから開始。
MCU停止中の低電圧を独立監視する機能、汎用カードの電圧判別、独立したRESET許可ラッチは削除。

PC Cardの接点は電源/GND 5.00 mm、信号4.25 mm、CD3.50 mmの順。
抜去時はCDが先に離れる（[DVB公式資料 §11.3](https://dvb.org/wp-content/uploads/2020/02/R206-001.V1.pdf)）。
これは定常論理の確認であり、実装後の波形・伝搬時間の実測は未実施。

## RESETと電圧確認

U5を削除し、RESETはU4の第8チャネル（A8=pin10、B8=pin14）へ統合。
U4の他の制御信号は非選択状態を先に設定し、データ変換器は常に無効のまま電源立上りを待つ。

U8/U11の専用監視ICを外し、R4=10 kΩ、R5=10 kΩ（各0.1%）、C3=100 nFで
5V_CARDを1/2にしてGPIO45/ADC5へ入力。正常5 V時にADC入力は約2.5 V。
ADC換算は公称3.3 V基準であり、精密なしきい値保証ではない。
実装後にテスター表示とADC表示を照合する。

ファームウェアの開始手順：
1. 全許可をL、バス制御を非選択、データを入力にする。
2. CDが両方LならMASTER_CLEAR_Nを解除し、新しいPOWER_ARM_CLKパルスを出す。
3. POWER_ENをGPIO35で読み返す。
4. ADCが公称4.65〜5.35 Vを示し、20 msの連続した安定時間を確認する。
5. MCU_RESETをHに先に設定し、GPIO44でU4を含むバスを許可する。
6. RESETを20 ms保持、Lにして20 ms待ち、その後バス通信を許可する。
7. 各転送時はGPIO39をLにしてからDIRを切り替え、必要な期間だけHにする。

電圧低下はソフトで停止する。CD抜去とU7 FAULTはソフトの応答を待たず停止する。
ADCの監視はサンプリング間の変化を保証せず、旧版の独立電圧監視とは機能が異なる。

## 削除したIC

U5、U8、U10、U11、U13、U14、U16、U18、U19、U20。
VS1/VS2はソフトに接続せず、J1と既存の測定点のみ。専用5 Vカードという前提を回路とソフトで共有する。

[RP2350公式データシート](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)、
[SN74LVC1G74](https://www.ti.com/lit/ds/symlink/sn74lvc1g74.pdf)、
[SN74LVC8T245](https://www.ti.com/lit/ds/symlink/sn74lvc8t245.pdf)。


## 2026-09-30 受信回路の修正

U1/U6はNexperia 74LVC4245APW,118に変更。A側が5V_CARD、B側が3V3。
U1のGPIO38 DIRはLowでカード書込み、Highで読取り。U6 DIRは3V3固定。
U2/U3/U4のSN74LVC8T245は送信専用のため従来どおり。
ADCは校正と誤差分を含む判定へ変更。詳細は[修正記録](final-fixes/README.md)。
