#include "qr_detector.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tolerância do teste de proporção, como fração do módulo estimado.
 * Um valor de 0,5 aceita que cada faixa varie meio módulo em relação ao
 * tamanho ideal, o que absorve o erro de arredondamento da binarização.
 */
#define QR_TOLERANCIA 0.5f

/* Número de padrões mais confiáveis considerados na combinação em trios. */
#define QR_PADROES_COMBINADOS 48

/* Distância mínima e máxima entre centros de padrões, em módulos.
 * Um QR versão 1 (21 módulos) tem 14 módulos entre centros; um QR
 * versão 40 (177 módulos) tem 170. */
#define QR_DISTANCIA_MINIMA  10.0f
#define QR_DISTANCIA_MAXIMA 180.0f

/* ------------------------------------------------------------------------- */
/* Etapa 1: proporção 1:1:3:1:1                                              */
/* ------------------------------------------------------------------------- */

/*
 * Verifica se cinco faixas consecutivas (escuro-claro-escuro-claro-escuro)
 * respeitam a proporção 1:1:3:1:1 do padrão de localização.
 *
 * O padrão tem 7 módulos de largura, então o módulo estimado é a soma das
 * faixas dividida por 7.
 */
static bool proporcao_valida(const int faixas[5], float *modulo_estimado) {
    int total = 0;

    for (int i = 0; i < 5; ++i) {
        if (faixas[i] <= 0) {
            return false;
        }

        total += faixas[i];
    }

    /* Abaixo de 7 pixels não há como distinguir os 7 módulos. */
    if (total < 7) {
        return false;
    }

    const float modulo = (float)total / 7.0f;
    const float variacao = modulo * QR_TOLERANCIA;

    if (fabsf(modulo - (float)faixas[0]) >= variacao ||
        fabsf(modulo - (float)faixas[1]) >= variacao ||
        fabsf(modulo - (float)faixas[3]) >= variacao ||
        fabsf(modulo - (float)faixas[4]) >= variacao) {

        return false;
    }

    /* A faixa central vale 3 módulos e por isso recebe o triplo da folga. */
    if (fabsf(3.0f * modulo - (float)faixas[2]) >= 3.0f * variacao) {
        return false;
    }

    *modulo_estimado = modulo;

    return true;
}

/* Lê um pixel ao longo do eixo escolhido, mantendo a outra coordenada fixa. */
static bool escuro_no_eixo(
    const ImagemCinza *binaria,
    bool vertical,
    int cx,
    int cy,
    int posicao
) {
    const int x = vertical ? cx : posicao;
    const int y = vertical ? posicao : cy;

    return cinza_pixel(binaria, x, y) == IMG_ESCURO;
}

/*
 * Confirma um candidato percorrendo o eixo perpendicular ao da varredura
 * que o encontrou. Partindo do centro, conta as faixas para trás e para
 * frente e exige a mesma proporção 1:1:3:1:1.
 *
 * Em caso de sucesso devolve o centro refinado ao longo do eixo percorrido
 * e o módulo medido nele.
 */
static bool confirmar_eixo(
    const ImagemCinza *binaria,
    int cx,
    int cy,
    float modulo_esperado,
    bool vertical,
    float *centro_refinado,
    float *modulo_medido
) {
    const int limite = vertical ? binaria->altura : binaria->largura;
    const int inicio = vertical ? cy : cx;

    /* Nenhuma faixa de um padrão válido passa de 5 módulos. */
    const int maximo = (int)(modulo_esperado * 5.0f) + 3;

    int faixas[5] = { 0, 0, 0, 0, 0 };
    int posicao = inicio;

    /* Faixa central (3 módulos), metade anterior ao centro. */
    while (posicao >= 0 &&
           escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[2] <= maximo) {

        ++faixas[2];
        --posicao;
    }

    if (posicao < 0 || faixas[2] == 0 || faixas[2] > maximo) {
        return false;
    }

    while (posicao >= 0 &&
           !escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[1] <= maximo) {

        ++faixas[1];
        --posicao;
    }

    if (posicao < 0 || faixas[1] == 0 || faixas[1] > maximo) {
        return false;
    }

    while (posicao >= 0 &&
           escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[0] <= maximo) {

        ++faixas[0];
        --posicao;
    }

    if (faixas[0] == 0 || faixas[0] > maximo) {
        return false;
    }

    /* Metade posterior da faixa central e as duas faixas seguintes. */
    posicao = inicio + 1;

    while (posicao < limite &&
           escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[2] <= 2 * maximo) {

        ++faixas[2];
        ++posicao;
    }

    if (posicao >= limite || faixas[2] > 2 * maximo) {
        return false;
    }

    while (posicao < limite &&
           !escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[3] <= maximo) {

        ++faixas[3];
        ++posicao;
    }

    if (posicao >= limite || faixas[3] == 0 || faixas[3] > maximo) {
        return false;
    }

    while (posicao < limite &&
           escuro_no_eixo(binaria, vertical, cx, cy, posicao) &&
           faixas[4] <= maximo) {

        ++faixas[4];
        ++posicao;
    }

    if (faixas[4] == 0 || faixas[4] > maximo) {
        return false;
    }

    const int total =
        faixas[0] + faixas[1] + faixas[2] + faixas[3] + faixas[4];

    /*
     * O padrão é um quadrado: a largura medida nos dois eixos precisa ser
     * compatível. Rejeita, por exemplo, faixas de texto ou molduras.
     */
    const float total_esperado = modulo_esperado * 7.0f;

    if (fabsf((float)total - total_esperado) >= 0.4f * total_esperado) {
        return false;
    }

    float modulo = 0.0f;

    if (!proporcao_valida(faixas, &modulo)) {
        return false;
    }

    /*
     * `posicao` está na primeira amostra depois da última faixa escura.
     * Recuando as duas últimas faixas e metade da central chega-se ao
     * centro do padrão.
     */
    *centro_refinado =
        (float)posicao
        - (float)faixas[4]
        - (float)faixas[3]
        - (float)faixas[2] / 2.0f;

    *modulo_medido = modulo;

    return true;
}

/* ------------------------------------------------------------------------- */
/* Etapa 2: agrupamento dos candidatos                                       */
/* ------------------------------------------------------------------------- */

/*
 * O mesmo padrão é encontrado em várias linhas vizinhas. Candidatos
 * próximos e com módulos semelhantes são fundidos em um único registro,
 * cujo centro é a média dos centros acumulados.
 */
static void registrar_padrao(
    QrListaPadroes *lista,
    QrPonto centro,
    float modulo
) {
    for (int i = 0; i < lista->quantidade; ++i) {
        QrPadrao *padrao = &lista->itens[i];

        const bool mesma_posicao =
            fabsf(padrao->centro.x - centro.x) <= padrao->modulo &&
            fabsf(padrao->centro.y - centro.y) <= padrao->modulo;

        const bool mesmo_tamanho =
            fabsf(padrao->modulo - modulo) <= padrao->modulo * 0.5f + 1.0f;

        if (mesma_posicao && mesmo_tamanho) {
            const float n = (float)padrao->confirmacoes;

            padrao->centro.x = (padrao->centro.x * n + centro.x) / (n + 1.0f);
            padrao->centro.y = (padrao->centro.y * n + centro.y) / (n + 1.0f);
            padrao->modulo   = (padrao->modulo * n + modulo) / (n + 1.0f);

            ++padrao->confirmacoes;

            return;
        }
    }

    if (lista->quantidade >= QR_MAX_PADROES) {
        return;
    }

    QrPadrao *novo = &lista->itens[lista->quantidade++];

    novo->centro = centro;
    novo->modulo = modulo;
    novo->confirmacoes = 1;
}

int qr_localizar_padroes(const ImagemCinza *binaria, QrListaPadroes *lista) {
    if (lista == NULL) {
        return 0;
    }

    lista->quantidade = 0;

    if (binaria == NULL || binaria->dados == NULL) {
        return 0;
    }

    const int largura = binaria->largura;
    const int altura = binaria->altura;

    /* Um QR Code ocupa no mínimo 21x21 módulos. */
    if (largura < 21 || altura < 21) {
        return 0;
    }

    /*
     * Cada linha é descrita pelas suas sequências de pixels de mesma cor
     * (runs). Como a imagem está binarizada, as sequências alternam entre
     * escuro e claro, e basta percorrer janelas de cinco sequências
     * consecutivas que comecem em uma faixa escura.
     */
    int *inicio = (int *)malloc((size_t)largura * sizeof(int));
    int *tamanho = (int *)malloc((size_t)largura * sizeof(int));
    Uint8 *cor = (Uint8 *)malloc((size_t)largura);

    if (inicio == NULL || tamanho == NULL || cor == NULL) {
        free(inicio);
        free(tamanho);
        free(cor);

        return 0;
    }

    for (int y = 0; y < altura; ++y) {
        const Uint8 *linha = binaria->dados + (size_t)y * (size_t)largura;

        int quantidade_faixas = 0;
        int x = 0;

        while (x < largura) {
            const Uint8 valor = linha[x];

            int fim = x;

            while (fim < largura && linha[fim] == valor) {
                ++fim;
            }

            inicio[quantidade_faixas] = x;
            tamanho[quantidade_faixas] = fim - x;
            cor[quantidade_faixas] = valor;

            ++quantidade_faixas;

            x = fim;
        }

        for (int i = 0; i + 4 < quantidade_faixas; ++i) {
            if (cor[i] != IMG_ESCURO) {
                continue;
            }

            const int faixas[5] = {
                tamanho[i],
                tamanho[i + 1],
                tamanho[i + 2],
                tamanho[i + 3],
                tamanho[i + 4]
            };

            float modulo_horizontal = 0.0f;

            if (!proporcao_valida(faixas, &modulo_horizontal)) {
                continue;
            }

            const float centro_x =
                (float)inicio[i + 2] + (float)tamanho[i + 2] / 2.0f;

            const int coluna = (int)(centro_x + 0.5f);

            /* Confirma na vertical, na coluna do centro encontrado. */
            float centro_y = 0.0f;
            float modulo_vertical = 0.0f;

            if (!confirmar_eixo(
                    binaria,
                    coluna,
                    y,
                    modulo_horizontal,
                    true,
                    &centro_y,
                    &modulo_vertical
                )) {

                continue;
            }

            /* E novamente na horizontal, agora na linha refinada. */
            float centro_x_refinado = 0.0f;
            float modulo_horizontal_refinado = 0.0f;

            if (!confirmar_eixo(
                    binaria,
                    coluna,
                    (int)(centro_y + 0.5f),
                    modulo_vertical,
                    false,
                    &centro_x_refinado,
                    &modulo_horizontal_refinado
                )) {

                continue;
            }

            const QrPonto centro = { centro_x_refinado, centro_y };

            registrar_padrao(
                lista,
                centro,
                (modulo_vertical + modulo_horizontal_refinado) / 2.0f
            );
        }
    }

    free(inicio);
    free(tamanho);
    free(cor);

    return lista->quantidade;
}

/* ------------------------------------------------------------------------- */
/* Etapa 3: combinação dos padrões em QR Codes                               */
/* ------------------------------------------------------------------------- */

/*
 * Verifica o padrão de temporização do símbolo.
 *
 * Todo QR Code tem, na linha 6 e na coluna 6, uma sequência de módulos que
 * alterna escuro e claro ligando os padrões de localização: é o padrão de
 * temporização, usado pelos leitores para medir a grade. Um módulo dessa
 * sequência é escuro quando o seu índice é par.
 *
 * Com os cantos e o tamanho do módulo já estimados, cada módulo (coluna,
 * linha) tem o seu centro em
 *
 *   canto superior esquerdo + modulo * ((coluna + 0,5) * eixo_x
 *                                     + (linha  + 0,5) * eixo_y)
 *
 * Esta é a verificação que distingue um QR Code real de três padrões que
 * por acaso formaram o triângulo esperado: é o caso, por exemplo, de dois
 * QR Codes lado a lado, em que um padrão de um deles e dois do outro podem
 * satisfazer a geometria sem haver símbolo algum entre eles.
 */
static bool verificar_temporizacao(
    const ImagemCinza *binaria,
    const QrCodigo *codigo,
    float modulo_x,
    float modulo_y,
    float eixo_x_x,
    float eixo_x_y,
    float eixo_y_x,
    float eixo_y_y
) {
    const int modulos = codigo->modulos;

    if (modulos < 21 || modulo_x <= 0.0f || modulo_y <= 0.0f) {
        return false;
    }

    const QrPonto origem = codigo->cantos[0];

    int acertos = 0;
    int total = 0;

    /* A sequência vai do fim de um separador ao início do outro. */
    for (int i = 8; i <= modulos - 9; ++i) {
        /* i na horizontal (linha 6) e, em seguida, na vertical (coluna 6) */
        const float coordenadas[2][2] = {
            { (float)i + 0.5f, 6.5f },
            { 6.5f, (float)i + 0.5f }
        };

        for (int eixo = 0; eixo < 2; ++eixo) {
            const float coluna = coordenadas[eixo][0];
            const float linha = coordenadas[eixo][1];

            const float x =
                origem.x +
                coluna * modulo_x * eixo_x_x +
                linha * modulo_y * eixo_y_x;

            const float y =
                origem.y +
                coluna * modulo_x * eixo_x_y +
                linha * modulo_y * eixo_y_y;

            const bool escuro =
                cinza_pixel(binaria, (int)(x + 0.5f), (int)(y + 0.5f)) ==
                IMG_ESCURO;

            /* Módulos de índice par são escuros. */
            const bool esperado = (i % 2 == 0);

            if (escuro == esperado) {
                ++acertos;
            }

            ++total;
        }
    }

    if (total == 0) {
        return false;
    }

    /*
     * A folga absorve o erro de amostragem em imagens rotacionadas ou com
     * leve perspectiva, em que a grade estimada se desloca em relação à
     * real nas extremidades do símbolo.
     */
    return (float)acertos >= 0.75f * (float)total;
}

/*
 * Tenta montar um QR Code a partir de três padrões de localização.
 *
 * Os três padrões de um QR Code ocupam os cantos superior esquerdo,
 * superior direito e inferior esquerdo do símbolo: formam um triângulo
 * retângulo isósceles cujo vértice do ângulo reto é o canto superior
 * esquerdo. A partir dessa geometria, os quatro cantos do símbolo são
 * obtidos deslocando os centros 3,5 módulos para fora (metade da largura
 * do padrão de localização).
 */
static bool montar_codigo(
    const ImagemCinza *binaria,
    const QrPadrao *p0,
    const QrPadrao *p1,
    const QrPadrao *p2,
    QrCodigo *codigo
) {
    /* Os três padrões precisam ter módulos de tamanho semelhante. */
    float menor = p0->modulo;
    float maior = p0->modulo;

    const float tamanhos[3] = { p0->modulo, p1->modulo, p2->modulo };

    for (int i = 1; i < 3; ++i) {
        if (tamanhos[i] < menor) {
            menor = tamanhos[i];
        }

        if (tamanhos[i] > maior) {
            maior = tamanhos[i];
        }
    }

    if (menor <= 0.0f || maior > menor * 1.8f) {
        return false;
    }

    const float modulo = (tamanhos[0] + tamanhos[1] + tamanhos[2]) / 3.0f;

    const QrPonto a = p0->centro;
    const QrPonto b = p1->centro;
    const QrPonto c = p2->centro;

    const float dab = qr_distancia(a, b);
    const float dac = qr_distancia(a, c);
    const float dbc = qr_distancia(b, c);

    /*
     * O vértice do ângulo reto é o oposto ao maior lado (a hipotenusa,
     * que liga o canto superior direito ao inferior esquerdo).
     */
    QrPonto vertice;
    QrPonto lado_a;
    QrPonto lado_b;

    float hipotenusa;
    float cateto_a;
    float cateto_b;

    if (dab >= dac && dab >= dbc) {
        vertice = c;
        lado_a = a;
        lado_b = b;
        hipotenusa = dab;
        cateto_a = dac;
        cateto_b = dbc;
    } else if (dac >= dab && dac >= dbc) {
        vertice = b;
        lado_a = a;
        lado_b = c;
        hipotenusa = dac;
        cateto_a = dab;
        cateto_b = dbc;
    } else {
        vertice = a;
        lado_a = b;
        lado_b = c;
        hipotenusa = dbc;
        cateto_a = dab;
        cateto_b = dac;
    }

    const float maior_cateto = (cateto_a > cateto_b) ? cateto_a : cateto_b;

    if (maior_cateto <= 0.0f) {
        return false;
    }

    /* Triângulo isósceles: os dois catetos devem ser próximos. */
    if (fabsf(cateto_a - cateto_b) > 0.3f * maior_cateto) {
        return false;
    }

    /* E retângulo: precisa satisfazer o teorema de Pitágoras. */
    const float hipotenusa_esperada =
        sqrtf(cateto_a * cateto_a + cateto_b * cateto_b);

    if (fabsf(hipotenusa - hipotenusa_esperada) > 0.2f * hipotenusa_esperada) {
        return false;
    }

    /* A distância entre centros tem de ser compatível com algum QR Code. */
    if (maior_cateto < QR_DISTANCIA_MINIMA * modulo ||
        maior_cateto > QR_DISTANCIA_MAXIMA * modulo) {

        return false;
    }

    /*
     * Produto vetorial para descobrir qual dos dois padrões restantes está
     * à direita e qual está abaixo do canto superior esquerdo. Como o eixo
     * Y cresce para baixo, o produto é positivo quando o primeiro vetor
     * aponta para a direita e o segundo para baixo.
     */
    const float v1x = lado_a.x - vertice.x;
    const float v1y = lado_a.y - vertice.y;
    const float v2x = lado_b.x - vertice.x;
    const float v2y = lado_b.y - vertice.y;

    const float produto_vetorial = v1x * v2y - v1y * v2x;

    QrPonto superior_direito = lado_a;
    QrPonto inferior_esquerdo = lado_b;

    if (produto_vetorial < 0.0f) {
        superior_direito = lado_b;
        inferior_esquerdo = lado_a;
    }

    const QrPonto superior_esquerdo = vertice;

    /* Eixos unitários do símbolo. */
    float eixo_x_x = superior_direito.x - superior_esquerdo.x;
    float eixo_x_y = superior_direito.y - superior_esquerdo.y;
    float eixo_y_x = inferior_esquerdo.x - superior_esquerdo.x;
    float eixo_y_y = inferior_esquerdo.y - superior_esquerdo.y;

    const float norma_x = sqrtf(eixo_x_x * eixo_x_x + eixo_x_y * eixo_x_y);
    const float norma_y = sqrtf(eixo_y_x * eixo_y_x + eixo_y_y * eixo_y_y);

    if (norma_x <= 0.0f || norma_y <= 0.0f) {
        return false;
    }

    eixo_x_x /= norma_x;
    eixo_x_y /= norma_x;
    eixo_y_x /= norma_y;
    eixo_y_y /= norma_y;

    /*
     * O quarto canto não tem padrão de localização: sua posição é estimada
     * completando o paralelogramo formado pelos outros três centros.
     */
    const QrPonto inferior_direito = {
        superior_direito.x + inferior_esquerdo.x - superior_esquerdo.x,
        superior_direito.y + inferior_esquerdo.y - superior_esquerdo.y
    };

    /*
     * A distância entre os centros dos padrões equivale a (modulos - 7)
     * módulos. Como todo QR Code tem 4 * versao + 17 módulos de lado, essa
     * medida é convertida na versão válida mais próxima.
     */
    const float distancia_media = (cateto_a + cateto_b) / 2.0f;
    const float lado_em_modulos = distancia_media / modulo + 7.0f;

    int versao_estimada = (int)((lado_em_modulos - 17.0f) / 4.0f + 0.5f);

    if (versao_estimada < 1) {
        versao_estimada = 1;
    }

    if (versao_estimada > 40) {
        versao_estimada = 40;
    }

    const float distancia_x =
        qr_distancia(superior_esquerdo, superior_direito);
    const float distancia_y =
        qr_distancia(superior_esquerdo, inferior_esquerdo);

    /*
     * A versão é confirmada pelo padrão de temporização. Um erro de poucos
     * por cento na medida do módulo já é suficiente para arredondar a
     * versão para a vizinha, então as candidatas adjacentes também são
     * testadas antes de descartar o trio.
     */
    static const int deslocamentos[3] = { 0, -1, 1 };

    for (int tentativa = 0; tentativa < 3; ++tentativa) {
        const int versao = versao_estimada + deslocamentos[tentativa];

        if (versao < 1 || versao > 40) {
            continue;
        }

        const int modulos = 4 * versao + 17;

        /*
         * Com a versão fixada, o tamanho do módulo decorre da distância
         * entre os centros. Isso evita o erro que se acumularia ao
         * multiplicar o módulo medido nos padrões pelo índice de cada
         * módulo até a outra extremidade do símbolo.
         */
        const float modulo_x = distancia_x / (float)(modulos - 7);
        const float modulo_y = distancia_y / (float)(modulos - 7);

        /* Cada centro está a 3,5 módulos das duas bordas vizinhas. */
        const float recuo_x = 3.5f * modulo_x;
        const float recuo_y = 3.5f * modulo_y;

        codigo->cantos[0].x =
            superior_esquerdo.x - recuo_x * eixo_x_x - recuo_y * eixo_y_x;
        codigo->cantos[0].y =
            superior_esquerdo.y - recuo_x * eixo_x_y - recuo_y * eixo_y_y;

        codigo->cantos[1].x =
            superior_direito.x + recuo_x * eixo_x_x - recuo_y * eixo_y_x;
        codigo->cantos[1].y =
            superior_direito.y + recuo_x * eixo_x_y - recuo_y * eixo_y_y;

        codigo->cantos[2].x =
            inferior_direito.x + recuo_x * eixo_x_x + recuo_y * eixo_y_x;
        codigo->cantos[2].y =
            inferior_direito.y + recuo_x * eixo_x_y + recuo_y * eixo_y_y;

        codigo->cantos[3].x =
            inferior_esquerdo.x - recuo_x * eixo_x_x + recuo_y * eixo_y_x;
        codigo->cantos[3].y =
            inferior_esquerdo.y - recuo_x * eixo_x_y + recuo_y * eixo_y_y;

        codigo->padroes[0] = superior_esquerdo;
        codigo->padroes[1] = superior_direito;
        codigo->padroes[2] = inferior_esquerdo;
        codigo->num_padroes = 3;

        codigo->centro = qr_centro_quadrilatero(codigo->cantos);
        codigo->modulo = (modulo_x + modulo_y) / 2.0f;
        codigo->versao = versao;
        codigo->modulos = modulos;
        codigo->origem = QR_ORIGEM_DETECCAO;

        codigo->decodificado = false;
        codigo->conteudo[0] = '\0';
        codigo->tamanho_conteudo = 0;
        codigo->qualidade = 0;

        SDL_strlcpy(codigo->tipo, "QR-Code", QR_MAX_TIPO);

        /* Só é aceito o trio que realmente delimita um símbolo. */
        if (verificar_temporizacao(
                binaria,
                codigo,
                modulo_x,
                modulo_y,
                eixo_x_x,
                eixo_x_y,
                eixo_y_x,
                eixo_y_y
            )) {

            return true;
        }
    }

    return false;
}

/* Ordena os padrões pelo número de confirmações, do mais forte ao mais fraco. */
static void ordenar_por_confirmacoes(QrPadrao *itens, int quantidade) {
    for (int i = 1; i < quantidade; ++i) {
        const QrPadrao atual = itens[i];

        int j = i - 1;

        while (j >= 0 && itens[j].confirmacoes < atual.confirmacoes) {
            itens[j + 1] = itens[j];
            --j;
        }

        itens[j + 1] = atual;
    }
}

/* Copia para destino apenas os padrões com confirmações suficientes. */
static int filtrar_padroes(
    const QrListaPadroes *origem,
    int minimo_confirmacoes,
    QrPadrao *destino,
    int capacidade
) {
    int quantidade = 0;

    for (int i = 0; i < origem->quantidade && quantidade < capacidade; ++i) {
        if (origem->itens[i].confirmacoes >= minimo_confirmacoes) {
            destino[quantidade++] = origem->itens[i];
        }
    }

    return quantidade;
}

/* Evita registrar duas vezes o mesmo símbolo a partir de trios diferentes. */
static bool codigo_repetido(const QrResultado *resultado, const QrCodigo *codigo) {
    for (int i = 0; i < resultado->quantidade; ++i) {
        const float distancia =
            qr_distancia(resultado->codigos[i].centro, codigo->centro);

        if (distancia < codigo->modulo * 4.0f) {
            return true;
        }
    }

    return false;
}

/* Ordena os códigos de cima para baixo e da esquerda para a direita. */
static void ordenar_codigos(QrResultado *resultado) {
    for (int i = 1; i < resultado->quantidade; ++i) {
        const QrCodigo atual = resultado->codigos[i];

        int j = i - 1;

        while (j >= 0) {
            const QrCodigo *anterior = &resultado->codigos[j];

            const bool depois =
                (anterior->centro.y > atual.centro.y + anterior->modulo * 7.0f) ||
                (fabsf(anterior->centro.y - atual.centro.y) <=
                     anterior->modulo * 7.0f &&
                 anterior->centro.x > atual.centro.x);

            if (!depois) {
                break;
            }

            resultado->codigos[j + 1] = resultado->codigos[j];
            --j;
        }

        resultado->codigos[j + 1] = atual;
    }
}

int qr_detectar(const ImagemCinza *binaria, QrResultado *resultado) {
    if (resultado == NULL) {
        return 0;
    }

    memset(resultado, 0, sizeof(*resultado));

    QrListaPadroes lista;

    resultado->padroes_localizados = qr_localizar_padroes(binaria, &lista);

    if (resultado->padroes_localizados < 3) {
        return 0;
    }

    ordenar_por_confirmacoes(lista.itens, lista.quantidade);

    QrPadrao candidatos[QR_PADROES_COMBINADOS];

    /*
     * Exigir duas confirmações descarta ruído isolado. Quando isso deixa
     * padrões de menos para formar um trio, a exigência é relaxada: em
     * imagens pequenas um padrão pode aparecer em uma única linha.
     */
    int quantidade = filtrar_padroes(
        &lista,
        2,
        candidatos,
        QR_PADROES_COMBINADOS
    );

    if (quantidade < 3) {
        quantidade = filtrar_padroes(
            &lista,
            1,
            candidatos,
            QR_PADROES_COMBINADOS
        );
    }

    for (int i = 0; i < quantidade; ++i) {
        for (int j = i + 1; j < quantidade; ++j) {
            for (int k = j + 1; k < quantidade; ++k) {
                if (resultado->quantidade >= QR_MAX_CODIGOS) {
                    ordenar_codigos(resultado);
                    return resultado->quantidade;
                }

                QrCodigo codigo;

                memset(&codigo, 0, sizeof(codigo));

                if (!montar_codigo(
                        binaria,
                        &candidatos[i],
                        &candidatos[j],
                        &candidatos[k],
                        &codigo
                    )) {

                    continue;
                }

                if (codigo_repetido(resultado, &codigo)) {
                    continue;
                }

                resultado->codigos[resultado->quantidade++] = codigo;
            }
        }
    }

    ordenar_codigos(resultado);

    return resultado->quantidade;
}
