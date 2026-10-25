#ifndef QR_TIPOS_H
#define QR_TIPOS_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Tipos compartilhados entre a detecção (qr_detector), a decodificação
 * (qr_decoder) e a interface gráfica (gui).
 *
 * Um QR Code é descrito por:
 *  - o quadrilátero que delimita a sua região na imagem (4 cantos);
 *  - os centros dos padrões de localização (os três quadrados concêntricos
 *    que aparecem nos cantos do símbolo);
 *  - o conteúdo decodificado, quando a leitura é bem-sucedida.
 */

#define QR_MAX_CODIGOS   8     /* quantidade máxima de códigos por imagem */
#define QR_MAX_CONTEUDO  2048  /* bytes do conteúdo decodificado (com '\0') */
#define QR_MAX_TIPO      32

/* Ponto em coordenadas de pixel da imagem de entrada. */
typedef struct {
    float x;
    float y;
} QrPonto;

/* Indica qual etapa foi responsável por localizar o código na imagem. */
typedef enum {
    QR_ORIGEM_DETECCAO = 0, /* padrões de localização encontrados por qr_detector */
    QR_ORIGEM_BIBLIOTECA    /* região informada pela biblioteca de decodificação */
} QrOrigem;

typedef struct {
    /* Cantos na ordem: 0 = superior esquerdo, 1 = superior direito,
     * 2 = inferior direito, 3 = inferior esquerdo. */
    QrPonto  cantos[4];

    QrPonto  padroes[3];        /* centros dos padrões de localização */
    int      num_padroes;

    QrPonto  centro;            /* centro geométrico do quadrilátero */
    float    modulo;            /* lado estimado de um módulo, em pixels */
    int      modulos;           /* largura estimada do símbolo em módulos */
    int      versao;            /* versão estimada do QR (1 a 40), 0 se desconhecida */

    QrOrigem origem;
    bool     decodificado;
    char     tipo[QR_MAX_TIPO]; /* simbologia informada pela biblioteca */
    char     conteudo[QR_MAX_CONTEUDO];
    int      tamanho_conteudo;  /* bytes úteis em conteudo */
    int      qualidade;         /* confiança informada pela biblioteca */
} QrCodigo;

typedef struct {
    QrCodigo codigos[QR_MAX_CODIGOS];
    int      quantidade;
    int      padroes_localizados;  /* padrões de localização confirmados na imagem */
} QrResultado;

/* Distância euclidiana entre dois pontos. */
static inline float qr_distancia(QrPonto a, QrPonto b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return SDL_sqrtf(dx * dx + dy * dy);
}

/* Centro geométrico (média dos vértices) de um quadrilátero. */
static inline QrPonto qr_centro_quadrilatero(const QrPonto cantos[4]) {
    QrPonto centro = { 0.0f, 0.0f };

    for (int i = 0; i < 4; ++i) {
        centro.x += cantos[i].x;
        centro.y += cantos[i].y;
    }

    centro.x /= 4.0f;
    centro.y /= 4.0f;

    return centro;
}

/* Maior distância entre o centro e os cantos: usado como raio de tolerância
 * ao associar um símbolo decodificado a uma região já detectada. */
static inline float qr_raio_quadrilatero(const QrPonto cantos[4], QrPonto centro) {
    float raio = 0.0f;

    for (int i = 0; i < 4; ++i) {
        const float d = qr_distancia(cantos[i], centro);

        if (d > raio) {
            raio = d;
        }
    }

    return raio;
}

/* Ordena quatro pontos no sentido horário a partir do canto superior
 * esquerdo, para que o contorno desenhado não se cruze. */
void qr_ordenar_quadrilatero(QrPonto cantos[4]);

/* Quantidade de códigos efetivamente decodificados em um resultado. */
int qr_total_decodificados(const QrResultado *resultado);

/* Testa se um ponto está dentro do quadrilátero (assumido convexo). */
bool qr_ponto_no_quadrilatero(const QrPonto cantos[4], QrPonto ponto);

#endif
