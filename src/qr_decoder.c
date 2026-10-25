#include "qr_decoder.h"

#include <stdio.h>
#include <string.h>

#include <zbar.h>

/*
 * Extrai o quadrilátero que a biblioteca associou ao símbolo.
 *
 * Para QR Codes a ZBar costuma devolver quatro pontos (os cantos). Nesse
 * caso eles são reordenados no sentido horário para que o contorno
 * desenhado pela interface não se cruze. Com qualquer outra quantidade de
 * pontos usa-se a caixa envolvente.
 */
static bool quadrilatero_do_simbolo(
    const zbar_symbol_t *simbolo,
    QrPonto cantos[4]
) {
    const unsigned total = zbar_symbol_get_loc_size(simbolo);

    if (total == 0) {
        return false;
    }

    float minimo_x = 0.0f;
    float minimo_y = 0.0f;
    float maximo_x = 0.0f;
    float maximo_y = 0.0f;

    for (unsigned i = 0; i < total; ++i) {
        const float x = (float)zbar_symbol_get_loc_x(simbolo, i);
        const float y = (float)zbar_symbol_get_loc_y(simbolo, i);

        if (i == 0) {
            minimo_x = x;
            maximo_x = x;
            minimo_y = y;
            maximo_y = y;
        } else {
            if (x < minimo_x) {
                minimo_x = x;
            }

            if (x > maximo_x) {
                maximo_x = x;
            }

            if (y < minimo_y) {
                minimo_y = y;
            }

            if (y > maximo_y) {
                maximo_y = y;
            }
        }
    }

    if (total == 4) {
        for (unsigned i = 0; i < 4; ++i) {
            cantos[i].x = (float)zbar_symbol_get_loc_x(simbolo, i);
            cantos[i].y = (float)zbar_symbol_get_loc_y(simbolo, i);
        }

        qr_ordenar_quadrilatero(cantos);

        return true;
    }

    cantos[0].x = minimo_x;
    cantos[0].y = minimo_y;
    cantos[1].x = maximo_x;
    cantos[1].y = minimo_y;
    cantos[2].x = maximo_x;
    cantos[2].y = maximo_y;
    cantos[3].x = minimo_x;
    cantos[3].y = maximo_y;

    return true;
}

/*
 * Procura, entre as regiões já localizadas pela detecção própria, aquela
 * que contém o centro do símbolo lido. Retorna -1 quando o símbolo não
 * corresponde a nenhuma região ainda sem conteúdo.
 */
static int regiao_correspondente(
    const QrResultado *resultado,
    QrPonto centro_simbolo
) {
    int escolhida = -1;
    float menor_distancia = 0.0f;

    for (int i = 0; i < resultado->quantidade; ++i) {
        const QrCodigo *codigo = &resultado->codigos[i];

        if (codigo->decodificado) {
            continue;
        }

        const float distancia = qr_distancia(codigo->centro, centro_simbolo);
        const float raio = qr_raio_quadrilatero(codigo->cantos, codigo->centro);

        if (distancia > raio) {
            continue;
        }

        if (escolhida < 0 || distancia < menor_distancia) {
            escolhida = i;
            menor_distancia = distancia;
        }
    }

    return escolhida;
}

/* Copia o conteúdo lido para a região, respeitando o limite do buffer. */
static void guardar_conteudo(
    QrCodigo *codigo,
    const zbar_symbol_t *simbolo
) {
    const char *dados = zbar_symbol_get_data(simbolo);
    const unsigned int tamanho = zbar_symbol_get_data_length(simbolo);

    unsigned int copiados = tamanho;

    if (copiados > QR_MAX_CONTEUDO - 1) {
        copiados = QR_MAX_CONTEUDO - 1;
    }

    if (dados != NULL && copiados > 0) {
        memcpy(codigo->conteudo, dados, copiados);
    }

    codigo->conteudo[copiados] = '\0';
    codigo->tamanho_conteudo = (int)copiados;
    codigo->decodificado = true;
    codigo->qualidade = zbar_symbol_get_quality(simbolo);

    const char *nome = zbar_get_symbol_name(zbar_symbol_get_type(simbolo));

    SDL_strlcpy(
        codigo->tipo,
        (nome != NULL) ? nome : "QR-Code",
        QR_MAX_TIPO
    );
}

int qr_decodificar(const ImagemCinza *cinza, QrResultado *resultado) {
    if (cinza == NULL || cinza->dados == NULL || resultado == NULL) {
        return -1;
    }

    zbar_image_scanner_t *leitor = zbar_image_scanner_create();

    if (leitor == NULL) {
        fprintf(
            stderr,
            "Erro ao criar o leitor da biblioteca de decodificacao.\n"
        );

        return -1;
    }

    /*
     * Desabilita todas as simbologias e habilita apenas QR Code: evita
     * que códigos de barras lineares presentes na imagem sejam lidos.
     */
    zbar_image_scanner_set_config(leitor, 0, ZBAR_CFG_ENABLE, 0);
    zbar_image_scanner_set_config(leitor, ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);

    zbar_image_t *imagem = zbar_image_create();

    if (imagem == NULL) {
        zbar_image_scanner_destroy(leitor);

        fprintf(stderr, "Erro ao preparar a imagem para a decodificacao.\n");

        return -1;
    }

    /* Y800: um canal de 8 bits, exatamente o formato de ImagemCinza. */
    zbar_image_set_format(imagem, zbar_fourcc('Y', '8', '0', '0'));

    zbar_image_set_size(
        imagem,
        (unsigned)cinza->largura,
        (unsigned)cinza->altura
    );

    /*
     * O último parâmetro é a função de liberação: NULL porque o buffer
     * pertence ao chamador e continua válido depois da leitura.
     */
    zbar_image_set_data(
        imagem,
        cinza->dados,
        (unsigned long)cinza->largura * (unsigned long)cinza->altura,
        NULL
    );

    const int encontrados = zbar_scan_image(leitor, imagem);

    if (encontrados < 0) {
        zbar_image_destroy(imagem);
        zbar_image_scanner_destroy(leitor);

        fprintf(stderr, "Erro durante a decodificacao da imagem.\n");

        return -1;
    }

    int lidos = 0;

    const zbar_symbol_t *simbolo = zbar_image_first_symbol(imagem);

    for (; simbolo != NULL; simbolo = zbar_symbol_next(simbolo)) {
        if (zbar_symbol_get_type(simbolo) != ZBAR_QRCODE) {
            continue;
        }

        QrPonto cantos[4];

        if (!quadrilatero_do_simbolo(simbolo, cantos)) {
            continue;
        }

        const QrPonto centro = qr_centro_quadrilatero(cantos);
        const int indice = regiao_correspondente(resultado, centro);

        if (indice >= 0) {
            /*
             * A região já foi localizada pela detecção própria: mantém-se
             * o quadrilátero calculado a partir dos padrões de localização
             * e apenas o conteúdo é preenchido.
             */
            guardar_conteudo(&resultado->codigos[indice], simbolo);

            ++lidos;

            continue;
        }

        /*
         * Símbolo lido fora de qualquer região detectada: a posição vem
         * da própria biblioteca e isso é sinalizado na interface.
         */
        if (resultado->quantidade >= QR_MAX_CODIGOS) {
            continue;
        }

        QrCodigo *codigo = &resultado->codigos[resultado->quantidade++];

        memset(codigo, 0, sizeof(*codigo));

        for (int i = 0; i < 4; ++i) {
            codigo->cantos[i] = cantos[i];
        }

        codigo->centro = centro;
        codigo->origem = QR_ORIGEM_BIBLIOTECA;
        codigo->num_padroes = 0;
        codigo->modulo = 0.0f;
        codigo->versao = 0;
        codigo->modulos = 0;

        guardar_conteudo(codigo, simbolo);

        ++lidos;
    }

    zbar_image_destroy(imagem);
    zbar_image_scanner_destroy(leitor);

    return lidos;
}

const char *qr_decodificador_descricao(void) {
    static char descricao[64];

    unsigned maior = 0;
    unsigned menor = 0;
    unsigned correcao = 0;

    zbar_version(&maior, &menor, &correcao);

    SDL_snprintf(
        descricao,
        sizeof(descricao),
        "ZBar %u.%u.%u",
        maior,
        menor,
        correcao
    );

    return descricao;
}
