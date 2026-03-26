# Saper WebView2 (Light)

Легкая версия: UI на WebView2, а игровая логика используется из вашего файла `saper logic/b.cpp`.

## Сборка

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Запуск

```powershell
.\build\Release\saper_webview.exe
```

## Управление

- ЛКМ: открыть клетку
- ПКМ: поставить/снять флаг
- Выбор сложности и кнопка `Новая игра` в верхней панели

## Примечания

- `src/main.cpp` — WebView2 + обмен командами между UI и C++, использует `saper logic/b.cpp`
- Анимации: библиотека `anime.js` (`ui/vendor/anime.min.js`)
- Поле автоматически масштабируется, чтобы `16x16` было видно целиком без скролла
