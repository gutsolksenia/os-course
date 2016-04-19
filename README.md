# os-course 

# Файловая система 
Файловая система - это дерево, в узлах которого находятся fs_node. 

fs_node структура, в которой есть следующие поля: имя файла, файл или, если это папка, список файлов, 
которые находятся в ней.

Файл хранится как размер файла, ссылка на данные, размер файла, его тип и lock. Файл не делится на части
все данные хранятся одним большим куском памяти.



# Файлы 
* fs.h, fs.c - файловая система. 
* initramfs.h, initramfs.c - загрузка начальной файловой системы.
