#!/usr/bin/env python3
"""
protect_pdfs.py - Надёжная защита PDF через pdftoppm
"""
import sys
from pathlib import Path
import tempfile
import subprocess

def protect_pdf(input_path: Path, output_path: Path, dpi: int = 150):
    """PDF → JPEG → PDF с помощью pdftoppm"""
    print(f"🔒 Защита {input_path.name}")
    
    temp_dir = Path(tempfile.mkdtemp())
    
    try:
        # 1. Конвертируем PDF в JPEG
        cmd = [
            "pdftoppm", "-jpeg", "-jpegopt", "quality=95",
            "-r", str(dpi), str(input_path), str(temp_dir / "page")
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        
        # Получаем JPEG файлы
        jpegs = sorted(temp_dir.glob("page-*.jpg"))
        if not jpegs:
            print("❌ Не создано JPEG файлов")
            return False
            
        print(f"   Страниц: {len(jpegs)}")
        
        # 2. Конвертируем JPEG в PDF
        cmd = ["img2pdf", "--output", str(output_path)] + [str(j) for j in jpegs]
        subprocess.run(cmd, check=True)
        
        # 3. Результат
        size_mb = output_path.stat().st_size / 1024 / 1024
        print(f"✅ Защищен ({size_mb:.1f} MB)")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"❌ Ошибка: {e.stderr.decode() if e.stderr else str(e)}")
        return False
    finally:
        # Удаляем временные файлы
        for jpeg in temp_dir.glob("*"):
            jpeg.unlink()
        temp_dir.rmdir()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"📌 Использование: {sys.argv[0]} <входная_папка> <выходная_папка>")
        sys.exit(1)
    
    src_dir = Path(sys.argv[1])
    dst_dir = Path(sys.argv[2])
    
    if not src_dir.exists():
        print(f"❌ Папка не найдена: {src_dir}")
        sys.exit(1)
    
    dst_dir.mkdir(parents=True, exist_ok=True)
    
    success = 0
    total = 0
    
    for pdf in src_dir.glob("*.pdf"):
        total += 1
        if protect_pdf(pdf, dst_dir / pdf.name):
            success += 1
    
    print(f"\n📊 Итого: {success}/{total} защищено")
    sys.exit(0 if success == total else 1)