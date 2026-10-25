#ifndef QR_DECODER_H
#define QR_DECODER_H

#include "image_processing.h"
#include "qr_tipos.h"

/*
 * Decodificação do conteúdo dos QR Codes.
 *
 * Esta é a única etapa delegada a uma biblioteca externa (ZBar), conforme
 * previsto no escopo do projeto: a recuperação dos bits do símbolo envolve
 * correção de erros Reed-Solomon, remoção de máscara e decodificação dos
 * modos de codificação, o que está fora do objetivo do trabalho.
 *
 * O módulo recebe a imagem em escala de cinza (a biblioteca aplica a sua
 * própria binarização interna) e o resultado produzido por qr_detector,
 * associando cada conteúdo lido à região correspondente já localizada.
 * Símbolos que a biblioteca leu mas que a detecção própria não localizou
 * são acrescentados ao resultado marcados como QR_ORIGEM_BIBLIOTECA.
 */

/*
 * Decodifica os QR Codes da imagem e preenche o conteúdo das regiões de
 * `resultado`. Retorna a quantidade de símbolos lidos, ou -1 em caso de
 * falha ao preparar a biblioteca.
 */
int qr_decodificar(const ImagemCinza *cinza, QrResultado *resultado);

/* Versão da biblioteca de decodificação, para exibição na interface. */
const char *qr_decodificador_descricao(void);

#endif
