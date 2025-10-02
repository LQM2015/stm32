#!/usr/bin/env python3
"""
重排ELF文件，将Program Headers移到文件末尾
并合并LOAD段，确保只有2个Program Headers
这样.dev_info段可以紧跟ELF Header，从offset 0x34开始
"""

import struct
import sys

def merge_load_segments(ph_data, e_phentsize, e_phnum):
    """合并相邻的LOAD段 (类型1) 成单个段"""
    
    PT_LOAD = 1
    segments = []
    
    # 解析所有Program Headers
    for i in range(e_phnum):
        offset = i * e_phentsize
        ph = ph_data[offset:offset + e_phentsize]
        
        p_type = struct.unpack('<I', ph[0:4])[0]
        p_offset = struct.unpack('<I', ph[4:8])[0]
        p_vaddr = struct.unpack('<I', ph[8:12])[0]
        p_paddr = struct.unpack('<I', ph[12:16])[0]
        p_filesz = struct.unpack('<I', ph[16:20])[0]
        p_memsz = struct.unpack('<I', ph[20:24])[0]
        p_flags = struct.unpack('<I', ph[24:28])[0]
        p_align = struct.unpack('<I', ph[28:32])[0]
        
        segments.append({
            'type': p_type,
            'offset': p_offset,
            'vaddr': p_vaddr,
            'paddr': p_paddr,
            'filesz': p_filesz,
            'memsz': p_memsz,
            'flags': p_flags,
            'align': p_align
        })
    
    # 分离StorageInfo段 (vaddr=0) 和代码段 (vaddr≠0)
    storage_seg = None
    code_segs = []
    
    for seg in segments:
        if seg['type'] == PT_LOAD:
            if seg['vaddr'] == 0:
                storage_seg = seg
            else:
                code_segs.append(seg)
    
    if not storage_seg:
        print("Warning: No StorageInfo segment found!")
        return ph_data, e_phnum
    
    if len(code_segs) == 0:
        print("Warning: No code segments found!")
        return ph_data, e_phnum
    
    # 合并所有代码段成一个
    if len(code_segs) > 1:
        print(f"\nMerging {len(code_segs)} code segments into one...")
        
        # 找到最小offset和最大end
        min_offset = min(seg['offset'] for seg in code_segs)
        min_vaddr = min(seg['vaddr'] for seg in code_segs)
        
        max_end_file = max(seg['offset'] + seg['filesz'] for seg in code_segs)
        max_end_mem = max(seg['vaddr'] + seg['memsz'] for seg in code_segs)
        
        merged_filesz = max_end_file - min_offset
        merged_memsz = max_end_mem - min_vaddr
        
        # 合并flags (OR所有flags)
        merged_flags = 0
        for seg in code_segs:
            merged_flags |= seg['flags']
        
        merged_code_seg = {
            'type': PT_LOAD,
            'offset': min_offset,
            'vaddr': min_vaddr,
            'paddr': min_vaddr,
            'filesz': merged_filesz,
            'memsz': merged_memsz,
            'flags': merged_flags,
            'align': 4
        }
        
        print(f"  Merged segment: offset=0x{min_offset:X}, vaddr=0x{min_vaddr:X}, filesz=0x{merged_filesz:X}, memsz=0x{merged_memsz:X}")
    else:
        merged_code_seg = code_segs[0]
    
    # 构建新的Program Headers (只有2个)
    new_segments = [storage_seg, merged_code_seg]
    new_phnum = len(new_segments)
    
    # 创建新的ph_data
    new_ph_data = bytearray()
    for seg in new_segments:
        ph_entry = struct.pack('<IIIIIIII',
            seg['type'],
            seg['offset'],
            seg['vaddr'],
            seg['paddr'],
            seg['filesz'],
            seg['memsz'],
            seg['flags'],
            seg['align']
        )
        new_ph_data.extend(ph_entry)
    
    print(f"  Reduced Program Headers: {e_phnum} -> {new_phnum}")
    
    return bytes(new_ph_data), new_phnum

def reorder_elf(input_file, output_file):
    """重排ELF文件的Program Headers并合并LOAD段"""
    
    with open(input_file, 'rb') as f:
        data = bytearray(f.read())
    
    # 验证ELF magic
    if data[0:4] != b'\x7fELF':
        print("Error: Not an ELF file!")
        return False
    
    # 读取ELF header信息 (假设32-bit little-endian)
    ei_class = data[4]  # 1 = 32-bit
    ei_data = data[5]   # 1 = little endian
    
    if ei_class != 1:
        print("Error: Only 32-bit ELF supported")
        return False
    
    if ei_data != 1:
        print("Error: Only little-endian supported")
        return False
    
    # ELF32 header offsets
    e_phoff_offset = 28  # Program header table file offset
    e_shoff_offset = 32  # Section header table file offset
    e_phentsize_offset = 42  # Program header entry size
    e_phnum_offset = 44  # Number of program headers
    e_shentsize_offset = 46  # Section header entry size
    e_shnum_offset = 48  # Number of section headers
    
    # 读取原始值
    e_phoff = struct.unpack('<I', data[e_phoff_offset:e_phoff_offset+4])[0]
    e_shoff = struct.unpack('<I', data[e_shoff_offset:e_shoff_offset+4])[0]
    e_phentsize = struct.unpack('<H', data[e_phentsize_offset:e_phentsize_offset+2])[0]
    e_phnum = struct.unpack('<H', data[e_phnum_offset:e_phnum_offset+2])[0]
    e_shentsize = struct.unpack('<H', data[e_shentsize_offset:e_shentsize_offset+2])[0]
    e_shnum = struct.unpack('<H', data[e_shnum_offset:e_shnum_offset+2])[0]
    
    print(f"Original ELF Info:")
    print(f"  Program Headers offset: 0x{e_phoff:X} ({e_phoff})")
    print(f"  Program Header size: {e_phentsize} bytes")
    print(f"  Number of Program Headers: {e_phnum}")
    print(f"  Section Headers offset: 0x{e_shoff:X} ({e_shoff})")
    
    ph_size = e_phentsize * e_phnum
    print(f"  Total PH size: {ph_size} bytes")
    
    # 提取Program Headers
    ph_data = data[e_phoff:e_phoff + ph_size]
    
    # 合并LOAD段
    merged_ph_data, new_phnum = merge_load_segments(ph_data, e_phentsize, e_phnum)
    new_ph_size = e_phentsize * new_phnum
    
    # 删除原位置的Program Headers
    data_without_ph = data[:e_phoff] + data[e_phoff + ph_size:]
    
    # 将新的Program Headers追加到文件末尾
    new_phoff = len(data_without_ph)
    data_without_ph.extend(merged_ph_data)
    
    # 更新ELF header中的e_phoff和e_phnum
    struct.pack_into('<I', data_without_ph, e_phoff_offset, new_phoff)
    struct.pack_into('<H', data_without_ph, e_phnum_offset, new_phnum)
    
    # 更新Section Headers offset (因为删除了PH，SH的offset也需要调整)
    if e_shoff > e_phoff:
        new_shoff = e_shoff - ph_size
        struct.pack_into('<I', data_without_ph, e_shoff_offset, new_shoff)
        print(f"  Updated Section Headers offset: 0x{e_shoff:X} -> 0x{new_shoff:X}")
    
    print(f"\nNew ELF Info:")
    print(f"  Program Headers offset: 0x{new_phoff:X} ({new_phoff})")
    print(f"  Program Headers count: {new_phnum}")
    print(f"  .dev_info now starts at: 0x{e_phoff:X} ({e_phoff})")
    
    # 现在需要更新Program Headers中的p_offset值
    # 因为我们删除了原PH后，后面数据的offset都减少了ph_size
    for i in range(new_phnum):
        ph_offset = new_phoff + (i * e_phentsize)
        
        # p_offset在Program Header的offset 4-7
        p_offset_in_ph = 4
        p_offset = struct.unpack('<I', data_without_ph[ph_offset + p_offset_in_ph:ph_offset + p_offset_in_ph + 4])[0]
        
        # 如果这个LOAD段的offset大于原PH位置，需要调整
        if p_offset > e_phoff:
            new_p_offset = p_offset - ph_size
            struct.pack_into('<I', data_without_ph, ph_offset + p_offset_in_ph, new_p_offset)
            print(f"  Updated LOAD segment {i}: offset 0x{p_offset:X} -> 0x{new_p_offset:X}")
        else:
            print(f"  LOAD segment {i}: offset 0x{p_offset:X} (unchanged)")
    
    # 更新Section Headers中的sh_offset值
    if e_shoff > e_phoff:
        sh_offset = new_shoff
        for i in range(e_shnum):
            sh_entry_offset = sh_offset + (i * e_shentsize)
            
            # sh_offset在Section Header的offset 16-19
            sh_offset_in_sh = 16
            sh_data_offset = struct.unpack('<I', data_without_ph[sh_entry_offset + sh_offset_in_sh:sh_entry_offset + sh_offset_in_sh + 4])[0]
            
            # 如果section的offset大于原PH位置，需要调整
            if sh_data_offset > e_phoff and sh_data_offset < 0xFFFFFFFF:  # 排除无效值
                new_sh_data_offset = sh_data_offset - ph_size
                struct.pack_into('<I', data_without_ph, sh_entry_offset + sh_offset_in_sh, new_sh_data_offset)
    
    # 写入输出文件
    with open(output_file, 'wb') as f:
        f.write(data_without_ph)
    
    print(f"\nSuccess! Written to {output_file}")
    print(f"  File size: {len(data)} -> {len(data_without_ph)} bytes")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python reorder_elf.py <input.elf> <output.stldr>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    if reorder_elf(input_file, output_file):
        print("\nELF reordering complete!")
    else:
        print("\nELF reordering failed!")
        sys.exit(1)
