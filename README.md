# Отчёт по тестированию SimpleFS (Гусенов С.Ю. 1304)

Реализован модуль ядра Linux `simplefs` и userspace-утилита
[userspace/simplefs_cli](userspace/simplefs_cli.c).

Модуль:

* регистрирует ФС `simplefs` в VFS, монтируется через `mount -t simplefs`;
* при первом монтировании пишет две копии суперблока по смещениям
  `sb_first_offset` / `sb_second_offset` и создаёт фиксированное число
  файлов размера `max_file_sectors` секторов каждый;
* считает CRC32 суперблока, при монтировании проверяет обе копии и
  восстанавливает битую из валидной;
* файловые операции: `read`, `write`, `llseek`, `lookup`, `iterate_shared`;
* IOCTL: `ZERO_ALL`, `ERASE_FS`, `GET_META` (имя, offset, размер,
  CRC32 содержимого по каждому файлу), `GET_MAPPING` (список секторов файла).

Утилита `simplefs_cli`:

* `demo` — в каждый файл пишет случайное 64-битное число и читает обратно
  со сверкой;
* `zero`, `erase`, `meta`, `mapping` — вызовы соответствующих IOCTL.

## 2. Окружение

* root: Ubuntu 24.04, ядро `6.12.25-061225-generic`
* Loop-устройство на обычном файле `disk.img` (1 МБ = 2048 секторов)
* Параметры модуля при тестах: `sb_first_offset=0 sb_second_offset=64
  max_file_sectors=8`

## 3. Собираем проект

```
root@said-linux:~/myfs# make clean
make -C /lib/modules/6.12.25-061225-generic/build M=/root/myfs clean
make[1]: Entering directory '/usr/src/linux-headers-6.12.25-061225-generic'
  CLEAN   /root/myfs/Module.symvers
make[1]: Leaving directory '/usr/src/linux-headers-6.12.25-061225-generic'
make -C userspace clean
make[1]: Entering directory '/root/myfs/userspace'
rm -f simplefs_cli
make[1]: Leaving directory '/root/myfs/userspace'
root@said-linux:~/myfs# make
make -C /lib/modules/6.12.25-061225-generic/build M=/root/myfs modules
make[1]: Entering directory '/usr/src/linux-headers-6.12.25-061225-generic'
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: x86_64-linux-gnu-gcc-14 (Ubuntu 14.2.0-4ubuntu2) 14.2.0
  You are using:           gcc-14 (Ubuntu 14.2.0-4ubuntu2~24.04.1) 14.2.0
  CC [M]  /root/myfs/super.o
  CC [M]  /root/myfs/inode.o
  CC [M]  /root/myfs/file.o
  CC [M]  /root/myfs/dir.o
  CC [M]  /root/myfs/ioctl.o
  CC [M]  /root/myfs/utils.o
  LD [M]  /root/myfs/simplefs.o
  MODPOST /root/myfs/Module.symvers
  CC [M]  /root/myfs/simplefs.mod.o
  CC [M]  /root/myfs/.module-common.o
  LD [M]  /root/myfs/simplefs.ko
  BTF [M] /root/myfs/simplefs.ko
Skipping BTF generation for /root/myfs/simplefs.ko due to unavailability of vmlinux
make[1]: Leaving directory '/usr/src/linux-headers-6.12.25-061225-generic'
make -C userspace
make[1]: Entering directory '/root/myfs/userspace'
gcc -std=gnu11 -Wall -Wextra -Wpedantic -O2 -o simplefs_cli simplefs_cli.c
make[1]: Leaving directory '/root/myfs/userspace'
root@said-linux:~/myfs#
```

## 4. Подготовка диска и монтирование

```
root@said-linux:~/myfs# dd if=/dev/zero of=disk.img bs=512 count=2048 status=none
root@said-linux:~/myfs# sudo losetup -fP --show disk.img
/dev/loop0
root@said-linux:~/myfs# sudo insmod simplefs.ko \
    sb_first_offset=0 \
    sb_second_offset=64 \
    max_file_sectors=8
root@said-linux:~/myfs# sudo mkdir -p /mnt/simplefs
root@said-linux:~/myfs# sudo mount -t simplefs /dev/loop0 /mnt/simplefs
root@said-linux:~/myfs# ls /mnt/simplefs
file_0    file_115  file_132  file_15   file_167  file_184  file_200  file_218  file_235  file_252  file_41  file_59  file_76  file_93
file_1    file_116  file_133  file_150  file_168  file_185  file_201  file_219  file_236  file_253  file_42  file_6   file_77  file_94
file_10   file_117  file_134  file_151  file_169  file_186  file_202  file_22   file_237  file_26   file_43  file_60  file_78  file_95
file_100  file_118  file_135  file_152  file_17   file_187  file_203  file_220  file_238  file_27   file_44  file_61  file_79  file_96
file_101  file_119  file_136  file_153  file_170  file_188  file_204  file_221  file_239  file_28   file_45  file_62  file_8   file_97
file_102  file_12   file_137  file_154  file_171  file_189  file_205  file_222  file_24   file_29   file_46  file_63  file_80  file_98
file_103  file_120  file_138  file_155  file_172  file_19   file_206  file_223  file_240  file_3    file_47  file_64  file_81  file_99
file_104  file_121  file_139  file_156  file_173  file_190  file_207  file_224  file_241  file_30   file_48  file_65  file_82
file_105  file_122  file_14   file_157  file_174  file_191  file_208  file_225  file_242  file_31   file_49  file_66  file_83
file_106  file_123  file_140  file_158  file_175  file_192  file_209  file_226  file_243  file_32   file_5   file_67  file_84
file_107  file_124  file_141  file_159  file_176  file_193  file_21   file_227  file_244  file_33   file_50  file_68  file_85
file_108  file_125  file_142  file_16   file_177  file_194  file_210  file_228  file_245  file_34   file_51  file_69  file_86
file_109  file_126  file_143  file_160  file_178  file_195  file_211  file_229  file_246  file_35   file_52  file_7   file_87
file_11   file_127  file_144  file_161  file_179  file_196  file_212  file_23   file_247  file_36   file_53  file_70  file_88
file_110  file_128  file_145  file_162  file_18   file_197  file_213  file_230  file_248  file_37   file_54  file_71  file_89
file_111  file_129  file_146  file_163  file_180  file_198  file_214  file_231  file_249  file_38   file_55  file_72  file_9
file_112  file_13   file_147  file_164  file_181  file_199  file_215  file_232  file_25   file_39   file_56  file_73  file_90
file_113  file_130  file_148  file_165  file_182  file_2    file_216  file_233  file_250  file_4    file_57  file_74  file_91
file_114  file_131  file_149  file_166  file_183  file_20   file_217  file_234  file_251  file_40   file_58  file_75  file_92
root@said-linux:~/myfs# sudo dmesg | grep simplefs
[ 5956.904254] simplefs: loading out-of-tree module taints kernel.
[ 5956.904299] simplefs: module verification failed: signature and/or required key missing - tainting kernel
[ 5956.907822] simplefs: filesystem registered
[ 5967.952030] simplefs: no valid superblock found
[ 5967.952043] simplefs: creating new filesystem
[ 5967.954271] simplefs: initialized 254 files (max sectors per file=8)
[ 5967.954353] simplefs: mounted (files=254, sb1=0, sb2=64)
```

При монтировании на чистом диске модуль создал 254 файла по M=8 секторов
и записал обе копии суперблока.

## 5. Запись и чтение случайных значений

В каждый файл пишется случайное 64-битное число и сразу читается обратно.

```
root@said-linux:~/myfs# ./userspace/simplefs_cli demo /mnt/simplefs | head -10
[OK]   file_0  0x48f570ef8cd39ff1
[OK]   file_1  0x2aef9dc8db4c0bdc
[OK]   file_2  0x95e9ae1e3cf888d8
[OK]   file_3  0x13c76da218780914
[OK]   file_4  0x8767ef12331b0ba5
[OK]   file_5  0xfc6b74d092215bd8
[OK]   file_6  0xcb8b8d1f8ada27b7
[OK]   file_7  0x000350986417eea7
[OK]   file_8  0xad418402e37d92a3
[OK]   file_9  0x7f2242e37876c92f
root@said-linux:~/myfs# ./userspace/simplefs_cli demo /mnt/simplefs | tail -10
[OK]   file_246  0x997d5a53e55c0f71
[OK]   file_247  0x530f8fa99eb7a6ff
[OK]   file_248  0xfdc8f76908df37d0
[OK]   file_249  0x808f70f7bac44480
[OK]   file_250  0x4c0523d0e1fc5a13
[OK]   file_251  0x85e30abf91adf177
[OK]   file_252  0xb188e371e963eb64
[OK]   file_253  0xcb9c4a87881c26e6

Total: ok=254 fail=0
```

## 6. IOCTL: GET_META (метаинформация и хеши)

```
root@said-linux:~/myfs# ./userspace/simplefs_cli meta /mnt/simplefs | head -10
NAME                 OFFSET       SIZE         CRC32
--------------------------------------------------
file_0               1            4096         0xa724cd01
file_1               9            4096         0xdb1cc970
file_2               17           4096         0x5d1db4d4
file_3               25           4096         0x875c2639
file_4               33           4096         0x7a40a73a
file_5               41           4096         0x634c3c89
file_6               49           4096         0x214caecc
file_7               65           4096         0x44cbc726
root@said-linux:~/myfs# ./userspace/simplefs_cli meta /mnt/simplefs | tail -3
file_251             2017         4096         0xd620b04f
file_252             2025         4096         0x4420c8a8
file_253             2033         4096         0xa9223a95
```

Между `file_6` (сектора 49..56) и `file_7` (offset 65) пропущен
сектор 64 — он зарезервирован под backup-копию суперблока.

## 7. IOCTL: GET_MAPPING

```
root@said-linux:~/myfs# ./userspace/simplefs_cli mapping /mnt/simplefs file_3
name:          file_3
start_sector:  25
sector_count:  8
size_bytes:    4096
sectors:       [25 .. 32]
```

## 8. IOCTL: ZERO_ALL

```
root@said-linux:~/myfs# ./userspace/simplefs_cli zero /mnt/simplefs
root@said-linux:~/myfs# hexdump -C -n 32 -s $((25*512)) /dev/loop0
00003200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00003220
root@said-linux:~/myfs# ./userspace/simplefs_cli meta /mnt/simplefs | head -5
NAME                 OFFSET       SIZE         CRC32
--------------------------------------------------
file_0               1            4096         0x00000000
file_1               9            4096         0x00000000
file_2               17           4096         0x00000000
```

Сектора файлов занулены, CRC32 содержимого = 0.

## 9. Восстановление primary SB из backup

Размонтируем, забиваем первый сектор случайными данными, монтируем снова.

```
root@said-linux:~/myfs# umount /mnt/simplefs
root@said-linux:~/myfs# dd if=/dev/urandom of=/dev/loop0 bs=512 count=1 conv=notrunc status=none
root@said-linux:~/myfs# mount -t simplefs /dev/loop0 /mnt/simplefs
root@said-linux:~/myfs# dmesg | tail -5
[ 8367.039490] simplefs: primary superblock corrupted
[ 8367.039502] simplefs: restoring primary from backup
[ 8367.039643] simplefs: mounted (files=254, sb1=0, sb2=64)
root@said-linux:~/myfs# ls /mnt/simplefs | wc -l
254

```

## 10. IOCTL: ERASE_FS и повторная инициализация

```
root@said-linux:~/myfs# ./userspace/simplefs_cli erase /mnt/simplefs
root@said-linux:~/myfs# umount /mnt/simplefs
root@said-linux:~/myfs# mount -t simplefs /dev/loop0 /mnt/simplefs
root@said-linux:~/myfs# dmesg | tail -5
[ 8486.605432] simplefs: no valid superblock found
[ 8486.605445] simplefs: creating new filesystem
[ 8486.605755] simplefs: initialized 254 files (max sectors per file=8)
[ 8486.605901] simplefs: mounted (files=254, sb1=0, sb2=64)
root@said-linux:~/myfs# ls /mnt/simplefs | wc -l
254
```

После `erase` обе копии SB занулены, при следующем монтировании ФС
инициализируется заново.

## 11. Завершение

```
root@said-linux:~/myfs# umount /mnt/simplefs
root@said-linux:~/myfs# rmmod simplefs
root@said-linux:~/myfs# losetup -d /dev/loop0
```

# Исправления:
Нужно было исправить:
1. Если "поломать" оба суперблока (не сходится checksum), то вместо ошибки она будет заново сформрована
2. Команда meta возвращает только 1024 первых файлов
3. После команды erase файлы остаются и можно продолжать писать в fs

После исправления тестирование:
### Подготовка

```
dd if=/dev/zero of=disk.img bs=512 count=2048 status=none

losetup -fP --show disk.img
mkdir -p /mnt/simplefs
insmod simplefs.ko sb_first_offset=0 sb_second_offset=64 max_file_sectors=1

mount -t simplefs /dev/loop0 /mnt/simplefs
dmesg | tail -4

# ожидается:
# simplefs: no valid superblock found
# simplefs: creating new filesystem
# simplefs: initialized 2046 files (max sectors per file=1)
# simplefs: mounted (files=2046, sb1=0, sb2=64)
```
### Тест 1 — meta возвращает все файлы, не только 1024
```
ls /mnt/simplefs | wc -l
./userspace/simplefs_cli meta /mnt/simplefs | grep -c "^file_"

Оба числа должны быть 2046. До исправления второй вызов возвращал 1024.
```
### Тест 2 — после erase запись и чтение запрещены
```
# До erase — запись работает
dd if=/dev/urandom of=/mnt/simplefs/file_0 bs=8 count=1 2>/dev/null
echo "write before erase: $?"
```
ожидается 0
 
Стираем ФС
```
./userspace/simplefs_cli erase /mnt/simplefs
dd if=/dev/urandom of=/mnt/simplefs/file_0 bs=8 count=1 2>/dev/null
echo "write after erase: $?"

cat /mnt/simplefs/file_0
echo "read after erase: $?"

ls /mnt/simplefs
echo "ls after erase: $?"
```

Ожидается ненулевые результаты в echo.

До исправления после erase запись продолжала работать. Теперь любой доступ к файлам возвращает -EIO.

Дополнительно проверяем, что после размонтирования и повторного монтирования ФС переинициализируется (оба суперблока
были занулены erase):

```
umount /mnt/simplefs
mount -t simplefs /dev/loop0 /mnt/simplefs
dmesg | tail -4
# simplefs: no valid superblock found
# simplefs: creating new filesystem   ← blank-диск, не ошибка
# simplefs: initialized 2046 files ...
# simplefs: mounted ...
```

### Тест 3 — оба суперблока сломаны (ненулевые, невалидный CRC) → ошибка

Портим оба суперблока случайными байтами

sb_first_offset=0, sb_second_offset=64
```
dd if=/dev/urandom of=/dev/loop0 bs=512 count=1 conv=notrunc seek=0  status=none
dd if=/dev/urandom of=/dev/loop0 bs=512 count=1 conv=notrunc seek=64 status=none

# Монтируем — должно упасть
mount -t simplefs /dev/loop0 /mnt/simplefs
echo "exit: $?"
# Должно выдать не нуль

dmesg | tail -3
# ожидается:
# simplefs: mounted (files=2046, sb1=0, sb2=64)
# simplefs: no valid superblock found
# simplefs: filesystem is corrupted
```