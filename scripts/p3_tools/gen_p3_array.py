# p3_to_carray.py
import binascii
import os
import sys

def convert_with_annotations(input_file, output_file, array_name):
    # 检查输入文件是否存在
    if not os.path.exists(input_file):
        print(f"错误: 输入文件 '{input_file}' 不存在")
        return False
    
    try:
        with open(input_file, 'rb') as f:
            data = f.read()
        
        # 检查文件是否为空
        if not data:
            print(f"错误: 输入文件 '{input_file}' 为空")
            return False
        
        hex_str = binascii.hexlify(data).decode('utf-8')
        hex_pairs = [hex_str[i:i+2] for i in range(0, len(hex_str), 2)]
        
        frame_counter = 0
        byte_counter = 0
        output = []
        
        output.append(f"const uint8_t {array_name}[] = {{")
        
        while byte_counter < len(hex_pairs):
            # 检查是否有足够的字节表示帧头
            if byte_counter % 4 == 0 and byte_counter + 4 <= len(data):  # 每帧头部开始
                frame_header = data[byte_counter:byte_counter+4]
                # 确保有足够的数据读取帧长度
                if len(frame_header) >= 4:
                    frame_len = int.from_bytes(frame_header[2:4], 'little')
                    output.append(f"\n  /* Frame {frame_counter} */")
                    output.append(f"  /* Type:0x{frame_header[0]:02X}, Reserved:0x{frame_header[1]:02X}, Len:{frame_len} */")
                    frame_counter += 1
            
            # 每行16个字节，美观显示
            if byte_counter % 16 == 0:
                output.append("\n  ")
            
            output.append(f"0x{hex_pairs[byte_counter]},")
            byte_counter += 1
        
        output.append("\n};")
        output.append(f"const uint32_t {array_name}_len = {len(data)};")
        
        with open(output_file, 'w') as f:
            f.write(" ".join(output))
        
        print(f"成功将 '{input_file}' 转换为 '{output_file}'")
        print(f"共处理 {frame_counter} 个帧, {len(data)} 字节")
        return True
    
    except Exception as e:
        print(f"转换过程中出错: {str(e)}")
        return False

def main():
    if len(sys.argv) >= 4:
        # 从命令行参数读取文件名
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        array_name = sys.argv[3]
    else:
        # 默认值
        input_file = "test2.p3"
        output_file = "test2.h"
        array_name = "test2_data"
        print(f"使用默认参数: {input_file}, {output_file}, {array_name}")
        print("您也可以指定参数: python gen_p3_array.py 输入文件.p3 输出文件.h 数组名")
    
    convert_with_annotations(input_file, output_file, array_name)

if __name__ == "__main__":
    main()