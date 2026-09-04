from PIL import Image

def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)

img = Image.open("photo_SE01.png").convert("RGB")
# 如果你的图片不是 240x320，先缩放到这个尺寸（居中裁剪或拉伸，这里直接用 resize）
img = img.resize((240, 320), Image.Resampling.LANCZOS)

with open("photo_SE01.h", "w") as f:
    f.write("#ifndef IMAGE_H\n#define IMAGE_H\n\n#include <stdint.h>\n\n")
    f.write("const uint8_t image_data[] = {\n")
    pixels = img.load()
    count = 0
    for y in range(320):
        for x in range(240):
            r, g, b = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            # 高字节在前（ST7789 默认）
            f.write(f"0x{(rgb565 >> 8) & 0xFF:02X}, ")
            f.write(f"0x{rgb565 & 0xFF:02X}, ")
            count += 2
            if count % 32 == 0:  # 每16个像素换行
                f.write("\n")
    f.write("};\n\n#endif\n")