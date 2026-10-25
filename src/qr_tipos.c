#include "qr_tipos.h"

#include <math.h>

/*
 * Ordena os quatro vértices de um quadrilátero no sentido horário.
 *
 * Os pontos são ordenados pelo ângulo em relação ao centro. Como o eixo Y
 * cresce para baixo em coordenadas de imagem, ângulos crescentes
 * correspondem ao sentido horário na tela. Em seguida a lista é rotacionada
 * para que o primeiro elemento seja o vértice mais próximo da origem
 * (canto superior esquerdo).
 */
void qr_ordenar_quadrilatero(QrPonto cantos[4]) {
    const QrPonto centro = qr_centro_quadrilatero(cantos);

    float angulos[4];

    for (int i = 0; i < 4; ++i) {
        angulos[i] = atan2f(
            cantos[i].y - centro.y,
            cantos[i].x - centro.x
        );
    }

    /*
     * Ordenação por inserção: são apenas quatro elementos.
     */
    for (int i = 1; i < 4; ++i) {
        const QrPonto ponto = cantos[i];
        const float   angulo = angulos[i];

        int j = i - 1;

        while (j >= 0 && angulos[j] > angulo) {
            cantos[j + 1]  = cantos[j];
            angulos[j + 1] = angulos[j];
            --j;
        }

        cantos[j + 1]  = ponto;
        angulos[j + 1] = angulo;
    }

    /*
     * Descobre qual vértice está mais próximo da origem da imagem
     * e o adota como canto superior esquerdo.
     */
    int inicio = 0;
    float menor = cantos[0].x + cantos[0].y;

    for (int i = 1; i < 4; ++i) {
        const float soma = cantos[i].x + cantos[i].y;

        if (soma < menor) {
            menor = soma;
            inicio = i;
        }
    }

    if (inicio != 0) {
        QrPonto rotacionado[4];

        for (int i = 0; i < 4; ++i) {
            rotacionado[i] = cantos[(inicio + i) % 4];
        }

        for (int i = 0; i < 4; ++i) {
            cantos[i] = rotacionado[i];
        }
    }
}

int qr_total_decodificados(const QrResultado *resultado) {
    int total = 0;

    if (resultado == NULL) {
        return 0;
    }

    for (int i = 0; i < resultado->quantidade; ++i) {
        if (resultado->codigos[i].decodificado) {
            ++total;
        }
    }

    return total;
}

/*
 * Teste de ponto interno para um quadrilátero convexo: o ponto está dentro
 * quando fica do mesmo lado de todas as quatro arestas, o que é verificado
 * pelo sinal do produto vetorial entre a aresta e o vetor até o ponto.
 */
bool qr_ponto_no_quadrilatero(const QrPonto cantos[4], QrPonto ponto) {
    int positivos = 0;
    int negativos = 0;

    for (int i = 0; i < 4; ++i) {
        const QrPonto a = cantos[i];
        const QrPonto b = cantos[(i + 1) % 4];

        const float aresta_x = b.x - a.x;
        const float aresta_y = b.y - a.y;

        const float ate_ponto_x = ponto.x - a.x;
        const float ate_ponto_y = ponto.y - a.y;

        const float produto =
            aresta_x * ate_ponto_y - aresta_y * ate_ponto_x;

        if (produto > 0.0f) {
            ++positivos;
        } else if (produto < 0.0f) {
            ++negativos;
        }
    }

    return positivos == 0 || negativos == 0;
}
