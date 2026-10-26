#ifndef ANALISE_H
#define ANALISE_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "image_processing.h"
#include "qr_tipos.h"

/*
 * Análise de uma imagem: encadeia as etapas de processamento e guarda
 * tudo o que a interface precisa exibir.
 *
 *   1. image_loader      carrega o arquivo e converte para RGBA32;
 *   2. image_processing  converte para escala de cinza e binariza;
 *   3. qr_detector       localiza os padrões de localização e a região
 *                        de cada QR Code;
 *   4. qr_decoder        decodifica o conteúdo (biblioteca ZBar) e o
 *                        associa às regiões localizadas.
 *
 * Este módulo não desenha nada: produz a imagem binarizada pronta para
 * exibição, o conjunto de códigos encontrados e as linhas de texto que
 * descrevem o resultado.
 */

#define ANALISE_MAX_CAMINHO 1024
#define ANALISE_MAX_LINHAS  8     /* linhas do resumo */
#define ANALISE_MAX_LINHA   160   /* caracteres por linha do resumo */

typedef struct {
    char caminho[ANALISE_MAX_CAMINHO];

    SDL_Surface *imagem;           /* imagem carregada, em RGBA32 */
    SDL_Surface *visual_binaria;   /* binarização, pronta para exibição */

    ImagemCinza cinza;
    ImagemCinza binaria;

    QrResultado resultado;

    double media;              /* intensidade média, em tons de cinza */
    int    limiar_otsu;        /* limiar calculado pelo método de Otsu */
    int    janela_adaptativa;  /* lado da janela da binarização adaptativa */
    bool   usou_adaptativa;    /* qual binarização produziu o resultado */

    bool valida;               /* false enquanto nenhuma imagem for analisada */
} Analise;

/*
 * Linhas de texto que descrevem a análise, prontas para a interface.
 * `linhas` aponta para as posições de `textos`, no formato esperado por
 * GuiDados.
 */
typedef struct {
    char        textos[ANALISE_MAX_LINHAS][ANALISE_MAX_LINHA];
    const char *linhas[ANALISE_MAX_LINHAS];
    int         quantidade;
} AnaliseResumo;

/* Deixa a análise em estado vazio. Deve ser chamada antes do primeiro uso. */
void analise_iniciar(Analise *analise);

/* Libera a imagem e os buffers. Seguro chamar mais de uma vez. */
void analise_liberar(Analise *analise);

/*
 * Executa todas as etapas sobre um arquivo de imagem, substituindo o
 * conteúdo anterior da análise, e imprime um resumo no terminal.
 * Retorna false se a imagem não puder ser lida ou processada.
 */
bool analise_executar(Analise *analise, const char *caminho);

/* Preenche o resumo exibido na janela de leitura. */
void analise_descrever(
    const Analise *analise,
    int selecionado,
    AnaliseResumo *resumo
);

/*
 * Índice do primeiro código decodificado, usado como seleção inicial.
 * Retorna 0 se houver apenas regiões sem leitura e -1 se não houver
 * nenhum código.
 */
int analise_primeiro_codigo(const Analise *analise);

#endif
