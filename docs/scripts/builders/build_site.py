#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_site.py - Генерация high-tech сайта документации САСП-2
"""
import os
import sys
import shutil
from pathlib import Path
from datetime import datetime
import yaml
import io
import jinja2
from jinja2 import Template, UndefinedError

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

# ──────────────────────────────────────────────────────────────────────────────
# КОНФИГУРАЦИЯ И ВАЛИДАЦИЯ ПУТЕЙ
# ──────────────────────────────────────────────────────────────────────────────

def load_config_and_validate():
    """Загружает конфигурацию и валидирует все пути"""
    # 1. Определяем корень проекта
    if 'GITHUB_WORKSPACE' in os.environ:
        PROJECT_ROOT = Path(os.environ['GITHUB_WORKSPACE'])
        print(f"🔧 Режим GitHub Actions. PROJECT_ROOT: {PROJECT_ROOT}")
    else:
        # ВАЖНО: Используем метод который всегда находит esp_cam_blufi
        # Способ 1: Ищем вверх по дереву пока не найдём esp_cam_blufi
        script_path = Path(__file__).resolve()
        current = script_path.parent
        
        # Поднимаемся вверх пока не найдём папку esp_cam_blufi
        while current.name != 'esp_cam_blufi' and current != current.parent:
            current = current.parent
        
        if current.name == 'esp_cam_blufi':
            PROJECT_ROOT = current
            print(f"🔧 Локальный режим. Найдена папка проекта: {PROJECT_ROOT}")
        else:
            # Способ 2: Если не нашли esp_cam_blufi, используем дефолтный путь
            # docs/scripts/builders/build_site.py -> на 5 уровней вверх
            PROJECT_ROOT = script_path.parent.parent.parent.parent.parent
            print(f"🔧 Локальный режим. Использую расчетный путь: {PROJECT_ROOT}")
        
        print(f"   Script: {script_path}")
    
    # 2. Проверяем что в корне есть нужные папки
    print(f"📁 Проверяю структуру в {PROJECT_ROOT}:")
    for folder in ['docs', 'hardware', 'software']:
        if (PROJECT_ROOT / folder).exists():
            print(f"   ✅ {folder}/")
        else:
            print(f"   ❌ {folder}/ не найдена!")
    
    # 3. Теперь загружаем конфигурацию с правильного пути
    config_path = PROJECT_ROOT / "docs" / "scripts" / "config_paths.yaml"
    print(f"🔍 Ищу конфигурацию: {config_path}")
    
    if not config_path.exists():
        # Показываем что есть в папке scripts
        scripts_dir = PROJECT_ROOT / "docs" / "scripts"
        if scripts_dir.exists():
            print(f"📄 Файлы в {scripts_dir}:")
            for f in scripts_dir.iterdir():
                print(f"   • {f.name}")
        raise FileNotFoundError(f"❌ ФАЙЛ КОНФИГУРАЦИИ НЕ НАЙДЕН: {config_path}")
    
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f) or {}
    except Exception as e:
        raise RuntimeError(f"❌ ОШИБКА ЧТЕНИЯ КОНФИГУРАЦИИ {config_path}: {e}")
    
    # 3. Строим и валидируем пути
    base_dirs = config.get('base_dirs', {})
    if not base_dirs:
        raise ValueError("❌ СЕКЦИЯ 'base_dirs' ОТСУТСТВУЕТ В КОНФИГУРАЦИИ")
    
    # Обязательные пути
    required_paths = {}
    
    # hardware/
    hw_path = PROJECT_ROOT / base_dirs.get('hardware')
    if not hw_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА hardware НЕ НАЙДЕНА: {hw_path}")
    required_paths['hardware'] = hw_path
    
    # docs/
    docs_path = PROJECT_ROOT / base_dirs.get('docs')
    if not docs_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА docs НЕ НАЙДЕНА: {docs_path}")
    required_paths['docs'] = docs_path
    
    # docs/output/
    output_path = PROJECT_ROOT / base_dirs.get('output', 'docs/output')
    required_paths['output'] = output_path
    
    # docs/content/
    content_path = PROJECT_ROOT / base_dirs.get('content', 'docs/content')
    if not content_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА content НЕ НАЙДЕНА: {content_path}")
    required_paths['content'] = content_path
    
    # 4. Строим полные пути для конфигурации
    config_paths = {
        "PROJECT_ROOT": PROJECT_ROOT,
        "web_output": output_path / "web",
        "pdf_dir": output_path / "pdf",
        "media_src": docs_path / "media",
        "media_dest": output_path / "web" / "media",
        "templates_dir": PROJECT_ROOT / base_dirs.get('templates', 'docs/templates') / "web",
    }
    
    # 5. Проверяем файлы данных
    data_files_config = config.get('data_files', {})
    if not data_files_config:
        raise ValueError("❌ СЕКЦИЯ 'data_files' ОТСУТСТВУЕТ В КОНФИГУРАЦИИ")
    
    config_paths["data_files"] = {}
    for key, rel_path in data_files_config.items():
        full_path = PROJECT_ROOT / rel_path
        if not full_path.exists():
            raise FileNotFoundError(f"❌ ФАЙЛ ДАННЫХ НЕ НАЙДЕН [{key}]: {full_path}")
        config_paths["data_files"][key] = full_path
        print(f"✅ {key}: {full_path}")
    
    # 6. Проверяем файлы контента
    content_config = config.get('content', {})
    if not content_config:
        raise ValueError("❌ СЕКЦИЯ 'content' ОТСУТСТВУЕТ В КОНФИГУРАЦИИ")
    
    config_paths["content_yaml"] = {}
    for key, rel_path in content_config.items():
        if key in ['re', 'api']:  # только нужные для сайта
            full_path = PROJECT_ROOT / rel_path
            if not full_path.exists():
                raise FileNotFoundError(f"❌ ФАЙЛ КОНТЕНТА НЕ НАЙДЕН [{key}]: {full_path}")
            config_paths["content_yaml"][key] = full_path
            print(f"✅ {key}_content: {full_path}")
    
    # 7. Проверяем templates/web/
    templates_web = config_paths["templates_dir"]
    if not templates_web.exists():
        raise FileNotFoundError(f"❌ ПАПКА ШАБЛОНОВ НЕ НАЙДЕНА: {templates_web}")
    
    site_template = templates_web / "site_template.html"
    if not site_template.exists():
        raise FileNotFoundError(f"❌ ШАБЛОН САЙТА НЕ НАЙДЕН: {site_template}")
    
    print("✅ ВСЕ ПУТИ ПРОВЕРЕНЫ УСПЕШНО")
    return config_paths

# Загружаем и валидируем конфигурацию
try:
    CONFIG = load_config_and_validate()
except Exception as e:
    print(f"\n🔥 КРИТИЧЕСКАЯ ОШИБКА: {e}")
    print("🛑 ПРЕРЫВАЮ ВЫПОЛНЕНИЕ")
    sys.exit(1)

# ──────────────────────────────────────────────────────────────────────────────
# УТИЛИТЫ
# ──────────────────────────────────────────────────────────────────────────────

def load_metadata():
    """Загружает и обрабатывает метаданные из general_info.yaml"""
    meta_path = CONFIG["data_files"]["general"]
    print(f"📖 Загружаю метаданные из: {meta_path}")
    
    try:
        with open(meta_path, "r", encoding="utf-8") as f:
            metadata = yaml.safe_load(f)
        
        if not metadata:
            raise ValueError("Файл метаданных пуст")
        
        # Преобразуем даты для удобного отображения
        dev_date = metadata.get("developer", {}).get("responsible", {}).get("document_date", "")
        if dev_date and str(dev_date).isdigit():
            metadata["developer"]["responsible"]["formatted_date"] = f"{dev_date} г."
        
        # Добавляем год для копирайта
        current_year = datetime.now().year
        metadata["current_year"] = current_year
        
        # Форматируем стандарты
        standards = metadata.get("regulatory", {}).get("compliance_standards", [])
        metadata["regulatory"]["formatted_standards"] = ", ".join(standards) if standards else "не указаны"
        
        return metadata
    except Exception as e:
        print(f"❌ Ошибка загрузки метаданных: {e}")
        return {}

def load_yaml(path: Path) -> dict:
    """Загружает YAML файл с обработкой ошибок"""
    if not path.exists():
        raise FileNotFoundError(f"Файл не найден: {path}")
    
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
            return data if isinstance(data, dict) else {}
    except Exception as e:
        raise RuntimeError(f"Ошибка чтения YAML {path}: {e}")

def clean_output():
    if CONFIG["web_output"].exists():
        shutil.rmtree(CONFIG["web_output"])
    CONFIG["web_output"].mkdir(parents=True, exist_ok=True)
    CONFIG["media_dest"].mkdir(parents=True, exist_ok=True)

def copy_media():
    if CONFIG["media_src"].exists():
        for item in CONFIG["media_src"].rglob("*"):
            if item.is_file():
                rel = item.relative_to(CONFIG["media_src"])
                dest = CONFIG["media_dest"] / rel
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(item, dest)

def get_available_pdfs():
    """Автоматически находит все PDF-файлы в папке pdf_dir"""
    available = []
    pdf_folder = CONFIG["pdf_dir"]
    if not pdf_folder.exists() or not pdf_folder.is_dir():
        print(f"Папка PDF не найдена или недоступна: {pdf_folder}")
        return available

    for pdf_file in pdf_folder.glob("*.pdf"):
        name = pdf_file.stem  # имя файла без .pdf
        fname = pdf_file.name
        available.append((name, fname))
        print(f"Найден PDF: {name} ({fname})")

    if not available:
        print("PDF-файлы не найдены в папке:", pdf_folder)

    return available

def render_text(text: str, context: dict) -> str:
    """Подставляет placeholders {{ key }} из context"""
    if not text or not isinstance(text, str):
        return text
    try:
        t = Template(text)
        return t.render(**context)
    except UndefinedError:
        return text  # Если placeholder не найден — оставляем как есть
    except Exception as e:
        print(f"Ошибка рендера текста: {e}")
        return text

def simple_render_section(section: dict, level: int = 1, context: dict | None = None) -> str:
    context = context or {}
    html = []
    tag = f"h{level}"
    if "name" in section and section["name"].strip():
        name = render_text(section["name"], context)
        anchor = section.get("id", name.lower().replace(" ", "-").replace(".", ""))
        html.append(f"<{tag} id='{anchor}' class='text-{6-level}xl font-bold mt-10 mb-6 border-b border-blue-600 pb-2'>{name}</{tag}>")

    # content
    if "content" in section:
        for block in section.get("content") or []:
            if isinstance(block, dict):
                if block.get("type") == "text" and "value" in block:
                    text = render_text(block["value"], context)
                    html.append(f"<p class='mb-4 text-gray-300 leading-relaxed'>{text.replace('\n', '<br>')}</p>")
                elif block.get("type") == "blank_line":
                    html.append("<br>" * block.get("count", 1))
                elif block.get("type") == "bottom_info" and "value" in block:
                    value = render_text(block["value"], context)
                    html.append(f"<p class='text-gray-400 mt-8'>{value}</p>")

    # blocks
    if "blocks" in section:
        for block in section.get("blocks") or []:
            if isinstance(block, dict):
                if "text" in block:
                    text = render_text(block["text"], context)
                    html.append(f"<p class='mb-4 text-gray-300'>{text.replace('\n', '<br>')}</p>")
                elif "list" in block:
                    if block["list"].get("style") == "no_bullet":
                        html.append("<ul class='list-none pl-0 mb-4 space-y-1'>")  # без маркеров
                    elif block["list"].get("style") == "bullet":
                        html.append("<ul class='list-disc pl-6 mb-4 space-y-1'>")
                    else:
                        html.append("<ul class='list-decimal pl-6 mb-4 space-y-1'>")
                    for item in block["list"].get("items") or []:
                        html.append(f"<li>{render_text(item, context)}</li>")
                    html.append("</ul>")
                elif "table" in block:
                    html.append("<div class='overflow-x-auto mb-6'><table class='w-full border-collapse'>")
                    html.append("<thead><tr class='bg-gray-800'>")
                    for h in block["table"].get("headers") or []:
                        html.append(f"<th class='border border-gray-700 p-3 text-left'>{render_text(h, context)}</th>")
                    html.append("</tr></thead><tbody>")
                    for row in block["table"].get("rows") or []:
                        html.append("<tr>")
                        for cell in row.get("cells") or []:
                            html.append(f"<td class='border border-gray-700 p-3'>{render_text(cell, context)}</td>")
                        html.append("</tr>")
                    html.append("</tbody></table></div>")
                elif "image" in block:
                    path = block["image"].get("path", "").replace("docs/media/", "media/")
                    caption = render_text(block["image"].get("caption", ""), context)
                    width = block["image"].get("width", "auto")
                    html.append(f"<figure class='my-8'><img src='{path}' alt='{caption}' class='mx-auto rounded-lg shadow-lg' style='width:{width};' /><figcaption class='text-center text-gray-400 mt-3'>{caption}</figcaption></figure>")

    # subsections
    if "subsections" in section:
        for sub in section.get("subsections") or []:
            html.append(simple_render_section(sub, level + 1, context))

    # points
    if "points" in section:
        for point in section.get("points") or []:
            html.append(simple_render_section(point, level + 1, context))

    return "\n".join(html)

def generate_toc(sections: list, context: dict) -> str:
    """Генерирует HTML-оглавление"""
    if not sections:
        return "<p class='text-gray-400'>Оглавление отсутствует</p>"

    html = ['<div class="toc sticky top-20 bg-gray-800/80 backdrop-blur-md p-6 rounded-xl border border-blue-700 max-h-[70vh] overflow-y-auto">']
    html.append('<h3 class="text-xl font-bold mb-4 text-blue-400">Оглавление</h3>')
    html.append('<ul class="space-y-2">')

    for section in sections:
        name = render_text(section.get("name", "Без названия"), context)
        anchor = section.get("id", name.lower().replace(" ", "-").replace(".", ""))
        html.append(f'<li><a href="#{anchor}" class="hover:text-blue-400 transition">{name}</a></li>')

        # subsections
        if "subsections" in section:
            for sub in section.get("subsections", []):
                sub_name = render_text(sub.get("name", "Без названия"), context)
                sub_anchor = sub.get("id", sub_name.lower().replace(" ", "-").replace(".", ""))
                html.append(f'<li class="ml-4"><a href="#{sub_anchor}" class="text-gray-300 hover:text-blue-400 transition">{sub_name}</a></li>')

        # points
        if "points" in section:
            for point in section.get("points", []):
                point_name = render_text(point.get("name", "Без названия"), context)
                point_anchor = point.get("id", point_name.lower().replace(" ", "-").replace(".", ""))
                html.append(f'<li class="ml-4"><a href="#{point_anchor}" class="text-gray-300 hover:text-blue-400 transition">{point_name}</a></li>')

    html.append('</ul>')
    html.append('</div>')
    return "\n".join(html)

def build_site():
    """Основная функция генерации сайта"""
    try:
        print("\n" + "=" * 60)
        print("🚀 НАЧИНАЮ ГЕНЕРАЦИЮ САЙТА ДОКУМЕНТАЦИИ")
        print("=" * 60)
        
        # 1. Очистка и копирование медиа
        print("\n📦 ПОДГОТОВКА ВЫХОДНОЙ ДИРЕКТОРИИ")
        clean_output()
        copy_media()
        print("✅ Выходная директория подготовлена")
        
        # 2. Загружаем все необходимые данные
        print("\n📖 ЗАГРУЗКА ДАННЫХ")
        
        # Метаданные проекта
        metadata = load_metadata()
        print(f"✅ Метаданные загружены: {metadata.get('product', {}).get('name', 'N/A')}")
        
        # Спецификации
        spec = load_yaml(CONFIG["data_files"]["specification"])
        print(f"✅ Спецификации загружены: {len(spec.get('specifications', {}))} пунктов")
        
        # Контент документации
        re_data = load_yaml(CONFIG["content_yaml"]["re"])
        api_data = load_yaml(CONFIG["content_yaml"]["api"])
        print(f"✅ Руководство по эксплуатации: {len(re_data.get('sections', []))} разделов")
        print(f"✅ Справочник по API: {len(api_data.get('sections', []))} разделов")
        
        # 3. Подготавливаем контекст для шаблонов
        print("\n🎨 ПОДГОТОВКА КОНТЕКСТА ДЛЯ ШАБЛОНОВ")
        context = {
            "product": metadata.get("product", {}),
            "developer": metadata.get("developer", {}),
            "regulatory": metadata.get("regulatory", {}),
            "repository": metadata.get("repository", {}),
            "version": metadata.get("version", {}),
            "specifications": spec.get("specifications", {}),
            "current_year": metadata.get("current_year", datetime.now().year),
            "generated_at": datetime.now().strftime("%Y-%m-%d %H:%M"),
        }
        
        # Выводим информацию о проекте
        print(f"📋 Проект: {context['product'].get('name', 'N/A')} ({context['product'].get('code', 'N/A')})")
        print(f"🏢 Разработчик: {context['developer'].get('company', 'N/A')}")
        print(f"📅 Дата генерации: {context['generated_at']}")
        
        # 4. Загружаем шаблон
        print("\n🎭 ЗАГРУЗКА ШАБЛОНОВ")
        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(CONFIG["templates_dir"]),
            autoescape=True,
        )
        
        try:
            template = env.get_template("site_template.html")
            print("✅ Шаблон сайта загружен")
        except jinja2.TemplateNotFound as e:
            raise FileNotFoundError(f"❌ ШАБЛОН НЕ НАЙДЕН: {e}")
        
        # 5. Генерация главной страницы
        print("\n🏠 ГЕНЕРАЦИЯ ГЛАВНОЙ СТРАНИЦЫ")
        index_ctx = context.copy()
        index_ctx.update({
            "title": "Документация САСП-2",
            "page_id": "index",
            "toc": "",
            "content": f"""
            <div class="text-center">
                <h1 class="text-5xl font-bold neon mb-6">{context['product'].get('full_name', 'САСП-2')}</h1>
                <p class="text-xl text-gray-400 mb-8">Разработчик: {context['developer'].get('company', '')}, {context['developer'].get('city', '')}</p>
                <div class="mt-16 grid grid-cols-1 md:grid-cols-3 gap-8">
                    <a href="user_guide.html" class="bg-gray-800/50 backdrop-blur-sm p-8 rounded-2xl border border-blue-700 hover:border-blue-500 transition-all shadow-lg hover:shadow-blue-500/20">
                        <h3 class="text-2xl font-semibold mb-4">Руководство пользователя</h3>
                        <p class="text-gray-400">Полное описание эксплуатации изделия</p>
                    </a>
                    <a href="maintenance.html" class="bg-gray-800/50 backdrop-blur-sm p-8 rounded-2xl border border-blue-700 hover:border-blue-500 transition-all shadow-lg hover:shadow-blue-500/20">
                        <h3 class="text-2xl font-semibold mb-4">Руководство по обслуживанию</h3>
                        <p class="text-gray-400">ТО, ремонт, хранение, транспортировка</p>
                    </a>
                    <a href="api.html" class="bg-gray-800/50 backdrop-blur-sm p-8 rounded-2xl border border-blue-700 hover:border-blue-500 transition-all shadow-lg hover:shadow-blue-500/20">
                        <h3 class="text-2xl font-semibold mb-4">Справочник по API</h3>
                        <p class="text-gray-400">Программный интерфейс устройства</p>
                    </a>
                </div>
            </div>
            """
        })
        render_page(template, index_ctx, CONFIG["web_output"] / "index.html")
        print("✅ Главная страница сгенерирована")
        
        # 6. Генерация руководства пользователя
        print("\n📘 ГЕНЕРАЦИЯ РУКОВОДСТВА ПОЛЬЗОВАТЕЛЯ")
        user_excluded_ids = {
            "title_page", "table_of_contents", "product_description",
            "tools_and_equipment", "personnel_requirements",
            "safety_measures", "safety_rules",
            "maintenance_procedure", "maintenance_steps", "maintenance_features",
            "maintenance_check", "maintenance_methods",
            "technical_inspection", "inspection_frequency", "inspection_program",
            "conservation", "conservation_procedure", "deconservation_procedure",
            "repair_safety", "repair_safety_rules",
            "storage", "storage_warehousing", "storage_rules", "warehousing",
            "limited_life_parts", "storage_conditions",
            "transportation", "transport_requirements", "transport_handling"
        }
        
        re_sections = re_data.get("sections") or []
        user_sections = [s for s in re_sections if s.get("id") not in user_excluded_ids]
        
        user_ctx = context.copy()
        user_ctx.update({
            "title": "Руководство пользователя",
            "page_id": "user_guide",
            "toc": generate_toc(user_sections, context),
            "content": "<div class='content'>" + 
                      "\n".join(simple_render_section(s, context=context) for s in user_sections) + 
                      "</div>"
        })
        render_page(template, user_ctx, CONFIG["web_output"] / "user_guide.html")
        print(f"✅ Руководство пользователя: {len(user_sections)} разделов")
        
        # 7. Генерация руководства по обслуживанию
        print("\n🔧 ГЕНЕРАЦИЯ РУКОВОДСТВА ПО ОБСЛУЖИВАНИЮ")
        allowed = {"maintenance", "general_instructions", "maintenance_purpose", "maintenance_executors", 
                   "disassembly_warning", "personnel_requirements", "safety_measures", "safety_rules", 
                   "maintenance_procedure", "maintenance_steps", "maintenance_features", "maintenance_check", 
                   "maintenance_methods", "technical_inspection", "inspection_frequency", "inspection_program", 
                   "conservation", "conservation_procedure", "deconservation_procedure", "current_repair", 
                   "general_repair_instructions", "repair_safety", "repair_safety_rules", "storage", 
                   "storage_warehousing", "storage_rules", "warehousing", "limited_life_parts", "storage_conditions", 
                   "transportation", "transport_requirements", "transport_preparation", "transport_characteristics", 
                   "transport_handling", "disposal", "safety_disposal", "disposal_safety_rules", "disposal_prohibitions", 
                   "preparation_disposal", "disposal_preparation", "disposable_parts", "parts_for_disposal", 
                   "methods_disposal", "disposal_methods", "organizations_disposal", "disposal_organizations"}
        
        maint_sections = [s for s in re_sections if s.get("id") in allowed]
        maint_ctx = context.copy()
        maint_ctx.update({
            "title": "Руководство по обслуживанию",
            "page_id": "maintenance",
            "toc": generate_toc(maint_sections, context),
            "content": "<div class='content'>" + 
                      "\n".join(simple_render_section(s, context=context) for s in maint_sections) + 
                      "</div>"
        })
        render_page(template, maint_ctx, CONFIG["web_output"] / "maintenance.html")
        print(f"✅ Руководство по обслуживанию: {len(maint_sections)} разделов")
        
        # 8. Генерация справочника по API
        print("\n🔌 ГЕНЕРАЦИЯ СПРАВОЧНИКА ПО API")
        api_sections = api_data.get("sections") or []
        api_ctx = context.copy()
        api_ctx.update({
            "title": "Справочник по API",
            "page_id": "api",
            "toc": generate_toc(api_sections, context),
            "content": "<div class='content'>" + 
                      "\n".join(simple_render_section(s, context=context) for s in api_sections) + 
                      "</div>"
        })
        render_page(template, api_ctx, CONFIG["web_output"] / "api.html")
        print(f"✅ Справочник по API: {len(api_sections)} разделов")
        
        # 9. Генерация страницы с PDF документами
        print("\n📄 ГЕНЕРАЦИЯ СТРАНИЦЫ PDF ДОКУМЕНТОВ")
        available_pdfs = get_available_pdfs()
        
        if available_pdfs:
            pdf_content = "<h1 class='text-4xl font-bold neon mb-12'>Нормативная документация</h1>"
            pdf_content += "<div class='grid grid-cols-1 md:grid-cols-3 gap-8'>"
            for name, fname in available_pdfs:
                pdf_content += f"""
                <a href="docs/{fname}" target="_blank" class="bg-gray-800/50 backdrop-blur-sm p-8 rounded-2xl border border-blue-700 hover:border-blue-500 transition-all shadow-lg hover:shadow-blue-500/20">
                    <h3 class="text-2xl font-semibold mb-4">{name}</h3>
                    <p class="text-gray-400">Открыть PDF</p>
                </a>
                """
            pdf_content += "</div>"
            print(f"✅ Найдено PDF документов: {len(available_pdfs)}")
        else:
            pdf_content = "<h1 class='text-4xl font-bold neon mb-12'>Нормативная документация</h1>"
            pdf_content += "<p class='text-gray-400 text-center'>PDF-документы отсутствуют</p>"
            print("⚠️ PDF документы не найдены")
        
        pdf_ctx = context.copy()
        pdf_ctx.update({
            "title": "ГОСТ / Нормативные документы",
            "page_id": "standards",
            "toc": "",
            "content": pdf_content
        })
        render_page(template, pdf_ctx, CONFIG["web_output"] / "standards.html")
        print("✅ Страница PDF документов сгенерирована")
        
        # 10. Итоговый отчёт
        print("\n" + "=" * 60)
        print("✅ САЙТ УСПЕШНО СГЕНЕРИРОВАН")
        print("=" * 60)
        
        # Считаем файлы
        html_files = list(CONFIG["web_output"].glob("*.html"))
        media_files = list(CONFIG["web_output"].rglob("*.*"))
        
        print(f"\n📊 СТАТИСТИКА:")
        print(f"   • HTML страниц: {len(html_files)}")
        print(f"   • Медиа файлов: {len(media_files) - len(html_files)}")
        print(f"   • Всего файлов: {len(media_files)}")
        print(f"\n📁 Выходная директория: {CONFIG['web_output']}")
        
        # Показываем созданные файлы
        print(f"\n📄 СОЗДАННЫЕ ФАЙЛЫ:")
        for html_file in html_files:
            print(f"   • {html_file.name}")
        
        print("\n🌐 Для просмотра локально запустите:")
        print(f"   cd {CONFIG['web_output']} && python -m http.server 8000")
        print("   Затем откройте: http://localhost:8000")
        
    except Exception as e:
        print(f"\n" + "=" * 60)
        print(f"🔥 КРИТИЧЕСКАЯ ОШИБКА ПРИ ГЕНЕРАЦИИ САЙТА")
        print("=" * 60)
        print(f"Ошибка: {e}")
        print(f"\nТип ошибки: {type(e).__name__}")
        import traceback
        traceback.print_exc()
        print("\n🛑 ПРЕРЫВАЮ ВЫПОЛНЕНИЕ")
        raise

def render_page(template, context, path: Path):
    html = template.render(**context)
    path.write_text(html, encoding="utf-8")

if __name__ == "__main__":
    try:
        build_site()
    except Exception as e:
        print(f"Ошибка сборки: {e}")
        exit(1)