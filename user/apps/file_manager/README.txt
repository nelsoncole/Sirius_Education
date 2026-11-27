# File Manager

FILE_MANAGER_MODE (0): gestor normal; buffer pode ser NULL.
FILE_MANAGER_OPEN (1): buffer recebe o caminho completo do ficheiro existente.
FILE_MANAGER_SAVE (2): buffer contém inicialmente o nome sugerido; ao confirmar recebe o caminho completo de destino.

Exemplo OPEN:
```c
char path[PATH_MAX] = {0};
file_manager_t *fm = file_manager_create(
    &ui, x, y, width, height, "/home/Transferências",
    FILE_MANAGER_OPEN, path, sizeof(path)
);
/* Após Abrir, path contém o caminho completo. */
```

Exemplo SAVE:
```c
char path[PATH_MAX] = "relatorio.pdf";
file_manager_t *fm = file_manager_create(
    &ui, x, y, width, height, "/home/Documentos",
    FILE_MANAGER_SAVE, path, sizeof(path)
);
/* Após Guardar, path contém /home/Documentos/relatorio.pdf. */
/* O file manager não cria nem grava o ficheiro. */
```

O buffer deve permanecer válido enquanto o file manager estiver ativo e ter capacidade para guardar o caminho completo.

Foi preservado o painel separador inferior personalizado, criado com create_panel(), e o estilo personalizado do editbox (raio, espessura, alinhamento, cores e barras de scroll).