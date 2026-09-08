# Accessories — сборка для Windows + Android

## Что делать

1. Создай GitHub-репозиторий.
2. Загрузи в него содержимое этой папки.
3. Открой вкладку **Actions**.
4. Выбери **Build Accessories**.
5. Нажми **Run workflow**.
6. После завершения открой нужный build artifact.

Будут собраны:
- Windows 64-bit
- Android 32-bit
- Android 64-bit

Для большинства современных Android-телефонов нужен **Android64**.

## Установка Android-сборки

Распакуй artifact и полученный `.geode` положи в:

`/storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/`

После этого перезапусти Geometry Dash / Geode Launcher.

## Важно

В `mod.json` указана Geometry Dash 2.2081 и Geode SDK 5.8.2. Если у тебя другая версия GD, значение `gd.android` нужно поменять на фактическую версию игры.
