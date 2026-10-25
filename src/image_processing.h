#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Pré-processamento da imagem para a detecção do QR Code.
 *
 * A detecção trabalha sobre uma imagem de um canal (8 bits por pixel),
 * representada por ImagemCinza. O fluxo é:
 *
 *   superfície RGBA32  ->  escala de cinza  ->  binarização  ->  detecção
 */

#define IMG_NIVEIS 256  /* níveis de intensidade do histograma */

/* Convenção usada nas imagens binarizadas. */
enum {
    IMG_ESCURO = 0,    /* módulo escuro do QR Code */
    IMG_CLARO  = 255   /* fundo claro */
};

/* Imagem de um canal, com um byte por pixel e linhas contíguas. */
typedef struct {
    Uint8 *dados;    /* largura * altura bytes */
    int    largura;
    int    altura;
} ImagemCinza;

/* Aloca o buffer de uma imagem de um canal. Retorna false se faltar memória. */
bool cinza_criar(ImagemCinza *imagem, int largura, int altura);

/* Libera o buffer. Seguro chamar mais de uma vez ou em imagem não criada. */
void cinza_liberar(ImagemCinza *imagem);

/* Lê o valor de um pixel, devolvendo IMG_CLARO fora dos limites. */
Uint8 cinza_pixel(const ImagemCinza *imagem, int x, int y);

/*
 * Converte uma superfície RGBA32 para escala de cinza usando a
 * luminância perceptual:
 *
 *   Y = 0.2125 * R + 0.7154 * G + 0.0721 * B
 */
bool converter_para_cinza(const SDL_Surface *imagem, ImagemCinza *destino);

/* Histograma de 256 níveis da imagem em escala de cinza. */
void calcular_histograma(
    const ImagemCinza *imagem,
    Uint32 histograma[IMG_NIVEIS]
);

/*
 * Calcula o limiar que separa objeto e fundo pelo método de Otsu,
 * maximizando a variância entre as duas classes do histograma.
 */
int calcular_limiar_otsu(
    const Uint32 histograma[IMG_NIVEIS],
    Uint64 total_pixels
);

/* Média das intensidades a partir do histograma. */
double calcular_media(
    const Uint32 histograma[IMG_NIVEIS],
    Uint64 total_pixels
);

/*
 * Binarização global: pixels com intensidade menor ou igual ao limiar
 * tornam-se IMG_ESCURO e os demais IMG_CLARO.
 */
bool binarizar_global(
    const ImagemCinza *entrada,
    int limiar,
    ImagemCinza *saida
);

/*
 * Binarização adaptativa pela média local.
 *
 * Cada pixel é comparado com a média de uma janela quadrada centrada nele,
 * descontada de `desvio`. A média é obtida em tempo constante por pixel
 * usando uma imagem integral, o que mantém o custo linear no total de
 * pixels. É mais robusta que a binarização global em fotografias com
 * iluminação irregular ou sombras sobre o código.
 *
 * `janela` é o lado da vizinhança em pixels (ajustado para ímpar e para um
 * mínimo de 3). Passe 0 para que o tamanho seja estimado a partir das
 * dimensões da imagem.
 */
bool binarizar_adaptativo(
    const ImagemCinza *entrada,
    int janela,
    int desvio,
    ImagemCinza *saida
);

/* Tamanho de janela sugerido para a binarização adaptativa. */
int janela_adaptativa_sugerida(const ImagemCinza *imagem);

/* Converte uma imagem de um canal em superfície RGBA32, para exibição. */
SDL_Surface *cinza_para_superficie(const ImagemCinza *imagem);

#endif
