#include "graph.h"
#include "terminal.h"
#include <stdlib.h>
#include <string.h>


/* Agrega un entero a un arreglo dinamico, duplicando capacidad si es necesario */
static void int_push(int **arr, int *count, int *cap, int val)
{
	if (*count >= *cap) {
		*cap = *cap ? *cap * 2 : 4;
		*arr = realloc(*arr, (size_t)*cap * sizeof(int));
	}
	(*arr)[(*count)++] = val;
}

/* Inicializa un nodo con valores por defecto */
static void node_init(struct node *n, int id, const char *name, const char *cmd)
{
	memset(n, 0, sizeof(*n));
	n->id          = id;
	n->scc_id      = -1;
	n->target      = -1;
	n->status      = STATUS_PENDING;
	n->pid         = -1;
	n->tj_index    = -1;
	n->tj_lowlink  = -1;
	strncpy(n->name, name, MAX_NAME - 1);
	strncpy(n->cmd,  cmd,  MAX_CMD  - 1);
}

/* Libera la memoria dinamica de un nodo (aristas y dependencias) */
static void node_free(struct node *n)
{
	free(n->edges);
	free(n->deps);
}

/* ── ciclo de vida del grafo ────────────────────────────────────────── */

void graph_init(struct graph *g)
{
	memset(g, 0, sizeof(*g));
}

void graph_free(struct graph *g)
{
	for (int i = 0; i < g->count; i++)
		node_free(&g->nodes[i]);
	free(g->nodes);
	memset(g, 0, sizeof(*g));
}

/* ── construccion del grafo ─────────────────────────────────────────── */

/* Busca un nodo por nombre; retorna su indice o -1 si no existe */
int graph_find_node(const struct graph *g, const char *name)
{
	for (int i = 0; i < g->count; i++)
		if (strcmp(g->nodes[i].name, name) == 0)
			return i;
	return -1;
}

/* Agrega un nuevo nodo al grafo; rechaza duplicados */
static int graph_add_node(struct graph *g, const char *name, const char *cmd)
{
	if (graph_find_node(g, name) >= 0) {
		fprintf(stderr, "nodo duplicado: %s\n", name);
		return -1;
	}
	/* expandir el arreglo de nodos si es necesario */
	if (g->count >= g->cap) {
		g->cap = g->cap ? g->cap * 2 : 8;
		g->nodes = realloc(g->nodes, (size_t)g->cap * sizeof(struct node));
	}
	int id = g->count++;
	node_init(&g->nodes[id], id, name, cmd);
	return id;
}

/* Agrega una arista (from -> to) al grafo; rechaza auto-ciclos */
static int graph_add_edge(struct graph *g, const char *from, const char *to)
{
	if (strcmp(from, to) == 0) {
		fprintf(stderr, "auto-ciclo no permitido: %s\n", from);
		return -1;
	}
	int f = graph_find_node(g, from);
	int t = graph_find_node(g, to);
	if (f < 0 || t < 0)
		return -1;
	/* registrar arista saliente en 'from' y dependencia entrante en 'to' */
	int_push(&g->nodes[f].edges, &g->nodes[f].edge_count, &g->nodes[f].edge_cap, t);
	int_push(&g->nodes[t].deps,  &g->nodes[t].dep_count,  &g->nodes[t].dep_cap,  f);
	return 0;
}

/* ── generateGraph (parser de JDL) ──────────────────────────────────── */

/*
 * Lee lineas del JDL y construye el grafo en memoria.
 * Formato:
 *   node <nombre> <comando>
 *   edge <origen> <destino>
 * Lineas vacias y con '#' se ignoran. "end" termina la lectura.
 */
int generate_graph(struct graph *g, FILE *f)
{
	graph_init(g);

	char line[MAX_CMD + MAX_NAME + 16];
	while (fgets(line, (int)sizeof(line), f)) {
		/* quitar salto de linea */
		char *nl = strchr(line, '\n');
		if (nl) *nl = '\0';

		/* saltar espacios al inicio */
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		/* ignorar lineas vacias y comentarios */
		if (*p == '\0' || *p == '#')
			continue;
		/* "end" marca el final de entrada inline */
		if (strcmp(p, "end") == 0)
			break;

		if (strncmp(p, "node ", 5) == 0) {
			/* extraer nombre del nodo */
			char *q = p + 5;
			while (*q == ' ') q++;
			char name[MAX_NAME] = {0};
			int i = 0;
			while (*q && *q != ' ' && i < MAX_NAME - 1)
				name[i++] = *q++;
			while (*q == ' ') q++;
			/* el resto de la linea es el comando a ejecutar */
			if (graph_add_node(g, name, q) < 0) {
				graph_free(g);
				return -1;
			}
		} else if (strncmp(p, "edge ", 5) == 0) {
			/* extraer nodos origen y destino de la arista */
			char from[MAX_NAME] = {0}, to[MAX_NAME] = {0};
			if (sscanf(p + 5, "%31s %31s", from, to) != 2) {
				fprintf(stderr, "arista mal formada: %s\n", p);
				graph_free(g);
				return -1;
			}
			if (graph_add_edge(g, from, to) < 0) {
				fprintf(stderr, "arista invalida: %s -> %s (nodo desconocido?)\n", from, to);
				graph_free(g);
				return -1;
			}
		} else {
			fprintf(stderr, "directiva JDL desconocida: %s\n", p);
		}
	}

	if (g->count == 0) {
		fprintf(stderr, "el JDL no contiene nodos\n");
		return -1;
	}
	return 0;
}

/* ── SCC: algoritmo de Tarjan ───────────────────────────────────────── */

/* Contexto interno para la recursion de Tarjan */
struct tj_ctx {
	struct graph *g;
	int           index;
	int          *stack;
	int           top;
	int           scc_count;
	int           has_cycle;
};

/*
 * Visita recursiva de Tarjan: recorre el nodo v y sus sucesores.
 * Asigna tj_index y tj_lowlink; al retroceder, detecta si v es
 * raiz de un componente fuertemente conexo (SCC).
 */
static void tj_visit(struct tj_ctx *ctx, int v)
{
	struct node *nv = &ctx->g->nodes[v];
	nv->tj_index   = ctx->index;
	nv->tj_lowlink = ctx->index;
	ctx->index++;
	ctx->stack[ctx->top++] = v;
	nv->on_stack = 1;

	/* explorar cada arista saliente v -> w */
	for (int i = 0; i < nv->edge_count; i++) {
		int w = nv->edges[i];
		struct node *nw = &ctx->g->nodes[w];
		if (nw->tj_index < 0) {
			/* w no ha sido visitado: recurrir */
			tj_visit(ctx, w);
			if (nw->tj_lowlink < nv->tj_lowlink)
				nv->tj_lowlink = nw->tj_lowlink;
		} else if (nw->on_stack) {
			/* w esta en la pila: forma parte del mismo SCC potencial */
			if (nw->tj_index < nv->tj_lowlink)
				nv->tj_lowlink = nw->tj_index;
		}
	}

	/* si v es raiz de un SCC, extraer todos los nodos del componente */
	if (nv->tj_lowlink == nv->tj_index) {
		int scc_id = ctx->scc_count++;
		int size   = 0;
		int w;
		do {
			w = ctx->stack[--ctx->top];
			ctx->g->nodes[w].on_stack = 0;
			ctx->g->nodes[w].scc_id   = scc_id;
			size++;
		} while (w != v);

		/* un SCC con mas de 1 nodo implica un ciclo */
		if (size > 1) {
			ctx->has_cycle = 1;
			fprintf(stderr, "ciclo detectado en SCC %d (%d nodos)\n", scc_id, size);
		}
	}
}

/*
 * Ejecuta Tarjan sobre todo el grafo.
 * Retorna 0 si el grafo es un DAG (sin ciclos), -1 si tiene ciclos.
 */
int scc_tag(struct graph *g)
{
	int *stack = malloc((size_t)g->count * sizeof(int));
	if (!stack) return -1;

	struct tj_ctx ctx = { .g = g, .stack = stack };

	/* visitar todos los nodos que aun no fueron alcanzados */
	for (int i = 0; i < g->count; i++)
		if (g->nodes[i].tj_index < 0)
			tj_visit(&ctx, i);

	g->scc_count = ctx.scc_count;
	free(stack);

	if (ctx.has_cycle) {
		fprintf(stderr, "el grafo tiene ciclos — no se puede despachar\n");
		return -1;
	}
	return 0;
}

/* ── mapGraph ───────────────────────────────────────────────────────── */

/* Asigna cada nodo a un destino de ejecucion. Por ahora todos son locales. */
void map_graph(struct graph *g)
{
	for (int i = 0; i < g->count; i++)
		g->nodes[i].target = 0;   /* 0 = ejecucion local */
}

/* ── graph_print ────────────────────────────────────────────────────── */

/* Imprime el grafo completo a stderr para depuracion */
void graph_print(const struct graph *g)
{
	fprintf(stderr, "grafo: %d nodo(s), %d SCC(s)\n", g->count, g->scc_count);
	for (int i = 0; i < g->count; i++) {
		const struct node *n = &g->nodes[i];
		fprintf(stderr, "  [%d] %-8s scc=%-2d destino=%-2d  %s\n",
		        n->id, n->name, n->scc_id, n->target, n->cmd);
		if (n->dep_count > 0) {
			fprintf(stderr, "       depende de: ");
			for (int j = 0; j < n->dep_count; j++)
				fprintf(stderr, "%s ", g->nodes[n->deps[j]].name);
			fprintf(stderr, "\n");
		}
		if (n->edge_count > 0) {
			fprintf(stderr, "       ->   ");
			for (int j = 0; j < n->edge_count; j++)
				fprintf(stderr, "%s ", g->nodes[n->edges[j]].name);
			fprintf(stderr, "\n");
		}
	}
}
