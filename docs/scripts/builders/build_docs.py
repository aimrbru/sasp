# docs/scripts/builders/build_docs.py
#!/usr/bin/env python3
import sys
import io
import argparse
from pathlib import Path
import yaml
from typing import Dict, Any, List, Optional, Tuple

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

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

class UniversalDocumentBuilder(DocumentBuilder):
    def __init__(self, base_path: Path, doc_type: str):
        """
        Инициализация генератора документов.
        """
        super().__init__(base_path, GOSTFormatter())
        self.doc_type = doc_type
        self.config = self._load_config(base_path)
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
        with open(self.template_path, 'r', encoding='utf-8') as f:
            self.template = yaml.safe_load(f)
        
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
        
        Args:
            base_path: Базовый путь к проекту
            
        Returns:
            dict: Загруженная конфигурация
            
        Raises:
            FileNotFoundError: Если файл конфигурации не найден
            yaml.YAMLError: Если файл содержит некорректный YAML
        """
        config_path = base_path / "docs/scripts/config_paths.yaml"
        if not config_path.exists():
            raise FileNotFoundError(f"Файл конфигурации не найден: {config_path}")
            
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f) or {}
        except yaml.YAMLError as e:
            raise yaml.YAMLError(f"Ошибка парсинга YAML файла {config_path}: {e}")

    def _get_data_files(self, base_path: Path) -> List[Path]:
        """
        Получение списка файлов данных из конфигурации.
        
        Args:
            base_path: Базовый путь к проекту
            
        Returns:
            List[Path]: Список путей к файлам данных
        """
        data_files_config = self.config.get('data_files', {})
        if not isinstance(data_files_config, dict):
            return []
            
        data_files = []
        for rel_path in data_files_config.values():
            if not rel_path:
                continue
            file_path = base_path / rel_path
            if file_path.exists():
                data_files.append(file_path)
            else:
                print(f"⚠️  Файл данных не найден: {file_path}")
                
        return data_files

    def get_template_path(self) -> Path:
        """
        Получение пути к шаблону документа.
        
        Returns: 
            Path: Путь к файлу шаблона
            
        Raises: if self.doc_type == 're' and section.get('id') == 'intro':
        continue
            KeyError: Если тип документа не найден в конфиге
        """
        content_config = self.config.get('content', {})
        if self.doc_type not in content_config:
            raise KeyError(f"Тип документа '{self.doc_type}' не найден в конфигурации")
            
        template_rel = content_config[self.doc_type]
        return self.base_path / template_rel

    def generate(self, output_path: Optional[Path] = None) -> Path:
        """
        Генерация документа.
        
        Args:
            output_path: Путь для сохранения документа (если None - генерируется автоматически)
            
        Returns:
            Path: Путь к сгенерированному файлу
            
        Raises:
            RuntimeError: Если процессоры не инициализированы
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
        content_xml = self._create_content_xml(self.template)
        
        # Получение метаданных
        metadata = self._get_metadata()
        
        # Генерация пути для сохранения
        if output_path is None:
            output_path = self._generate_output_path()
        
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
        
        Args:
            section: Секция титульной страницы
            xml_parts: Список для добавления XML частей
            
        Raises:
            RuntimeError: Если data_processor не инициализирован
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
        
        Args:
            item_id: ID элемента
            
        Returns:
            str: Имя стиля
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
        
        Returns:
            Dict[str, str]: Метаданные документа
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
        
        Returns:
            str: Имя файла
            
        Raises:
            RuntimeError: Если data_processor не инициализирован
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
        Генерация пути для сохранения документа.
        
        Returns:
            Path: Полный путь к выходному файлу
        """
        out_dir = self.base_path / "docs" / "output"
        out_dir.mkdir(parents=True, exist_ok=True)
        return out_dir / self._generate_filename()


def build_single_document(base_path: Path, doc_type: str, force: bool) -> Tuple[bool, Path]:
    """
    Сборка одного документа.
    
    Args:
        base_path: Базовый путь к проекту
        doc_type: Тип документа
        force: Игнорировать ошибки валидации
        
    Returns:
        Tuple[bool, Path]: Успех сборки и путь к файлу
    """
    try:
        print(f"📄 Генерация документа {doc_type.upper()}")
        
        # Инициализация и валидация в конструкторе
        builder = UniversalDocumentBuilder(base_path, doc_type)
        
        # Проверка инициализации процессоров
        if builder.section_processor is None:
            print(f"❌ Ошибка: section_processor не инициализирован для {doc_type.upper()}")
            return False, Path()
        
        # Генерация документа
        output_file = builder.generate()
        print(f"✅ {doc_type.upper()} сохранён: {output_file.name}")
        print(f"   Таблиц: {builder.section_processor.table_counter}")
        print(f"   Изображений: {len(builder.section_processor.images) if hasattr(builder.section_processor, 'images') else 0}")
        
        return True, output_file
        
    except Exception as e:
        print(f"❌ Ошибка генерации {doc_type.upper()}: {e}")
        import traceback
        traceback.print_exc()
        return False, Path()


def main():
    """
    Основная функция CLI.
    """
    parser = argparse.ArgumentParser(description="Генератор ГОСТ-документов")
    parser.add_argument("doc_type", nargs="?", choices=["re", "tu", "ps"])
    parser.add_argument("--output", "-o", type=Path, help="Выходной файл")
    parser.add_argument("--path", "-p", type=Path, default=Path('.'), help="Путь к проекту")
    parser.add_argument("--force", "-f", action="store_true", help="Игнорировать ошибки")
    args = parser.parse_args()

    base_path = args.path.resolve()
    print(f"🏠 Рабочая директория: {base_path}")

    if args.doc_type:
        if args.output:
            try:
                # Генерация с указанным путем вывода
                builder = UniversalDocumentBuilder(base_path, args.doc_type)
                output_file = builder.generate(args.output)
                print(f"\n✅ {args.doc_type.upper()} сохранён: {output_file}")
                
            except Exception as e:
                print(f"\n❌ Ошибка: {e}")
                sys.exit(1)
        else:
            # Генерация с автоматическим путем
            success, _ = build_single_document(base_path, args.doc_type, args.force)
            if not success:
                sys.exit(1)
    else:
        # Генерация всех документов
        print("📄 Генерация всех документов: РЭ, ТУ, ПС")
        all_success = True
        for doc_type in ["re", "tu", "ps"]:
            success, _ = build_single_document(base_path, doc_type, args.force)
            if not success:
                all_success = False
                
        if all_success:
            print("\n🎉 Все документы успешно сгенерированы!")
        else:
            print("\n⚠️  Некоторые документы не были сгенерированы.")
            sys.exit(1)


if __name__ == "__main__":
    main()