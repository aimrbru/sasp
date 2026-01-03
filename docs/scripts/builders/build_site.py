# docs/scripts/builders/build_site.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_site.py - Генерация high-tech сайта документации САСП-2.
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
    if 'GITHUB_WORKSPACE' in os.environ:
        PROJECT_ROOT = Path(os.environ['GITHUB_WORKSPACE'])
        print(f"🔧 Режим GitHub Actions. PROJECT_ROOT: {PROJECT_ROOT}")
    else:
        script_path = Path(__file__).resolve()
        current = script_path.parent
        
        while current.name != 'esp_cam_blufi' and current != current.parent:
            current = current.parent
        
        if current.name == 'esp_cam_blufi':
            PROJECT_ROOT = current
            print(f"🔧 Локальный режим. Найдена папка проекта: {PROJECT_ROOT}")
        else:
            PROJECT_ROOT = script_path.parent.parent.parent.parent.parent
            print(f"🔧 Локальный режим. Использую расчетный путь: {PROJECT_ROOT}")
        
        print(f"   Script: {script_path}")
    
    print(f"📁 Проверяю структуру в {PROJECT_ROOT}:")
    for folder in ['docs', 'hardware', 'software']:
        if (PROJECT_ROOT / folder).exists():
            print(f"   ✅ {folder}/")
        else:
            print(f"   ❌ {folder}/ не найдена!")
    
    config_path = PROJECT_ROOT / "docs" / "scripts" / "config_paths.yaml"
    print(f"🔍 Ищу конфигурацию: {config_path}")
    
    if not config_path.exists():
        raise FileNotFoundError(f"❌ ФАЙЛ КОНФИГУРАЦИИ НЕ НАЙДЕН: {config_path}")
    
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f) or {}
    except Exception as e:
        raise RuntimeError(f"❌ ОШИБКА ЧТЕНИЯ КОНФИГУРАЦИИ {config_path}: {e}")
    
    base_dirs = config.get('base_dirs', {})
    if not base_dirs:
        raise ValueError("❌ СЕКЦИЯ 'base_dirs' ОТСУТСТВУЕТ В КОНФИГУРАЦИИ")
    
    required_paths = {}
    
    hw_path = PROJECT_ROOT / base_dirs.get('hardware')
    if not hw_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА hardware НЕ НАЙДЕНА: {hw_path}")
    required_paths['hardware'] = hw_path
    
    docs_path = PROJECT_ROOT / base_dirs.get('docs')
    if not docs_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА docs НЕ НАЙДЕНА: {docs_path}")
    required_paths['docs'] = docs_path
    
    output_path = PROJECT_ROOT / base_dirs.get('output', 'docs/output')
    required_paths['output'] = output_path
    
    content_path = PROJECT_ROOT / base_dirs.get('content', 'docs/content')
    if not content_path.exists():
        raise FileNotFoundError(f"❌ ПАПКА content НЕ НАЙДЕНА: {content_path}")
    required_paths['content'] = content_path
    
    config_paths = {
        "PROJECT_ROOT": PROJECT_ROOT,
        "web_output": output_path / "web",
        "pdf_dir": output_path / "pdf",
        "media_src": docs_path / "media",
        "media_dest": output_path / "web" / "media",
        "templates_dir": PROJECT_ROOT / base_dirs.get('templates', 'docs/templates') / "web",
    }
    
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
    
    content_config = config.get('content', {})
    if not content_config:
        raise ValueError("❌ СЕКЦИЯ 'content' ОТСУТСТВУЕТ В КОНФИГУРАЦИИ")
    
    config_paths["content_yaml"] = {}
    for key, rel_path in content_config.items():
        if key in ['re', 'api']:
            full_path = PROJECT_ROOT / rel_path
            if not full_path.exists():
                raise FileNotFoundError(f"❌ ФАЙЛ КОНТЕНТА НЕ НАЙДЕН [{key}]: {full_path}")
            config_paths["content_yaml"][key] = full_path
            print(f"✅ {key}_content: {full_path}")
    
    templates_web = config_paths["templates_dir"]
    if not templates_web.exists():
        raise FileNotFoundError(f"❌ ПАПКА ШАБЛОНОВ НЕ НАЙДЕНА: {templates_web}")
    
    site_template = templates_web / "site_template.html"
    if not site_template.exists():
        raise FileNotFoundError(f"❌ ШАБЛОН САЙТА НЕ НАЙДЕН: {site_template}")
    
    print("✅ ВСЕ ПУТИ ПРОВЕРЕНЫ УСПЕШНО")
    return config_paths

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
        
        dev_date = metadata.get("developer", {}).get("responsible", {}).get("document_date", "")
        if dev_date and str(dev_date).isdigit():
            metadata["developer"]["responsible"]["formatted_date"] = f"{dev_date} г."
        
        current_year = datetime.now().year
        metadata["current_year"] = current_year
        
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
    """Автоматически находит все PDF-файлы"""
    available = []
    pdf_folder = CONFIG["pdf_dir"]
    
    if pdf_folder.exists() and pdf_folder.is_dir():
        for pdf_file in pdf_folder.glob("*.pdf"):
            name = pdf_file.stem
            fname = pdf_file.name
            available.append((name, fname))
            print(f"✅ PDF: {name}")
    
    return available

def render_text(text: str, context: dict) -> str:
    """Подставляет placeholders {{ key }} из context"""
    if not text or not isinstance(text, str):
        return text
    try:
        t = Template(text)
        return t.render(**context)
    except UndefinedError:
        return text
    except Exception as e:
        print(f"Ошибка рендера текста: {e}")
        return text

def simple_render_section(section: dict, level: int = 1, context: dict | None = None) -> str:
    context = context or {}
    html = []
    
    if not isinstance(section, dict):
        return ""
    
    tag = f"h{level}"
    if "name" in section and section["name"].strip():
        name = render_text(section["name"], context)
        anchor = section.get("id", name.lower().replace(" ", "-").replace(".", ""))

        size_classes = {
            1: "text-3xl",
            2: "text-2xl",
            3: "text-xl",
            4: "text-lg",
            5: "text-base",
            6: "text-sm"
        }
        
        size_class = size_classes.get(level, "text-base")
        html.append(f"<{tag} id='{anchor}' class='{size_class} font-bold mt-8 mb-4 border-b border-blue-600 pb-2'>{name}</{tag}>")

    if "content" in section:
        for block in section.get("content") or []:
            if isinstance(block, dict):
                if block.get("type") == "text" and "value" in block:
                    text = render_text(block["value"], context)
                    # ФИКС: Используем переменную для замены символов
                    processed_text = text.replace('\n', '<br>')
                    html.append(f"<p class='mb-4 text-gray-800 leading-relaxed'>{processed_text}</p>")
                elif block.get("type") == "blank_line":
                    html.append("<br>" * block.get("count", 1))
                elif block.get("type") == "bottom_info" and "value" in block:
                    value = render_text(block["value"], context)
                    html.append(f"<p class='text-gray-800 mt-8'>{value}</p>")

    if "blocks" in section:
        for block in section.get("blocks") or []:
            if isinstance(block, dict):
                if "text" in block:
                    text = render_text(block["text"], context)
                    # ФИКС: Используем переменную
                    processed_text = text.replace('\n', '<br>')
                    html.append(f"<p class='mb-4 text-gray-800'>{processed_text}</p>")
                elif "list" in block:
                    if block["list"].get("style") == "no_bullet":
                        html.append("<ul class='list-none pl-0 mb-4 space-y-1'>")
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

    # subsections - безопасно обрабатываем
    subsections = section.get("subsections")
    if subsections:
        for sub in subsections:
            html.append(simple_render_section(sub, level + 1, context))

    # points - безопасно обрабатываем
    points = section.get("points")
    if points:
        for point in points:
            html.append(simple_render_section(point, level + 1, context))

    return "\n".join(html)

def generate_toc(sections: list, context: dict) -> str:
    """Генерирует HTML-оглавление только с разделами верхнего уровня"""
    if not sections:
        return "<p class='text-gray-400 py-4'>Оглавление отсутствует</p>"

    html = ['<ul class="space-y-3">']

    for section in sections:
        if not isinstance(section, dict):
            continue
        
        name = render_text(section.get("name", ""), context)
        if not name or not name.strip() or name == "  ":
            continue
            
        # Пропускаем технические разделы без значимых названий
        if len(name) > 100 or ":" in name or ";" in name:  # Пропускаем слишком длинные и списковые названия
            continue
            
        anchor = section.get("id", name.lower().replace(" ", "-").replace(".", ""))
        html.append(f'<li><a href="#{anchor}" class="toc-link hover:text-blue-400 transition-all py-2 px-3 rounded-lg block bg-gray-800/50 hover:bg-gray-700/50 border border-gray-700 hover:border-blue-600 font-medium">{name}</a></li>')

    html.append('</ul>')
    return "\n".join(html)

def has_tag(section: dict, tag: str) -> bool:
    """Проверяет, имеет ли секция нужный тег"""
    tags = section.get("site", [])
    if isinstance(tags, list):
        return tag in tags
    return False

def filter_sections_by_tag(sections: list, target_tag: str, preserve_structure: bool = False):
    """
    Фильтрует секции по тегу.
    Секции с пустым тегом site: [] не включаются НИКОГДА.
    Если у родителя нет тега, но есть дочерние с тегом, дочерние добавляются без родителя.
    """
    if sections is None:
        return []
    
    result = []
    
    for section in sections:
        if not isinstance(section, dict):
            continue
            
        section_id = section.get("id", "")
        
        if section_id in ["title_page", "table_of_contents"]:
            continue
        
        section_tags = section.get("site", [])
        has_target = target_tag in section_tags if isinstance(section_tags, list) else False
        
        # Обработка дочерних элементов
        subsections = section.get("subsections")
        points = section.get("points")
        
        processed_subsections = filter_sections_by_tag(subsections, target_tag, preserve_structure) if subsections is not None else []
        processed_points = filter_sections_by_tag(points, target_tag, preserve_structure) if points is not None else []
        
        # Проверяем, есть ли дочерние элементы с нужным тегом
        has_children_with_tag = bool(processed_subsections or processed_points)
        
        # Решаем, что делать с этой секцией
        if has_target:
            # Секция имеет целевой тег - включаем её
            new_section = section.copy()
            if processed_subsections:
                new_section["subsections"] = processed_subsections
            elif "subsections" in new_section:
                del new_section["subsections"]
                
            if processed_points:
                new_section["points"] = processed_points
            elif "points" in new_section:
                del new_section["points"]
            result.append(new_section)
        elif has_children_with_tag:
            # Секция не имеет тега, но имеет дочерних с тегом
            # Добавляем дочерних напрямую в результат (без родителя)
            if processed_subsections:
                result.extend(processed_subsections)
            if processed_points:
                result.extend(processed_points)
        # Если нет тега и нет дочерних с тегом - не включаем ничего
    
    return result

def copy_pdfs_to_web():
    """Копирует PDF файлы в web папку для локальной разработки"""
    pdf_source = CONFIG["pdf_dir"]
    pdf_dest = CONFIG["web_output"] / "pdf"
    
    if not pdf_source.exists():
        print(f"⚠️ Исходная папка PDF не найдена: {pdf_source}")
        return False
    
    # Создаем папку назначения
    pdf_dest.mkdir(parents=True, exist_ok=True)
    
    # Копируем PDF файлы
    pdf_files = list(pdf_source.glob("*.pdf"))
    if not pdf_files:
        print(f"⚠️ PDF файлы не найдены в: {pdf_source}")
        return False
    
    copied_count = 0
    for pdf_file in pdf_files:
        dest_file = pdf_dest / pdf_file.name
        try:
            import shutil
            shutil.copy2(pdf_file, dest_file)
            copied_count += 1
            print(f"✅ Скопирован: {pdf_file.name}")
        except Exception as e:
            print(f"❌ Ошибка копирования {pdf_file.name}: {e}")
    
    print(f"📄 Скопировано {copied_count} PDF файлов в {pdf_dest}")
    return copied_count > 0

def build_site():
    """Основная функция генерации сайта"""
    try:
        print("\n" + "=" * 60)
        print("🚀 НАЧИНАЮ ГЕНЕРАЦИЮ САЙТА ДОКУМЕНТАЦИИ")
        print("=" * 60)
        
       
        print("\n📦 ПОДГОТОВКА ВЫХОДНОЙ ДИРЕКТОРИИ")
        clean_output()
        copy_media()
        print("✅ Выходная директория подготовлена")

        # КОПИРУЕМ PDF ДЛЯ ЛОКАЛЬНОЙ РАЗРАБОТКИ
        print("\n📄 Подготовка PDF файлов...")
        copy_pdfs_to_web()
        
        print("\n📖 ЗАГРУЗКА ДАННЫХ")
        
        metadata = load_metadata()
        print(f"✅ Метаданные загружены: {metadata.get('product', {}).get('name', 'N/A')}")
        
        spec = load_yaml(CONFIG["data_files"]["specification"])
        print(f"✅ Спецификации загружены: {len(spec.get('specifications', {}))} пунктов")
        
        re_data = load_yaml(CONFIG["content_yaml"]["re"])
        api_data = load_yaml(CONFIG["content_yaml"]["api"])
        
        # Отладочная информация о структуре данных
        print(f"\n📊 АНАЛИЗ СТРУКТУРЫ ДАННЫХ:")
        print(f"   re_data keys: {list(re_data.keys())}")
        print(f"   api_data keys: {list(api_data.keys())}")
        
        if "sections" in re_data:
            re_sections = re_data["sections"]
            print(f"   ✅ Руководство по эксплуатации: {len(re_sections)} разделов")
            print(f"   📋 Первые 3 раздела:")
            for i, section in enumerate(re_sections[:3]):
                if isinstance(section, dict):
                    print(f"     {i}. id='{section.get('id')}', name='{section.get('name')}'")
                else:
                    print(f"     {i}. НЕ СЛОВАРЬ: {type(section)}")
        else:
            print(f"   ❌ В re_data нет ключа 'sections'")
            re_sections = []
        
        if "sections" in api_data:
            api_sections = api_data["sections"]
            print(f"   ✅ Разработчикам: {len(api_sections)} разделов")
        else:
            print(f"   ❌ В api_data нет ключа 'sections'")
            api_sections = []
        
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
        
        print("\n🎭 ЗАГРУЗКА ШАБЛОНОВ")
        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(CONFIG["templates_dir"]),
            autoescape=True,
        )
        
        template = env.get_template("site_template.html")
        print("✅ Шаблон сайта загружен")
        
        print("\n🏠 ГЕНЕРАЦИЯ ГЛАВНОЙ СТРАНИЦЫ")
        index_ctx = context.copy()
        index_ctx.update({
            "title": "Документация САСП-2",
            "page_id": "index",
            "toc": "",
            "content": ""  # Пустой контент - всё будет в шаблоне
        })
        render_page(template, index_ctx, CONFIG["web_output"] / "index.html")
        print("✅ Главная страница сгенерирована")
        
        print("\n📘 ГЕНЕРАЦИЯ РУКОВОДСТВА ПОЛЬЗОВАТЕЛЯ (r)")
        re_sections = re_data.get("sections") or []
        user_sections = filter_sections_by_tag(re_sections, "r", preserve_structure=False)
        
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
        
        print("\n🔧 ГЕНЕРАЦИЯ РУКОВОДСТВА ПО ОБСЛУЖИВАНИЮ (m)")
        maint_sections = filter_sections_by_tag(re_sections, "m", preserve_structure=True)
        
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
        
        print("\n🔌 ГЕНЕРАЦИЯ РАЗРАБОТЧИКАМ")
        api_sections = api_data.get("sections") or []
        api_ctx = context.copy()
        api_ctx.update({
            "title": "Разработчикам",
            "page_id": "api",
            "toc": generate_toc(api_sections, context),
            "content": "<div class='content'>" + 
                      "\n".join(simple_render_section(s, context=context) for s in api_sections) + 
                      "</div>"
        })
        render_page(template, api_ctx, CONFIG["web_output"] / "api.html")
        print(f"✅ Разработчикам: {len(api_sections)} разделов")
        
        print("\n📄 ГЕНЕРАЦИЯ СТРАНИЦЫ PDF ДОКУМЕНТОВ")
        available_pdfs = get_available_pdfs()
        
        if available_pdfs:
            pdf_content = "<h3 class='text-2xl font-bold neon mb-12'>ГОСТ-документация</h3>"
            pdf_content += "<div class='grid grid-cols-1 md:grid-cols-3 gap-8'>"
            
            for name, fname in available_pdfs:
                safe_name = name.replace("'", "&apos;").replace('"', "&quot;")
                
                # Прямая ссылка на PDF (браузер откроет его)
                pdf_url = f"pdf/{fname}"
                
                pdf_content += f"""
                <div class="bg-gray-800/50 p-6 rounded-xl border border-blue-700 hover:border-blue-500 transition-all">
                    <div class="text-center">
                        <div class="w-12 h-12 bg-blue-600/20 rounded-lg flex items-center justify-center mx-auto mb-4">
                            <svg class="w-6 h-6 text-blue-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path>
                            </svg>
                        </div>
                        <h5 class="font-semibold mb-2">{safe_name}</h5>

                        <button data-pdf="pdf/{fname}" 
                                data-name="{safe_name}"
                                class="w-full py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors">
                            Открыть для просмотра
                        </button>
   
                    </div>
                </div>
                """
            
            pdf_content += "</div>"
            print(f"✅ Найдено PDF документов: {len(available_pdfs)}")
        else:
            pdf_content = "<h3 class='text-4xl font-bold neon mb-12'>ГОСТ-документация</h3>"
            pdf_content += """
            <div class="text-center py-12">
                <div class="inline-block p-6 bg-gray-800/50 rounded-2xl">
                    <div class="text-6xl mb-4">📄</div>
                    <p class="text-gray-400 text-lg">PDF-документы не сгенерированы</p>
                    <p class="text-gray-500 text-sm mt-2">Запустите сборку документации</p>
                </div>
            </div>
            """
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
        
        print("\n" + "=" * 60)
        print("✅ САЙТ УСПЕШНО СГЕНЕРИРОВАН")
        print("=" * 60)
        
        html_files = list(CONFIG["web_output"].glob("*.html"))
        media_files = list(CONFIG["web_output"].rglob("*.*"))
        
        print(f"\n📊 СТАТИСТИКА:")
        print(f"   • HTML страниц: {len(html_files)}")
        print(f"   • Медиа файлов: {len(media_files) - len(html_files)}")
        print(f"   • Всего файлов: {len(media_files)}")
        print(f"\n📁 Выходная директория: {CONFIG['web_output']}")
        
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