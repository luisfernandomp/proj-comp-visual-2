#include "image_processing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool cinza_criar(ImagemCinza *imagem, int largura, int altura) {
    if (imagem == NULL || largura <= 0 || altura <= 0) {
        return false;
    }

    imagem->dados = (Uint8 *)malloc((size_t)largura * (size_t)altura);

    if (imagem->dados == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para a imagem.\n");

        imagem->largura = 0;
        imagem->altura = 0;

        return false;
    }

    imagem->largura = largura;
    imagem->altura = altura;

    return true;
}

void cinza_liberar(ImagemCinza *imagem) {
    if (imagem == NULL) {
        return;
    }

    free(imagem->dados);

    imagem->dados = NULL;
    imagem->largura = 0;
    imagem->altura = 0;
}

Uint8 cinza_pixel(const ImagemCinza *imagem, int x, int y) {
    if (imagem == NULL || imagem->dados == NULL ||
        x < 0 || y < 0 || x >= imagem->largura || y >= imagem->altura) {

        return IMG_CLARO;
    }

    return imagem->dados[(size_t)y * (size_t)imagem->largura + (size_t)x];
}

bool converter_para_cinza(const SDL_Surface *imagem, ImagemCinza *destino) {
    if (imagem == NULL || destino == NULL) {
        return false;
    }

    const SDL_PixelFormatDetails *detalhes =
        SDL_GetPixelFormatDetails(imagem->format);

    if (detalhes == NULL || detalhes->bytes_per_pixel != 4) {
        fprintf(
            stderr,
            "Erro: a conversao para cinza espera uma imagem RGBA32.\n"
        );

        return false;
    }

    if (!cinza_criar(destino, imagem->w, imagem->h)) {
        return false;
    }

    const Uint8 *linha_base = (const Uint8 *)imagem->pixels;

    for (int y = 0; y < imagem->h; ++y) {
        const Uint32 *linha = (const Uint32 *)(const void *)
            (linha_base + (size_t)y * (size_t)imagem->pitch);

        Uint8 *saida = destino->dados + (size_t)y * (size_t)destino->largura;

        for (int x = 0; x < imagem->w; ++x) {
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;

            SDL_GetRGB(linha[x], detalhes, NULL, &r, &g, &b);

            /*
             * Luminância perceptual: o canal verde recebe o maior peso
             * porque a visão humana é mais sensível a ele.
             */
            const double luminancia =
                0.2125 * (double)r +
                0.7154 * (double)g +
                0.0721 * (double)b;

            int valor = (int)(luminancia + 0.5);

            if (valor < 0) {
                valor = 0;
            } else if (valor > 255) {
                valor = 255;
            }

            saida[x] = (Uint8)valor;
        }
    }

    return true;
}

void calcular_histograma(
    const ImagemCinza *imagem,
    Uint32 histograma[IMG_NIVEIS]
) {
    memset(histograma, 0, sizeof(Uint32) * IMG_NIVEIS);

    if (imagem == NULL || imagem->dados == NULL) {
        return;
    }

    const size_t total = (size_t)imagem->largura * (size_t)imagem->altura;

    for (size_t i = 0; i < total; ++i) {
        ++histograma[imagem->dados[i]];
    }
}

double calcular_media(
    const Uint32 histograma[IMG_NIVEIS],
    Uint64 total_pixels
) {
    if (total_pixels == 0) {
        return 0.0;
    }

    double soma = 0.0;

    for (int i = 0; i < IMG_NIVEIS; ++i) {
        soma += (double)i * (double)histograma[i];
    }

    return soma / (double)total_pixels;
}

int calcular_limiar_otsu(
    const Uint32 histograma[IMG_NIVEIS],
    Uint64 total_pixels
) {
    if (total_pixels == 0) {
        return 128;
    }

    /*
     * Soma ponderada de todas as intensidades: permite obter a média de
     * cada classe de forma incremental, sem percorrer o histograma
     * novamente para cada limiar testado.
     */
    double soma_total = 0.0;

    for (int i = 0; i < IMG_NIVEIS; ++i) {
        soma_total += (double)i * (double)histograma[i];
    }

    double peso_fundo = 0.0;   /* pixels com intensidade <= limiar */
    double soma_fundo = 0.0;
    double maior_variancia = -1.0;

    int limiar = 128;

    for (int t = 0; t < IMG_NIVEIS; ++t) {
        peso_fundo += (double)histograma[t];

        if (peso_fundo == 0.0) {
            continue;
        }

        const double peso_objeto = (double)total_pixels - peso_fundo;

        if (peso_objeto <= 0.0) {
            break;
        }

        soma_fundo += (double)t * (double)histograma[t];

        const double media_fundo  = soma_fundo / peso_fundo;
        const double media_objeto = (soma_total - soma_fundo) / peso_objeto;

        /*
         * Variância entre classes. O limiar que a maximiza é o que melhor
         * separa os dois grupos de intensidade da imagem.
         */
        const double diferenca = media_fundo - media_objeto;
        const double variancia =
            peso_fundo * peso_objeto * diferenca * diferenca;

        if (variancia > maior_variancia) {
            maior_variancia = variancia;
            limiar = t;
        }
    }

    return limiar;
}

/* Garante que saida tenha o mesmo tamanho de entrada. */
static bool preparar_saida(const ImagemCinza *entrada, ImagemCinza *saida) {
    if (saida->dados != NULL &&
        saida->largura == entrada->largura &&
        saida->altura == entrada->altura) {

        return true;
    }

    cinza_liberar(saida);

    return cinza_criar(saida, entrada->largura, entrada->altura);
}

bool binarizar_global(
    const ImagemCinza *entrada,
    int limiar,
    ImagemCinza *saida
) {
    if (entrada == NULL || entrada->dados == NULL || saida == NULL) {
        return false;
    }

    if (!preparar_saida(entrada, saida)) {
        return false;
    }

    const size_t total = (size_t)entrada->largura * (size_t)entrada->altura;

    for (size_t i = 0; i < total; ++i) {
        saida->dados[i] =
            (entrada->dados[i] <= (Uint8)limiar) ? IMG_ESCURO : IMG_CLARO;
    }

    return true;
}

int janela_adaptativa_sugerida(const ImagemCinza *imagem) {
    if (imagem == NULL) {
        return 15;
    }

    const int menor_lado =
        (imagem->largura < imagem->altura) ? imagem->largura : imagem->altura;

    /*
     * A janela precisa ser bem maior que um módulo do código: dentro de uma
     * região uniforme maior que a janela, a média local se iguala ao próprio
     * pixel e o resultado se inverte. Um oitavo do menor lado cobre com
     * folga os módulos dos códigos que ocupam uma parte razoável da imagem.
     */
    int janela = menor_lado / 8;

    if (janela < 15) {
        janela = 15;
    }

    if (janela > 151) {
        janela = 151;
    }

    if (janela % 2 == 0) {
        ++janela;
    }

    return janela;
}

bool binarizar_adaptativo(
    const ImagemCinza *entrada,
    int janela,
    int desvio,
    ImagemCinza *saida
) {
    if (entrada == NULL || entrada->dados == NULL || saida == NULL) {
        return false;
    }

    if (janela <= 0) {
        janela = janela_adaptativa_sugerida(entrada);
    }

    if (janela < 3) {
        janela = 3;
    }

    if (janela % 2 == 0) {
        ++janela;
    }

    if (!preparar_saida(entrada, saida)) {
        return false;
    }

    const int largura = entrada->largura;
    const int altura = entrada->altura;

    /*
     * Imagem integral: integral[(y+1)*(largura+1) + (x+1)] guarda a soma de
     * todos os pixels do retângulo (0,0)-(x,y). Com ela a soma de qualquer
     * retângulo é obtida com quatro acessos, o que mantém o custo da
     * binarização linear no total de pixels independente da janela.
     */
    const size_t colunas = (size_t)largura + 1;
    const size_t linhas = (size_t)altura + 1;

    Uint64 *integral = (Uint64 *)calloc(linhas * colunas, sizeof(Uint64));

    if (integral == NULL) {
        fprintf(
            stderr,
            "Erro: memoria insuficiente para a binarizacao adaptativa.\n"
        );

        return false;
    }

    for (int y = 0; y < altura; ++y) {
        const Uint8 *linha = entrada->dados + (size_t)y * (size_t)largura;

        Uint64 *atual = integral + (size_t)(y + 1) * colunas;
        const Uint64 *anterior = integral + (size_t)y * colunas;

        Uint64 soma_linha = 0;

        for (int x = 0; x < largura; ++x) {
            soma_linha += linha[x];
            atual[x + 1] = anterior[x + 1] + soma_linha;
        }
    }

    const int raio = janela / 2;

    for (int y = 0; y < altura; ++y) {
        int y0 = y - raio;
        int y1 = y + raio;

        if (y0 < 0) {
            y0 = 0;
        }

        if (y1 > altura - 1) {
            y1 = altura - 1;
        }

        const Uint8 *linha_entrada = entrada->dados + (size_t)y * (size_t)largura;
        Uint8 *linha_saida = saida->dados + (size_t)y * (size_t)largura;

        for (int x = 0; x < largura; ++x) {
            int x0 = x - raio;
            int x1 = x + raio;

            if (x0 < 0) {
                x0 = 0;
            }

            if (x1 > largura - 1) {
                x1 = largura - 1;
            }

            const Uint64 soma =
                  integral[(size_t)(y1 + 1) * colunas + (size_t)(x1 + 1)]
                - integral[(size_t)y0 * colunas + (size_t)(x1 + 1)]
                - integral[(size_t)(y1 + 1) * colunas + (size_t)x0]
                + integral[(size_t)y0 * colunas + (size_t)x0];

            const Uint64 area = (Uint64)(y1 - y0 + 1) * (Uint64)(x1 - x0 + 1);
            const double media = (double)soma / (double)area;

            linha_saida[x] =
                ((double)linha_entrada[x] <= media - (double)desvio)
                    ? IMG_ESCURO
                    : IMG_CLARO;
        }
    }

    free(integral);

    return true;
}

SDL_Surface *cinza_para_superficie(const ImagemCinza *imagem) {
    if (imagem == NULL || imagem->dados == NULL) {
        return NULL;
    }

    SDL_Surface *superficie = SDL_CreateSurface(
        imagem->largura,
        imagem->altura,
        SDL_PIXELFORMAT_RGBA32
    );

    if (superficie == NULL) {
        fprintf(
            stderr,
            "Erro ao criar superficie de visualizacao: %s\n",
            SDL_GetError()
        );

        return NULL;
    }

    const SDL_PixelFormatDetails *detalhes =
        SDL_GetPixelFormatDetails(superficie->format);

    Uint8 *linha_base = (Uint8 *)superficie->pixels;

    for (int y = 0; y < imagem->altura; ++y) {
        Uint32 *linha = (Uint32 *)(void *)
            (linha_base + (size_t)y * (size_t)superficie->pitch);

        const Uint8 *entrada =
            imagem->dados + (size_t)y * (size_t)imagem->largura;

        for (int x = 0; x < imagem->largura; ++x) {
            const Uint8 v = entrada[x];

            linha[x] = SDL_MapRGBA(detalhes, NULL, v, v, v, 255);
        }
    }

    return superficie;
}
