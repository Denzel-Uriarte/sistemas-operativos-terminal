#include "terminal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int parse_args(char *line, char *argv[], int max_args)
{
	int argc = 0;
	char *p = line;

	while (argc < max_args - 1) {
		/* saltar espacios y tabs, reemplazandolos con terminadores */
		while (*p == ' ' || *p == '\t')
			*p++ = '\0';
		if (!*p || *p == '\n')
			break;
		argv[argc++] = p;
		/* avanzar hasta el siguiente espacio o fin de linea */
		while (*p && *p != ' ' && *p != '\t' && *p != '\n')
			p++;
	}
	if (*p == '\n')
		*p = '\0';
	argv[argc] = NULL;
	return argc;
}

int resolve_path(const char *name, char *out, size_t out_size)
{
	/* si el nombre ya contiene '/', usarlo tal cual como ruta */
	if (strchr(name, '/')) {
		if (strlen(name) >= out_size)
			return -1;
		strncpy(out, name, out_size - 1);
		out[out_size - 1] = '\0';
		return 0;
	}

	/* copiar PATH a un buffer local para poder recorrerlo */
	const char *path_env = getenv("PATH");
	if (!path_env)
		return -1;

	char path_buf[MAX_LINE];
	size_t n = strlen(path_env);
	if (n >= sizeof(path_buf))
		return -1;
	memcpy(path_buf, path_env, n + 1);

	/* recorrer cada directorio de PATH separado por ':' */
	const char *dir = path_buf;
	for (;;) {
		const char *end = strchr(dir, ':');
		size_t dlen = end ? (size_t)(end - dir) : strlen(dir);
		if (dlen > 0) {
			/* construir ruta candidata: directorio/nombre */
			int need = snprintf(out, out_size, "%.*s/%s", (int)dlen, dir, name);
			/* verificar si existe y es ejecutable */
			if (need > 0 && (size_t)need < out_size && access(out, X_OK) == 0)
				return 0;
		}
		if (!end)
			break;
		dir = end + 1;
	}
	return -1;
}
