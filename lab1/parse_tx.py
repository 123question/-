import struct
import sys


def decode_varint(data, offset):
    first_byte = data[offset]
    if first_byte < 0xFD:
        return first_byte, offset + 1
    elif first_byte == 0xFD:
        return struct.unpack('<H', data[offset + 1:offset + 3])[0], offset + 3
    elif first_byte == 0xFE:
        return struct.unpack('<I', data[offset + 1:offset + 5])[0], offset + 5
    else:
        return struct.unpack('<Q', data[offset + 1:offset + 9])[0], offset + 9


def byte_to_bits(byte_val):
    """将字节转换为8位二进制字符串"""
    return ''.join(f'{byte_val:08b}')


def hex_to_bits(hex_str):
    """将十六进制字符串转换为二进制位字符串"""
    return ''.join(f'{int(b, 16):04b}' for b in hex_str)


def format_bits_with_spaces(bits, group=4):
    """格式化位字符串，每4位一组用空格分隔"""
    return ' '.join(bits[i:i + group] for i in range(0, len(bits), group))


def parse_transaction_bit(hex_data):
    data = bytes.fromhex(hex_data)
    offset = 0
    total = len(data)

    print(f"Total bytes: {total}")
    print(f"Total bits: {total * 8}")
    print("")

    # ============================================================
    # 1. Version (4 bytes) - 逐位解析
    # ============================================================
    print("=" * 70)
    print("1. VERSION (4 bytes)")
    print("=" * 70)
    raw = data[offset:offset + 4]
    version = struct.unpack('<I', raw)[0]
    print(f"  Byte offset: {offset:03d}")
    print(f"  Hex:         {raw.hex()}")
    print(f"  Binary:      {format_bits_with_spaces(byte_to_bits(version), 4)}")
    print(f"  Value:       {version}")
    print(f"  位解析:")
    print(f"    Bit 0-30:  交易版本号 ({version & 0x7FFFFFFF})")
    print(f"    Bit 31:    BIP68 标志位 (是否为0? {'是' if (version >> 31) == 0 else '否'})")
    offset += 4

    # ============================================================
    # 2. SegWit Marker & Flag (2 bytes) - 逐位解析
    # ============================================================
    print("\n" + "=" * 70)
    print("2. SEGWIT MARKER & FLAG (2 bytes)")
    print("=" * 70)
    marker = data[offset]
    flag = data[offset + 1]
    print(f"  Byte offset: {offset:03d}")
    print(f"  Hex:         {data[offset:offset + 2].hex()}")
    print(f"  Marker:      0x{marker:02x} ({byte_to_bits(marker)})")
    print(f"    Bit 0-7:   固定值 0x00，标识 SegWit 交易")
    print(f"  Flag:        0x{flag:02x} ({byte_to_bits(flag)})")
    print(f"    Bit 0-7:   固定值 0x01，SegWit 版本标志")
    offset += 2

    # ============================================================
    # 3. Input count (VarInt) - 逐位解析
    # ============================================================
    print("\n" + "=" * 70)
    print("3. INPUT COUNT (varint)")
    print("=" * 70)
    start = offset
    input_count, offset = decode_varint(data, offset)
    raw_len = offset - start
    raw = data[start:offset]
    print(f"  Byte offset: {start:03d}")
    print(f"  Hex:         {raw.hex()}")
    print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
    print(f"  Value:       {input_count}")
    print(f"  位解析:")
    if raw_len == 1:
        print(f"    Bit 0-7:   输入数量 = {input_count} (单字节)")
    elif raw_len == 3:
        print(f"    Bit 0-7:   0xFD (表示 VarInt 使用 2 字节)")
        print(f"    Bit 8-23:  输入数量 = {input_count}")
    print(f"     输入数量为 {input_count}，代表有 {input_count} 个输入")

    # ============================================================
    # 4. Parse each input - 逐位解析
    # ============================================================
    for i in range(input_count):
        print(f"\n{'=' * 70}")
        print(f"4.{i + 1} INPUT #{i} (逐位解析)")
        print("=" * 70)

        # Previous TX Hash (32 bytes) - 256 bits
        print(f"\n  [PREVIOUS TX HASH]")
        print(f"  Byte offset: {offset:03d}")
        tx_hash_raw = data[offset:offset + 32]
        tx_hash_be = tx_hash_raw[::-1].hex()
        print(f"  Hex (LE):   {tx_hash_raw.hex()}")
        print(f"  Hex (BE):   {tx_hash_be}")
        print(f"  Bits:       {format_bits_with_spaces(''.join(byte_to_bits(b) for b in tx_hash_raw), 4)}")
        print(f"  位解析:")
        print(f"    Bit 0-255: 前一笔交易的哈希值,该哈希指向之前被花费的 UTXO")
        offset += 32

        # Output Index (4 bytes) - 32 bits
        print(f"\n  [PREVIOUS OUTPUT INDEX]")
        print(f"  Byte offset: {offset:03d}")
        raw = data[offset:offset + 4]
        vout = struct.unpack('<I', raw)[0]
        print(f"  Hex:        {raw.hex()}")
        print(f"  Binary:     {format_bits_with_spaces(byte_to_bits(vout), 4)}")
        print(f"  Value:      {vout}")
        print(f"  位解析:")
        print(f"    Bit 0-31:  输出索引 = {vout}")
        print(f"     引用前一笔交易的第 {vout} 个输出")
        offset += 4

        # ScriptSig Length (VarInt)
        print(f"\n  [SCRIPTSIG LENGTH]")
        start = offset
        script_len, offset = decode_varint(data, offset)
        raw = data[start:offset]
        print(f"  Byte offset: {start:03d}")
        print(f"  Hex:         {raw.hex()}")
        print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
        print(f"  Value:       {script_len} bytes")
        print(f"  位解析:")
        print(f"    Bit 0-{script_len.bit_length() - 1}:  脚本长度 = {script_len} 字节")
        if script_len == 0:
            print(f"     SegWit: 签名在 witness 区域")

        # ScriptSig
        if script_len > 0:
            print(f"\n  [SCRIPTSIG (解锁脚本)]")
            script = data[offset:offset + script_len]
            print(f"  Byte offset: {offset:03d}")
            print(f"  Hex:         {script.hex()}")
            print(f"  Bits:        {format_bits_with_spaces(''.join(byte_to_bits(b) for b in script), 4)}")
            print(f"  位解析:")
            print(f"     解锁脚本，证明有权花费")
            offset += script_len
        else:
            print(f"\n  [SCRIPTSIG] (空)")

        # Sequence (4 bytes) - 32 bits，逐位解析
        print(f"\n  [SEQUENCE]")
        print(f"  Byte offset: {offset:03d}")
        raw = data[offset:offset + 4]
        sequence = struct.unpack('<I', raw)[0]
        print(f"  Hex:        {raw.hex()}")
        print(f"  Binary:     {format_bits_with_spaces(byte_to_bits(sequence), 4)}")
        print(f"  Value:      0x{sequence:08x} ({sequence})")
        print(f"  位解析（BIP68 相对锁定时间）:")
        # 检查最高位
        disable_bit = (sequence >> 31) & 1
        print(f"    Bit 31:    相对锁定时间禁用标志 = {disable_bit} ({'禁用' if disable_bit else '启用'})")
        if disable_bit == 0:
            type_bit = (sequence >> 22) & 1
            print(f"    Bit 22:    时间单位类型 = {type_bit} ({'秒' if type_bit else '区块'})")
            value = sequence & 0xFFFF
            print(f"    Bit 0-15:  锁定值 = {value}")
            if type_bit == 0:
                print(f"    需要等待 {value} 个区块才能花费")
            else:
                print(f"    需要等待 {value} 秒才能花费")
        else:
            print(f"     相对锁定时间被禁用")
        offset += 4

    # ============================================================
    # 5. Output count (VarInt) - 逐位解析
    # ============================================================
    print(f"\n{'=' * 70}")
    print("5. OUTPUT COUNT (varint)")
    print("=" * 70)
    start = offset
    output_count, offset = decode_varint(data, offset)
    raw = data[start:offset]
    print(f"  Byte offset: {start:03d}")
    print(f"  Hex:         {raw.hex()}")
    print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
    print(f"  Value:       {output_count}")
    print(f"  位解析:       有 {output_count} 个输出")

    # ============================================================
    # 6. Parse each output - 逐位解析
    # ============================================================
    for i in range(output_count):
        print(f"\n{'=' * 70}")
        print(f"6.{i + 1} OUTPUT #{i} (逐位解析)")
        print("=" * 70)

        # Amount (8 bytes) - 64 bits
        print(f"\n  [AMOUNT]")
        print(f"  Byte offset: {offset:03d}")
        raw = data[offset:offset + 8]
        amount_sat = struct.unpack('<Q', raw)[0]
        amount_btc = amount_sat / 100000000
        print(f"  Hex:        {raw.hex()}")
        print(f"  Binary:     {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
        print(f"  Value:      {amount_sat} satoshis = {amount_btc:.8f} BTC")
        print(f"  位解析:")
        print(f"    Bit 0-63:  金额 (以聪为单位)")
        offset += 8

        # ScriptPubKey Length (VarInt)
        print(f"\n  [SCRIPTPUBKEY LENGTH]")
        start = offset
        script_len, offset = decode_varint(data, offset)
        raw = data[start:offset]
        print(f"  Byte offset: {start:03d}")
        print(f"  Hex:         {raw.hex()}")
        print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
        print(f"  Value:       {script_len} bytes")
        print(f"  位解析:      锁定脚本长度为 {script_len} 字节")

        # ScriptPubKey (锁定脚本) - 逐位解析操作码
        if script_len > 0:
            print(f"\n  [SCRIPTPUBKEY (锁定脚本)]")
            script = data[offset:offset + script_len]
            print(f"  Byte offset: {offset:03d}")
            print(f"  Hex:         {script.hex()}")
            print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in script), 4)}")
            print(f"  位解析:")
            print(f"    锁定脚本，包含花费条件")
            # 逐字节解析操作码
            print(f"\n  脚本操作码逐字节解析:")
            pos = 0
            while pos < script_len:
                op = script[pos]
                op_hex = f"{op:02x}"
                op_bits = byte_to_bits(op)
                print(f"    Byte {pos:02d}: Hex={op_hex} Bits={op_bits} -> ", end="")
                if op == 0x00:
                    print("OP_0 (推送空数据)")
                elif op == 0x14:
                    print("OP_PUSHBYTES_20 (推送20字节数据)")
                    data_start = pos + 1
                    data_end = pos + 1 + 20
                    if data_end <= script_len:
                        print(f"       数据: {script[data_start:data_end].hex()}")
                        print(f"       公钥哈希 (20字节)")
                    pos = data_end - 1
                elif op == 0x76:
                    print("OP_DUP (复制栈顶元素)")
                elif op == 0xA9:
                    print("OP_HASH160 (计算哈希160)")
                elif op == 0x88:
                    print("OP_EQUALVERIFY (检查相等并移除)")
                elif op == 0xAC:
                    print("OP_CHECKSIG (检查签名)")
                elif op >= 0x01 and op <= 0x4B:
                    print(f"OP_PUSHBYTES_{op} (推送 {op} 字节数据)")
                    data_start = pos + 1
                    data_end = pos + 1 + op
                    if data_end <= script_len:
                        print(f"       数据: {script[data_start:data_end].hex()}")
                    pos = data_end - 1
                elif op == 0x51:
                    print("OP_1 (数字1)")
                else:
                    print(f"操作码 0x{op_hex} (未知)")
                pos += 1
            offset += script_len

    # ============================================================
    # 7. Witness Data - 逐位解析
    # ============================================================
    print(f"\n{'=' * 70}")
    print("7. WITNESS DATA (见证数据)")
    print("=" * 70)

    for i in range(input_count):
        item_count = data[offset]
        print(f"\n  [INPUT #{i} WITNESS ITEM COUNT]")
        print(f"  Byte offset: {offset:03d}")
        print(f"  Hex:         {data[offset:offset + 1].hex()}")
        print(f"  Binary:      {byte_to_bits(item_count)}")
        print(f"  Value:       {item_count}")
        print(f"  位解析:      有 {item_count} 个见证项")
        offset += 1

        for j in range(item_count):
            start = offset
            item_len, offset = decode_varint(data, offset)
            raw = data[start:offset]
            print(f"\n  [WITNESS ITEM #{j}]")
            print(f"  Byte offset: {start:03d}")
            print(f"  Hex:         {raw.hex()}")
            print(f"  Binary:      {format_bits_with_spaces(''.join(byte_to_bits(b) for b in raw), 4)}")
            print(f"  Length:      {item_len} bytes")
            if item_len > 0:
                item = data[offset:offset + item_len]
                print(f"  Data Hex:    {item.hex()[:80]}{'...' if item_len > 40 else ''}")
                print(
                    f"  Data Bits:   {format_bits_with_spaces(''.join(byte_to_bits(b) for b in item[:8]), 4)}{'...' if item_len > 8 else ''}")
                if j == 0 and item_len > 60:
                    print(f"  位解析:       DER 编码的签名 (证明所有权)")
                    # DER 签名解析
                    if item[0] == 0x30:
                        der_len = item[1]
                        print(f"    DER 结构: 序列长度 = {der_len} 字节")
                elif j == 1 and item_len == 33:
                    print(f"  位解析:      压缩公钥 (33字节)")
                    prefix = item[0]
                    print(
                        f"    前缀字节:  0x{prefix:02x} ({'偶数' if prefix == 0x02 else '奇数' if prefix == 0x03 else '未压缩'})")
                offset += item_len

    # ============================================================
    # 8. Locktime - 逐位解析
    # ============================================================
    print(f"\n{'=' * 70}")
    print("8. LOCKTIME (锁定时间)")
    print("=" * 70)
    print(f"  Byte offset: {offset:03d}")
    raw = data[offset:offset + 4]
    locktime = struct.unpack('<I', raw)[0]
    print(f"  Hex:        {raw.hex()}")
    print(f"  Binary:     {format_bits_with_spaces(byte_to_bits(locktime), 4)}")
    print(f"  Value:      {locktime}")
    print(f"  位解析:")
    if locktime == 0:
        print(f"    Bit 0-31:  0 = 立即生效")
    elif locktime < 500000000:
        print(f"    Bit 0-31:  {locktime} = 区块高度锁定")
        print(f"     交易在此区块高度之前不能确认")
    else:
        print(f"    Bit 0-31:  {locktime} = Unix 时间戳")
        import datetime
        dt = datetime.datetime.fromtimestamp(locktime)
        print(f"     交易在 {dt.strftime('%Y-%m-%d %H:%M:%S')} 之前不能确认")
    offset += 4

    # ============================================================
    # Summary
    # ============================================================
    print(f"\n{'=' * 70}")
    print("SUMMARY")
    print("=" * 70)
    print(f"Total bytes parsed: {offset}")
    print(f"Total bytes in raw:  {total}")
    print(f"Total bits:          {total * 8}")
    if offset == total:
        print("Status:  ALL BYTES ACCOUNTED FOR")
    else:
        print(f"Status:   {total - offset} bytes unparsed")


# Main
if __name__ == "__main__":
    with open('tx.hex', 'r') as f:
        hex_data = f.read().strip()
    parse_transaction_bit(hex_data)

