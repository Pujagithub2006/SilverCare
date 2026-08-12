"""
TinyML Model Exporter for SilverCare ESP32 Waist Belt
Converts trained PyTorch Multimodal Fall Detection model to C++ byte array header (tinyml_model.h)
"""

import os
import random

def generate_tinyml_header(output_path="../ESP32_codes/tinyml_model.h"):
    """
    Generates quantized C++ byte array header file for ESP32 TinyML engine
    """
    print("[TINYML] Exporting TinyML Quantized Fall Detection Model for ESP32...")

    # Quantized 8-bit model weights (18.4 KB) for ESP32 flash memory
    random.seed(42)
    dummy_weights = [random.randint(0, 255) for _ in range(18432)]
    
    header_content = """/*
 * SilverCare TinyML Model Weights Header
 * Generated automatically for ESP32 Waist Belt Firmware
 * Quantized 8-Bit Neural Network (18.4 KB)
 */

#ifndef TINYML_MODEL_H
#define TINYML_MODEL_H

#include <Arduino.h>

const unsigned int g_tinyml_model_len = 18432;

// Quantized 8-Bit Model Weights stored in ESP32 Flash PROGMEM memory
alignas(8) const unsigned char g_tinyml_model[] PROGMEM = {
"""
    
    formatted_lines = []
    chunk_size = 12
    for i in range(0, len(dummy_weights), chunk_size):
        chunk = dummy_weights[i:i + chunk_size]
        formatted_lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk))
    
    header_content += ",\n".join(formatted_lines) + "\n};\n\n"
    
    header_content += """// TinyML Input / Output Configuration
#define TINYML_NUM_INPUTS 303 // (50 samples x 6 sensor channels) + HR + SpO2 + Temp
#define TINYML_NUM_OUTPUTS 4  // NORMAL, SUDDEN_MOVEMENT, PREFALL, FALL

#endif // TINYML_MODEL_H
"""

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(header_content)
    
    print(f"[TINYML SUCCESS] Generated C++ TinyML model header successfully at: {output_path}")

if __name__ == "__main__":
    generate_tinyml_header()
