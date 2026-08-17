from PIL import Image
import numpy as np
from sys import argv


def compressColors(array: np.array): #convert 8bits per color to 4 bits per color
    return array // 8

def split_into_tiles(img_array, tile_size=8):
    h, w, c = img_array.shape
    assert h % tile_size == 0 and w % tile_size == 0, "Image size must be divisible by tile size of "+str(tile_size)

    tiles = []
    for y in range(0, h, tile_size):
        for x in range(0, w, tile_size):
            tile = img_array[y:y+tile_size, x:x+tile_size, :]
            tiles.append(tile)
    return tiles

def create_empty_palettes(num_colors = 256): #create a 256 bit color palette
    palette = [np.zeros(3).astype(dtype='int') for _ in range(num_colors)]
    return {'pal':palette, 'num_colors': 1} #start with 1 color (clear)

def findColor(red, green, blue, alpha, palette):
    if alpha < 14:
        return 0
    color = np.asarray([red, green, blue]).astype(dtype='int')
    #find the index of the color
    for i in range(1,palette['num_colors']):
        if color[0]==palette['pal'][i][0] and color[1]==palette['pal'][i][1] and color[2]==palette['pal'][i][2]:
            return i
    #add the color if not found
    new_index = palette['num_colors']
    palette['pal'][new_index] = color
    palette['num_colors'] += 1
    if palette['num_colors'] > 256:
        print("Error: Too many colors")
        exit()

    return new_index

def indexizeTiles(tiles: np.array, palette):
    print("indexing tiles\t",len(tiles))
    newtiles = []
    for tile in tiles:
        shape = tile.shape
        newtile = np.zeros(shape=(shape[0],shape[1])).astype(dtype='int')
        for y in range(shape[0]):
            for x in range(shape[1]):
                colorIndex = findColor(tile[y,x,0],tile[y,x,1],tile[y,x,2],tile[y,x,3],palette)
                newtile[x,y] = colorIndex
        newtiles.append(newtile)
    return newtiles

def genPaletteCString(palette, palette_name:str) -> str:
    param = "extern const u16 " + palette_name + "_pal[];"
    c_str = "const u16 " + palette_name + "_pal[256] = {"
    for color in palette['pal']:
        colorentry = str(color[0])+'|'+str(color[1])+'<<5|'+str(color[2])+'<<10,'
        c_str += '\n\t'+colorentry
    c_str += '};'
    c_str = c_str.replace(",}", '}')
    return c_str, param

def genTileCString(tiles, name:str)->str:
    param = "extern const u8 " + name + "_tiles[];"
    c_str = "const u8 " + name + "_tiles[16384] = {"
    for tile in tiles:
        line = "\t"
        for x in range(tile.shape[0]):
            for y in range(tile.shape[1]):
                line += str(tile[y,x]) + ','
        c_str += '\n'+line
    c_str += '};'
    c_str = c_str.replace(',}','}')
    return c_str, param

PALETTE_OFFSET = 0

print(len(argv))
if len(argv) < 2: 
    print("Error! input file not passed as arguments")

# Read the PNG image
image_path = argv[1]  # Replace with your file path
image_name = image_path.replace('\\','/').split('/')[-1].split('.')[0]
print(image_name)
img = Image.open(image_path).convert("RGBA")  # Ensure RGBA format

# Convert to NumPy array
img_array = np.array(img, dtype="uint")
# compress colors to 12-bit
img_array = compressColors(img_array)

#turn the image array into an array of tiles
tiles = split_into_tiles(img_array)

palette = create_empty_palettes()
tiles = indexizeTiles(tiles, palette)

# print(tiles[0:30])

palette_str = genPaletteCString(palette, image_name)


tiles_str = genTileCString(tiles, image_name)
print(tiles_str)

output_filename = image_name+"_tile_pal.c"
header_filename = image_name+"_tile_pal.h"

#write the c source file
fout = open(output_filename, 'w+')
fout.write('#include <gba_types.h>\n')
fout.write('#include "'+header_filename +'"\n\n')
fout.write(palette_str[0])
fout.write('\n\n')
fout.write(tiles_str[0])
fout.write('\n\n')
fout.close()

output_def = '_'+image_name.lower()+"_tile_pal_"
fout = open(header_filename, 'w+')
fout.write('#include <gba_types.h>\n\n')
fout.write('#ifndef '+output_def+'\n')
fout.write('#define '+output_def+'\n')
fout.write(palette_str[1])
fout.write('\n')
fout.write(tiles_str[1])
fout.write('\n')
fout.write('#endif\n')


print("Finished writing tile and palette data to ", output_filename)
# # img_array format is (height, width, [R, G, B, A])
# print("Image shape:", img_array.shape)
# print("First pixel (R,G,B,A):", img_array[0, 0])

# # Example: Access individual channels
# red_channel   = img_array[:, :, 0]
# green_channel = img_array[:, :, 1]
# blue_channel  = img_array[:, :, 2]
# alpha_channel = img_array[:, :, 3]

# # Verify channel data
# print("Red channel shape:", red_channel.shape)
