#include "image_loader.h"

#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <string.h>

/*
 * Leitura do cabeçalho de um PNG.
 *
 * A assinatura do formato tem 8 bytes e é sempre seguida pelo bloco IHDR,
 * cujos campos de profundidade e de tipo de cor ocupam o 25o e o 26o byte
 * do arquivo. Esses dois valores são usados para tratar dois problemas da
 * versão instalada do SDL3_image (3.4.x), descritos nas funções abaixo.
 *
 * Retorna false quando o arquivo não é um PNG.
 */
static bool ler_cabecalho_png(
    const char *caminho,
    unsigned char *profundidade,
    unsigned char *tipo_de_cor
) {
    static const unsigned char assinatura[8] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
    };

    unsigned char cabecalho[26];

    SDL_IOStream *arquivo = SDL_IOFromFile(caminho, "rb");

    if (arquivo == NULL) {
        return false;
    }

    const size_t lidos = SDL_ReadIO(arquivo, cabecalho, sizeof(cabecalho));

    SDL_CloseIO(arquivo);

    if (lidos < sizeof(cabecalho)) {
        return false;
    }

    if (memcmp(cabecalho, assinatura, sizeof(assinatura)) != 0 ||
        memcmp(cabecalho + 12, "IHDR", 4) != 0) {

        return false;
    }

    *profundidade = cabecalho[24];
    *tipo_de_cor = cabecalho[25];

    return true;
}

/*
 * Ajusta a paleta de um PNG em tons de cinza com menos de 8 bits por pixel.
 *
 * Esses arquivos são carregados como imagem indexada, mas o SDL3_image
 * monta a paleta com um degradê que não chega ao branco: em uma imagem de
 * 1 bit, o valor claro vira 127 em vez de 255, e a imagem aparece
 * acinzentada. Como o PNG é, por definição do seu tipo de cor, uma escala
 * de cinza uniforme, a paleta é reescrita para cobrir a faixa completa.
 *
 * A detecção não depende disso (toda a análise é por limiar, portanto
 * relativa), mas a imagem exibida e a imagem salva ficam corretas.
 */
static void normalizar_paleta_cinza(SDL_Surface *imagem) {
    SDL_Palette *paleta = SDL_GetSurfacePalette(imagem);

    if (paleta == NULL || paleta->ncolors < 2 || paleta->ncolors > 16) {
        return;
    }

    int maior = 0;

    for (int i = 0; i < paleta->ncolors; ++i) {
        if (paleta->colors[i].r > maior) {
            maior = paleta->colors[i].r;
        }
    }

    if (maior == 0 || maior == 255) {
        return;
    }

    SDL_Color cores[16];

    for (int i = 0; i < paleta->ncolors; ++i) {
        const Uint8 valor =
            (Uint8)(i * 255 / (paleta->ncolors - 1));

        cores[i].r = valor;
        cores[i].g = valor;
        cores[i].b = valor;
        cores[i].a = paleta->colors[i].a;
    }

    SDL_SetPaletteColors(paleta, cores, 0, paleta->ncolors);
}

SDL_Surface *carregar_imagem(const char *caminho) {
    if (caminho == NULL || caminho[0] == '\0') {
        fprintf(stderr, "Erro: nenhum caminho de imagem foi informado.\n");
        return NULL;
    }

    unsigned char profundidade = 0;
    unsigned char tipo_de_cor = 0;

    const bool eh_png =
        ler_cabecalho_png(caminho, &profundidade, &tipo_de_cor);

    /* Tipo de cor 0 no PNG significa tons de cinza sem canal alfa. */
    const bool png_cinza = eh_png && tipo_de_cor == 0;

    /*
     * Um PNG em tons de cinza com 16 bits por canal encerra o processo
     * dentro do IMG_Load desta versão do SDL3_image, sem retornar erro.
     * Não há como tratar a falha depois que ela acontece, então o caso é
     * recusado antes da leitura.
     */
    if (png_cinza && profundidade == 16) {
        fprintf(
            stderr,
            "Erro: '%s' e um PNG em tons de cinza de 16 bits por canal,\n"
            "formato que a versao instalada do SDL3_image nao consegue ler.\n"
            "Converta a imagem para 8 bits por canal e tente novamente.\n",
            caminho
        );

        return NULL;
    }

    SDL_Surface *original = IMG_Load(caminho);

    if (original == NULL) {
        fprintf(
            stderr,
            "Erro ao carregar '%s': %s\n",
            caminho,
            SDL_GetError()
        );

        return NULL;
    }

    printf(
        "Imagem '%s' carregada (%dx%d, %d bpp).\n",
        caminho,
        original->w,
        original->h,
        SDL_BITSPERPIXEL(original->format)
    );

    if (png_cinza && profundidade < 8) {
        normalizar_paleta_cinza(original);
    }

    /*
     * Converte para RGBA32 para garantir um formato previsível no acesso
     * aos pixels durante o processamento.
     */
    SDL_Surface *imagem = SDL_ConvertSurface(
        original,
        SDL_PIXELFORMAT_RGBA32
    );

    SDL_DestroySurface(original);

    if (imagem == NULL) {
        fprintf(
            stderr,
            "Erro ao converter formato da imagem: %s\n",
            SDL_GetError()
        );

        return NULL;
    }

    return imagem;
}
