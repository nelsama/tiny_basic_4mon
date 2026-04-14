import sys

def convert_bas(input_txt, output_bin):
    # Leer como texto ASCII
    with open(input_txt, 'r', encoding='ascii') as f:
        text = f.read().upper()
    
    # Reemplazar saltos de línea por 0x00
    text = text.replace('\r\n', '\x00').replace('\n', '\x00').replace('\r', '\x00')
    
    # Eliminar líneas vacías (0x00 consecutivos)
    while '\x00\x00' in text:
        text = text.replace('\x00\x00', '\x00')
        
    # Asegurar terminación nula
    if not text.endswith('\x00'):
        text += '\x00'
        
    # Escribir binario
    with open(output_bin, 'wb') as f:
        f.write(text.encode('ascii'))
    print(f"✅ Convertido: {len(text)} bytes → {output_bin}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Uso: python conv_bas.py programa.txt programa.bin")
    else:
        convert_bas(sys.argv[1], sys.argv[2])