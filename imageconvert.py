from PIL import Image
import sys

def image_to_4bit_bin(input_path, output_path, size, pitch=256):
    """
    Convert an image to 4-bit-per-channel color and output as a .bin file
    with little-endian pixel order and row padding based on display pitch.

    Format per pixel (16 bits total):
        Bits [15–12] = Alpha (0)
        Bits [11–8]  = Red (4-bit)
        Bits [7–4]   = Green (4-bit)
        Bits [3–0]   = Blue (4-bit)
    
    Stored as: [low byte first, high byte second] (little-endian)
    """
    img = Image.open(input_path).convert("RGB")
    img = img.resize(size)

    img.save("resized.jpg")

    bin_data = bytearray()

    for y in range(img.height):
        row_bytes = bytearray()

        for x in range(img.width):
            r, g, b = img.getpixel((x, y))

            # Reduce to 4 bits per channel
            r4 = r >> 4
            g4 = g >> 4
            b4 = b >> 4

            # Pack 16-bit pixel: 0x0RGB
            pixel_16bit = (r4 << 8) | (g4 << 4) | b4

            # Write as little-endian: low byte first
            low_byte = pixel_16bit & 0xFF
            high_byte = (pixel_16bit >> 8) & 0xFF
            row_bytes.append(low_byte)
            row_bytes.append(high_byte)

        # Pad each row to the pitch (in pixels)
        expected_row_bytes = pitch * 2
        pad_len = expected_row_bytes - len(row_bytes)
        if pad_len > 0:
            row_bytes.extend(b'\x00' * pad_len)

        bin_data.extend(row_bytes)

    with open(output_path, "wb") as f:
        f.write(bin_data)

    print(f"✅ Converted '{input_path}' → '{output_path}' ({size[0]}x{size[1]}) with pitch={pitch}, little-endian")


# Example usage
if __name__ == "__main__":
    # Example: python script.py input.png output.bin 64 64 [pitch]
    if len(sys.argv) < 5:
        print("Usage: python script.py <input_image> <output_bin> <width> <height> [pitch]")
        sys.exit(1)

    input_image = sys.argv[1]
    output_bin = sys.argv[2]
    width = int(sys.argv[3])
    height = int(sys.argv[4])
    pitch = int(sys.argv[5]) if len(sys.argv) > 5 else 256

    image_to_4bit_bin(input_image, output_bin, (width, height), pitch)
