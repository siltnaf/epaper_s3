import os
import glob
from PIL import Image

# Find all icons8-*-key-50.png files and the extra ones
png_files = glob.glob('images/icons8-*-key-50.png')
print(f"Found {len(png_files)} png files.")

output_file = 'examples/demo/wifi_key_icons.h'
with open(output_file, 'w') as f:
    f.write('#pragma once\n\n')
    f.write('// Auto-generated 1bpp key icons (50x50px), MSB first, black pixels are 1 bits.\n\n')
    
    for png in sorted(png_files):
        name = os.path.basename(png).replace('icons8-', '').replace('-key-50.png', '_key_icon')
        print(f"Converting {png} -> {name}...")
        
        img = Image.open(png).convert('RGBA')
        width, height = img.size
        
        data_bytes = []
        current_byte = 0
        bit_count = 0
        
        for y in range(height):
            for x in range(width):
                r, g, b, a = img.getpixel((x, y))
                is_foreground = False
                if a >= 128:
                    brightness = (r + g + b) / 3
                    if brightness < 128:
                        is_foreground = True
                
                if is_foreground:
                    current_byte |= (0x80 >> bit_count)
                
                bit_count += 1
                if bit_count == 8:
                    data_bytes.append(current_byte)
                    current_byte = 0
                    bit_count = 0
            if bit_count > 0:
                data_bytes.append(current_byte)
                current_byte = 0
                bit_count = 0
                
        f.write(f'static const uint8_t wifi_kbd_{name}[] = {{\n    ')
        for i, b in enumerate(data_bytes):
            f.write(f'0x{b:02X}, ')
            if (i + 1) % 12 == 0:
                f.write('\n    ')
        f.write('\n};\n\n')

    # Convert icons8-cancel-100.png to 100x100 MSB-first 1bpp
    cancel_png = 'images/icons8-cancel-100.png'
    if os.path.exists(cancel_png):
        print(f"Converting {cancel_png}...")
        img = Image.open(cancel_png).convert('RGBA')
        img = img.resize((60, 60), Image.Resampling.LANCZOS) # resize to 60x60 to fit nicely in 226x60 area!
        width, height = img.size
        data_bytes = []
        current_byte = 0
        bit_count = 0
        for y in range(height):
            for x in range(width):
                r, g, b, a = img.getpixel((x, y))
                is_foreground = False
                if a >= 128:
                    brightness = (r + g + b) / 3
                    if brightness < 128:
                        is_foreground = True
                if is_foreground:
                    current_byte |= (0x80 >> bit_count)
                bit_count += 1
                if bit_count == 8:
                    data_bytes.append(current_byte)
                    current_byte = 0
                    bit_count = 0
            if bit_count > 0:
                data_bytes.append(current_byte)
                current_byte = 0
                bit_count = 0
        f.write('// Resize 60x60 cancel icon\n')
        f.write('static const uint8_t wifi_cancel_icon_60x60[] = {\n    ')
        for i, b in enumerate(data_bytes):
            f.write(f'0x{b:02X}, ')
            if (i + 1) % 12 == 0:
                f.write('\n    ')
        f.write('\n};\n\n')

    # Convert icons8-connect-90.png to 60x60 MSB-first 1bpp
    connect_png = 'images/icons8-connect-90.png'
    if os.path.exists(connect_png):
        print(f"Converting {connect_png}...")
        img = Image.open(connect_png).convert('RGBA')
        img = img.resize((60, 60), Image.Resampling.LANCZOS)
        width, height = img.size
        data_bytes = []
        current_byte = 0
        bit_count = 0
        for y in range(height):
            for x in range(width):
                r, g, b, a = img.getpixel((x, y))
                is_foreground = False
                if a >= 128:
                    brightness = (r + g + b) / 3
                    if brightness < 128:
                        is_foreground = True
                if is_foreground:
                    current_byte |= (0x80 >> bit_count)
                bit_count += 1
                if bit_count == 8:
                    data_bytes.append(current_byte)
                    current_byte = 0
                    bit_count = 0
            if bit_count > 0:
                data_bytes.append(current_byte)
                current_byte = 0
                bit_count = 0
        f.write('// Resize 60x60 connect icon\n')
        f.write('static const uint8_t wifi_connect_icon_60x60[] = {\n    ')
        for i, b in enumerate(data_bytes):
            f.write(f'0x{b:02X}, ')
            if (i + 1) % 12 == 0:
                f.write('\n    ')
        f.write('\n};\n\n')

print("Generation complete!")
