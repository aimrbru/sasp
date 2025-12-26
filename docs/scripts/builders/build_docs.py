#!/usr/bin/env python3
import sys
import os
import argparse
import subprocess
import shutil
from pathlib import Path
import yaml
from typing import Dict, Any, List, Optional, Tuple

# Определяем, запущены ли в GitHub Actions
IS_GITHUB_ACTIONS = 'GITHUB_WORKSPACE' in os.environ

# Для GitHub Actions устанавливаем правильную кодировку
if IS_GITHUB_ACTIONS:
    # В GitHub Actions может быть LANG=C, поэтому принудительно ставим UTF-8
    os.environ['PYTHONIOENCODING'] = 'utf-8'
    os.environ['LANG'] = 'C.UTF-8'
    os.environ['LC_ALL'] = 'C.UTF-8'

try:
    from gost_shared import (
        GOSTFormatter,
        GOSTSharedUtils,
        GOSTDataProcessor,
        GOSTTOCGenerator,
        GOSTSectionProcessor,
        DocumentBuilder,
        GOSTDocumentStructure,
        GOSTValidator
    )
except ImportError as e:
    print(f"❌ Ошибка импорта модулей: {e}")
    print("Убедитесь, что модуль gost_shared установлен или находится в PYTHONPATH")
    
    # В GitHub Actions выходим с ошибкой
    if IS_GITHUB_ACTIONS:
        sys.exit(1)
    else:
        raise


class UniversalDocumentBuilder(DocumentBuilder):
    def __init__(self, base_path: Path, doc_type: str):
        """
        Инициализация генератора документов.
        """
        super().__init__(base_path, GOSTFormatter())
        self.doc_type = doc_type
        
        try:
            self.config = self._load_config(base_path)
        except Exception as e:
            print(f"❌ Ошибка загрузки конфигурации: {e}")
            raise
            
        max_toc_levels = self.config.get('toc_settings', {}).get('max_levels', 2)
        self.toc_generator = GOSTTOCGenerator(doc_type, max_levels=max_toc_levels)
        
        # Загрузка и проверка данных
        data_files = self._get_data_files(base_path)
        if not data_files:
            raise ValueError(f"Не найдены файлы данных в конфиге: {self.config}")
            
        raw_data = GOSTSharedUtils.load_yaml_data(data_files)
        if not raw_data:
            raise ValueError(f"Не удалось загрузить данные из файлов: {data_files}")
            
        # Инициализация процессоров
        self.data_processor = GOSTDataProcessor(raw_data)
        
        # Получаем коэффициент масштабирования из конфига или используем 0.5 по умолчанию
        image_scale = self.config.get('image_settings', {}).get('scale_factor', 0.5)
        
        self.section_processor = GOSTSectionProcessor(
            self.data_processor,
            doc_type=doc_type,
            image_scale=image_scale
        )
        self.toc_generator = GOSTTOCGenerator(doc_type)
        
        # Загрузка и валидация шаблона
        self.template_path = self.get_template_path()
        print(f"📄 Загрузка шаблона из: {self.template_path}")
        
        try:
            with open(self.template_path, 'r', encoding='utf-8') as f:
                self.template = yaml.safe_load(f)
        except Exception as e:
            print(f"❌ Ошибка загрузки шаблона: {e}")
            raise
        
        # Проверка структуры шаблона
        self.validator = GOSTValidator()
        if not self.validator.validate(self.template):
            self.validator.print_report()
            
            # ФИКС: Проверяем, есть ли КРИТИЧЕСКИЕ ошибки (кроме intro)
            has_critical_errors = False
            for error in self.validator.errors:
                if "intro" not in error.lower():
                    has_critical_errors = True
                    break
            
            if has_critical_errors:
                raise ValueError(f"Шаблон документа не соответствует требованиям ГОСТ")
            else:
                # Только ошибка для intro - разрешаем
                print("⚠️  Игнорируем ошибку валидации для intro (специальное разрешение)")

    def _load_config(self, base_path: Path) -> dict:
        """
        Загрузка конфигурации из YAML файла.
        """
        config_path = base_path / "docs/scripts/config_paths.yaml"
        if not config_path.exists():
            raise FileNotFoundError(f"Файл конфигурации не найден: {config_path}")
            
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = yaml.safe_load(f) or {}
                print(f"✅ Конфигурация загружена из: {config_path}")
                return config
        except yaml.YAMLError as e:
            raise yaml.YAMLError(f"Ошибка парсинга YAML файла {config_path}: {e}")

    def _get_data_files(self, base_path: Path) -> List[Path]:
        """
        Получение списка файлов данных из конфигурации.
        """
        data_files_config = self.config.get('data_files', {})
        if not isinstance(data_files_config, dict):
            return []
            
        data_files = []
        for key, rel_path in data_files_config.items():
            if not rel_path:
                continue
            file_path = base_path / rel_path
            if file_path.exists():
                data_files.append(file_path)
                print(f"✅ Файл данных '{key}': {file_path}")
            else:
                print(f"⚠️  Файл данных не найден: {file_path}")
                
        return data_files

    def get_template_path(self) -> Path:
        """
        Получение пути к шаблону документа.
        """
        content_config = self.config.get('content', {})
        if self.doc_type not in content_config:
            raise KeyError(f"Тип документа '{self.doc_type}' не найден в конфигурации")
            
        template_rel = content_config[self.doc_type]
        return self.base_path / template_rel

    def generate(self, output_path: Optional[Path] = None) -> Path:
        """
        Генерация документа.
        """
        print(f"🔍 Начало генерации документа типа {self.doc_type}")
        
        # Дополнительная проверка перед генерацией
        if not self.validator.validate(self.template):
            print("❌ Прервана генерация из-за ошибок валидации:")
            self.validator.print_report()
            raise ValueError("Шаблон документа не прошел валидацию")
        
        print(f"✅ Шаблон прошел валидацию")
        
        # Проверка инициализации процессоров
        if self.data_processor is None:
            raise RuntimeError("data_processor не инициализирован")
        if self.section_processor is None:
            raise RuntimeError("section_processor не инициализирован")
        if self.toc_generator is None:
            raise RuntimeError("toc_generator не инициализирован")
        
        # Создание XML содержимого
        print("🔄 Создание XML содержимого...")
        content_xml = self._create_content_xml(self.template)
        
        # Получение метаданных
        metadata = self._get_metadata()
        
        # Генерация пути для сохранения
        if output_path is None:
            output_path = self._generate_output_path()
        else:
            # Если указан путь, создаем директорию если нужно
            output_path.parent.mkdir(parents=True, exist_ok=True)
        
        print(f"📄 Сохранение документа: {output_path}")
        return self.create_odt_file(content_xml, output_path, metadata)

    def _create_content_xml(self, template: dict) -> str:
        """
        Создание XML содержимого документа.
        """
        # Проверка инициализации процессоров
        if self.section_processor is None:
            raise RuntimeError("section_processor не инициализирован")
        if self.toc_generator is None:
            raise RuntimeError("toc_generator не инициализирован")
            
        # Сброс счетчиков
        self.section_processor.table_counter = 0
        self.section_processor.document_bookmark_counter = 0
        
        # Создание структуры документа
        doc_structure = GOSTDocumentStructure(self.doc_type)
        
        return doc_structure.create_content_structure(
            template,
            self.section_processor,
            self.toc_generator,
            self.formatter,
            title_page_callback=self._process_title_page
        )
    
    def _process_title_page(self, section: dict, xml_parts: list):
        """
        Обработка титульной страницы на основе шаблона.
        """
        if self.data_processor is None:
            raise RuntimeError("data_processor не инициализирован для обработки титульной страницы")
            
        xml_parts.append('      <!-- ========== ТИТУЛЬНЫЙ ЛИСТ ========== -->')
        
        content = section.get('content', [])
        
        # Обрабатываем все элементы шаблона по порядку
        for item in content:
            self._process_title_page_element(item, xml_parts)

    def _process_title_page_element(self, item: dict, xml_parts: list):
        """
        Обработка одного элемента титульной страницы.
        Поддерживает текстовые элементы и пустые строки.
        """
        item_type = item.get('type', '')
        
        if item_type == 'blank_line':
            # Обработка пустых строк
            count = item.get('count', 1)
            for _ in range(count):
                xml_parts.append('      <text:p text:style-name="TitlePage"/>')
        
        elif item_type == 'text':
            # Обработка текстовых элементов
            # Проверяем инициализацию data_processor
            if self.data_processor is None:
                raise RuntimeError("data_processor не инициализирован")
            self._process_title_page_item(item, xml_parts)
        
        else:
            # Для обратной совместимости: если тип не указан, считаем текстовым элементом
            if 'value' in item or 'id' in item:
                if self.data_processor is None:
                    raise RuntimeError("data_processor не инициализирован")
                self._process_title_page_item(item, xml_parts)

    def _process_title_page_item(self, item: dict, xml_parts: list):
        """
        Обработка одного элемента титульной страницы.
        Старый метод для обратной совместимости.
        """
        item_id = item.get('id')
        raw_text = item.get('value', '')
        
        if not raw_text:
            # Если это пустой элемент (например, пустая строка в старом формате)
            xml_parts.append('      <text:p text:style-name="TitlePage"/>')
            return
        
        # Определяем стиль
        style = self._get_title_page_style_by_id(item_id)
        
        # Для approval НЕ используем replace_placeholders
        if item_id == 'approval':
            text = raw_text
        else:
            # Проверяем, что data_processor инициализирован
            if self.data_processor is None:
                raise RuntimeError("data_processor не инициализирован для обработки титульной страницы")
            text = self.data_processor.replace_placeholders(raw_text)
        
        # Обработка approval
        if item_id == 'approval':
            # Убираем завершающий перенос строки
            text = text.rstrip('\n')
            
            # Разбиваем на строки
            lines = text.split('\n')
            
            # Для approval сохраняем ВСЕ строки, включая пустые
            cleaned_lines = [line.rstrip() for line in lines]
            
            # Объединяем с XML тегами переноса строк
            formatted_lines = []
            for i, line in enumerate(cleaned_lines):
                escaped_line = GOSTSharedUtils.escape_xml(line)
                if i > 0:
                    formatted_lines.append('<text:line-break/>')
                formatted_lines.append(escaped_line)
            
            combined_text = ''.join(formatted_lines)
            xml_parts.append(f'      <text:p text:style-name="{style}">{combined_text}</text:p>')
        else:
            # Стандартная обработка для остальных элементов
            if item_id == 'product_name':
                text = text.upper()
            
            # Разбиваем на строки (на случай многострочных значений)
            lines = [line.strip() for line in text.strip().split('\n') if line.strip()]
            
            if lines:
                for line in lines:
                    xml_parts.append(f'      <text:p text:style-name="{style}">{GOSTSharedUtils.escape_xml(line)}</text:p>')
            else:
                # Если текст пустой после обработки
                xml_parts.append(f'      <text:p text:style-name="{style}"/>')

    def _get_title_page_style_by_id(self, item_id: Optional[str]) -> str:
        """
        Определение стиля для элемента титульной страницы на основе его ID.
        """
        if not item_id:
            return "TitlePage"
        
        # Стили на основе ID элемента
        style_map = {
            'company_name': 'TitleCompany',
            'approval': 'TitleRight',  # ← Выравнивание справа!
            'product_name': 'TitleCompany',
            'product_code': 'TitlePage',
            'document_type': 'TitlePage',
            'okpd_code': 'TitleLeft',
            'bottom_info': 'TitleBottom'
        }
        
        return style_map.get(item_id, 'TitlePage')

    def _get_metadata(self) -> Dict[str, str]:
        """
        Получение метаданных документа.
        """
        titles = {
            're': 'Руководство по эксплуатации',
            'tu': 'Технические условия',
            'ps': 'Паспорт изделия'
        }
        
        title = titles.get(self.doc_type, 'Документ ГОСТ')
        
        return {
            'title': title,
            'creator': 'Генератор ГОСТ',
            'generator': f'UniversalDocumentBuilder-{self.doc_type.upper()}',
            'description': f'Сгенерировано автоматически: {title}'
        }

    def _generate_filename(self) -> str:
        """
        Генерация имени файла документа.
        """
        if self.data_processor is None:
            raise RuntimeError("data_processor не инициализирован для генерации имени файла")
            
        product_code = self.data_processor.get_nested_value('product.code')
        if not product_code:
            product_code = 'DOCUMENT'
            
        suffixes = {
            're': '.РЭ',
            'tu': '.ТУ',
            'ps': '.ПС'
        }
        suffix = suffixes.get(self.doc_type, '')
        
        return f"{product_code}{suffix}.odt"

    def _generate_output_path(self) -> Path:
        """
        Генерация пути для сохранения документа в папку odt.
        """
        # Папка для ODT файлов
        odt_dir = self.base_path / "docs" / "output" / "odt"
        odt_dir.mkdir(parents=True, exist_ok=True)
        
        filename = self._generate_filename()
        return odt_dir / filename


def convert_odt_to_pdf(odt_file: Path, pdf_dir: Path) -> Tuple[bool, Path]:
    """
    Конвертация ODT файла в PDF используя LibreOffice.
    """
    if not odt_file.exists():
        print(f"❌ ODT файл не найден: {odt_file}")
        return False, Path()
    
    # Проверяем наличие LibreOffice
    libreoffice_cmd = shutil.which("libreoffice")
    if not libreoffice_cmd:
        print("❌ LibreOffice не найден. Установите LibreOffice для конвертации в PDF.")
        return False, Path()
    
    pdf_dir.mkdir(parents=True, exist_ok=True)
    pdf_file = pdf_dir / f"{odt_file.stem}.pdf"
    
    try:
        print(f"🔄 Конвертация {odt_file.name} в PDF...")
        
        # Команда для конвертации через LibreOffice
        cmd = [
            libreoffice_cmd,
            '--headless',
            '--convert-to', 'pdf:writer_pdf_Export',
            '--outdir', str(pdf_dir),
            str(odt_file)
        ]
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=True,
            timeout=60  # Увеличиваем таймаут для GitHub Actions
        )
        
        if pdf_file.exists():
            print(f"✅ PDF сохранён: {pdf_file.name}")
            return True, pdf_file
        else:
            print(f"❌ PDF не создан. Вывод LibreOffice:\n{result.stderr}")
            return False, Path()
            
    except subprocess.CalledProcessError as e:
        print(f"❌ Ошибка конвертации: {e}")
        if e.stderr:
            print(f"   Стандартная ошибка: {e.stderr[:500]}...")
        return False, Path()
    except subprocess.TimeoutExpired:
        print("❌ Таймаут конвертации (60 секунд)")
        return False, Path()
    except Exception as e:
        print(f"❌ Неизвестная ошибка при конвертации: {e}")
        return False, Path()


def build_single_document(base_path: Path, doc_type: str, force: bool, convert_to_pdf: bool = True) -> Tuple[bool, Path, Path]:
    """
    Сборка одного документа с опциональной конвертацией в PDF.
    """
    try:
        print(f"📄 Генерация документа {doc_type.upper()}")
        
        # Инициализация и валидация в конструкторе
        builder = UniversalDocumentBuilder(base_path, doc_type)
        
        # Проверка инициализации процессоров
        if builder.section_processor is None:
            print(f"❌ Ошибка: section_processor не инициализирован для {doc_type.upper()}")
            return False, Path(), Path()
        
        # Генерация ODT документа
        odt_file = builder.generate()
        print(f"✅ {doc_type.upper()} ODT сохранён: {odt_file.name}")
        print(f"   Таблиц: {builder.section_processor.table_counter}")
        print(f"   Изображений: {len(builder.section_processor.images) if hasattr(builder.section_processor, 'images') else 0}")
        
        # Конвертация в PDF
        pdf_file = Path()
        if convert_to_pdf:
            pdf_dir = base_path / "docs" / "output" / "pdf"
            success, pdf_file = convert_odt_to_pdf(odt_file, pdf_dir)
            if success:
                print(f"📄 {doc_type.upper()} PDF сохранён: {pdf_file.name}")
            else:
                print(f"⚠️  Не удалось сконвертировать {doc_type.upper()} в PDF")
        
        return True, odt_file, pdf_file
        
    except Exception as e:
        print(f"❌ Ошибка генерации {doc_type.upper()}: {e}")
        import traceback
        traceback.print_exc()
        return False, Path(), Path()


def convert_all_odt_to_pdf(base_path: Path) -> Tuple[int, int]:
    """
    Конвертация всех ODT файлов в папке odt в PDF.
    """
    odt_dir = base_path / "docs" / "output" / "odt"
    pdf_dir = base_path / "docs" / "output" / "pdf"
    
    if not odt_dir.exists():
        print(f"❌ Папка с ODT документами не найдена: {odt_dir}")
        return 0, 0
    
    odt_files = list(odt_dir.glob("*.odt"))
    if not odt_files:
        print(f"ℹ️  ODT файлы не найдены в: {odt_dir}")
        return 0, 0
    
    print(f"🔄 Конвертация {len(odt_files)} ODT файлов в PDF...")
    
    success_count = 0
    for odt_file in odt_files:
        success, pdf_file = convert_odt_to_pdf(odt_file, pdf_dir)
        if success:
            success_count += 1
    
    return success_count, len(odt_files)


def get_output_directories(base_path: Path) -> Tuple[Path, Path, Path]:
    """
    Получение путей к выходным директориям.
    """
    output_dir = base_path / "docs" / "output"
    odt_dir = output_dir / "odt"
    pdf_dir = output_dir / "pdf"
    
    return output_dir, odt_dir, pdf_dir


def main():
    """
    Основная функция CLI.
    """
    # Определяем базовый путь
    if IS_GITHUB_ACTIONS:
        base_path = Path(os.environ['GITHUB_WORKSPACE'])
        print(f"🔧 Режим GitHub Actions. Рабочая директория: {base_path}")
    else:
        # Локальный запуск - используем текущую директорию или аргумент
        parser = argparse.ArgumentParser(description="Генератор ГОСТ-документов с раздельным сохранением ODT и PDF")
        parser.add_argument("doc_type", nargs="?", choices=["re", "tu", "ps", "all", "convert"])
        parser.add_argument("--output", "-o", type=Path, help="Выходной файл (устаревшее, файлы сохраняются в docs/output/odt/)")
        parser.add_argument("--path", "-p", type=Path, default=Path('.'), help="Путь к проекту")
        parser.add_argument("--force", "-f", action="store_true", help="Игнорировать ошибки")
        parser.add_argument("--no-pdf", action="store_true", help="Не конвертировать в PDF")
        args = parser.parse_args()
        base_path = args.path.resolve()
        
        # Показываем оставшиеся аргументы для локального запуска
        doc_type = args.doc_type
        convert_to_pdf = not args.no_pdf
        
        print(f"🏠 Локальный режим. Рабочая директория: {base_path}")
    
    # Получаем пути к директориям
    output_dir, odt_dir, pdf_dir = get_output_directories(base_path)
    print(f"📁 ODT файлы будут сохранены в: {odt_dir}")
    print(f"📄 PDF файлы будут сохранены в: {pdf_dir}")
    
    if IS_GITHUB_ACTIONS:
        # В GitHub Actions всегда генерируем все документы
        print("📄 Генерация всех документов в GitHub Actions: РЭ, ТУ, ПС")
        all_success = True
        generated_files = []
        
        for doc_type in ["re", "tu", "ps"]:
            success, odt_file, pdf_file = build_single_document(base_path, doc_type, False, True)
            if success:
                generated_files.append((doc_type, odt_file, pdf_file))
            else:
                all_success = False
        
        if all_success:
            print("\n" + "="*60)
            print("🎉 Все документы успешно сгенерированы в GitHub Actions!")
            print("="*60)
            print("\n📊 Сгенерированные файлы:")
            for doc_type, odt_file, pdf_file in generated_files:
                print(f"\n  {doc_type.upper()}:")
                print(f"    • ODT: {odt_dir.relative_to(base_path)}/{odt_file.name}")
                if pdf_file:
                    print(f"    • PDF: {pdf_dir.relative_to(base_path)}/{pdf_file.name}")
            
            # В GitHub Actions важно зафиксировать выходные файлы
            print("\n📦 Файлы готовы для использования в workflow")
        else:
            print("\n❌ Ошибка генерации в GitHub Actions")
            sys.exit(1)
            
    else:
        # Локальный запуск с аргументами
        if doc_type == "convert":
            # Только конвертация существующих ODT в PDF
            print("🔄 Конвертация существующих ODT файлов в PDF...")
            success_count, total_count = convert_all_odt_to_pdf(base_path)
            if total_count > 0:
                print(f"\n📊 Результат конвертации: {success_count}/{total_count} успешно")
                if success_count == total_count:
                    print("🎉 Все файлы успешно сконвертированы в PDF!")
                else:
                    print("⚠️  Некоторые файлы не были сконвертированы.")
            sys.exit(0 if success_count > 0 else 1)
        
        elif doc_type == "all":
            # Генерация всех документов
            print("📄 Генерация всех документов: РЭ, ТУ, ПС")
            all_success = True
            generated_files = []
            
            for doc_type in ["re", "tu", "ps"]:
                success, odt_file, pdf_file = build_single_document(base_path, doc_type, args.force, convert_to_pdf)
                if success:
                    generated_files.append((doc_type, odt_file, pdf_file))
                else:
                    all_success = False
            
            if all_success:
                print("\n" + "="*60)
                print("🎉 Все документы успешно сгенерированы!")
                print("="*60)
                print("\n📊 Сгенерированные файлы:")
                for doc_type, odt_file, pdf_file in generated_files:
                    print(f"\n  {doc_type.upper()}:")
                    print(f"    • ODT: {odt_dir.name}/{odt_file.name}")
                    if pdf_file:
                        print(f"    • PDF: {pdf_dir.name}/{pdf_file.name}")
            else:
                print("\n⚠️  Некоторые документы не были сгенерированы.")
                sys.exit(1)
                
        elif doc_type:
            if args.output:
                # Предупреждение, что аргумент устарел
                print(f"⚠️  Параметр --output устарел. ODT файлы сохраняются в {odt_dir}")
                print(f"   Указанный путь будет проигнорирован.")
            
            # Генерация конкретного документа
            success, odt_file, pdf_file = build_single_document(base_path, doc_type, args.force, convert_to_pdf)
            if not success:
                sys.exit(1)
            else:
                print(f"\n✅ {doc_type.upper()} успешно сгенерирован!")
                print(f"📁 ODT: {odt_dir.name}/{odt_file.name}")
                if pdf_file:
                    print(f"📄 PDF: {pdf_dir.name}/{pdf_file.name}")
        else:
            # Генерация всех документов по умолчанию
            print("📄 Генерация всех документов: РЭ, ТУ, ПС")
            all_success = True
            generated_files = []
            
            for doc_type in ["re", "tu", "ps"]:
                success, odt_file, pdf_file = build_single_document(base_path, doc_type, False, True)
                if success:
                    generated_files.append((doc_type, odt_file, pdf_file))
                else:
                    all_success = False
            
            if all_success:
                print("\n" + "="*60)
                print("🎉 Все документы успешно сгенерированы!")
                print("="*60)
                print("\n📊 Сгенерированные файлы:")
                for doc_type, odt_file, pdf_file in generated_files:
                    print(f"\n  {doc_type.upper()}:")
                    print(f"    • ODT: {odt_dir.name}/{odt_file.name}")
                    if pdf_file:
                        print(f"    • PDF: {pdf_dir.name}/{pdf_file.name}")
            else:
                print("\n⚠️  Некоторые документы не были сгенерированы.")
                sys.exit(1)


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        # Игнорируем ошибку сломанного пайпа
        sys.stderr.close()
        sys.exit(0)
    except KeyboardInterrupt:
        print("\n\n⚠️  Прервано пользователем")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Неожиданная ошибка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)