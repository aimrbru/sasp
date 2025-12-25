//software/src/main/home_html.cpp

// HTML-ui
const char *home_html = R"html(
<!DOCTYPE html>
<html lang="ru">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OCR Meter Reader</title>
    <link rel="icon" type="image/png"
        href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC8AAAAnCAYAAACfdBHBAAAABHNCSVQICAgIfAhkiAAABTlJREFUWIXFmF1sVEUUx38zd9vubukHFPmoCvELMFFMjFEwEUM08UElPEiEB0WjEDVGE0IwSow1xBgD8YXEKJhoGsUPBI0JyAvGGKLEWH0AFFSCVKRgkbZru7t3d+8cH3Zp996d297trvX/dmfOnPndM2fOzL2KqFr/VhtX3biZhvgamhKzABV5LEqhHQelqhgTkOcVcId7cdPdnD64lR1dab+zFS/MZum983BzGbqWHx1tf2hrMzfdtZ9k67JJT14viQjDF3fx/ba1MQCe37uUjnmvEG9ZgtJNDA9+Adw3OmDRkqdJtNzxP+H6pZRi2vTVLHz47RibP7uduYsOoGMtYxYivgGJlhU1LXm9pbRDc8cKTVtnlx+cymxuam6bOrKISrZeF6MxsdTSJZY2i5UY0qnDKP0rjpp4jCcK48UmdmwU4rWTaL+NWEOH1UTpRIyGeHMUTGvryFA3m255DDARfFSvp96Zw9WLd5FsW17Rpxyj7bkcbAoJan/vHv4rcIA3Hj3H6SPr8LwRW3cMEbG8gJ9WlFirejqVKX/8lCuvaUbNufRsEJ1HnGp4E4h7iMyxLi6mANj+yEm29Zwi2XaDz1BBhPwrYoCFQZnRlzzI/I2X42xxUPFqYIMSRFbSePIKmu5+nL7TAHiFbKWhiLZ6UBV5Ys8bUyypq5jeNhfnpVrBi1Mr1Yy+dgGN68sbbbZ2+KCtCcl5XeyYhbRraJoEa6gS6IWjD2Lblzpi2lSuRFFeMfKVawoGyXuQjuBdAzgQ16iGsinLA1sJ7yB2eAnChpXwgin6URI0GqDw5RaG1kxE3opxADbQsbMdZ6XVKCRtYqVJJzj6JSTyzjgHk3b3MTQwvt8xPcfMXGin0tZyHnXD2lXK+TzW07Wq+q8CAZTylRQJ27CWeYMvasIiH7aTwYStVoiC8BNKCCmVlZ7tILoImLNEQGqMPL7VtN9otRWrYsOGcHh6nOiqKuHtqVHqrKXOh0DqnAA4ZCr6PUyV8DqY8+WdNnjRQbvSyKoinyFpgYlwRfbZBzesmTBtIt5twnLelCKvKta8Fb3sEPP3BxAL5b4URkptCOrW8NlD4W3VJphNIUEs1XmPjIFWX1cS3ZlEd4YCAdbL3qjKVk7b6nxYzqtAmatIo0uji5H/iZGLOWRwXM4qlUfOj81v2cxiIpbKsJotxYpyDHJnKTzjYs4bpKaPE0HExZw9h7dzrNUSeJGQnJdAdZEQIM8btbuHPz5axWX7niC5wMXMzGIaJ8HONJzMUQZ/3EDq4mijttC7I8N2+OB6uJm/iE+z2PnvNrvpH94NP1SPPJEsGzaXPmIvlRWGub3W6Ovw60FdFSxmefcCvUffs+e8BGr05+/vItX/LhLI/cIUwZcfYHm3n/7etbz55O/R6nzPjjz3d67jzM3dzJj7AF7heqCZnGv9qq+7hi98g5lxmJGBw/zc8yEfbLwAoNh+Io/j+F8im/6YDYsfnBKwGmS/Ehfc2VOPUr0U24/ncGINvlYxef7+cxPnTu2hvzdPrEEoDJdt2GEoGEM6639z1/Nv6n9GhLjrbxtMCUOux5lvff986gcPxf/ghVwaTKF0wgarTcV3a8nG1lY+pZB3h/jukzvZ/Wp/LfDhG1YpRUNTlP+Yk1GcbK7mSqXJu6l60FQlxQBnvxqq1Y0ml91XD57IEhFGUgfo6cnX6krTd+JFctmT9eCKpLzbx5njr9XDVfHkerZ7Hp2LumhsWkljst3319gYD/HGvymKGIzxxrMgnx0Avqbvt5d5ffUvdWDnX9fQB23ulnHZAAAAAElFTkSuQmCC">
</head>

<style>
    /* Общие стили */
    body {
        width: 100%;
        font-family: Arial, sans-serif;
        background-color: #F5F5F5;
        color: #333333;
        margin: 0;
        padding: 0;
        display: grid;
        place-items: center;
    }

    /* Кнопки */
    button {
        color: rgb(255, 255, 255);
        background-color: #5490b8;
        border: none;
        border-radius: 5px;
        cursor: pointer;
        transition: background-color 0.3s ease;
        font-size: 1rem;
        width: 75px;
        height: 30px;
        display: flex;
        justify-content: center;
        align-items: center;
        text-align: center;
        font-weight: bold;
    }

    button:hover {
        background-color: #346485;
    }

    /* Типографика */
    a {
        text-decoration: none;
        color: #5490b8;
        font-size: 1rem;
        cursor: pointer;
        transition: color 0.3s ease;
        overflow-wrap: break-word;
        white-space: normal;

    }

    a:hover {
        color: #004da0;
    }

    .small-p {
        font-size: 0.8rem !important;

    }

    /* Контейнеры и блоки */
    .container {
        display: flex;
        justify-content: center;
        gap: 100px;
        width: 100%;
        max-width: 600px;
        box-sizing: border-box;
        flex-wrap: nowrap;
        align-items: center;
        position: relative;
        margin: 15px 0 15px 0;
    }

    #cameraSettingsLink {
        position: absolute;
        right: 0;
        /* Прижимаем к правому краю */
        font-size: 1.2rem;
        text-decoration: none;
    }

    .device-box {
        display: flex;
        justify-content: center;
        gap: 20px;
        width: 100%;
        max-width: 600px;
        margin: 10px auto;
        box-sizing: border-box;
        flex-wrap: nowrap;
        margin-bottom: 10px;
        margin-top: 5px;
    }

    .device-data {
        width: 100%;
        max-width: 300px;
        min-width: 150px;
        background-color: #ffffff;
        border-radius: 5px;
        box-sizing: border-box;
        padding: 9px;
        font-size: 1rem;
        flex-direction: column;
        justify-content: space-between;
        height: 100%;

    }

    .device-data p {
        display: flex;
        align-items: center;
        margin: 5px 5px;
        line-height: 1;
        color: #8a8a8a;
        font-size: 1rem;
        overflow-wrap: anywhere;
        white-space: normal;

    }


    /* Стили для изображений */

    .image {
        display: flex;
        aspect-ratio: 3 / 1;
        width: 100%;
        border-radius: 5px;
        align-items: center;
        justify-content: center;
        margin-bottom: 0px;
        margin-top: 0px;
        background-color: #ffffff;

    }

    .image img {
        max-width: 100%;
        max-height: 100%;
        object-fit: contain;
        border-radius: 5px;
        margin-bottom: 0px;
        margin-top: 0px;

    }

    /* Модальные окна */
    .modal {
        display: none;
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.5);
        justify-content: center;
        align-items: center;
        z-index: 999;
        opacity: 0;
        transition: opacity 0.3s ease;
    }

    .modal.show {
        display: flex;
        opacity: 1;
    }

    .modal-title {
        margin: 20px;
        font-size: 14px;
        color: #333;
        font-weight: bold;
    }

    .modal-content {
        background-color: white;
        padding: 20px;
        margin: 0 20px;
        border-radius: 5px;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        max-width: 250px;
        width: 100%;
        text-align: center;
        display: flex;
        flex-direction: column;
    }

    .modal-actions {
        display: flex;
        justify-content: space-between;
        width: 100%;
        margin-top: 30px;
    }

    .modal-input {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        font-size: 16px;
        color: #333;
        padding: 8px;
        border: 1px solid #ccc;
        width: 150px;
        height: 25px;
        box-sizing: border-box;
    }

    .modal-textarea {
        height: 50px;
        white-space: normal;
        word-wrap: break-word;
        overflow-wrap: break-word;
        resize: both;
    }

    .modal-select {
        font-size: 14px;
        color: #333;
        border: 1px solid #ccc;
        width: 120px;
        height: 25px;
        box-sizing: border-box;
        margin-bottom: 10px;
    }

    /* Вспомогательные элементы */
    .switch-container {
        display: flex;
        align-items: center;
    }

    .switch {
        display: inline-block;
        position: relative;
        width: 42px;
        height: 18px;
        margin-left: 5px;
    }

    .switch input {
        opacity: 0;
        width: 0;
        height: 0;
    }

    .slider {
        position: absolute;
        cursor: pointer;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background-color: #ccc;
        transition: 0.4s;
        border-radius: 34px;
    }

    .slider:before {
        position: absolute;
        content: "";
        height: 13px;
        width: 13px;
        left: 3px;
        bottom: 3px;
        background-color: white;
        transition: 0.4s;
        border-radius: 50%;
    }

    input:checked+.slider {
        background-color: #2196F3;
    }

    input:checked+.slider:before {
        transform: translateX(22px);
    }

    /* Логирование */
    #logs {
        width: 100%;
        height: 185px;
        overflow-y: auto;
        background-color: #ffffff;
        border: 1px solid #ccc;
        padding: 2px;
        box-sizing: border-box;
        font-family: monospace;
        white-space: pre-wrap;
        word-wrap: break-word;
        line-height: 1;
        font-size: 0.8rem;
        margin-bottom: 0px;
        margin-top: 0px;
    }

    /* Медиазапросы */
    @media (max-width: 375px) {
        a {
            font-size: 0.75rem;
        }

        .device-data {
            font-size: 0.75rem;
            padding: 1px;
        }

        .container {
            gap: 10px;
            max-width: 320px;
        }

        .small-p {
            font-size: 0.6rem !important;
        }
    }

    @media (max-width: 400px) {
        a {
            font-size: 0.9rem;
        }

        .small-p {
            font-size: 0.7rem !important;
        }

        button {
            font-size: 0.85rem;
            width: 70px;
            height: 28px;
        }

        .container {
            margin: 5px 0 8px 0;
            gap: 50px;
            max-width: 360px;
        }

        .switch {
            width: 32px;
            height: 15px;
        }

        .slider::before {
            height: 11px;
            width: 11px;
            left: 2px;
            bottom: 2px;
        }

        input:checked+.slider::before {
            transform: translateX(16px);
        }

        .device-box {
            margin-bottom: 5px;
            gap: 5px;
        }

        .device-data {
            padding: 0px;
            margin: 0 5px;
        }

        .device-data p {
            font-size: 0.9rem;
            padding: 0px;
        }

        .image img {
            max-width: 100%;
            height: auto;
            object-fit: contain;
        }

        #logs {
            font-size: 0.6rem;
            height: 140px;
        }
    }

    @media (max-width: 600px) {
        .device-box {
            max-width: 550px;
            /* Чтобы не растягивалось слишком сильно */
        }

        .device-data {
            font-size: 0.9rem;
            /* Чуть больше шрифт */
        }
    }

    @media (max-width: 768px) {
        .device-box {
            max-width: 700px;
        }

        .device-data {
            font-size: 1rem;
            /* Оптимальный размер */
        }
    }
</style>

</head>

<body>
    <!-- Основной контент -->

    <div class="container">
        <button onclick="takeInitImage()">Init</button>
        <button id="testButton">Action</button>
        <a href="#" id="cameraSettingsLink">⚙</a>
    </div>

    <div class="device-box" id="CommonSettings">
        <div class="device-data">
            <p>OCR enabled:<label class="switch"><input type="checkbox" id="ocrEnabledSwitch"><span
                        class="slider"></span></label></p>
            <p>Sleep enabled:<label class="switch"><input type="checkbox" id="sleepEnabledSwitch"><span
                        class="slider"></span></label></p>
            <p><a href="#" id="sleepDurationLink">Sleep (sec.): </a> <span id="sleep_seconds"></span></p>
        </div>
        <div class="device-data">
            <p>Copy to server:<label class="switch"><input type="checkbox" id="copyToServerSwitch"><span
                        class="slider"></span></label></p>
            <p><a href="#" id="serverPathLink">Server path:</a>&nbsp;<span class="small-p" id="server_path"></span>
            </p>
        </div>
    </div>

    <div class="device-box">
        <div class="device-data" id="device1Data">
            <p><a href="#" id="device1SettingsLink">Meter</a>&nbsp<span id="device1_id"></span></p>
            <p id="device1_type"></p>
            <p class="small-p"><label>(⇱</label><span id="device1_x1_y1">&nbsp</span><label> ▧</label><span
                    id="device1_size"></span></p>
        </div>
        <div class="device-data" id="device2Data">
            <p><a href="#" id="device2SettingsLink">Meter</a>&nbsp<span id="device2_id"></span></p>
            <p id="device2_type"></p>
            <p class="small-p"><label>(⇱</label><span id="device2_x1_y1">&nbsp</span><label> ▧</label><span
                    id="device2_size"></span></p>
        </div>
    </div>

    <div id="imagesMainContainer" class="device-box">
        <div class="device-data">
            <div class="image"><img id="image1"></div>
        </div>
        <div class="device-data">
            <div class="image"><img id="image2"></div>
        </div>
    </div>

    <div class="device-box">
        <div class="device-data" id="device1_data"></div>
        <div class="device-data" id="device2_data"></div>
    </div>

    <canvas class="container" id="imageCanvas" width="1600" height="600"></canvas>

    <div class="container">
        <pre id="logs"></pre>
    </div>

    <!-- Модальное окно -->
    <div id="modal" class="modal">
        <div class="modal-content">
            <h4 id="modalTitle"></h4>
            <div id="modalBody"></div>
            <div class="modal-actions">
                <button id="modalCancelButton">Cancel</button>
                <button id="modalSaveButton">Save</button>
            </div>
        </div>
    </div>

    <script>
        // Глобальные переменные
        // Переменные для изображений
        let canvas, ctx, img;
        // Переменные для прямоугольника
        let startRegionX, startRegionY; // Начальные координаты прямоугольника
        let dragOffsetX, dragOffsetY;   // Смещение внутри прямоугольника при захвате
        // Переменные для хранения координат областей
        let regions = { 1: null, 2: null };
        let currentRegion = 1;
        // Флаги для перемещения и изменения размеров
        let dragging = false, resizing = false;
        let startX, startY;

        // Объект соответствия значений и заголовков для device_type
        const deviceTypeLabels = {
            hot: "Hot water",
            cold: "Cold water",
            electric: "Electric"
        };

        // Функция обновления текста с учетом заголовка для device1_type
        function updateDeviceType(id, typeValue) {
            const label = deviceTypeLabels[typeValue] || typeValue; // Если нет в объекте, оставить как есть
            updateElementText(id, label);
        }

        // Вспомогательная функция для обновления текстового содержимого элемента
        function updateElementText(id, text) {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = text;
            }
        }

        // Вспомогательная функция для обновления состояния чекбокса
        function updateElementChecked(id, checked) {
            const element = document.getElementById(id);
            if (element && element.type === 'checkbox') {
                element.checked = checked;
            }
        }

        // Инициализация при загрузке страницы
        document.addEventListener("DOMContentLoaded", () => {
            // Инициализация canvas и контекста
            canvas = document.getElementById('imageCanvas');
            if (!canvas) {
                console.error("Canvas element not found!");
                return;
            }
            ctx = canvas.getContext('2d');
            if (!ctx) {
                console.error("Failed to get 2D context for canvas!");
                return;
            }
            // Очистка канваса (если это необходимо)
            if (ctx) {
                ctx.clearRect(0, 0, canvas.width, canvas.height);
            }

            // Инициализация объекта Image
            img = new Image();

            // Обработка событий мыши 
            canvas.addEventListener("mousedown", handleMouseDown);
            canvas.addEventListener("mousemove", handleMouseMove);
            canvas.addEventListener("mouseup", handleMouseUp);
            // Обработка событий  тачскрина
            canvas.addEventListener("touchstart", (e) => {
                e.preventDefault(); // Предотвращаем прокрутку страницы
                handleMouseDown(e);
            });
            canvas.addEventListener("touchmove", (e) => {
                e.preventDefault(); // Предотвращаем прокрутку страницы
                handleMouseMove(e);
            });
            canvas.addEventListener("touchend", handleMouseUp);

            // Инициализация обработчиков для всех переключателей
            setupSwitchHandlers();

            // Загрузка настроек с сервера
            fetch("/load_settings")
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! Status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    // Инициализация данных для Device 1
                    updateElementText('device1_id', data.device1_id);
                    updateDeviceType('device1_type', data.device1_type);
                    const device1_dx = data.device1_x2 - data.device1_x1;
                    const device1_dy = data.device1_y2 - data.device1_y1;
                    updateElementText('device1_x1_y1', `${data.device1_x1}:${data.device1_y1}`);
                    updateElementText('device1_size', device1_dx && device1_dy ? `${device1_dx}x${device1_dy})` : ")");


                    // Инициализация данных для Device 2
                    updateElementText('device2_id', data.device2_id);
                    updateDeviceType('device2_type', data.device2_type);
                    const device2_dx = data.device2_x2 - data.device2_x1;
                    const device2_dy = data.device2_y2 - data.device2_y1;
                    updateElementText('device2_x1_y1', `${data.device2_x1}:${data.device2_y1}`);
                    updateElementText('device2_size', device2_dx && device2_dy ? `${device2_dx}x${device2_dy})` : "");

                    // Инициализация данных для Common Settings
                    updateElementChecked('ocrEnabledSwitch', data.ocr_enabled);
                    updateElementChecked('sleepEnabledSwitch', data.sleep_enabled);
                    updateElementText('sleep_seconds', data.sleep_seconds);
                    updateElementChecked('copyToServerSwitch', data.copy_to_server);
                    updateElementText('server_path', data.server_path || "Not set");
                    updateCopyToServerSwitchState(); // Обновляем состояние переключателя после загрузки данных
                    // Настройки камеры
                    window.cameraSettings = { agc_gain: data.agc_gain, aec_value: data.aec_value, flash_duty: data.flash_duty };
                })
                .catch(error => {
                    console.error("Ошибка проверки настроек:", error);
                    alert("Не удалось загрузить настройки. Попробуйте позже.");
                });

            takeInitImage(false);

        });

        // Загрузка изображения
        function takeInitImage(drawRectangle = true) {
            fetch("/take_init_image")
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.blob();
                })
                .then(imageBlob => {
                    const imageUrl = URL.createObjectURL(imageBlob);
                    img = new Image(); // Убедитесь, что img создан
                    img.src = imageUrl;
                    img.onload = () => {
                        ctx.clearRect(0, 0, canvas.width, canvas.height);
                        ctx.drawImage(img, 0, 0, canvas.width, canvas.height);

                        if (drawRectangle) {
                            // Добавляем начальный прямоугольник
                            regions[currentRegion] = { x: 64, y: 64, width: 256, height: 128 };
                            drawRegions();
                            updatePreview();
                        }
                    };
                })
                .catch(error => console.error("Ошибка при получении изображения:", error));
        }


        // Отрисовка областей (прямоугольник)
        function drawRegions() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
            for (let region in regions) {
                if (regions[region]) {
                    const { x, y, width, height } = regions[region];
                    ctx.strokeStyle = "red";
                    ctx.lineWidth = 3;
                    ctx.strokeRect(x, y, width, height);

                    // Рисуем указатель (маленький квадрат) в правом нижнем углу
                    const handleSize = 30; // Размер указателя
                    ctx.fillStyle = "blue"; // Цвет указателя
                    ctx.fillRect(x + width - handleSize / 2, y + height - handleSize / 2, handleSize, handleSize);

                    // Обновляем превью
                    if (region == currentRegion) {
                        updatePreview();
                    }
                }
            }
        }

        function updatePreview() {
            if (!currentRegion || !regions[currentRegion]) {
                console.error("Invalid region or currentRegion is not set.");
                return;
            }
            const region = regions[currentRegion];

            // Проверка корректности координат региона
            if (
                region.x < 0 || region.y < 0 ||
                region.width <= 0 || region.height <= 0 ||
                region.x + region.width > canvas.width ||
                region.y + region.height > canvas.height
            ) {
                console.error("Invalid region coordinates:", region);
                return;
            }

            // Создаем временный canvas для превью
            const previewCanvas = document.createElement('canvas');
            const previewCtx = previewCanvas.getContext('2d');

            // Устанавливаем размеры превью
            previewCanvas.width = region.width;
            previewCanvas.height = region.height;

            // Рисуем вырезанную область на временном canvas
            try {
                previewCtx.drawImage(
                    canvas,
                    region.x, region.y, region.width, region.height,
                    0, 0, previewCanvas.width, previewCanvas.height
                );
            } catch (error) {
                console.error("Error drawing preview image:", error);
                return;
            }

            // Определяем, в какой блок выводить изображение и данные
            const halfCanvasWidth = canvas.width / 2;
            const targetImageId = region.x < halfCanvasWidth ? 'image1' : 'image2';
            const targetDeviceDataId = region.x < halfCanvasWidth ? 'device1_data' : 'device2_data';

            // Очищаем противоположные блоки
            const oppositeImageId = targetImageId === 'image1' ? 'image2' : 'image1';
            const oppositeDeviceDataId = targetDeviceDataId === 'device1_data' ? 'device2_data' : 'device1_data';

            // Очищаем противоположное изображение
            const oppositeImage = document.querySelector(`#${oppositeImageId}`);
            if (oppositeImage) {
                oppositeImage.src = ''; // Очищаем src
            }

            // Очищаем противоположный блок данных
            const oppositeDeviceData = document.getElementById(oppositeDeviceDataId);
            if (oppositeDeviceData) {
                oppositeDeviceData.innerHTML = ''; // Очищаем содержимое
            }

            // Находим существующий элемент <img> или создаём его
            let previewImg = document.querySelector(`#${targetImageId}`);
            if (!previewImg) {
                previewImg = document.createElement('img'); // Создаем новый элемент <img>
                previewImg.id = targetImageId; // Устанавливаем ID
                document.getElementById(targetImageId).appendChild(previewImg); // Добавляем в DOM
            }

            // Обновляем src изображения
            previewImg.src = ''; // Очищаем src
            setTimeout(() => {
                previewImg.src = previewCanvas.toDataURL();
            }, 50);

            // Обновляем src изображения через requestAnimationFrame
            requestAnimationFrame(() => {
                previewImg.src = previewCanvas.toDataURL();
            });

            // Находим целевой элемент данных
            const deviceDataElement = document.getElementById(targetDeviceDataId);
            if (!deviceDataElement) {
                console.error(`Element with ID ${targetDeviceDataId} not found!`);
                return;
            }

            // Очищаем содержимое
            deviceDataElement.innerHTML = ''; // Убираем все предыдущие данные

            // Добавляем ссылку для сохранения координат
            const paragraph = document.createElement('p');
            const saveLink = document.createElement('a');
            saveLink.href = '#';
            saveLink.textContent = 'Save Coordinates';
            saveLink.onclick = () => {
                openSaveCoordinates(region, targetDeviceDataId === 'device1_data' ? 'device1' : 'device2');
            };
            paragraph.appendChild(saveLink);
            deviceDataElement.appendChild(paragraph);

            // Добавляем координаты (x1, y1) и размеры (width × height)
            const coordsParagraph = document.createElement('p');
            coordsParagraph.classList.add('small-p');
            coordsParagraph.innerHTML = `
                        <label>(⇱</label><span>${region.x}</span>:<span>${region.y}</span>
                        <label>&nbsp;▧</label><span>${region.width}</span>×<span>${region.height})</span>
                                        `;
            deviceDataElement.appendChild(coordsParagraph);

        }

        // Проверка, находится ли курсор внутри прямоугольника
        function isInsideRect(x, y, rect) {
            return x > rect.x && x < rect.x + rect.width && y > rect.y && y < rect.y + rect.height;
        }

        // Проверка, находится ли курсор на границе прямоугольника
        function isOnBorder(x, y, rect) {
            const borderArea = 30; // Новый размер области захвата
            return Math.abs(x - (rect.x + rect.width)) < borderArea &&
                Math.abs(y - (rect.y + rect.height)) < borderArea;
        }

        // Функция для расчета масштаба
        function getCanvasScale() {
            const rect = canvas.getBoundingClientRect();
            return {
                scaleX: canvas.width / rect.width,
                scaleY: canvas.height / rect.height
            };
        }

        // Универсальная функция для получения координат
        function getEventCoordinates(e) {
            const rect = canvas.getBoundingClientRect();
            const scale = getCanvasScale(); // Получаем масштаб
            if (e.touches) {
                // Для тачскрина
                const touch = e.touches[0];
                return {
                    x: (touch.clientX - rect.left) * scale.scaleX,
                    y: (touch.clientY - rect.top) * scale.scaleY
                };
            } else {
                // Для мыши
                return {
                    x: (e.clientX - rect.left) * scale.scaleX,
                    y: (e.clientY - rect.top) * scale.scaleY
                };
            }
        }

        // Обработчик нажатия
        function handleMouseDown(e) {
            const { x, y } = getEventCoordinates(e);
            if (regions[currentRegion]) {
                const region = regions[currentRegion];
                if (isOnBorder(x, y, region)) {
                    resizing = true;
                } else if (isInsideRect(x, y, region)) {
                    dragging = true;
                    // Сохраняем начальные координаты прямоугольника
                    startRegionX = region.x;
                    startRegionY = region.y;
                    // Сохраняем смещение внутри прямоугольника относительно курсора
                    dragOffsetX = x - region.x;
                    dragOffsetY = y - region.y;
                }
                startX = x;
                startY = y;
            }

        }

        // Обработчик движения
        function snapToGrid(x, y) {
            const gridSizeX = 8; // Размер ячейки по оси X
            const gridSizeY = 4; // Размер ячейки по оси Y
            return {
                x: Math.round(x / gridSizeX) * gridSizeX,
                y: Math.round(y / gridSizeY) * gridSizeY
            };
        }

        function handleMouseMove(e) {
            const { x, y } = getEventCoordinates(e);
            const snappedCoordinates = snapToGrid(x, y); // Привязка координат мыши к сетке
            if (!dragging && !resizing) {
                // Изменяем курсор в зависимости от положения
                if (regions[currentRegion] && isOnBorder(x, y, regions[currentRegion])) {
                    canvas.style.cursor = "nwse-resize"; // Курсор для изменения размеров
                } else if (regions[currentRegion] && isInsideRect(x, y, regions[currentRegion])) {
                    canvas.style.cursor = "move"; // Курсор для перемещения
                } else {
                    canvas.style.cursor = "default";
                }
                return;
            }
            const region = regions[currentRegion];
            if (dragging) {
                // Рассчитываем новое положение с учетом смещения внутри прямоугольника
                const targetX = x - dragOffsetX;
                const targetY = y - dragOffsetY;
                // Привязываем новое положение к сетке
                region.x = Math.round(targetX / 8) * 8;
                region.y = Math.round(targetY / 4) * 4;
                // Ограничиваем перемещение границами canvas
                region.x = Math.max(0, Math.min(region.x, canvas.width - region.width));
                region.y = Math.max(0, Math.min(region.y, canvas.height - region.height));
            } else if (resizing) {
                // Привязка размера
                region.width = Math.max(128, Math.round((snappedCoordinates.x - region.x) / 16) * 16); // Округление ширины по сетке
                region.height = Math.max(64, Math.round((snappedCoordinates.y - region.y) / 8) * 8); // Округление высоты по сетке
            }
            startX = snappedCoordinates.x;
            startY = snappedCoordinates.y;
            drawRegions();
            updatePreview();

        }

        // Обработчик отпускания
        function handleMouseUp() {
            dragging = resizing = false;
            canvas.style.cursor = "default"; // Возвращаем стандартный курсор
        }

        // обработчик для saveCoordinates
        function openSaveCoordinates(region, deviceName) {
            const { x: x1, y: y1, width, height } = region;

            // Вычисляем x2 и y2
            const x2 = x1 + width;
            const y2 = y1 + height;

            // Формируем контент для модального окна
            const bodyContent = `
        <p><strong>x1:y1=</strong>${x1}:${y1}</p>
        <p><strong>size=</strong>${width}x${height}</p>
    `;
            // Открываем модальное окно
            openModal(
                `Save new position for ${deviceName}`,
                bodyContent,
                async () => {
                    // Формируем данные для отправки
                    const data = {
                        device: deviceName,
                        x1,
                        y1,
                        x2,
                        y2
                    };

                    // Отправляем данные на сервер
                    try {
                        const response = await sendToServer('/save_coordinates', 'POST', data);
                        console.log("Server response:", response);
                        alert("Coordinates saved successfully!");
                    } catch (error) {
                        console.error("Failed to save coordinates:", error);
                        alert("Failed to save coordinates. Please try again.");
                    }
                }
            );
        }


        // Функция для инициализации данных в модальное окно 
        function initializeModalWithData(data) {
            // Проходим по всем ключам в объекте данных
            for (const [key, value] of Object.entries(data)) {
                const inputElement = document.getElementById(`${key}Input`);

                if (inputElement) {
                    inputElement.value = value; // Заполняем соответствующее поле значением
                }
            }
        }

        // Функция открытия модального окна
        function openModal(title, bodyContent, onSave, currentDevice = null) {
            // Устанавливаем заголовок и содержимое
            modalTitle.textContent = currentDevice ? `${title} ${currentDevice}` : title;
            modalTitle.classList.add('modal-title')
            modalBody.innerHTML = bodyContent;

            // Показываем модальное окно
            modal.style.display = 'flex';
            setTimeout(() => modal.classList.add('show'), 10);
            document.body.classList.add('modal-open');  // Используем класс для блокировки прокрутки

            // Обработчики для кнопок "Сохранить" и "Отмена"
            modalSaveButton.onclick = () => {
                onSave();
                closeModal();
            };
            modalCancelButton.onclick = closeModal;

            // Добавляем обработчик клика вне модального окна
            setTimeout(() => {
                document.addEventListener('click', handleOutsideClick);
            }, 100); // Задержка в 100 мс для предотвращения конфликтов
        }


        // Функция для закрытия модального окна
        function closeModal() {
            modal.classList.remove('show');
            setTimeout(() => {
                modal.style.display = 'none';
                document.body.style.overflow = '';
            }, 300); // Ждём завершения анимации
            modalBody.innerHTML = ''; // Очищаем содержимое
            document.removeEventListener('click', handleOutsideClick); // Удаляем обработчик
        }
        // Функция для обработки кликов вне модального окна
        function handleOutsideClick(event) {
            const modalContent = document.querySelector('.modal-content');
            if (!modalContent.contains(event.target)) {
                closeModal(); // Закрываем модальное окно
            }
        }

        // Универсальная функция для отправки данных на сервер
        async function sendToServer(url, method = 'POST', data = {}) {
            try {
                // console.log('Отправляемые данные:', JSON.stringify(data));  // Логируем данные перед отправкой
                const response = await fetch(url, {
                    method: method,
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (!response.ok) {
                    throw new Error(`HTTP error! Status: ${response.status}`);
                }

                const result = await response.json();
                //   console.log('Ответ от сервера:', result);  // Логируем ответ от сервера
                return result;
            } catch (error) {
                console.error("Ошибка при отправке данных:", error);
                alert("Произошла ошибка при отправке данных на сервер.");
                throw error;
            }
        }


        // Обработчик для ссылки cameraSettingsLink
        document.getElementById('cameraSettingsLink').addEventListener('click', () => {
            // Создаем контент для модального окна
            const bodyContent = `
        <div class="input-row">
            <label for="agcGainInput">AGC Gain:</label>
            <input class="modal-input" type="number" id="agcGainInput" value="${window.cameraSettings.agc_gain}" 
                   min="0" max="30" step="1">
        </div>
        <div class="input-row">
            <label for="aecValueInput">AEC Value:</label>
            <input class="modal-input" type="number" id="aecValueInput" value="${window.cameraSettings.aec_value}" 
                   min="0" max="1200" step="1">
        </div>
        <div class="input-row">
            <label for="flashDutyInput">Flash Duty:</label>
            <input class="modal-input" type="number" id="flashDutyInput" value="${window.cameraSettings.flash_duty}" 
                   min="0" max="1024" step="1">
        </div>
    `;

            // Открываем модальное окно и передаем данные
            openModal("Camera Settings", bodyContent, async () => {
                const agcGain = parseInt(document.getElementById('agcGainInput').value, 10);
                const aecValue = parseInt(document.getElementById('aecValueInput').value, 10);
                const flashDuty = parseInt(document.getElementById('flashDutyInput').value, 10);

                // Флаг для отслеживания валидности данных
                let isValid = true;

                // Проверка на корректность введенных данных
                if (isNaN(agcGain) || agcGain < 0 || agcGain > 30) {
                    alert("AGC Gain must be a number between 0 and 30.");
                    isValid = false;
                }

                if (isNaN(aecValue) || aecValue < 0 || aecValue > 1200) {
                    alert("AEC Value must be a number between 0 and 1200.");
                    isValid = false;
                }

                if (isNaN(flashDuty) || flashDuty < 0 || flashDuty > 1024) {
                    alert("Flash Duty must be a number between 0 and 1024.");
                    isValid = false;
                }

                // Если данные невалидны, не закрываем окно
                if (!isValid) {
                    return; // Прерываем выполнение функции
                }

                try {
                    const result = await sendToServer('/save_common_settings', 'POST', {
                        agc_gain: agcGain,
                        aec_value: aecValue,
                        flash_duty: flashDuty
                    });

                    if (result.status === 'success') {
                        // Обновляем настройки
                        window.cameraSettings = { agc_gain: agcGain, aec_value: aecValue, flash_duty: flashDuty };
                        alert("Camera settings saved successfully.");
                        closeModal(); // Закрываем окно только при успешном сохранении
                    } else {
                        alert("Failed to save camera settings.");
                    }
                } catch (error) {
                    console.error("Error saving camera settings:", error);
                    alert("An error occurred while saving the settings.");
                }
            });
        });

        // Обработчик для ссылки Sleep duration
        document.getElementById('sleepDurationLink').addEventListener('click', () => {
            const currentSleepSeconds = document.getElementById('sleep_seconds').textContent || "30";
            const bodyContent = `
        <div class="input-row">
            <label for="sleepSecondsInput">Seconds:</label>
            <input class="modal-input" type="number" id="sleepSecondsInput" value="${currentSleepSeconds}" 
                   placeholder="Enter sleep duration" min="30" step="1">
        </div>
    `;
            openModal("Set Sleep Duration", bodyContent, async () => {
                const sleepSeconds = parseInt(document.getElementById('sleepSecondsInput').value, 10);

                // Проверка на нечисловое значение или отрицательные числа
                if (isNaN(sleepSeconds) || sleepSeconds < 30) {
                    alert("Please enter a valid sleep duration. The value must be at least 30 seconds.");
                    return;
                }

                try {
                    const result = await sendToServer('/save_common_settings', 'POST', { sleep_seconds: sleepSeconds });
                    if (result.status === 'success') {
                        // alert("Sleep duration saved.");
                        // Обновляем интерфейс
                        document.getElementById('sleep_seconds').textContent = sleepSeconds;
                    } else {
                        alert("Failed to save sleep duration.");
                    }
                } catch (error) {
                    console.error("Error saving sleep duration:", error);
                }
            });
        });

        // Обработчик для ссылки Server path
        document.getElementById('serverPathLink').addEventListener('click', () => {
            const currentServerPath = document.getElementById('server_path').textContent || "";
            const bodyContent = `
                <div class="input-row">
                <textarea class="modal-textarea" id="serverPathInput" placeholder="Enter server path">${currentServerPath}</textarea>
                </div>
             `;
            openModal("Set Server Path", bodyContent, async () => {
                const serverPath = document.getElementById('serverPathInput').value.trim();
                if (!serverPath) {
                    alert("Please enter a valid server path.");
                    return;
                }

                try {
                    const result = await sendToServer('/save_common_settings', 'POST', { server_path: serverPath });
                    if (result.status === 'success') {
                        //  alert("Server path saved.");
                        // Обновляем интерфейс
                        document.getElementById('server_path').textContent = serverPath;
                    } else {
                        alert("Failed to save server path.");
                    }
                } catch (error) {
                    console.error("Error saving server path:", error);
                }
            });
        });

        // Универсальный обработчик для всех переключателей
        function setupSwitchHandlers() {
            const switches = [
                { id: 'ocrEnabledSwitch', serverKey: 'ocr_enabled', displayId: 'ocr_enabled' },
                { id: 'sleepEnabledSwitch', serverKey: 'sleep_enabled', displayId: 'sleep_enabled' },
                { id: 'copyToServerSwitch', serverKey: 'copy_to_server', displayId: 'copy_to_server' }
            ];

            switches.forEach(({ id, serverKey, displayId }) => {
                const element = document.getElementById(id);
                if (element) {
                    element.addEventListener('change', async () => {
                        const isChecked = element.checked ? 1 : 0; // Преобразуем true/false в 1/0
                        try {
                            const result = await sendToServer('/save_common_settings', 'POST', { [serverKey]: isChecked });
                            if (result.status === 'success') {
                                const displayElement = document.getElementById(displayId);
                                if (displayElement) {
                                    displayElement.textContent = isChecked ? "Yes" : "No";
                                }
                            } else {
                                alert(`Ошибка при сохранении ${serverKey.replace(/_/g, ' ')}.`);
                            }
                        } catch (error) {
                            console.error(`Error saving ${serverKey}:`, error);
                        }
                    });
                }
            });
        }


        // Функция для отключения copyToServerSwitch при пустом server_path
        function updateCopyToServerSwitchState() {
            const serverPath = document.getElementById('server_path')?.textContent.trim();
            const copyToServerSwitch = document.getElementById('copyToServerSwitch');
            if (copyToServerSwitch) {
                copyToServerSwitch.disabled = !serverPath || serverPath === "Not set"; // Отключаем, если путь пуст или "Not set"
            }
        }

        // Обработчик для ссылки deviceSettingsLink
        function setupDeviceDataHandler(device_key) {
            document.getElementById(`${device_key}SettingsLink`).addEventListener('click', () => {
                const currentDevice = device_key;

                // Создаем содержимое модального окна с текущим ID
                const bodyContent = `
        <div class="input-row">
            <label for="deviceTypeSelect">Type:</label>
            <select id="deviceTypeSelect" class="modal-select">
                <option value="hot">Hot water</option>
                <option value="cold">Cold water</option>
                <option value="electric">Electric</option>
            </select>
        </div>
        <div class="input-row">
            <label for="newDeviceId">ID:</label>
            <input type="text" id="newDeviceId" class="modal-input" placeholder="New ID">
        </div>
        `;

                openModal(
                    "Set new data for Meter ",
                    bodyContent,
                    async () => { // Добавляем async, так как используем await
                        const selectedDeviceType = document.getElementById('deviceTypeSelect').value;
                        const newId = document.getElementById('newDeviceId').value.trim();

                        if (!newId) {
                            alert("Set new ID.");
                            return;
                        }

                        try {
                            // Создаем JSON объект с данными
                            const payload = {
                                key: currentDevice, // Передаем текущий device_key
                                type: selectedDeviceType,
                                id: newId
                            };

                            // Используем универсальную функцию sendToServer для отправки данных в формате JSON
                            const result = await sendToServer('/save_device_id', 'POST', payload);

                            if (result.status === 'success') {
                                alert("Device data saved.");
                                // Обновляем интерфейс для выбранного устройства
                                const deviceIdElement = document.getElementById(`${currentDevice}_id`);
                                if (deviceIdElement) {
                                    deviceIdElement.textContent = newId;
                                }
                                updateDeviceType(`${currentDevice}_type`, selectedDeviceType);

                            } else {
                                alert("Failed to save device data.");
                            }
                        } catch (error) {
                            console.error("Error:", error);
                        }
                    },
                    currentDevice // Передаем текущий Device в openModal
                );
            });
        }

        // Привязываем обработчики для device1 и device2
        setupDeviceDataHandler('device1');
        setupDeviceDataHandler('device2');



        // Функция для отправки запроса на process_handler
        async function getImages(processEnabled) {
            try {
                const action = processEnabled ? 'process_enabled' : 'process_disabled';

                const response = await fetch('/get_images', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ action: action })
                });

                if (!response.ok) {
                    throw new Error(`HTTP error! Status: ${response.status}`);
                }

                const data = await response.json();

                if (!data.devices) {
                    console.error('No devices found in the response.');
                    return;
                }

                // Сбрасываем изображения и данные перед обновлением
                for (let i = 1; i <= 2; i++) {
                    const imageElement = document.getElementById(`image${i}`);
                    const dataElement = document.getElementById(`device${i}_data`);

                    if (imageElement) {
                        imageElement.src = '';
                        imageElement.style.visibility = 'hidden';
                        imageElement.style.opacity = '0';
                    }

                }

                // Обновляем данные только для существующих устройств
                Object.entries(data.devices).forEach(([deviceId, deviceData]) => {
                    // Получаем индекс устройства (device1, device2)
                    const match = deviceId.match(/\d+/); // Извлекаем число из deviceId (например, "device1" → "1")
                    if (!match) {
                        console.error(`Invalid device ID format: ${deviceId}`);
                        return;
                    }
                    const index = parseInt(match[0], 10); // Получаем числовой индекс

                    const imageElement = document.getElementById(`image${index}`);
                    const dataElement = document.getElementById(`device${index}_data`);

                    if (!imageElement || !dataElement) {
                        console.error(`Element with ID image${index} or device${index}_data not found.`);
                        return;
                    }

                    // Обновляем изображение
                    if (deviceData.device_image) {
                        imageElement.src = `data:image/jpeg;base64,${deviceData.device_image}`;
                        imageElement.style.visibility = 'visible';
                        imageElement.style.opacity = '1';
                    }

                    // Обновляем текстовые данные
                    if (deviceData.device_data) {
                        dataElement.innerHTML = `
                    <p><label>🕒&nbsp</label><span class="small-p"><strong>${formatTimestamp(deviceData.device_data.timestamp)}</strong></span></p>
                    <p><label>📃</label>&nbsp<span><strong>${deviceData.device_data.text || 'Unknown'}</strong></span></p>
                `;
                    }
                });

            } catch (error) {
                console.error('Error loading devices:', error);
                const imagesMainContainer = document.getElementById('imagesMainContainer');
                if (imagesMainContainer) {
                    imagesMainContainer.innerHTML = `<p>Error loading devices: ${error.message}</p>`;
                }
            }
        }


        // Функция для форматирования времени
        function formatTimestamp(timestamp) {
            if (!timestamp) return 'Unknown';
            const date = new Date(timestamp * 1000); // Преобразуем Unix-время в миллисекунды

            // Форматируем дату и время, исключая секунды
            return date.toLocaleString([], {
                year: 'numeric',
                month: '2-digit',
                day: '2-digit',
                hour: '2-digit',
                minute: '2-digit'
            });
        }

        // Привязываем функцию к кнопке
        document.getElementById('testButton').addEventListener('click', () => {
            takeInitImage(false); // Вызываем takeInitImage без прямоугольника
            getImages('process_enabled'); // Вызываем getImages
        });

        // 
        // Вывод логов
        const logsElement = document.getElementById('logs');
        const socket = new WebSocket(`ws://${location.host}/ws`);

        socket.onmessage = (event) => {
            logsElement.textContent += event.data + '\n'; // Добавляем новое сообщение
            logsElement.scrollTop = logsElement.scrollHeight; // Прокручиваем вниз
        };

        socket.onopen = () => {
            console.log("WebSocket connection established");
        };

        socket.onerror = (error) => {
            console.error("WebSocket error:", error);
        };

    </script>
</body>

</html>
)html";