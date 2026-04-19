/*
 * shell multi-usuario con planificador de trabajos
 * basado en grafos.
 *
 * Ciclo del shell: leer linea -> parsear -> ejecutar (fork/exec para
 * comandos externos, built-ins para exit/cd/submit).
 * "submit" activa el pipeline completo del grafo:
 *   generateGraph -> SCC -> mapGraph -> dispatch
 */

#include "terminal.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

extern char **environ;

/*  controlSubmit: JDL -> dispatch */

static int control_submit(const char *jdl_source)
{
	FILE *f;
	int from_stdin = (strcmp(jdl_source, "-") == 0);

	if (from_stdin) {
		f = stdin;
		fprintf(stderr, "Ingrese JDL (termine con 'end'):\n");
	} else {
		f = fopen(jdl_source, "r");
		if (!f) {
			perror(jdl_source);
			return -1;
		}
	}

	struct graph g;

	/* Paso 1: generateGraph — parsear el JDL y construir el grafo */
	fprintf(stderr, "[submit] parseando JDL...\n");
	if (generate_graph(&g, f) != 0) {
		fprintf(stderr, "[submit] fallo al parsear JDL\n");
		if (!from_stdin) fclose(f);
		return -1;
	}
	if (!from_stdin) fclose(f);

	/* Paso 2: SCC — etiquetar componentes y detectar ciclos */
	fprintf(stderr, "[submit] ejecutando analisis SCC...\n");
	if (scc_tag(&g) != 0) {
		graph_free(&g);
		return -1;
	}

	/* Paso 3: mapGraph — asignar nodos a destinos de ejecucion */
	fprintf(stderr, "[submit] mapeando grafo...\n");
	map_graph(&g);

	graph_print(&g);

	/* Paso 4: dispatch — ejecutar respetando dependencias */
	fprintf(stderr, "[submit] despachando...\n");
	int rc = dispatch(&g);

	graph_free(&g);
	return rc;
}

/* ── ciclo principal del shell ──────────────────────────────────────── */

int main(void)
{
	char line[MAX_LINE];
	char *argv[MAX_ARGS];
	char path[MAX_PATH_LEN];

	for (;;) {
		fprintf(stderr, "> ");
		fflush(stderr);

		if (!fgets(line, (int)sizeof(line), stdin))
			break;

		int argc = parse_args(line, argv, MAX_ARGS);
		if (argc == 0)
			continue;

		/* ── built-in: exit — termina el shell ─────────────────── */
		if (strcmp(argv[0], "exit") == 0)
			break;

		/* ── built-in: cd — cambia el directorio de trabajo NO TERMINADO */
		if (strcmp(argv[0], "cd") == 0) {
			const char *dir = (argc > 1) ? argv[1] : getenv("HOME");
			if (dir && chdir(dir) != 0)
				perror("cd");
			continue;
		}

		/* ── built-in: submit [archivo.jdl | -] ───────────────── */
		if (strcmp(argv[0], "submit") == 0) {
			const char *source = (argc > 1) ? argv[1] : "-";
			control_submit(source);
			continue;
		}

		/* ── comando externo: fork + exec ──────────────────────── */
		if (resolve_path(argv[0], path, sizeof(path)) != 0) {
			fprintf(stderr, "comando desconocido: %s\n", argv[0]);
			continue;
		}

		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");
			continue;
		}
		if (pid == 0) {
			execve(path, argv, environ);
			perror(path);
			_exit(127);
		}
		int status;
		if (waitpid(pid, &status, 0) < 0)
			perror("waitpid");
	}

	return 0;
}
