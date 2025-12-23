/* inchrw.s : 中難度Option (link/unlk版) */

    .section .text
    .global inbyte
    .even
    .equ SYSCALL_NUM_GETSTRING, 1

inbyte:
    /* 1. スタックフレームの作成 (バッファ確保) */
    /* 旧FP保存 -> FP更新 -> スタック4バイト確保 を1命令で行う */
    link    %a6, #-4

    /* 2. レジスタの退避 */
    /* linkの後に行うのが一般的です */
    movem.l %d2-%d3, -(%sp)
    
    move.l 8(%a6), %d0
    cmp.l #0,%d0
    beq port_0
    move.l #1,%d1
    bra retry_in
port_0:
    move.l #0,%d1

retry_in:
    move.l  #SYSCALL_NUM_GETSTRING, %d0

    /* 3. バッファアドレスの指定 */
    /* フレームポインタ(%a6)基準で -4 の位置が確保したバッファ */
    /* lea命令でその「アドレス」を計算してd2に入れる */
    lea     -4(%a6), %a0    /* アドレス -4(%a6) を a0 にロード */
    move.l  %a0, %d2        /* システムコール引数に設定 */

    move.l  #1, %d3
    trap    #0

    tst.l   %d0
    beq     retry_in

    /* 4. 戻り値の取得 */
    move.l  #0, %d0
    move.b  -4(%a6), %d0    /* フレームポインタ基準でデータ読み出し */

    /* 5. レジスタの復帰 */
    movem.l (%sp)+, %d2-%d3

    /* 6. スタックフレームの破棄 */
    /* SPを戻す -> 旧FP復帰 を1命令で行う */
    unlk    %a6
    
    rts
