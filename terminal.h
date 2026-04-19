#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

#define MAX_LINE      4096
#define MAX_ARGS      64
#define MAX_PATH_LEN  256

/* Divide la linea en argv[] modificandola in-place; retorna argc.
   El buffer de la linea es propiedad del llamador. */
int parse_args(char *line, char *argv[], int max_args);

/* Resuelve el nombre de un comando a su ruta completa buscando
   en la variable PATH en memoria.
   Retorna 0 si se encontro (out queda lleno), -1 si no existe. */
int resolve_path(const char *name, char *out, size_t out_size);

#endif
