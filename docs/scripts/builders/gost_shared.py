"""
Полный набор общих компонентов для генерации ГОСТ документов.
Включает форматтер, обработку данных и DocumentBuilder.

"""
import sys
import re
import yaml
from pathlib import Path
from typing import Dict, List, Any, Optional, Callable, Tuple
from datetime import datetime
import tempfile
import zipfile
import shutil  
import hashlib
from collections import defaultdict


# ============================================================================
# ГОСТ ФОРМАТТЕР 
# ============================================================================

class GOSTFormatter:
    """Форматирование документов по ГОСТ Р 2.105-2019."""
    
    # Константы стилей
    FONT_FAMILY = "FreeSerif"
    FONT_SIZE = "14pt"
    LINE_HEIGHT = "100%"  # Межстрочный интервал 1
    
    # Новые переменные для отступов и полей
    PARAGRAPH_INDENT = "1.2cm"      # Абзацный отступ (красная строка)
    PARAGRAPH_MARGIN_TOP = "0cm"    # Отступ сверху абзаца
    PARAGRAPH_MARGIN_BOTTOM = "0cm" # Отступ снизу абзаца
    
    # Поля страницы по ГОСТ 2.105-2019
    PAGE_WIDTH = "21.0cm"           # Ширина страницы А4
    PAGE_HEIGHT = "29.7cm"          # Высота страницы А4
    PAGE_MARGIN_TOP = "1.5cm"       # Верхнее поле
    PAGE_MARGIN_BOTTOM = "2.0cm"    # Нижнее поле
    PAGE_MARGIN_LEFT = "3.0cm"      # Левое поле (для подшивки)
    PAGE_MARGIN_RIGHT = "1.5cm"     # Правое поле
    
    # Расчетные значения
    @classmethod
    def get_available_width(cls):
        """Возвращает доступную ширину текста между полями."""
        # 21см - 2см слева - 1см справа = 18см
        left = float(cls.PAGE_MARGIN_LEFT.replace('cm', ''))
        right = float(cls.PAGE_MARGIN_RIGHT.replace('cm', ''))
        width = float(cls.PAGE_WIDTH.replace('cm', ''))
        available = width - left - right
        return f"{available:.1f}cm"
    
    @classmethod
    def get_toc_tab_position(cls):
        """Возвращает позицию табуляции для содержания."""
        # Позиция табуляции = ширина страницы - правое поле
        width = float(cls.PAGE_WIDTH.replace('cm', ''))
        right_margin = float(cls.PAGE_MARGIN_RIGHT.replace('cm', ''))
        
        # Два варианта:
        # 1. Табуляция до самого правого края (учитывая только правое поле)
        position = width - right_margin
        # 2. Или с небольшим отступом от правого края
        # position = width - right_margin - 0.5  # минус 0.5см
        
        return f"{position:.1f}cm"
    
    @classmethod
    def get_toc_left_indent(cls):
        """Возвращает левый отступ для содержания."""
        # Обычно такой же как у обычного текста (левое поле + абзацный отступ)
        left_margin = float(cls.PAGE_MARGIN_LEFT.replace('cm', ''))
        paragraph_indent = float(cls.PARAGRAPH_INDENT.replace('cm', ''))
        indent = left_margin + paragraph_indent
        return f"{indent:.1f}cm"
    
    @classmethod
    def get_table_width(cls):
        """Возвращает ширину таблицы."""
        # Ширина таблицы = доступная ширина между полями
        available = float(cls.get_available_width().replace('cm', ''))
        return f"{available:.1f}cm"
    
    @classmethod
    def get_table_column_width(cls, num_columns: int = 3):
        """Возвращает ширину столбца таблицы для заданного количества столбцов."""
        if num_columns <= 0:
            return "5.6cm"  # значение по умолчанию
        
        available = float(cls.get_available_width().replace('cm', ''))
        # Минус небольшие отступы между столбцами
        column_width = (available - 0.5) / num_columns
        return f"{max(column_width, 2.0):.1f}cm"  # не менее 2см
    
    # Буквы для подпунктов по ГОСТ
    SUBCLAUSE_LETTERS = ['а', 'б', 'в', 'г', 'д', 'е', 'ж', 'з', 'и', 'к', 
                        'л', 'м', 'н', 'о', 'п', 'р', 'с', 'т', 'у', 'ф', 
                        'х', 'ц', 'ч', 'ш', 'щ', 'э', 'ю', 'я']
    
    @staticmethod
    def format_number(level_counts: List[int]) -> str:
        """Форматирует номер на основе счетчиков уровней."""
        parts = []
        for count in level_counts:
            if count > 0:
                parts.append(str(count))
            else:
                break
        return ".".join(parts) if parts else ""
    
    @classmethod
    def get_subclause_letter(cls, index: int) -> str:
        """Возвращает букву для подпункта."""
        if 0 <= index < len(cls.SUBCLAUSE_LETTERS):
            return cls.SUBCLAUSE_LETTERS[index]
        return f"[{index + 1}]"
    
    @classmethod
    def format_subclause(cls, text: str, index: int, is_last: bool = False) -> str:
        """Форматирует подпункт: а) текст;"""
        letter = cls.get_subclause_letter(index)
        
        # НЕ убираем знаки препинания в конце текста!
        text = text.strip()
        
        # Проверяем, есть ли знак препинания в конце
        # Если нет - добавляем правильный знак препинания
        if not text or text[-1] not in ';.:':
            # Для последнего пункта - точка, для остальных - точка с запятой
            delimiter = "." if is_last else ";"
            return f"{letter}) {text}{delimiter}"
        else:
            # Если знак препинания уже есть - оставляем как есть
            return f"{letter}) {text}"
    
    @classmethod
    def format_list_item(cls, item_text: str, index: int, style: str, is_last: bool = False) -> str:
        """Форматирует элемент списка в зависимости от стиля."""
        item_text = item_text.strip()
        
        if style == 'no_bullet':
            # Без дефисов и без нумерации - просто текст
            # Убираем лишние знаки препинания, если они уже есть
            if item_text and item_text[-1] not in ';.:':
                return f"{item_text}."
            return item_text
        
        elif style == 'alpha':
            # а), б), в)
            letter = cls.get_subclause_letter(index)
            if not item_text or item_text[-1] not in ';.:':
                delimiter = "." if is_last else ";"
                return f"{letter}) {item_text}{delimiter}"
            else:
                return f"{letter}) {item_text}"
        
        elif style == 'numeric':
            # 1), 2), 3)
            number = index + 1
            if not item_text or item_text[-1] not in ';.:':
                delimiter = "." if is_last else ";"
                return f"{number}) {item_text}{delimiter}"
            else:
                return f"{number}) {item_text}"
        
        elif style == 'roman':
            # I), II), III)
            roman_numerals = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X']
            numeral = roman_numerals[index] if index < len(roman_numerals) else f"[{index + 1}]"
            if not item_text or item_text[-1] not in ';.:':
                delimiter = "." if is_last else ";"
                return f"{numeral}) {item_text}{delimiter}"
            else:
                return f"{numeral}) {item_text}"
        
        elif style == 'bullet':
            # – текст
            if not item_text or item_text[-1] not in ';.:':
                delimiter = "." if is_last else ";"
                return f"– {item_text}{delimiter}"
            else:
                return f"– {item_text}"
        
        else:
            # По умолчанию как bullet
            if not item_text or item_text[-1] not in ';.:':
                delimiter = "." if is_last else ";"
                return f"– {item_text}{delimiter}"
            else:
                return f"– {item_text}"
    
    @classmethod
    def get_level_style(cls, level: int) -> str:
        """Возвращает стиль для заданного уровня вложенности.
        
        Args:
            level: Уровень вложенности (0 - раздел, 1 - подраздел, 2 - пункт, 3 - подпункт)
        
        Returns:
            Имя стиля для использования в XML
        """
        if level == 0:
            return "Heading_20_1"      # Раздел 1
        elif level == 1:
            return "Heading_20_2"      # 1.1 Подраздел
        elif level == 2:
            return "Clause"            # 1.1.1 Пункт
        elif level == 3:
            return "Subclause"         # а) Подпункт
        else:
            # Для более глубоких уровней используем Normal с отступами
            return "Normal"

    @classmethod
    def get_styles_xml(cls) -> str:
        """Возвращает XML для автоматических стилей (content.xml)."""
        # Вычисляем значения на основе переменных
        toc_tab_position = cls.get_toc_tab_position()      # e.g., "20.0cm" (21см - 1см)
        toc_left_indent = cls.get_toc_left_indent()        # e.g., "3.5cm" (2см + 1.5см)
        table_width = cls.get_table_width()               # e.g., "18.0cm"
        table_column_width = cls.get_table_column_width(3) # e.g., "5.8cm" для 3 столбцов
        
        return f'''    <!-- Стили по ГОСТ Р 2.105-2019 -->
            <!-- Стили титульного листа -->
            <style:style style:name="TitleCompany" style:family="paragraph">
            <style:paragraph-properties fo:text-align="center" fo:margin-top="0cm" fo:margin-bottom="0cm" fo:line-height="{cls.LINE_HEIGHT}"/>
            <style:text-properties fo:font-family="{cls.FONT_FAMILY}" fo:font-size="14pt"/>
            </style:style>
            
            <style:style style:name="TitleRight" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="right" 
                fo:margin-top="0cm" 
                fo:margin-bottom="0cm" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:margin-right="0cm"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>

            <style:style style:name="TitleLeft" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="left" 
                fo:margin-top="0cm" 
                fo:margin-bottom="0cm" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:margin-right="0cm"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <style:style style:name="TitlePage" style:family="paragraph">
            <style:paragraph-properties fo:text-align="center" fo:margin-top="0cm" fo:margin-bottom="0cm" fo:line-height="{cls.LINE_HEIGHT}"/>
            <style:text-properties fo:font-family="{cls.FONT_FAMILY}" fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <style:style style:name="TitleBottom" style:family="paragraph">
            <style:paragraph-properties fo:text-align="center" fo:margin-top="0cm" fo:margin-bottom="0cm" fo:line-height="{cls.LINE_HEIGHT}"/>
            <style:text-properties fo:font-family="{cls.FONT_FAMILY}" fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Основные стили документа -->
            <!-- Заголовок 1 уровня -->
            <style:style style:name="Heading_20_1" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" 
                fo:font-weight="bold"/>
            </style:style>
            
            <!-- Заголовок 2 уровня -->
            <style:style style:name="Heading_20_2" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" 
                fo:font-weight="bold"/>
            </style:style>
            
            <!-- Обычный текст -->
            <style:style style:name="Normal" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Стиль для введения -->
            <style:style style:name="Intro" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Пункты (1.1.1) -->
            <style:style style:name="Clause" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}"
                fo:font-weight="normal" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Подпункты (а), б)) -->
            <style:style style:name="Subclause" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="{cls.PARAGRAPH_MARGIN_TOP}" 
                fo:margin-bottom="{cls.PARAGRAPH_MARGIN_BOTTOM}" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="{cls.PARAGRAPH_INDENT}" 
                style:contextual-spacing="true"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Заголовок таблицы -->
            <style:style style:name="TableTitle" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="left" 
                fo:margin-top="0.3cm" 
                fo:margin-bottom="0.1cm" 
                fo:line-height="{cls.LINE_HEIGHT}"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Ячейки таблицы -->
            <style:style style:name="TableCell" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="justify" 
                fo:margin-top="0.1cm" 
                fo:margin-bottom="0.1cm" 
                fo:margin-left="0.1cm"
                fo:margin-right="0.1cm"
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="0cm"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Заголовки столбцов таблицы -->
            <style:style style:name="TableHeader" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="center" 
                fo:margin-top="0.1cm" 
                fo:margin-bottom="0.1cm" 
                fo:line-height="{cls.LINE_HEIGHT}" 
                fo:text-indent="0cm"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" 
                fo:font-weight="bold"/>
            </style:style>
            
            <!-- Стиль для строк оглавления -->
            <style:style style:name="TOC" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="start" 
                fo:margin-top="0cm" 
                fo:margin-bottom="0cm" 
                fo:line-height="100%" 
                fo:text-indent="0cm"
                fo:margin-left="0cm">
                fo:margin-left="{toc_left_indent}">  <!-- Используем вычисленный отступ -->
                <style:tab-stops>
                <style:tab-stop style:position="{toc_tab_position}" style:type="right" style:leader-style="dotted" style:leader-text="."/>
                </style:tab-stops>
            </style:paragraph-properties>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}"/>
            </style:style>
            
            <!-- Заголовок содержания -->
            <style:style style:name="TOCTitle" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="center" 
                fo:margin-top="0cm" 
                fo:margin-bottom="0.5cm" 
                fo:line-height="{cls.LINE_HEIGHT}"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" 
                fo:font-weight="bold"/>
            </style:style>
            
            <!-- Разрыв страницы -->
            <style:style style:name="PageBreak" style:family="paragraph">
            <style:paragraph-properties 
                fo:break-before="page" 
                fo:margin-top="0cm" 
                fo:margin-bottom="0cm"/>
            <style:text-properties 
                fo:font-size="1pt"/>
            </style:style>
            
            <!-- Стили таблиц -->
            <!-- Основной стиль таблицы -->
            <style:style style:name="Table" style:family="table">
            <style:table-properties 
                table:align="margins" 
                style:width="{table_width}" 
                fo:margin-top="0.2cm" 
                fo:margin-bottom="0.2cm"
                style:border-model="collapsing"/>
            </style:style>
            
            <!-- Стиль столбцов таблицы (адаптивная ширина) -->
            <style:style style:name="TableColumn" style:family="table-column">
            <style:table-column-properties 
                style:column-width="{table_column_width}"/>
            </style:style>
            
            <!-- Стиль строк таблицы -->
            <style:style style:name="TableRow" style:family="table-row">
            <style:table-row-properties 
                fo:keep-together="always" 
                style:min-row-height="0.5cm"/>
            </style:style>
            
            <!-- Стиль ячеек таблицы (с границами) -->
            <style:style style:name="TableCellStyle" style:family="table-cell">
            <style:table-cell-properties 
                fo:border="0.35pt solid #000000"
                style:border-line-width="0.35pt"/>
            </style:style>

            <!-- Стиль для центрированных изображений БЕЗ ОБТЕКАНИЯ -->
            <style:style style:name="GraphicsCenter" style:family="graphic">
            <style:graphic-properties 
                style:vertical-pos="top"
                style:vertical-rel="paragraph"
                style:horizontal-pos="center"
                style:horizontal-rel="paragraph"
                style:wrap="none"
                style:number-wrapped-paragraphs="no-limit"
                style:wrap-contour="false"
                fo:margin-left="auto"
                fo:margin-right="auto"
                fo:margin-top="0.3cm"
                fo:margin-bottom="0.1cm"
                style:mirror="none"
                fo:padding="0.1cm"
                fo:border="0.5pt solid #cccccc"
                style:border-line-width="0.5pt"/>
            </style:style>

            <!-- Стиль для обычных изображений БЕЗ ОБТЕКАНИЯ -->
            <style:style style:name="Graphics" style:family="graphic">
            <style:graphic-properties 
                fo:margin-left="0cm"
                fo:margin-right="0cm"
                fo:margin-top="0.2cm"
                fo:margin-bottom="0.2cm"
                style:wrap="none"
                style:number-wrapped-paragraphs="no-limit"
                style:wrap-contour="false"
                style:vertical-pos="top"
                style:vertical-rel="paragraph"
                style:horizontal-pos="center"
                style:horizontal-rel="paragraph"
                style:mirror="none"
                fo:padding="0.05cm"
                fo:border="0.05pt solid #000000"
                style:border-line-width="0.05pt"/>
            </style:style>
            
            <!-- Стиль для подписи изображений (обычный) -->
            <style:style style:name="ImageCaption" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="center" 
                fo:margin-top="0.1cm" 
                fo:margin-bottom="0.2cm" 
                fo:line-height="100%"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" />
            </style:style>
            
            <!-- Стиль для центрированной подписи изображений -->
            <style:style style:name="ImageCaptionCenter" style:family="paragraph">
            <style:paragraph-properties 
                fo:text-align="center" 
                fo:margin-top="0.1cm" 
                fo:margin-bottom="0.3cm" 
                fo:line-height="100%"
                fo:text-indent="0cm"/>
            <style:text-properties 
                fo:font-family="{cls.FONT_FAMILY}" 
                fo:font-size="{cls.FONT_SIZE}" 
                />
        </style:style>'''

# ============================================================================
# ОСНОВНЫЕ УТИЛИТЫ ГОСТ
# ============================================================================

class GOSTSharedUtils:
    """Общие утилиты для ГОСТ документов."""
    
    @staticmethod
    def escape_xml(text: str) -> str:
        """Экранирует специальные XML символы."""
        if not isinstance(text, str):
            return str(text) if text is not None else ""
        
        return (text
                .replace('&', '&amp;')
                .replace('<', '&lt;')
                .replace('>', '&gt;')
                .replace('"', '&quot;')
                .replace("'", '&apos;'))
    
    @staticmethod
    def clean_text(text: str) -> str:
        """Очищает текст от лишних пробелов и переводов строк."""
        if not text:
            return ""
        
        import re
        # Заменяем множественные переводы строк на одинарные
        text = re.sub(r'\n\s*\n+', '\n', text)
        # Убираем пробелы в начале строк
        text = re.sub(r'^\s+', '', text, flags=re.MULTILINE)
        # Убираем пробелы в конце строк
        text = re.sub(r'\s+$', '', text, flags=re.MULTILINE)
        # Заменяем множественные пробелы на одинарные
        text = re.sub(r'\s+', ' ', text)
        
        return text.strip()
    
    @staticmethod
    def _deep_update(target: Dict, source: Dict):
        """Рекурсивно обновляет словарь."""
        for key, value in source.items():
            if key in target and isinstance(target[key], dict) and isinstance(value, dict):
                GOSTSharedUtils._deep_update(target[key], value)
            else:
                target[key] = value
    
    @staticmethod
    def load_yaml_data(file_paths: List[Path]) -> Dict:
        """Загружает данные из YAML файлов."""
        data: Dict[str, Any] = {}
        for file_path in file_paths:
            if file_path.exists():
                with open(file_path, 'r', encoding='utf-8') as f:
                    file_data = yaml.safe_load(f)
                    if file_data:
                        GOSTSharedUtils._deep_update(data, file_data)
        return data
    
    @staticmethod
    def create_xml_header() -> List[str]:
        """Создает заголовок XML документа ODT."""
        return [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"',
            '  xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"',
            '  xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"',
            '  xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"',
            '  xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"',
            '  xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"',
            '  xmlns:xlink="http://www.w3.org/1999/xlink"',
            '  xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"'  # ← ДОБАВЬТЕ ЭТУ СТРОКУ!
            '  office:version="1.2">'
        ]


# ============================================================================
# ОБРАБОТЧИК ДАННЫХ
# ============================================================================

class GOSTDataProcessor:
    """Обработчик данных для ГОСТ документов."""
    
    def __init__(self, data: Dict[str, Any]):
        self.data = data
    
    def get_nested_value(self, path: str) -> Any:
        """Получает значение по вложенному пути."""
        parts = path.split('.')
        current = self.data
        
        for part in parts:
            if '[' in part and ']' in part:
                key_part = part[:part.index('[')]
                idx = int(part[part.index('[')+1:part.index(']')])
                
                if key_part in current and isinstance(current[key_part], list) and idx < len(current[key_part]):
                    current = current[key_part][idx]
                else:
                    return None
            else:
                if part in current:
                    current = current[part]
                else:
                    return None
        
        if isinstance(current, dict) and 'value' in current:
            return current['value']
        
        return current
    
    def replace_placeholders(self, text: Optional[str]) -> str:
        """Заменяет плейсхолдеры в тексте."""
        if not text:
            return ""
        
        text = str(text).strip()  # Явное преобразование в str
        
        def replace(match):
            placeholder = match.group(1).strip()
            value = self.get_nested_value(placeholder)
            
            if value is None:
                return ""
            
            return str(value)
        
        # Заменяем все плейсхолдеры за один проход
        result = re.sub(r'\{\{\s*(.+?)\s*\}\}', replace, text)
        
        # Убираем возможные двойные пробелы
        result = re.sub(r'\s+', ' ', result)
        
        return result


# ============================================================================
# ГЕНЕРАТОР ОГЛАВЛЕНИЯ
# ============================================================================

class GOSTTOCGenerator:
    """Генератор оглавления для ГОСТ документов с поддержкой уровней."""
    
    def __init__(self, doc_type: Optional[str] = None, max_levels: int = 2):
        self.toc_entries: List[Dict] = []
        self.toc_bookmark_counter = 0
        self.doc_type = doc_type
        self.max_levels = max_levels  # Количество уровней нумерации
        # Счетчики для правильной нумерации
        self.section_counter = 0
        self.subsection_counter = 0
        self.point_counter = 0
        # Маппинг id -> запись TOC
        self.id_to_entry: Dict[str, Dict] = {}
        self.node_numbers: Dict[str, List[int]] = {}
    
    def _get_node_children(self, node: Dict) -> List[Dict]:
        """Получает дочерние элементы узла."""
        if 'subsections' in node:
            return node['subsections']
        elif 'points' in node:
            return node['points']
        elif 'subpoints' in node:
            return node['subpoints']
        else:
            return []

    def collect_toc_structure(self, sections: List[Dict]) -> None:
        """Собирает структуру документа для оглавления."""
        self.toc_entries = []
        self.id_to_entry = {}
        self.node_numbers = {}
        self.toc_bookmark_counter = 0
        
        # Сбрасываем счетчики
        self.section_counter = 0
        self.subsection_counter = 0
        self.point_counter = 0
        
        # Обрабатываем рекурсивно все уровни
        self._collect_nodes_recursive(sections, [], 0)

    def _collect_nodes_recursive(self, nodes: List[Dict], parent_numbers: List[int], level: int) -> None:
        """Рекурсивно собирает узлы всех уровней."""
        for i, node in enumerate(nodes):
            node_id = node.get('id', '')
            node_name = node.get('name', '').strip()
            
            if not node_id or not node_name:
                continue
            
            # Пропускаем служебные секции
            if node_id in ["title_page", "table_of_contents", "appendices"]:
                continue
            
            # Определяем, как обрабатывать узел в зависимости от типа документа
            should_number = True
            should_be_in_toc = True
            is_intro = False
            
            # Для РЭ: "intro" не нумеруется и не в TOC
            if node_id == "intro" and self.doc_type == 're':
                should_number = False
                should_be_in_toc = False
                is_intro = True
            
            # Для ТУ: "intro" не нумеруется, но в TOC
            elif node_id == "intro" and self.doc_type == 'tu':
                should_number = False
                should_be_in_toc = True
                is_intro = True
            
            # Рассчитываем номер для ВСЕХ узлов (даже если не попадут в TOC)
            if should_number:
                if level == 0 and not is_intro:
                    # Раздел: 1, 2, 3...
                    self.section_counter += 1
                    current_numbers = [self.section_counter]
                elif level == 1:
                    # Подраздел: 1.1, 1.2, 2.1...
                    current_subsection = i + 1
                    if parent_numbers and len(parent_numbers) > 0:
                        current_numbers = parent_numbers + [current_subsection]
                    else:
                        current_numbers = [1, current_subsection]
                else:
                    # Более глубокие уровни: 1.1.1, 1.1.2, 1.2.1...
                    current_numbers = parent_numbers + [i + 1]
                
                # ВСЕГДА сохраняем номера для всех узлов
                self.node_numbers[node_id] = current_numbers
            else:
                current_numbers = []
            
            # Добавляем в TOC только если должен быть и уровень меньше лимита
            if should_be_in_toc and level < self.max_levels:
                display_number = ".".join(str(num) for num in current_numbers) if current_numbers else ""
                
                self.toc_bookmark_counter += 1
                entry_id = f"toc_{node_id}_{self.toc_bookmark_counter}"
                
                entry = {
                    'id': entry_id,
                    'section_id': node_id,
                    'level': level,
                    'title': node_name,
                    'page': 1,
                    'numbered': should_number,
                    'display_number': display_number,
                    'is_intro': is_intro,
                    'in_toc': True
                }
                
                self.toc_entries.append(entry)
                self.id_to_entry[node_id] = entry
            else:
                # Запись для узлов не в TOC (нужна для закладок и нумерации)
                self.id_to_entry[node_id] = {
                    'id': f"toc_{node_id}_{node_id}",
                    'section_id': node_id,
                    'level': level,
                    'title': node_name,
                    'numbered': should_number,
                    'is_intro': is_intro,
                    'in_toc': False
                }
            
            # Рекурсивно обрабатываем дочерние элементы (ВСЕ уровни)
            children = self._get_node_children(node)
            if children:
                self._collect_nodes_recursive(children, current_numbers, level + 1)

    def _determine_node_type(self, node: Dict, level: int) -> str:
        """Определяет тип узла по его структуре."""
        if 'subsections' in node:
            return 'section'
        elif 'points' in node:
            return 'subsection'
        elif 'subpoints' in node:
            return 'point'
        elif 'blocks' in node:
            if level >= 2:
                return 'point'
            else:
                return 'clause'
        elif 'content' in node:
            return 'content'
        else:
            return 'clause'

  
    def get_entry_by_id(self, node_id: str) -> Optional[Dict]:
        """Получает запись TOC по id узла."""
        return self.id_to_entry.get(node_id)
    
    def get_node_number(self, node_id: str) -> str:
        """Получает номер узла в формате X.Y.Z.W..."""
        if node_id in self.node_numbers:
            numbers = self.node_numbers[node_id]
            # Для intro возвращаем пустую строку
            if node_id == "intro" and self.doc_type == 're':
                return ""
            return ".".join(str(num) for num in numbers)
        return ""
    
    def generate_toc_xml(self, title: str = "Содержание") -> List[str]:
        """Генерация XML для оглавления."""
        xml_parts = []
        
        # Заголовок оглавления
        xml_parts.append(f'      <text:p text:style-name="TOCTitle">{title}</text:p>')
        
        if not self.toc_entries:
            xml_parts.append('      <text:p text:style-name="TOC">[Оглавление будет сгенерировано]</text:p>')
            return xml_parts
        
        for entry in self.toc_entries:
            level = entry['level']
            title_text = GOSTSharedUtils.escape_xml(entry['title'])
            bookmark_id = entry['id']
            page_num = entry.get('page', 1)
            numbered = entry.get('numbered', True)
            
            # Формируем отображаемый текст
            if numbered and 'display_number' in entry:
                display_number = entry['display_number']
                display_text = f"{display_number} {title_text}"
            else:
                display_text = title_text
            
            # Отступ для вложенных уровней
            indent = "    " * level
            
            xml_parts.append(f'      <text:p text:style-name="TOC">')
            xml_parts.append(f'        <text:span>{indent}{display_text}</text:span>')
            xml_parts.append(f'        <text:tab/>')
            xml_parts.append(f'        <text:bookmark-ref text:reference-format="page" text:ref-name="{bookmark_id}">{page_num}</text:bookmark-ref>')
            xml_parts.append(f'      </text:p>')
        
        return xml_parts


# ============================================================================
# ПРОЦЕССОР СЕКЦИЙ С ПОДДЕРЖКОЙ УРОВНЕЙ И BLOCKS
# ============================================================================

class GOSTSectionProcessor:
    """Процессор секций с поддержкой вложенности и модели blocks."""
    
    def __init__(self, data_processor: GOSTDataProcessor, doc_type: Optional[str] = None, image_scale: float = 0.5):
        self.data_processor = data_processor
        self.doc_type = doc_type
        self.table_counter = 0
        self.image_counter = 0
        self.document_bookmark_counter = 0
        self.images: List[Dict[str, Any]] = []
        self.image_scale = image_scale
        
        # Глобальные счетчики для всего документа
        self._global_node_counters: Dict[tuple, int] = {}  # (parent_id, level) -> counter
        
        print(f"🚀 ИНИЦИАЛИЗАЦИЯ GOSTSectionProcessor с поддержкой blocks")
        print(f"   📊 doc_type: {doc_type}")
        print(f"   📈 image_scale: {image_scale}\n")
    
    def reset_document_counters(self):
        """Сбрасывает счетчики документа."""
        self._global_node_counters.clear()
        self.table_counter = 0
        self.image_counter = 0
        self.document_bookmark_counter = 0
        self.images = []
    
    def process_document_structure(self, sections: List[Dict], xml_parts: List[str], toc_generator: Optional['GOSTTOCGenerator'] = None):
        """Обрабатывает всю структуру документа."""
        for section in sections:
            section_id = section.get('id', '')
            
            # Особый случай для intro - обрабатываем с нумерацией, но без номера
            if section_id == "intro":
                self._process_intro_section(section, xml_parts, toc_generator)
            else:
                self._process_node_recursive(section, xml_parts, None, toc_generator)

    def _process_intro_section(self, section: Dict, xml_parts: List[str], 
                            toc_generator: Optional['GOSTTOCGenerator']):
        """Обрабатывает секцию введения."""
        node_id = section.get('id', '')
        node_name = section.get('name', '').strip()
        
        # Получаем запись TOC для закладки (если нужно)
        toc_bookmark_id = None
        if toc_generator:
            entry = toc_generator.get_entry_by_id(node_id)
            if entry:
                toc_bookmark_id = entry.get('id')
        
        # Добавляем заголовок только если не пустой
        if node_name:
            style_name = GOSTFormatter.get_level_style(0)
            
            if toc_bookmark_id:
                xml_parts.append(f'      <text:p text:style-name="{style_name}">')
                xml_parts.append(f'        <text:bookmark-start text:name="{toc_bookmark_id}"/>')
                xml_parts.append(f'        {GOSTSharedUtils.escape_xml(node_name)}')
                xml_parts.append(f'        <text:bookmark-end text:name="{toc_bookmark_id}"/>')
                xml_parts.append(f'      </text:p>')
            else:
                xml_parts.append(f'      <text:p text:style-name="{style_name}">{GOSTSharedUtils.escape_xml(node_name)}</text:p>')
        
        # Обрабатываем blocks
        if 'blocks' in section:
            self._process_blocks(section['blocks'], xml_parts, 0)
        
        # Разрыв страницы после введения
        xml_parts.append('      <text:p text:style-name="PageBreak"/>')
    
    def _process_node_recursive(self, node: Dict, xml_parts: List[str], parent_id: Optional[str], 
                            toc_generator: Optional['GOSTTOCGenerator'], parent_level: int = -1):
        """Рекурсивная обработка узла документа любого уровня вложенности."""
        node_id = node.get('id', '')
        node_name = node.get('name', '').strip()
        
        # Пропускаем служебные секции
        if node_id in ["title_page", "table_of_contents", "appendices"]:
            return
        
        # Обработка плейсхолдеров в имени узла
        if node_name:
            
            # Проверяем есть ли плейсхолдеры
            import re
            placeholders = re.findall(r'\{\{\s*(.+?)\s*\}\}', node_name)
            
            # Заменяем плейсхолдеры
            node_name = self.data_processor.replace_placeholders(node_name)
            
            if '{{image_counter_next}}' in node_name:
                # Нужно найти первое изображение в блоках этого узла
                first_image_num = None
                if 'blocks' in node:
                    for block in node.get('blocks', []):
                        if isinstance(block, dict) and 'image' in block:
                            first_image_num = self.image_counter + 1
                            break
                
                if first_image_num:
                    node_name = node_name.replace('{{image_counter_next}}', str(first_image_num))

        # Определяем уровень вложенности С УЧЕТОМ РОДИТЕЛЯ
        node_type = self._determine_node_type(node)
        
        # Уровень зависит от типа узла и уровня родителя
        if parent_level == -1:
            # Корневой уровень
            level_map = {
                'section': 0,
                'subsection': 1,
                'point': 2,
                'subpoint': 3,
                'clause': 4
            }
            level = level_map.get(node_type, 4)
        else:
            # Дочерний уровень: уровень = родительский уровень + 1
            # Но для некоторых типов может быть другой маппинг
            if node_type == 'subsection' and parent_level == 0:
                level = 1
            elif node_type == 'point' and parent_level == 1:
                level = 2  # Это то, что нам нужно!
            elif node_type == 'point' and parent_level == 0:
                level = 1
            elif node_type == 'subpoint':
                level = parent_level + 1
            else:
                level = parent_level + 1
        
        # Получаем номер узла из TOC генератора
        node_number = ""
        if toc_generator and node_id:
            node_number = toc_generator.get_node_number(node_id)
        
        # Добавляем заголовок узла (если есть имя)
        if node_name:
            style_name = GOSTFormatter.get_level_style(level)
            
            # Формируем полный заголовок
            if node_id == "intro" and self.doc_type == 're':
                full_title = node_name
            else:
                if node_number:
                    full_title = f"{node_number} {node_name}"
                else:
                    full_title = node_name
            
           
            # Ищем закладку в оглавлении
            toc_bookmark_id = None
            if toc_generator:
                entry = toc_generator.get_entry_by_id(node_id)
                if entry:
                    toc_bookmark_id = entry.get('id')
            
            if toc_bookmark_id:
                xml_parts.append(f'      <text:p text:style-name="{style_name}">')
                xml_parts.append(f'        <text:bookmark-start text:name="{toc_bookmark_id}"/>')
                xml_parts.append(f'        {GOSTSharedUtils.escape_xml(full_title)}')
                xml_parts.append(f'        <text:bookmark-end text:name="{toc_bookmark_id}"/>')
                xml_parts.append(f'      </text:p>')
            else:
                xml_parts.append(f'      <text:p text:style-name="{style_name}">{GOSTSharedUtils.escape_xml(full_title)}</text:p>')
        
        # Обрабатываем blocks
        if 'blocks' in node:
            self._process_blocks(node['blocks'], xml_parts, level)
        
        # Рекурсивно обрабатываем дочерние элементы
        children = []
        if 'subsections' in node:
            children = node['subsections']
        elif 'points' in node:
            children = node['points']
        elif 'subpoints' in node:
            children = node['subpoints']
        
        if children:
            for child in children:
                self._process_node_recursive(child, xml_parts, node_id, toc_generator, level)
    
    def _determine_node_type(self, node: Dict) -> str:
        """
        Определяет тип узла по его структуре (без поля type).
        Новая схема не использует поле 'type', поэтому определяем по содержимому.
        """
        # Определяем по наличию определенных ключей
        if 'subsections' in node:
            return 'section'
        elif 'points' in node:
            return 'subsection'
        elif 'subpoints' in node:
            return 'point'
        elif 'blocks' in node:
            # Для узлов с blocks проверяем контекст
            # Если выше был points, то это subpoint
            # Если выше был subpoints, то это более глубокий уровень
            return 'subpoint'
        else:
            # Узел без явных дочерних элементов
            return 'clause'
    
    def _determine_node_level(self, node: Dict) -> int:
        """
        Определяет уровень узла на основе его структуры.
        Заменяет старую логику, которая использовала поле 'type'.
        """
        # Сначала проверяем специальные случаи
        node_id = node.get('id', '')
        if node_id == "intro":
            return 0  # Введение - уровень 0
        
        # Определяем тип узла
        node_type = self._determine_node_type(node)
        
        # Маппинг типа на уровень с учетом вложенности
        # Уровни: 0=раздел, 1=подраздел, 2=пункт, 3=подпункт, 4=подподпункт
        level_map = {
            'section': 0,
            'subsection': 1,
            'point': 2,
            'subpoint': 3,
            'clause': 4
        }
        
        return level_map.get(node_type, 4)
    
    def _process_blocks(self, blocks: List[Dict], xml_parts: List[str], parent_level: int):
        """Обрабатывает блоки контента по новой схеме (без type)."""
        # Шаг 1: Сначала находим ВСЕ изображения в этих блоках и присваиваем им номера
        image_blocks = []
        for i, block in enumerate(blocks):
            if isinstance(block, dict) and 'image' in block:
                image_blocks.append((i, block))
        
        # Создаем маппинг позиция изображения -> его номер
        image_positions = {}
        for idx, (pos, block) in enumerate(image_blocks):
            image_positions[pos] = idx + 1 + self.image_counter  # +1 потому что нумерация с 1
        
        # Шаг 2: Теперь обрабатываем все блоки, зная номера изображений
        for i, block in enumerate(blocks):
            if not isinstance(block, dict):
                # Если это строка, обрабатываем как текст
                if isinstance(block, str):
                    processed_text = self.data_processor.replace_placeholders(block)
                    if processed_text.strip():
                        style_name = "Normal" if parent_level >= 3 else "Clause"
                        xml_parts.append(f'      <text:p text:style-name="{style_name}">{GOSTSharedUtils.escape_xml(processed_text)}</text:p>')
                continue
                
            # 1. Текстовый блок
            if 'text' in block:
                text_content = block['text']
                
                # ЗАМЕНА ПЛЕСХОЛДЕРА ДЛЯ ТАБЛИЦ
                if '{{table_counter_next}}' in text_content:
                    next_table_num = self.table_counter + 1
                    text_content = text_content.replace('{{table_counter_next}}', str(next_table_num))
                
                # ЗАМЕНА ПЛЕСХОЛДЕРА ДЛЯ РИСУНКОВ
                import re
                if '{{image_counter_next}}' in text_content:
                    # Ищем следующее изображение после этого текста
                    next_image_num = None
                    for pos in sorted(image_positions.keys()):
                        if pos > i:  # Изображение, которое идет ПОСЛЕ этого текста
                            next_image_num = image_positions[pos]
                            break
                    
                    if next_image_num is None:
                        # Если нет следующего изображения в этих блоках, используем следующий глобальный номер
                        next_image_num = self.image_counter + 1
                    
                    text_content = re.sub(r'\{\{image_counter_next\}\}', str(next_image_num), text_content)
                
                processed_text = self.data_processor.replace_placeholders(text_content)
                if processed_text.strip():
                    style_name = "Normal" if parent_level >= 3 else "Clause"
                    xml_parts.append(f'      <text:p text:style-name="{style_name}">{GOSTSharedUtils.escape_xml(processed_text)}</text:p>')
            
            # 2. Список
            elif 'list' in block:
                list_data = block['list']
                style = list_data.get('style', 'bullet')
                items = list_data.get('items', [])
                
                if items:
                    for item_idx, item in enumerate(items):
                        if isinstance(item, dict):
                            item_text = item.get('text', '')
                        else:
                            item_text = str(item)
                        
                        # ЗАМЕНА ПЛЕСХОЛДЕРА ДЛЯ РИСУНКОВ В ЭЛЕМЕНТАХ СПИСКА
                        import re
                        if '{{image_counter_next}}' in item_text:
                            # Для элементов списка ищем следующее изображение
                            next_image_num = None
                            for pos in sorted(image_positions.keys()):
                                if pos > i:  # Изображение, которое идет ПОСЛЕ всего списка
                                    next_image_num = image_positions[pos]
                                    break
                            
                            if next_image_num is None:
                                next_image_num = self.image_counter + 1
                            
                            item_text = re.sub(r'\{\{image_counter_next\}\}', str(next_image_num), item_text)
                        
                        # ЗАМЕНА ПЛЕСХОЛДЕРА ДЛЯ ТАБЛИЦ В ЭЛЕМЕНТАХ СПИСКА
                        if '{{table_counter_next}}' in item_text:
                            next_table_num = self.table_counter + 1
                            item_text = item_text.replace('{{table_counter_next}}', str(next_table_num))
                        
                        processed_item = self.data_processor.replace_placeholders(item_text)
                        if processed_item.strip():
                            is_last = (item_idx == len(items) - 1)
                            formatted_item = GOSTFormatter.format_list_item(processed_item, item_idx, style, is_last)
                            list_style = "Subclause" if parent_level >= 2 else "Normal"
                            xml_parts.append(f'      <text:p text:style-name="{list_style}">{GOSTSharedUtils.escape_xml(formatted_item)}</text:p>')
            
            # 3. Таблица
            elif 'table' in block:
                table_data = block['table']
                table_data['type'] = 'table'
                self._process_table(table_data, xml_parts, '      ')
            
            # 4. Изображение
            elif 'image' in block:
                image_data = block['image']
                image_data['type'] = 'image'
                self._process_image(image_data, xml_parts, '      ')
            
            # 5. Разрыв страницы
            elif 'page_break' in block:
                xml_parts.append('      <text:p text:style-name="PageBreak"/>')
        
    def _process_content_item(self, item: Dict, xml_parts: List[str], indent: str, 
                             level: int = 2, is_intro: bool = False):
        """Обработка элемента контента (для обратной совместимости)."""
        item_type = item.get('type', '')
        
        if item_type in ['text', 'paragraph']:
            text = item.get('value', '') or item.get('text', '')
            if text:
                processed = self.data_processor.replace_placeholders(str(text))
                if processed.strip():
                    style = "Normal" if is_intro or level >= 3 else "Clause"
                    xml_parts.append(f'{indent}<text:p text:style-name="{style}">{GOSTSharedUtils.escape_xml(processed)}</text:p>')
        
        elif item_type == 'list':
            items = item.get('items', [])
            for list_item in items:
                if list_item is None:
                    continue
                text = list_item.get('text') if isinstance(list_item, dict) else str(list_item)
                if text is None:
                    continue
                processed = self.data_processor.replace_placeholders(str(text))
                if processed.strip():
                    formatted_item = f"– {processed.strip()}"
                    xml_parts.append(f'{indent}<text:p text:style-name="Normal">{GOSTSharedUtils.escape_xml(formatted_item)}</text:p>')
        
        elif item_type == 'table':
            self._process_table(item, xml_parts, indent)
        
        elif item_type == 'image':
            self._process_image(item, xml_parts, indent)
        
        elif item_type == 'page_break':
            xml_parts.append(f'{indent}<text:p text:style-name="PageBreak"/>')
    
    def _process_point_content_item(self, item: Dict, xml_parts: List[str], indent: str):
        """Обработка элемента контента пункта (для обратной совместимости)."""
        item_type = item.get('type', '')
        
        if item_type in ['text', 'paragraph']:
            text = item.get('value', '') or item.get('text', '')
            if text:
                processed = self.data_processor.replace_placeholders(str(text))
                if processed.strip():
                    xml_parts.append(f'{indent}<text:p text:style-name="Clause">{GOSTSharedUtils.escape_xml(processed)}</text:p>')
        
        elif item_type == 'list':
            items = item.get('items', [])
            for list_item in items:
                if list_item is None:
                    continue
                text = list_item.get('text') if isinstance(list_item, dict) else str(list_item)
                if text is None:
                    continue
                processed = self.data_processor.replace_placeholders(str(text))
                if processed.strip():
                    formatted_item = f"– {processed.strip()}"
                    xml_parts.append(f'{indent}<text:p text:style-name="Normal">{GOSTSharedUtils.escape_xml(formatted_item)}</text:p>')
        
        elif item_type == 'table':
            self._process_table(item, xml_parts, indent)
        
        elif item_type == 'image':
            self._process_image(item, xml_parts, indent)
    
    def _process_table(self, item: Dict, xml_parts: List[str], indent: str):
        """Обработка таблицы."""
        self.table_counter += 1
        table_name = item.get('name', 'Таблица')
        
        if table_name:
            processed_name = self.data_processor.replace_placeholders(str(table_name))
            table_title = f"Таблица {self.table_counter} – {processed_name}"
            xml_parts.append(f'{indent}<text:p text:style-name="TableTitle">{GOSTSharedUtils.escape_xml(table_title)}</text:p>')
        
        headers = item.get('headers', [])
        rows = item.get('rows', [])
        
        col_count = len(headers) if headers else 0
        if col_count == 0 and rows:
            for row in rows:
                if 'cells' in row:
                    col_count = max(col_count, len(row['cells']))
        
        if col_count == 0:
            return
        
        xml_parts.append(f'{indent}<table:table table:name="Table{self.table_counter}" table:style-name="Table">')
        
        for _ in range(col_count):
            xml_parts.append(f'{indent}  <table:table-column table:style-name="TableColumn"/>')
        
        if headers:
            xml_parts.append(f'{indent}  <table:table-row table:style-name="TableRow">')
            for header in headers:
                if header is None:
                    continue
                header_text = self.data_processor.replace_placeholders(str(header))
                xml_parts.append(f'{indent}    <table:table-cell table:style-name="TableCellStyle" office:value-type="string">')
                xml_parts.append(f'{indent}      <text:p text:style-name="TableHeader">{GOSTSharedUtils.escape_xml(header_text)}</text:p>')
                xml_parts.append(f'{indent}    </table:table-cell>')
            xml_parts.append(f'{indent}  </table:table-row>')
        
        for row in rows:
            cells = row.get('cells', [])
            if len(cells) != col_count:
                continue
            
            xml_parts.append(f'{indent}  <table:table-row table:style-name="TableRow">')
            for cell in cells:
                if cell is None:
                    cell_text = " "
                else:
                    cell_text = self.data_processor.replace_placeholders(str(cell)).strip()
                if not cell_text:
                    cell_text = " "
                xml_parts.append(f'{indent}    <table:table-cell table:style-name="TableCellStyle" office:value-type="string">')
                xml_parts.append(f'{indent}      <text:p text:style-name="TableCell">{GOSTSharedUtils.escape_xml(cell_text)}</text:p>')
                xml_parts.append(f'{indent}    </table:table-cell>')
            xml_parts.append(f'{indent}  </table:table-row>')
        
        xml_parts.append(f'{indent}</table:table>')
        xml_parts.append(f'{indent}<text:p text:style-name="Normal"/>')
        
        text_after = item.get('text_after', '')
        if text_after:
            processed_after = self.data_processor.replace_placeholders(str(text_after))
            if processed_after.strip():
                xml_parts.append(f'{indent}<text:p text:style-name="Normal">{GOSTSharedUtils.escape_xml(processed_after)}</text:p>')
    
    def _process_image(self, item: Dict, xml_parts: List[str], indent: str):
        """Обработка изображения."""
        if not hasattr(self, 'images'):
            self.images = []
        
        self.image_counter += 1
        
        path = item.get('path', '')
        caption = item.get('caption', item.get('name', ''))
        
        # Формируем подпись для рисунка
        if caption:
            processed_caption = self.data_processor.replace_placeholders(str(caption))
            image_caption = f"Рисунок {self.image_counter} – {processed_caption}"
        else:
            image_caption = f"Рисунок {self.image_counter}"
        
        # Добавляем изображение
        if path:
            try:
                import hashlib
                path_hash = hashlib.md5(path.encode()).hexdigest()[:8]
                image_ext = Path(path).suffix.lower() or '.png'
                
                if image_ext not in ['.png', '.jpg', '.jpeg', '.gif', '.bmp', '.tiff', '.tif']:
                    image_ext = '.png'
                    
                image_name = f"Pictures/image_{self.image_counter}_{path_hash}{image_ext}"
                
                # Получаем ширину и высоту из шаблона
                original_width = item.get('width', '12cm')
                original_height = item.get('height', '')  # НОВОЕ: получаем высоту
                
                # Функция для уменьшения размеров
                def reduce_size(size_str, scale_factor=None):
                    if not size_str:
                        return ''
                    if scale_factor is None:
                        scale_factor = self.image_scale
                    
                    import re
                    match = re.match(r'([\d.]+)(\D*)', str(size_str).strip())
                    if not match:
                        return size_str
                    
                    value = float(match.group(1))
                    unit = match.group(2) or 'cm'
                    
                    reduced_value = value * scale_factor
                    
                    if unit in ['cm', 'mm', 'in']:
                        return f"{reduced_value:.2f}{unit}"
                    elif unit in ['pt', 'px']:
                        return f"{reduced_value:.1f}{unit}"
                    else:
                        return f"{reduced_value:.2f}{unit}"
                
                # Уменьшаем ширину
                display_width = reduce_size(original_width)
                
                # НОВАЯ ЛОГИКА: вычисляем высоту
                display_height = ''
                
                if original_height:
                    # Если высота указана явно - используем ее
                    display_height = reduce_size(original_height)
                else:
                    # Иначе вычисляем пропорционально
                    try:
                        from PIL import Image
                        img_path_obj = Path(path)
                        if img_path_obj.exists() and img_path_obj.is_file():
                            with Image.open(img_path_obj) as img:
                                real_width, real_height = img.size
                                aspect_ratio = real_height / real_width
                                
                                match = re.match(r'([\d.]+)(\D*)', display_width)
                                if match:
                                    width_value = float(match.group(1))
                                    unit = match.group(2) or 'cm'
                                    height_value = width_value * aspect_ratio
                                    display_height = f"{height_value:.2f}{unit}"
                    except ImportError:
                        match = re.match(r'([\d.]+)(\D*)', display_width)
                        if match:
                            width_value = float(match.group(1))
                            unit = match.group(2) or 'cm'
                            height_value = width_value * 0.75
                            display_height = f"{height_value:.2f}{unit}"
                    except Exception:
                        match = re.match(r'([\d.]+)(\D*)', display_width)
                        if match:
                            width_value = float(match.group(1))
                            unit = match.group(2) or 'cm'
                            height_value = width_value * 0.75
                            display_height = f"{height_value:.2f}{unit}"
                
                # Если всё еще нет высоты, используем дефолтную пропорцию
                if not display_height and display_width:
                    match = re.match(r'([\d.]+)(\D*)', display_width)
                    if match:
                        width_value = float(match.group(1))
                        unit = match.group(2) or 'cm'
                        display_height = f"{width_value * 0.75:.2f}{unit}"
                
                xml_parts.append(f'{indent}<text:p text:style-name="Normal"/>')
                
                xml_parts.append(f'{indent}<text:p text:style-name="Normal">')
                xml_parts.append(f'{indent}  <draw:frame draw:name="Image{self.image_counter}" '
                            f'svg:width="{display_width}" svg:height="{display_height}" '
                            f'draw:style-name="GraphicsCenter" draw:z-index="0">')
                xml_parts.append(f'{indent}    <draw:image xlink:href="{image_name}" '
                            f'xlink:type="simple" xlink:show="embed" '
                            f'xlink:actuate="onLoad"/>')
                xml_parts.append(f'{indent}  </draw:frame>')
                xml_parts.append(f'{indent}</text:p>')
                
                # Подпись под изображением
                xml_parts.append(f'{indent}<text:p text:style-name="ImageCaptionCenter">{GOSTSharedUtils.escape_xml(image_caption)}</text:p>')
                
                # Пустой параграф после
                xml_parts.append(f'{indent}<text:p text:style-name="Normal"/>')
                
                # Сохраняем информацию об изображении
                self.images.append({
                    'path': path,
                    'name': image_name,
                    'caption': image_caption,
                    'width': display_width,
                    'height': display_height
                })
                
            except Exception as e:
                print(f"⚠️ Ошибка при обработке изображения: {e}")
                xml_parts.append(f'{indent}<text:p text:style-name="TableTitle">{GOSTSharedUtils.escape_xml(image_caption)}</text:p>')
                xml_parts.append(f'{indent}<text:p text:style-name="Normal">[Изображение: {path}]</text:p>')
                xml_parts.append(f'{indent}<text:p text:style-name="Normal"/>')
        else:
            xml_parts.append(f'{indent}<text:p text:style-name="TableTitle">{GOSTSharedUtils.escape_xml(image_caption)}</text:p>')
            xml_parts.append(f'{indent}<text:p text:style-name="Normal">[Изображение отсутствует]</text:p>')
            xml_parts.append(f'{indent}<text:p text:style-name="Normal"/>')


# ============================================================================
# СТРУКТУРА ДОКУМЕНТА 
# ============================================================================

class GOSTDocumentStructure:
    """Структура ГОСТ документа."""
    
    def __init__(self, doc_type: Optional[str] = None, config: Optional[Dict] = None):
        self.doc_type = doc_type
        self.config = config or {}
    
    def create_content_structure(self, template: Dict, 
                               section_processor: 'GOSTSectionProcessor',
                               toc_generator: 'GOSTTOCGenerator',
                               formatter,
                               title_page_callback: Optional[Callable[[Dict, List[str]], None]] = None) -> str:
        """Создает структуру контента документа."""
        xml_parts = GOSTSharedUtils.create_xml_header()
        
        xml_parts.append('  <office:automatic-styles>')
        if formatter and hasattr(formatter, 'get_styles_xml'):
            xml_parts.append(formatter.get_styles_xml())
        xml_parts.append('  </office:automatic-styles>')
        
        xml_parts.extend([
            '  <office:body>',
            '    <office:text>'
        ])
        
        # Сбрасываем счетчики в начале генерации документа
        if hasattr(section_processor, 'reset_document_counters'):
            section_processor.reset_document_counters()
        else:
            # Старая версия: сбрасываем напрямую
            section_processor.table_counter = 0
            section_processor.image_counter = 0
            section_processor.document_bookmark_counter = 0
            if hasattr(section_processor, 'images'):
                section_processor.images = []
        
        sections = template.get('sections', [])
        
        # Передаем тип документа в процессор
        if hasattr(toc_generator, 'doc_type'):
            section_processor.doc_type = toc_generator.doc_type
            self.doc_type = toc_generator.doc_type
        
        # Собираем структуру для оглавления
        toc_generator.collect_toc_structure(sections)
        
        # Обрабатываем все секции
        for section in sections:
            section_id = section.get('id', '')
            
            if section_id == "title_page":
                if title_page_callback:
                    title_page_callback(section, xml_parts)
                else:
                    xml_parts.append('      <!-- ========== ТИТУЛЬНЫЙ ЛИСТ ========== -->')
                    xml_parts.append('      <text:p text:style-name="TitlePage">Титульный лист</text:p>')
                
                # Разрыв страницы после титула
                xml_parts.append('      <text:p text:style-name="PageBreak"/>')
                continue
                
            elif section_id == "table_of_contents":
                xml_parts.append('      <!-- ========== СОДЕРЖАНИЕ ========== -->')
                toc_title = "Содержание"
                
                # Генерируем оглавление
                toc_xml = toc_generator.generate_toc_xml(toc_title)
                xml_parts.extend(toc_xml)
                
                # ВСЕГДА разрыв страницы после содержания (по ГОСТ)
                xml_parts.append('      <text:p text:style-name="PageBreak"/>')
                continue
                
            elif section_id == "appendices":
                # Обработка приложений из шаблона
                xml_parts.append('      <!-- ========== ПРИЛОЖЕНИЯ ========== -->')
                xml_parts.append('      <text:p text:style-name="PageBreak"/>')
                
                name = section.get('name', 'Приложения')
                content = section.get('content', [])
                
                if name:
                    xml_parts.append(f'      <text:p text:style-name="Heading_20_1">{GOSTSharedUtils.escape_xml(name)}</text:p>')
                
                for item in content:
                    section_processor._process_content_item(item, xml_parts, '      ')
                continue
                
            else:
                # Обработка ВСЕХ остальных секций через process_document_structure
                # включая intro
                section_processor.process_document_structure([section], xml_parts, toc_generator)
        
        # Если в шаблоне нет секции приложений, добавляем заглушку
        #if not any(section.get('id') == 'appendices' for section in sections):
         #   xml_parts.extend([
        #        '      <!-- ========== ПРИЛОЖЕНИЯ ========== -->',
         #       '      <text:p text:style-name="PageBreak"/>',
         #       '      <text:p text:style-name="Heading_20_1">Приложения</text:p>',
          #      '      <text:p text:style-name="Normal">[Приложения будут добавлены здесь]</text:p>'
          #  ])
        
        xml_parts.extend([
            '    </office:text>',
            '  </office:body>',
            '</office:document-content>'
        ])
        
        return '\n'.join(xml_parts)


# ============================================================================
# ВАЛИДАТОР ШАБЛОНОВ
# ============================================================================

class GOSTValidator:
    """Валидатор структуры ГОСТ-документов."""
    
    def __init__(self):
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self._seen_ids = set()

    def validate(self, template: dict) -> bool:
        """
        Проверяет структуру шаблона документа.
        
        Args:
            template: Шаблон документа
            
        Returns:
            bool: True если валидация пройдена, False если есть ошибки
        """
        self.errors.clear()
        self.warnings.clear()
        self._seen_ids.clear()

        sections = template.get("sections", [])
        if not sections:
            self.errors.append("Документ не содержит разделов")
            return False

        for sec in sections:
            self._walk(sec, path=[], level=1)

        return not self.errors

    def _walk(self, node: Dict, path: List[str], level: int) -> None:
        """Рекурсивно обходит структуру документа."""
        node_id = node.get("id")
        name = node.get("name")

        # E5: уникальность id
        if node_id:
            if node_id in self._seen_ids:
                self.errors.append(f"Дублирующий id: {node_id}")
            self._seen_ids.add(node_id)

        # Для intro не проверяем заголовок
        if node_id != "intro" and (name is None or str(name).strip() == ""):
            self.errors.append(
                f"Отсутствует заголовок у узла "
                f"{self._fmt_path(path, node_id)}"
            )

        # Определяем дочерние узлы
        children = []
        for key in ['subsections', 'points', 'subpoints']:
            if key in node and node[key] is not None:
                children = node[key]
                break
        
        # W2: одиночный пункт
        if children and level >= 2 and len(children) == 1:
            self.warnings.append(
                f"'{name}' состоит из одного пункта "
                f"(допустимо по ГОСТ 6.5.7)"
            )

        # Рекурсия без ограничения глубины
        for child in children:
            self._walk(
                child,
                path + [name or node_id or "?"],
                level + 1
            )
        
        # Проверяем blocks если есть
        if 'blocks' in node and node['blocks']:
            blocks = node['blocks']
            for block in blocks:
                if not isinstance(block, dict):
                    self.errors.append(f"Некорректный блок в узле {node_id}")
                    continue
                
                # Проверяем текстовые блоки
                if 'text' in block:
                    text = block['text']
                    if not isinstance(text, str) or not text.strip():
                        self.warnings.append(f"Пустой текстовый блок в узле {node_id}")
                
                # Проверяем списки
                elif 'list' in block:
                    list_data = block['list']
                    if not isinstance(list_data, dict):
                        self.errors.append(f"Некорректный список в узле {node_id}")
                        continue
                    
                    items = list_data.get('items', [])
                    if not items:
                        self.warnings.append(f"Пустой список в узле {node_id}")

    def _fmt_path(self, path: List[str], node_id: Optional[str]) -> str:
        """Форматирует путь для сообщений об ошибках."""
        return " → ".join(path + [node_id or "?"])

    def print_report(self) -> bool:
        """Выводит отчет о валидации."""
        if self.errors:
            print("❌ Ошибки валидации:")
            for error in self.errors:
                print(f"   - {error}")
        
        if self.warnings:
            print("⚠️  Предупреждения:")
            for warning in self.warnings:
                print(f"   - {warning}")
        
        return not self.errors  

# ============================================================================
# DOCUMENT BUILDER (БАЗОВЫЙ КЛАСС)
# ============================================================================

class DocumentBuilder:
    """Базовый класс для построения и создания ODT документов."""
    
    def __init__(self, base_path: Path, formatter=None):
        self.base_path = base_path
        self.formatter = formatter
        self.data: Dict[str, Any] = {}
        self.data_processor: Optional[GOSTDataProcessor] = None
        self.section_processor: Optional[GOSTSectionProcessor] = None 
        self.toc_generator: Optional[GOSTTOCGenerator] = None  
        
    def get_template_path(self) -> Path:
        """Получение пути к шаблону документа (абстрактный метод)."""
        raise NotImplementedError("Метод get_template_path должен быть реализован в подклассе")
    
    def _create_content_xml(self, template: dict) -> str:
        """Создание XML содержимого документа (абстрактный метод)."""
        raise NotImplementedError("Метод _create_content_xml должен быть реализован в подклассе")
    
    def _get_metadata(self) -> Dict[str, str]:
        """Получение метаданных документа (абстрактный метод)."""
        raise NotImplementedError("Метод _get_metadata должен быть реализован в подклассе")
    
    def load_config(self, config_path: Optional[Path] = None) -> Dict:
        """Загружает конфигурацию из YAML файла."""
        if config_path is None:
            config_path = self.base_path / "docs/scripts/config_paths.yaml"
        
        if config_path and config_path.exists():
            with open(config_path, 'r', encoding='utf-8') as f:
                config = yaml.safe_load(f)
                return config if config else {}
        return {}
    
    def load_yaml_data(self, file_paths: List[Path]) -> Dict:
        """Загружает данные из YAML файлов."""
        return GOSTSharedUtils.load_yaml_data(file_paths)
    
    def create_odt_file(self, content_xml: str, output_path: Optional[Path] = None, 
                    metadata: Optional[Dict] = None) -> Path:
        """Создает ODT файл."""
        if metadata is None:
            metadata = {}
        
        styles_xml = self._get_styles_xml()

        # Получаем список изображений из section_processor
        images_to_add: List[Dict[str, Any]] = []
        if self.section_processor is not None and hasattr(self.section_processor, 'images'):
            images_to_add = self.section_processor.images

            for i, img in enumerate(images_to_add):
                print(f"   {i+1}. {img.get('path', 'Нет пути')} -> {img.get('name', 'Нет имени')}")
        
        odt_bytes = self._create_odt_bytes(content_xml, styles_xml, metadata, images_to_add)
        
        if output_path is None:
            output_path = self._generate_output_path()
        
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'wb') as f:
            f.write(odt_bytes)
        
        print(f"✅ Файл сохранен: {output_path}")
        return output_path
    
    def _get_styles_xml(self) -> str:
        """Возвращает стили для документа."""
        # Используем переменные из форматтера, если они есть
        margin_top = "2.0cm"
        margin_bottom = "2.0cm"
        margin_left = "2.0cm"
        margin_right = "1.0cm"
        
        if self.formatter:
            try:
                margin_top = self.formatter.PAGE_MARGIN_TOP
                margin_bottom = self.formatter.PAGE_MARGIN_BOTTOM
                margin_left = self.formatter.PAGE_MARGIN_LEFT
                margin_right = self.formatter.PAGE_MARGIN_RIGHT
            except AttributeError:
                pass
        
        return f'''<?xml version="1.0" encoding="UTF-8"?>
    <office:document-styles xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    office:version="1.2">
    <office:styles>
        <style:style style:name="Standard" style:family="paragraph">
        <style:text-properties fo:font-size="14pt"/>
        </style:style>
    </office:styles>
    <office:automatic-styles>
        <style:page-layout style:name="Mpm1">
        <style:page-layout-properties 
            fo:page-width="21.0cm" 
            fo:page-height="29.7cm" 
            fo:margin-top="{margin_top}" 
            fo:margin-bottom="{margin_bottom}" 
            fo:margin-left="{margin_left}" 
            fo:margin-right="{margin_right}" 
            style:writing-mode="lr-tb"/>
        </style:page-layout>
    </office:automatic-styles>
    <office:master-styles>
        <style:master-page style:name="Standard" style:page-layout-name="Mpm1">
        <style:header/>
        <style:footer/>
        </style:master-page>
    </office:master-styles>
    </office:document-styles>'''
        
    def _create_odt_bytes(self, content_xml: str, styles_xml: str, metadata: Dict, 
                        images: Optional[List[Dict]] = None) -> bytes:
        """Создает байты ODT файла."""
        if images is None:
            images = []
        
        print(f"🔍 Информация об изображениях:")
        print(f"   Всего изображений: {len(images)}")
        for i, img in enumerate(images):
            print(f"   {i+1}. Путь: {img.get('path', 'Нет пути')}")
            print(f"       Имя в архиве: {img.get('name', 'Нет имени')}")
            print(f"       Подпись: {img.get('caption', 'Без подписи')}")
            
            # Проверяем существование файла
            img_path = self.base_path / img.get('path', '')
            if img_path.exists():
                print(f"       ✅ Файл существует: {img_path}")
                print(f"       📏 Размер: {img_path.stat().st_size} байт")
            else:
                print(f"       ❌ Файл НЕ найден: {img_path}")
        
        current_date = datetime.now()
        date_str = current_date.strftime('%Y-%m-%dT%H:%M:%S')
        
        odt_files = {
            'mimetype': 'application/vnd.oasis.opendocument.text',
            'content.xml': content_xml,
            'meta.xml': self._create_meta_xml(date_str, metadata),
            'styles.xml': styles_xml,
            'settings.xml': self._create_settings_xml()
        }
        
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            
            # Создаем папку для изображений
            pictures_dir = tmp_path / "Pictures"
            pictures_dir.mkdir(parents=True, exist_ok=True)
            
            # Копируем изображения во временную директорию
            for img_info in images:
                try:
                    # Определяем полный путь к исходному изображению
                    img_path = self.base_path / img_info['path']
                    if img_path.exists() and img_path.is_file():
                        dest_path = tmp_path / img_info['name']
                        dest_path.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(img_path, dest_path)

                    else:
                        print(f"⚠️ Изображение не найдено: {img_path}")
                        # Создаем заглушку для отсутствующего изображения
                        dest_path = tmp_path / img_info['name']
                        dest_path.parent.mkdir(parents=True, exist_ok=True)
                        self._create_image_placeholder(dest_path, img_info.get('caption', 'Изображение'))
                except Exception as e:
                    print(f"⚠️ Ошибка при копировании изображения: {e}")
                    import traceback
                    traceback.print_exc()
            
            # Создаем основные файлы
            for name, content in odt_files.items():
                filepath = tmp_path / name
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)
            
            # Создаем META-INF и manifest.xml с изображениями
            (tmp_path / "META-INF").mkdir(exist_ok=True)
            with open(tmp_path / "META-INF" / "manifest.xml", 'w', encoding='utf-8') as f:
                f.write(self._create_manifest_xml(images))
            
            # Создаем архив
            output_path = tmp_path / "document.odt"
            with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zf:
                zf.writestr("mimetype", odt_files['mimetype'], compress_type=zipfile.ZIP_STORED)
                
                for file in ["content.xml", "meta.xml", "styles.xml", "settings.xml"]:
                    zf.write(tmp_path / file, file)
                
                # Добавляем изображения
                for img_info in images:
                    img_file = tmp_path / img_info['name']
                    if img_file.exists():
                        zf.write(img_file, img_info['name'])
                        print(f"✅ Изображение добавлено в архив: {img_info['name']}")
                    else:
                        print(f"❌ Изображение не найдено для архива: {img_file}")
                
                zf.write(tmp_path / "META-INF" / "manifest.xml", "META-INF/manifest.xml")
            
            # Проверяем размер архива
            archive_size = output_path.stat().st_size
            print(f"📦 Размер ODT архива: {archive_size} байт")
            
            with open(output_path, 'rb') as f:
                return f.read()

    @staticmethod
    def _create_image_placeholder(image_path: Path, caption: str):
        """Создает заглушку для отсутствующего изображения."""
        try:
            # Просто создаем пустой файл с правильным расширением
            with open(image_path, 'wb') as f:
                f.write(b'')
            print(f"📝 Создана пустая заглушка для: {caption}")
        except Exception as e:
            print(f"⚠️ Ошибка при создании заглушки: {e}")
    
    def _generate_output_path(self) -> Path:
        """Генерирует путь для сохранения файла."""
        output_dir = self.base_path / "docs" / "output"
        output_dir.mkdir(parents=True, exist_ok=True)
        
        filename = self._generate_filename()
        return output_dir / filename
    
    def _generate_filename(self) -> str:
        """Генерирует имя файла."""
        return "document.odt"
    
    @staticmethod
    def _get_default_styles_xml() -> str:
        """Возвращает стили по умолчанию."""
        return '''<?xml version="1.0" encoding="UTF-8"?>
    <office:document-styles xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    office:version="1.2">
    <office:styles>
        <style:style style:name="Standard" style:family="paragraph">
        <style:text-properties fo:font-size="14pt"/>
        </style:style>
    </office:styles>
    </office:document-styles>'''
    
    @staticmethod
    def _create_meta_xml(date_str: str, metadata: Dict) -> str:
        """Создает XML для метаданных."""
        title = metadata.get('title', 'Документ')
        creator = metadata.get('creator', 'Генератор документов')
        generator = metadata.get('generator', 'DocumentBuilder')
        
        return f'''<?xml version="1.0" encoding="UTF-8"?>
<office:document-meta xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0"
  office:version="1.2">
  <office:meta>
    <meta:generator>{generator}</meta:generator>
    <dc:title>{title}</dc:title>
    <dc:creator>{creator}</dc:creator>
    <meta:creation-date>{date_str}</meta:creation-date>
    <dc:date>{date_str}</dc:date>
  </office:meta>
</office:document-meta>'''
    

    @staticmethod
    def _create_manifest_xml(images: Optional[List[Dict]] = None) -> str:
        """Создает XML манифеста."""
        if images is None:
            images = []
        
        manifest_parts = [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0">',
            '  <manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.text"/>',
            '  <manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>',
            '  <manifest:file-entry manifest:full-path="styles.xml" manifest:media-type="text/xml"/>',
            '  <manifest:file-entry manifest:full-path="meta.xml" manifest:media-type="text/xml"/>',
            '  <manifest:file-entry manifest:full-path="settings.xml" manifest:media-type="text/xml"/>',
            '  <manifest:file-entry manifest:full-path="Pictures/" manifest:media-type=""/>'
        ]
        
        # Добавляем записи для изображений
        for img_info in images:
            img_name = img_info.get('name', '')
            if img_name:
                img_path = Path(img_name)
                ext = img_path.suffix.lower()
                
                # Определяем MIME-тип
                mime_map = {
                    '.png': 'image/png',
                    '.jpg': 'image/jpeg',
                    '.jpeg': 'image/jpeg',
                    '.gif': 'image/gif',
                    '.bmp': 'image/bmp',
                    '.svg': 'image/svg+xml',
                    '.tiff': 'image/tiff',
                    '.tif': 'image/tiff',
                    '.webp': 'image/webp',
                }
                
                mime_type = mime_map.get(ext, 'image/png')
                
                if not img_name.startswith('/'):
                    img_name_with_slash = img_name
                else:
                    img_name_with_slash = img_name
                    
                manifest_parts.append(f'  <manifest:file-entry manifest:full-path="{img_name_with_slash}" manifest:media-type="{mime_type}"/>')
        
        manifest_parts.append('</manifest:manifest>')
        return '\n'.join(manifest_parts)
    
    @staticmethod
    def _create_settings_xml() -> str:
        """Создает XML для настроек."""
        return '''<?xml version="1.0" encoding="UTF-8"?>
    <office:document-settings xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:config="urn:oasis:names:tc:opendocument:xmlns:config:1.0">
    <office:settings>
        <config:config-item-set config:name="ooo:view-settings">
        <config:config-item config:name="VisibleAreaTop" config:type="int">0</config:config-item>
        <config:config-item config:name="VisibleAreaLeft" config:type="int">0</config:config-item>
        <config:config-item config:name="VisibleAreaWidth" config:type="int">21000</config:config-item>
        <config:config-item config:name="VisibleAreaHeight" config:type="int">29700</config:config-item>
        </config:config-item-set>
    </office:settings>
    </office:document-settings>'''

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
        if not hasattr(self, 'data_processor') or self.data_processor is None:
            raise RuntimeError("data_processor не инициализирован")
        
        if self.section_processor is None:
            raise RuntimeError("section_processor не инициализирован")
            
        template_path = self.get_template_path()
        with open(template_path, 'r', encoding='utf-8') as f:
            template = yaml.safe_load(f)

        content_xml = self._create_content_xml(template)
        metadata = self._get_metadata()
        
        if output_path is None:
            output_path = self._generate_output_path()
        
        # Отладочная информация
        print(f"📊 Статистика обработки:")
        print(f"   Таблиц: {self.section_processor.table_counter}")
        print(f"   Изображений: {len(self.section_processor.images)}")
        for i, img in enumerate(self.section_processor.images):
            print(f"   {i+1}. {img.get('caption', 'Без подписи')}")
            print(f"       Путь: {img.get('path')}")
        
        return self.create_odt_file(content_xml, output_path, metadata)