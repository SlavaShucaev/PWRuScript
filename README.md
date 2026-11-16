# Project Wingman на русском языке

Скрипт установщик русской локализации от VGS Studio : озвучка, субтитры и интерфейс. Скрипт ставить в пару нажатий.

**Поддерживаемые устройства :**  любые Linux-дистрибутивы
**Оригинальный лаунчер :** [PWRuLauncher](https://github.com/RikudoSmpai/PWRuLauncher/tree/main)  

![Скриншот](shots/pwru_script_message.png)

## Цель проекта

Создать удобный способ установки русской локализации Project Wingman от VGS Studio на устроствах под управлением Linux-дистрибутивов

## Как собрать у себя

Чтобы собрать проект на своём ПК необходимо установить:
- CMake
- Libcurl
- Libarchive

**Шаг 1** Скопировать репозиторий
```bash
git clone "https://github.com/SlavaShucaev/PWRuScript.git"
```

**Шаг 2** Сгенерировать конфигурацию CMake 
```bash
# Создаём папку build и перодим в него
mkdir build && cd build
# Генерация конфигурации для сборки
cmake -G "Unix Makefiles" ..
```

**Шаг 3** Сборка проекта
```bash
cmake --build .
```

## Что именно попадает в папку игры

| Файл | Зачем |
|---|---|
| `ProjectWingman\Content\Paks\~mods\PW_RU_Voice.pak` | русская озвучка |
| `ProjectWingman\Content\Paks\~mods\PW_RU_Subs.pak` | субтитры и тексты игры |
| `ProjectWingman\Binaries\Win64\dwmapi.dll`, `UE4SS.dll`, `UE4SS-settings.ini`, папка `Mods\` | UE4SS: русский язык при каждом запуске, перевод текста мимо субтитров, титры локализации |
| `ProjectWingman\Content\Movies\HumbleIntro_PW.mp4` | интро (оригинал рядом в `.pwru-orig`) |

## Проблемы

Пишите в [Issue](https://github.com/SlavaShucaev/PWRuScript/issues), если столкнётесь с проблемами при работе скрипта.

---
Материалы локализации принадлежать : [VGS Studio](https://t.me/VGSPLF "Ссылка на Телеграм канал VGS Studio")
[Project Wingman](https://store.steampowered.com/app/895870/Project_Wingman/ "Страница игры в Steam") и связанные с ним материалы принадлежат : Sector D2 