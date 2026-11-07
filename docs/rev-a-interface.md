# Rev.A インターフェース案

**状態:** 信号レベルの設計案。製造用回路図ではない。SCP-55 実カードと選定ソケットのピン配置照合、電源電流、部品データシートによるタイミングの確認が必要。

根拠は [調査メモ](research.md)、[PC Card Standard 8.0 Vol.2](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)、[RP2350ファミリーデータシート](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)、[SN74LVC8T245 datasheet](https://www.ti.com/lit/ds/symlink/sn74lvc8t245.pdf)。RP2354B の外部 GPIO は 48 本で、USB と SWD は専用端子を使う。以下の番号は**GPIO番号**であり、RP2354B の実パッケージピン番号ではない。

## 8 bit バス信号

表の方向はカードを基準にした「RP → card」「card → RP」。`H` は card 側の 5 V high、`L` は low、`Z` は高インピーダンス。`#` は active low。card 側の全デジタル信号を 5 V ドメインとして扱い、RP2350 GPIO と直結しない。

| 信号 | 方向 | card 側電圧 | 電源投入時・非アクセス時 | 必要性 | RP2354B GPIO |
|---|---|---|---|---|---|
| D0–D7 | 双方向 | 5 V | translator Z、RP 側 input | 必須 | 0–7 |
| A0–A15 | RP → card | 5 V | translator Z、card 側 pulldown で 0 | 必須。I/O `0x330`、COR `0x400` を表現 | 8–23 |
| CE1# | RP → card | 5 V | H | 必須。8 bit cycle で L | 24 |
| CE2# | 固定 | 5 V | H | Rev.A は 5V_CARD への個別 pull-up で常時非選択 | なし |
| REG# | RP → card | 5 V | H | 属性/IO cycle で L | 26 |
| OE# | RP → card | 5 V | H | 属性 read のみ L | 27 |
| WE# | RP → card | 5 V | H | 属性 COR write のみ L | 28 |
| IORD# | RP → card | 5 V | H | IO read のみ L | 29 |
| IOWR# | RP → card | 5 V | H | IO write のみ L | 30 |
| RESET | RP → card | 5 V | **Z**、VCC 安定後も最低 1 ms Z | 必須、active high | 31 |
| WAIT# | card → RP | 5 V | 入力、弱 pull H | 必須として接続。assert 中は cycle 延長 | 32 |
| CD1#, CD2# | card → RP | カード内部で GND に接続。5 V 耐性入力と電源連動 pull-up を採用 | 両方 L で挿入。無給電でも検出 | 必須。68 ピンの 36/67 番。給電許可条件 | 33, 34 |
| VS1#, VS2# | card → RP | open/ground として検知 | 3.3 V 弱 pull H、**駆動禁止** | 必須。5 V キー確認にも使用 | 35, 36 |
| READY/IREQ# | card → RP | 5 V | 入力、pull H | **設定前は READY、設定後は IREQ#**。CIS 読出前に READY を確認 | 37 |
| WP/IOIS16#, INPACK# | card → host | 5 V | 入力専用テスト点 | 8 bit 固定では不要。観測点を残す | なし |
| D8–D15 | 双方向 | 5 V | 開放、card 側に接続しない | 属性は 8 bit、IO も 8 bit に限定 | なし |
| A16–A25 | RP → card | 5 V | 各線を個別の抵抗で L に固定 | 使用範囲外でも浮かせない | なし |

CE1#=L, CE2#=H, A0 で下位 8 bit の偶数/奇数バイトを選ぶ。[PC Card Standard §4.4.3](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)。属性領域では偶数アドレスだけ有効。`CIS[n] → A=2n`、`COR → A=0x0400`。属性 read/write は OE#/WE#、IO read/write は IORD#/IOWR# を使い分ける。IO 中も REG# は L。[同仕様 §4.4.11, §4.4.18–19, §4.5.2](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)。

## MCU の残りのピン

| 用途 | GPIO 候補 | 備考 |
|---|---|---|
| データ translator DIR | 38 | DIR 切替時は先に /OE を無効にする |
| 通常バス許可 CLK・データ translator 許可要求 | 39 | 新しい立上りでバスラッチを許可。通常 L、データ転送中のみ H。DIR 切替前に L |
| カード電源許可 CLK | 40 | 新しい立上りで電源ラッチを許可。起動時 L |
| DIN MIDI IN optocoupler → UART RX | 41 | 31.25 kbit/s |
| I2S BCLK/LRCLK/DOUT | 42, 43, 46 | capture 成功時に使用。PIO でのピン割当を要検証 |
| 許可回路の監視入力 | 44–45 | POWER_EN、RESET_ENABLE の実信号 |
| バス許可の監視入力 | 47 | BUS_ENABLE の実信号 |

GPIO25 は共通解除 `MASTER_CLEAR_N` に使う。起動時は外部 pulldown で L。この割当は 48 GPIO を全て使用する。PIO の連続ピン制約と I2S 実装に使える GPIO を [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf) で再確認して回路図で確定する。CE2# は固定の非選択とし、debug UART と activity LED は Rev.A の割当から外した。

## レベル変換と無給電時の扱い

SN74LVC8T245 の A 側を 3.3 V、B 側を 5V_CARD に置く。現行案は 6 個で、D0–D7 に 1 個、A0–A15 に 2 個、6 本の制御線に 1 個、RESET 専用に 1 個、WAIT# と READY/IREQ# に 1 個を割り当てる。[全 48 チャンネルの割当](../hardware/translator-pinmap.csv)を参照。すべての /OE は reset 時の抵抗で disable。特に RESET は 5V_CARD 有効後 1 ms 以上 Hi-Z を保つため、他の制御と独立に enable できる構成とする。[TI データシート](https://www.ti.com/lit/ds/symlink/sn74lvc8t245.pdf) の Ioff 条件と DIR/OE 論理を型番・パッケージごとに照合する。

**注意:** A0–A15 の card 側 pulldown と CE/OE/WE 等の card 側 pullup は 5V_CARD への逆流がない位置に置く。3.3 V 側 pull だけでは translator disabled 中の card 入力の既定値を保証できない。制御 IC の /OE をソフトウェアだけで制御せず、5V_CARD power-good でもゲートする。RESET 用ゲートには電源投入後の Hi-Z 時間を保証する回路を加える。

VCC は socket の 17/51、GND は 1/34/35/68 に供給する（[PC Card Standard §4.4.16](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)）。VPP1/VPP2 は各々 0 Ω option と測定箇所を介して **5V_CARD と同電位**にする。CIS 前の VPP=VCC が規格要求で、個別に未接続にはしない。[カード用 5 V の試作方針](../hardware/card-5v-budget.md)に従い、使用する電源を限定して電流制限付きスイッチを用い、soft start、電流測定点、電源断時の放電経路を置く。USB-C の給電能力とカード消費電流は実測・部品選定まで未確定。

## バスサイクルと起動状態機械

1. card 未挿入: 5V_CARD off、translator disabled、MCU data pins input、control card 側は CE#/OE#/WE#/IORD#/IOWR# high。RESET は Z。VS1#/VS2# は駆動しない。
2. CD1#/CD2# が共に low で安定し、VS が 5 V 対応と分かったときだけ +5V_CARD を有効化。カード入力はまだ Z または受動 pull のみ。CD の VCC pull-up は各 11 kΩ、無給電時の弱い検出 pull-up は各 100 kΩ を論理回路図に置いた。[ソケット確認記録](connector.md)参照。
3. power-good 後 1 ms 以上 RESET を Z のまま保ち、必要な reset pulse を high で与えて low に解除。解除後、CIS に触れる前に最低 20 ms 待つ。READY の確認と CIS データ安定待ちを行う。既存コードは READY が先に立っても CIS がまだ `0xFF` だった事例を記録している。[PC Card Standard §4.4.20](https://www.scribd.com/document/88185604/PC-Card-Standard-8-0-Volume-2-Electrical-Specification)、[SCP55GO.ASM](https://github.com/zikolas/scp55-enabler/blob/5ba277119fffa28a7655a2157d7874616227a968/SCP55GO.ASM)。
4. 属性 read: data translator を card → MCU に設定、address 安定、REG# low、CE1# low、OE# low。WAIT# が戻ってから所定の read pulse 幅を満たし D0–D7 をサンプル。全 strobes high、CE1# high。逆方向への DIR 変更前に translator /OE を disabled。
5. MANFID と CISTPL_CONFIG を確認できないときは COR を書かない。属性 write: data translator を MCU → card、REG# low、CE1# low、WE# low の順。書込後は方向を戻して読戻す。
6. I/O cycle は同じ data/address の安全順序で、OE#/WE# を high のまま IORD#/IOWR# を使う。WAIT# を時間制限付きで扱い、timeout 時は全 strobes を inactive に戻す。
7. いずれかの CD が high になれば即座に translator disabled と load switch off を要求し、次回は CIS からやり直す。ホット抜去時の電源断はソフトウェアだけに依存しない回路を検討する。

## 製造前に決める事項

- 選定 PCMCIA ソケットの型番、ピン番号、カードキー、card detect の接点仕様。ソケットの図をカード側と基板側で取り違えない。
- SCP-55 の VS パターンと VPP の電流。電流制限値はカードの実消費電流を基に決める。
- 5V_CARD の power-good で translator の無給電駆動を防ぐ論理と、RESET の Hi-Z 時間を保証する回路。
- PC Card の read/write/setup/hold と WAIT# の timing。GPIO 実装は保守的に始め、logic analyzer で確認する。
- 3.3 V regulator、USB-C CC 抵抗と保護、DIN MIDI IN の optocoupler、DAC の実部品を選んで各データシートで回路を検証する。
- LINE OUT を SCP-55 の外部端子なしで得る方法は capture 実験の成否で決まる。Rev.A に DAC の部品配置を設けても、その経路の動作は成功条件に含めて実測する。
