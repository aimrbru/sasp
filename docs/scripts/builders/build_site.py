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
# КОНФИГУРАЦИЯ
# ──────────────────────────────────────────────────────────────────────────────

# ИСПРАВЛЕННЫЙ БЛОК: Работает и локально, и в GitHub Actions

from pathlib import Path

if 'GITHUB_WORKSPACE' in os.environ:
    PROJECT_ROOT = Path(os.environ['GITHUB_WORKSPACE'])
else:
    # Для builders/build_site.py нужно на один уровень выше
    PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent

print(f"PROJECT_ROOT установлен: {PROJECT_ROOT}")

CONFIG = {
    "web_output": PROJECT_ROOT / "docs" / "output" / "web",
    "pdf_dir": PROJECT_ROOT / "docs" / "output" / "pdf",
    "media_src": PROJECT_ROOT / "docs" / "media",
    "media_dest": PROJECT_ROOT / "docs" / "output" / "web" / "media",
    "templates_dir": PROJECT_ROOT / "docs" / "templates" / "web",

    "data_files": {
        "general": PROJECT_ROOT / "hardware" / "general_info.yaml",
        "spec": PROJECT_ROOT / "hardware" / "specification.yaml",
    },
    "content_yaml": {
        "re": PROJECT_ROOT / "docs" / "content" / "re_content.yaml",
        "api": PROJECT_ROOT / "docs" / "content" / "api_content.yaml",
    },
}

# ──────────────────────────────────────────────────────────────────────────────
# УТИЛИТЫ
# ──────────────────────────────────────────────────────────────────────────────

def load_metadata():
    """Загружает и обрабатывает метаданные из general_info.yaml"""
    meta_path = PROJECT_ROOT / "hardware" / "general_info.yaml"
    if not meta_path.exists():
        print(f"⚠️ Файл метаданных не найден: {meta_path}")
        return {}
    
    try:
        with open(meta_path, "r", encoding="utf-8") as f:
            metadata = yaml.safe_load(f)
        
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
    if not path.exists():
        print(f"⚠️ Файл не найден: {path}")
        return {}
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
            return data if isinstance(data, dict) else {}
    except Exception as e:
        print(f"Ошибка чтения YAML {path}: {e}")
        return {}

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
    clean_output()
    copy_media()
    
    # Загружаем метаданные
    metadata = load_metadata()
    
    general = load_yaml(CONFIG["data_files"]["general"])
    spec = load_yaml(CONFIG["data_files"]["spec"])
    re_data = load_yaml(CONFIG["content_yaml"]["re"])
    api_data = load_yaml(CONFIG["content_yaml"]["api"])
    
    # Объединяем метаданные с существующим контекстом
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

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(CONFIG["templates_dir"]),
        autoescape=True,
    )
    try:
        template = env.get_template("site_template.html")
    except jinja2.TemplateNotFound:
        print("Ошибка: шаблон site_template.html не найден в", CONFIG["templates_dir"])
        return

    # Главная
    index_ctx = context.copy()
    index_ctx.update({
        "title": "Документация САСП-2",
        "page_id": "index",
        "toc": "",  # на главной оглавления нет
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

    # Руководство пользователя (с исключениями)
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
        "toc": generate_toc(user_sections, context),  # оглавление только из оставшихся разделов
        "content": "<div class='content'>" + \
                "\n".join(simple_render_section(s, context=context) for s in user_sections) + \
                "</div>"
    })
    render_page(template, user_ctx, CONFIG["web_output"] / "user_guide.html")

    # Обслуживание
    allowed = {"maintenance", "general_instructions", "maintenance_purpose", "maintenance_executors", "disassembly_warning", "personnel_requirements", "safety_measures", "safety_rules", "maintenance_procedure", "maintenance_steps", "maintenance_features", "maintenance_check", "maintenance_methods", "technical_inspection", "inspection_frequency", "inspection_program", "conservation", "conservation_procedure", "deconservation_procedure", "current_repair", "general_repair_instructions", "repair_safety", "repair_safety_rules", "storage", "storage_warehousing", "storage_rules", "warehousing", "limited_life_parts", "storage_conditions", "transportation", "transport_requirements", "transport_preparation", "transport_characteristics", "transport_handling", "disposal", "safety_disposal", "disposal_safety_rules", "disposal_prohibitions", "preparation_disposal", "disposal_preparation", "disposable_parts", "parts_for_disposal", "methods_disposal", "disposal_methods", "organizations_disposal", "disposal_organizations"}
    maint_sections = [s for s in re_sections if s.get("id") in allowed]
    maint_ctx = context.copy()
    maint_ctx.update({
        "title": "Руководство по обслуживанию",
        "page_id": "maintenance",
        "toc": generate_toc(maint_sections, context),
        "content": "<div class='content'>" + \
                   "\n".join(simple_render_section(s, context=context) for s in maint_sections) + \
                   "</div>"
    })
    render_page(template, maint_ctx, CONFIG["web_output"] / "maintenance.html")

    # API
    api_sections = api_data.get("sections") or []
    api_ctx = context.copy()
    api_ctx.update({
        "title": "Справочник по API",
        "page_id": "api",
        "toc": generate_toc(api_sections, context),
        "content": "<div class='content'>" + \
                   "\n".join(simple_render_section(s, context=context) for s in api_sections) + \
                   "</div>"
    })
    render_page(template, api_ctx, CONFIG["web_output"] / "api.html")

    # ГОСТ / PDF
    available_pdfs = get_available_pdfs()
    pdf_content = "<h1 class='text-4xl font-bold neon mb-12'>Нормативная документация</h1>"
    if available_pdfs:
        pdf_content += "<div class='grid grid-cols-1 md:grid-cols-3 gap-8'>"
        for name, fname in available_pdfs:
            pdf_content += f"""
            <a href="pdf/{fname}" target="_blank" class="bg-gray-800/50 backdrop-blur-sm p-8 rounded-2xl border border-blue-700 hover:border-blue-500 transition-all shadow-lg hover:shadow-blue-500/20">
                <h3 class="text-2xl font-semibold mb-4">{name}</h3>
                <p class="text-gray-400">Открыть PDF</p>
            </a>
            """
        pdf_content += "</div>"
    else:
        pdf_content += "<p class='text-gray-400 text-center'>PDF-документы отсутствуют</p>"

    pdf_ctx = context.copy()
    pdf_ctx.update({
        "title": "ГОСТ / Нормативные документы",
        "page_id": "standards",
        "toc": "",  # на ГОСТ оглавления нет
        "content": pdf_content
    })
    render_page(template, pdf_ctx, CONFIG["web_output"] / "standards.html")

    print(f"\nСайт успешно сгенерирован в: {CONFIG['web_output']}")
    print("Откройте index.html в браузере или запустите:")
    print("  python -m http.server --directory docs/output/web")

def render_page(template, context, path: Path):
    html = template.render(**context)
    path.write_text(html, encoding="utf-8")

if __name__ == "__main__":
    try:
        build_site()
    except Exception as e:
        print(f"Ошибка сборки: {e}")
        exit(1)