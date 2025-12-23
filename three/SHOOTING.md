# Shooting Duel (RS232C) 操作ガイド

このゲームは 68k ボードの RS232C 2ポートを使い、PC 2台から同時に操作します。

## ビルド

```
make shooting
```

生成物: `shooting.abs`

## 接続

- 68k ボードの RS232C ポート2つに、USBシリアルでPCを2台接続
- ポート0 = Player 1、ポート1 = Player 2

## 操作方法

- Player 1 (ポート0)
  - 移動: `W` (上) / `S` (下)
  - 発射: `Space`
- Player 2 (ポート1)
  - 移動: `I` (上) / `K` (下)
  - 発射: `P`

## 注意点 (入力が1文字ずつ届かない場合)

端末が「行バッファ」設定だと、Enterを押すまで入力が送信されません。  
このゲームは1文字ごとに即時入力が必要なので、USBシリアル側を「1文字即時送信 (canonical off)」に設定してください。

### 例: Linux

```
stty -F /dev/ttyUSB0 raw -echo
```

### 例: macOS

```
stty -f /dev/tty.usbserial-XXXX raw -echo
```

使用している端末ソフト（minicom, screen など）にも同様の設定があります。
