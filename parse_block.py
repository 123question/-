import requests
import struct
import hashlib
from datetime import datetime

def decode_varint(data, offset):
    if offset >= len(data):
        return 0, offset
    first_byte = data[offset]
    if first_byte < 0xFD:
        return first_byte, offset + 1
    elif first_byte == 0xFD:
        if offset + 3 > len(data):
            return 0, offset
        return struct.unpack('<H', data[offset+1:offset+3])[0], offset + 3
    elif first_byte == 0xFE:
        if offset + 5 > len(data):
            return 0, offset
        return struct.unpack('<I', data[offset+1:offset+5])[0], offset + 5
    else:
        if offset + 9 > len(data):
            return 0, offset
        return struct.unpack('<Q', data[offset+1:offset+9])[0], offset + 9

print("获取测试网最新区块...")

resp = requests.get("https://mempool.space/testnet/api/blocks/tip/hash")
block_hash = resp.text.strip()
print(f"区块哈希: {block_hash}")

resp = requests.get(f"https://mempool.space/testnet/api/block/{block_hash}/raw")
data = resp.content
total = len(data)
offset = 0

print(f"总字节: {total}")

print("=" * 70)
print("区块头 (80 字节)")

raw = data[offset:offset+4]
version = struct.unpack('<I', raw)[0]
print(f"[{offset:04d}] Version: {version} (hex: {raw.hex()})")
offset += 4

raw = data[offset:offset+32]
prev_hash = raw[::-1].hex()
print(f"[{offset:04d}] Previous Block Hash: {prev_hash}")
offset += 32

raw = data[offset:offset+32]
merkle_root = raw[::-1].hex()
print(f"[{offset:04d}] Merkle Root: {merkle_root}")
offset += 32

raw = data[offset:offset+4]
timestamp = struct.unpack('<I', raw)[0]
dt = datetime.fromtimestamp(timestamp)
print(f"[{offset:04d}] Timestamp: {timestamp} ({dt.strftime('%Y-%m-%d %H:%M:%S')})")
offset += 4

raw = data[offset:offset+4]
bits = struct.unpack('<I', raw)[0]
exponent = bits >> 24
coefficient = bits & 0xFFFFFF
print(f"[{offset:04d}] Bits: 0x{bits:08x} (exp={exponent}, coeff=0x{coefficient:06x})")
offset += 4

raw = data[offset:offset+4]
nonce = struct.unpack('<I', raw)[0]
print(f"[{offset:04d}] Nonce: {nonce}")
offset += 4

print("=" * 70)
print("工作量证明验证")

header = data[:80]
hash1 = hashlib.sha256(header).digest()
hash2 = hashlib.sha256(hash1).digest()
calc_hash = hash2[::-1].hex()
print(f"计算的区块哈希: {calc_hash}")
print(f"实际区块哈希:   {block_hash}")
print(f"匹配: {'是' if calc_hash == block_hash else '否'}")

tx_count, offset = decode_varint(data, offset)
print(f"\n交易数量: {tx_count}")
print(f"只解析前 5 笔，后续忽略")
print("")

print("=" * 70)
print("前 5 笔交易")

for tx_idx in range(min(tx_count, 5)):
    tx_start = offset
    
    if offset + 4 > len(data):
        print(f"\nTX #{tx_idx+1}: 数据不完整，停止")
        break
    
    tx_version = struct.unpack('<I', data[offset:offset+4])[0]
    offset += 4
    
    is_segwit = False
    if offset + 2 <= len(data) and data[offset] == 0x00 and data[offset+1] == 0x01:
        is_segwit = True
        offset += 2
    
    input_count, offset = decode_varint(data, offset)
    
    for i in range(input_count):
        if offset + 36 > len(data):
            break
        offset += 32  # prev hash
        offset += 4   # prev index
        script_len, offset = decode_varint(data, offset)
        if offset + script_len > len(data):
            break
        offset += script_len  # scriptSig
        if offset + 4 > len(data):
            break
        offset += 4   # sequence
    
    output_count, offset = decode_varint(data, offset)
    
    for i in range(output_count):
        if offset + 8 > len(data):
            break
        offset += 8   # value
        script_len, offset = decode_varint(data, offset)
        if offset + script_len > len(data):
            break
        offset += script_len  # scriptPubKey
    
    # WITNESS - 在 Locktime 之前
    if is_segwit:
        for i in range(input_count):
            if offset >= len(data):
                break
            witness_count, offset = decode_varint(data, offset)
            for j in range(witness_count):
                item_len, offset = decode_varint(data, offset)
                if offset + item_len > len(data):
                    break
                offset += item_len
    
    # LOCKTIME
    if offset + 4 > len(data):
        break
    locktime = struct.unpack('<I', data[offset:offset+4])[0]
    offset += 4
    
    tx_size = offset - tx_start
    tx_type = "SegWit" if is_segwit else "Legacy"
    
    print(f"\nTX #{tx_idx+1}: offset={tx_start:04d}, size={tx_size:4d} bytes, {tx_type}")
    print(f"  Version: {tx_version}, Inputs: {input_count}, Outputs: {output_count}, Locktime: {locktime}")

print(f"\n{'='*70}")
print(f"前 5 笔交易解析完成，当前 offset: {offset}")
print(f"总字节: {total}，剩余: {total - offset} 字节（已忽略）")