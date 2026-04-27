#!/usr/bin/env python3
# ─────────────────────────────────────────────────────────────────────────
# wordcloud_gen.py — FASE 3 del pipeline
#
# Proposito:
#   Leer un archivo con palabras (una por linea, ya tokenizadas y en
#   minusculas por la fase 1), contar frecuencias, filtrar stopwords en
#   espanol e ingles, y generar una imagen JPG con una nube de palabras.
#
# Uso:
#   ./wordcloud_gen.py <archivo_entrada> <archivo_salida.jpg>
#
# Ejemplo:
#   ./wordcloud_gen.py concatenated.txt wcloud.jpg
#
# Dependencias:
#   pip install wordcloud matplotlib
#
# Si las dependencias no estan instaladas, el script NO falla silenciosamente:
#   avisa al usuario y genera un .txt con el top-30 de frecuencias como
#   fallback, para que el pipeline no se rompa durante una demo.
# ─────────────────────────────────────────────────────────────────────────

import sys
from collections import Counter


# ── stopwords: palabras que NO aportan significado al word cloud ────────
STOPWORDS_ES = {
    "a", "al", "ante", "bajo", "cabe", "con", "contra", "de", "del", "desde",
    "durante", "en", "entre", "hacia", "hasta", "mediante", "para", "por",
    "segun", "sin", "so", "sobre", "tras", "versus", "via",
    "el", "la", "los", "las", "un", "una", "unos", "unas",
    "y", "e", "ni", "o", "u", "pero", "mas", "sino", "aunque", "porque",
    "pues", "si", "ya", "que", "cual", "cuales", "quien", "quienes",
    "cuyo", "cuya", "cuyos", "cuyas", "cuanto", "cuanta", "cuantos", "cuantas",
    "donde", "cuando", "como",
    "este", "esta", "estos", "estas", "ese", "esa", "esos", "esas",
    "aquel", "aquella", "aquellos", "aquellas", "esto", "eso", "aquello",
    "mi", "mis", "tu", "tus", "su", "sus", "nuestro", "nuestra", "nuestros",
    "nuestras", "vuestro", "vuestra", "vuestros", "vuestras",
    "yo", "me", "mi", "conmigo", "ti", "contigo", "el", "ella", "ello",
    "nosotros", "nosotras", "vosotros", "vosotras", "ellos", "ellas",
    "se", "si", "consigo", "le", "les", "lo", "los", "la", "las",
    "ser", "es", "son", "era", "eran", "fue", "fueron", "sera", "seran",
    "siendo", "sido", "estar", "esta", "estan", "estaba", "estaban", "estuvo",
    "estuvieron", "estando", "estado",
    "haber", "ha", "han", "habia", "habian", "hay", "hubo", "habra",
    "no", "ni", "tampoco", "nunca", "jamas",
    "muy", "mucho", "mucha", "muchos", "muchas", "poco", "poca", "pocos",
    "pocas", "todo", "toda", "todos", "todas", "otro", "otra", "otros", "otras",
    "mismo", "misma", "mismos", "mismas", "tan", "tanto", "tanta", "tantos",
    "tantas", "solo", "solamente", "tambien", "asi", "aqui", "alli", "ahi",
    "entonces", "ahora", "antes", "despues", "luego", "siempre", "nunca",
}
STOPWORDS_EN = {
    "the", "a", "an", "and", "or", "but", "if", "then", "is", "are", "was",
    "were", "be", "been", "being", "to", "of", "in", "on", "at", "for", "with",
    "by", "from", "as", "that", "this", "these", "those", "it", "its", "we",
    "you", "they", "he", "she", "him", "her", "his", "their", "them",
}
STOPWORDS = STOPWORDS_ES | STOPWORDS_EN


def cargar_palabras(ruta):
    """Lee el archivo, una palabra por linea, filtra vacias y stopwords."""
    palabras = []
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            w = linea.strip()
            if not w:
                continue
            if w in STOPWORDS:
                continue
            if len(w) <= 2:          # descarta tokens muy cortos (ruido)
                continue
            if w.isdigit():          # descarta numeros sueltos
                continue
            palabras.append(w)
    return palabras


def generar_imagen(frecuencias, salida):
    """Genera la imagen JPG usando wordcloud + matplotlib."""
    from wordcloud import WordCloud
    import matplotlib
    matplotlib.use("Agg")            # backend sin display para servidores
    import matplotlib.pyplot as plt

    wc = WordCloud(
        width=1600, height=900,
        background_color="white",
        colormap="viridis",
        relative_scaling=0.5,
        min_font_size=10,
        max_words=100,
    ).generate_from_frequencies(frecuencias)

    plt.figure(figsize=(16, 9))
    plt.imshow(wc, interpolation="bilinear")
    plt.axis("off")
    plt.tight_layout(pad=0)
    plt.savefig(salida, format="jpeg", bbox_inches="tight", dpi=120)
    plt.close()


def fallback_texto(frecuencias, salida):
    """Si no hay dependencias, escribe el top-30 como .txt."""
    salida_txt = salida.rsplit(".", 1)[0] + ".txt"
    with open(salida_txt, "w", encoding="utf-8") as f:
        f.write("# Top 30 palabras mas frecuentes\n")
        for w, c in frecuencias.most_common(30):
            f.write(f"{c:4d}  {w}\n")
    return salida_txt


def main():
    if len(sys.argv) != 3:
        print("[wordcloud] uso: wordcloud_gen.py <entrada> <salida.jpg>",
              file=sys.stderr)
        sys.exit(2)

    entrada, salida = sys.argv[1], sys.argv[2]

    palabras = cargar_palabras(entrada)
    if not palabras:
        print(f"[wordcloud] {entrada} no contiene palabras utiles", file=sys.stderr)
        sys.exit(3)

    frecuencias = Counter(palabras)
    print(f"[wordcloud] {len(palabras)} tokens, {len(frecuencias)} palabras unicas")

    try:
        generar_imagen(frecuencias, salida)
        print(f"[wordcloud] generado: {salida}")
    except ImportError as e:
        archivo_fb = fallback_texto(frecuencias, salida)
        print(f"[wordcloud] ADVERTENCIA: faltan dependencias ({e.name}).",
              file=sys.stderr)
        print(f"[wordcloud] instala con: pip install wordcloud matplotlib",
              file=sys.stderr)
        print(f"[wordcloud] fallback generado: {archivo_fb}")


if __name__ == "__main__":
    main()
