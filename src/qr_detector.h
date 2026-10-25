#ifndef QR_DETECTOR_H
#define QR_DETECTOR_H

#include "image_processing.h"
#include "qr_tipos.h"

/*
 * Localização de QR Codes em uma imagem binarizada.
 *
 * A detecção é feita pelo projeto (não pela biblioteca de decodificação) e
 * se baseia nos padrões de localização do QR Code: os três quadrados
 * concêntricos presentes nos cantos do símbolo. Ao ser cortado por uma reta
 * que passa pelo seu centro, esse padrão produz sempre a sequência de
 * faixas escuro-claro-escuro-claro-escuro na proporção 1:1:3:1:1.
 *
 * O algoritmo tem três etapas:
 *
 *   1. varredura das linhas da imagem em busca de sequências com essa
 *      proporção, confirmando cada candidato com uma varredura vertical
 *      na coluna do centro encontrado;
 *   2. agrupamento dos candidatos: o mesmo padrão é encontrado em várias
 *      linhas vizinhas e precisa ser reduzido a um único centro;
 *   3. combinação dos padrões em trios que formem um triângulo retângulo
 *      isósceles, geometria característica dos três cantos de um QR Code,
 *      de onde se obtém o quadrilátero que delimita o símbolo.
 */

#define QR_MAX_PADROES 128  /* candidatos mantidos por imagem */

/* Padrão de localização confirmado na imagem. */
typedef struct {
    QrPonto centro;
    float   modulo;        /* lado estimado de um módulo, em pixels */
    int     confirmacoes;  /* em quantas linhas o padrão foi reencontrado */
} QrPadrao;

typedef struct {
    QrPadrao itens[QR_MAX_PADROES];
    int      quantidade;
} QrListaPadroes;

/*
 * Etapa 1 e 2: localiza os padrões de localização na imagem binarizada.
 * Retorna a quantidade de padrões confirmados (também disponível em
 * lista->quantidade).
 */
int qr_localizar_padroes(const ImagemCinza *binaria, QrListaPadroes *lista);

/*
 * Etapa 3: agrupa os padrões em QR Codes e preenche o resultado com as
 * regiões encontradas, ainda sem conteúdo decodificado.
 * Retorna a quantidade de códigos localizados.
 */
int qr_detectar(const ImagemCinza *binaria, QrResultado *resultado);

#endif
