# MimeFDupes

Программа-аналог `fdupes` с расширенной функциональностью поиска и удаления дубликатов файлов по MIME-типу

## 📌 Оглавление

- [Особенности](#-особенности)
- [Установка](#-установка)
- [Использование](#-использование)

## ✨ Особенности

- 🔍 **Рекурсивный поиск** дубликатов в указанных директориях
- 🏷 **Фильтрация по MIME-типам** (поддержка wildcards: `image/*`, `text/plain`)
- 🗑️ **Безопасное удаление** с подтверждением и резервным копированием
- 📊 **Детальная статистика** по найденным дубликатам
- 🐧 **Кросс-платформенность** (Linux/macOS/Unix)

## 📥 Установка

### Требования
- C++17 компилятор
- Библиотека `libmagic` (`sudo apt install libmagic-dev`)
- Библиотека `OpenSSL/evp` (`sudo apt install libssl-dev`)

### Сборка из исходников
```bash
git clone https://github.com/nonpsycho/mimefdupes.git
cd mimefdupes

Для C-версии:
mkdir build && cd build
cmake .. && make
```
## 🚀 Использование
usage: ./mimefdupes [DIRECTORIES...] [MIME TYPE] [OPTIONS] 

### Mime type examples
- "/jpeg"
- "application/zip"
- "text/"

### Options:
- d - Delete duplicates automatically
- i - Interactive delete (ask before each deletion
- n - Dry run (only show what would be deleted
