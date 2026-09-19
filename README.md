# Karchiver

أداة سطر أوامر بسيطة وآمنة لفك ضغط الأرشيفات على GNU/Linux.
تعتمد على أدوات النظام (`tar`, `unzip`, `7z`, `unrar`, `ar`...)
عبر `execvp` بدون `system()`، وبدون ربط مكتبات ثقيلة.

طُوّرت في إطار مشروع **Delsion-OS** التابع لـ **C.a. Star Technology**.

## الصيغ المدعومة

| الصيغة | الأداة |
|---|---|
| `.tar`, `.tar.gz`, `.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst` | `tar` |
| `.gz`, `.bz2`, `.xz`, `.zst`, `.lzma`, `.z` | أدوات الضغط المناسبة |
| `.zip`, `.jar`, `.war`, `.apk`, `.zipx` | `unzip` |
| `.7z` | `7z` |
| `.rar` | `unrar` أو `7z` |
| `.lha`, `.lzh` | `lha` |
| `.cab` | `cabextract` |
| `.deb`, `.ar` | `ar` |

## البناء

    make

## التثبيت

    sudo make install

افتراضيًا يُثبّت في `/usr/local/bin`. يمكن تغيير ذلك:

    sudo make install PREFIX=/usr

## الاستخدام

    karchiver [-v] [-C DIR] <archive>
    karchiver --list-formats

أمثلة:

    karchiver -C /tmp/out archive.zip
    karchiver -v backup.tar.gz
    karchiver --list-formats

## التبعيات

**إلزامية:**
- `tar`, `gzip`, `bzip2`, `xz-utils`, `zstd`, `unzip`, `binutils` (لـ `ar`)

**اختيارية (حسب الصيغ):**
- `p7zip-full` أو `7zip` (لـ `.7z` و `.rar`)
- `unrar` (لـ `.rar`)
- `cabextract` (لـ `.cab`)
- `lha` (لـ `.lha` / `.lzh`)

## الترخيص

GPL-3.0-or-later. راجع ملف [LICENSE](LICENSE).

حقوق النشر (C) 2026 شركة C.a. Star Technology.
