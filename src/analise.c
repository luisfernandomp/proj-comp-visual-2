#include "analise.h"

#include <stdio.h>
#include <string.h>

#include "image_loader.h"
#include "qr_decoder.h"
#include "qr_detector.h"

/*
 * Encadeamento das etapas de processamento de uma imagem. A descrição do
 * fluxo e do papel de cada módulo está em analise.h.
 */

/*
 * Subtraído da média local na binarização adaptativa. Um valor pequeno e
 * positivo evita que regiões uniformes (fundo claro sem código) virem
 * ruído, mantendo a sensibilidade nas bordas dos módulos.
 */
#define DESVIO_ADAPTATIVO 7

/* ------------------------------------------------------------------------- */
/* Ciclo de vida                                                             */
/* ------------------------------------------------------------------------- */

void analise_iniciar(Analise *analise) {
    memset(analise, 0, sizeof(*analise));
}

void analise_liberar(Analise *analise) {
    if (analise->imagem != NULL) {
        SDL_DestroySurface(analise->imagem);
        analise->imagem = NULL;
    }

    if (analise->visual_binaria != NULL) {
        SDL_DestroySurface(analise->visual_binaria);
        analise->visual_binaria = NULL;
    }

    cinza_liberar(&analise->cinza);
    cinza_liberar(&analise->binaria);

    memset(&analise->resultado, 0, sizeof(analise->resultado));

    analise->valida = false;
}

/* ------------------------------------------------------------------------- */
/* Processamento                                                             */
/* ------------------------------------------------------------------------- */

/* Imprime no terminal o resumo da análise. */
static void imprimir_no_terminal(const Analise *analise) {
    const QrResultado *resultado = &analise->resultado;

    printf(
        "Intensidade media da imagem: %.2f\n",
        analise->media
    );

    printf(
        "Binarizacao: %s",
        analise->usou_adaptativa ? "adaptativa" : "global (Otsu)"
    );

    if (analise->usou_adaptativa) {
        printf(
            " (janela %d, desvio %d)\n",
            analise->janela_adaptativa,
            DESVIO_ADAPTATIVO
        );
    } else {
        printf(" (limiar %d)\n", analise->limiar_otsu);
    }

    printf(
        "Padroes de localizacao confirmados: %d\n",
        resultado->padroes_localizados
    );

    printf(
        "QR Codes localizados: %d (decodificados: %d)\n",
        resultado->quantidade,
        qr_total_decodificados(resultado)
    );

    for (int i = 0; i < resultado->quantidade; ++i) {
        const QrCodigo *codigo = &resultado->codigos[i];

        printf(
            "  QR %d: centro (%.0f, %.0f), origem: %s",
            i + 1,
            (double)codigo->centro.x,
            (double)codigo->centro.y,
            (codigo->origem == QR_ORIGEM_DETECCAO)
                ? "deteccao propria"
                : "biblioteca"
        );

        if (codigo->origem == QR_ORIGEM_DETECCAO) {
            printf(
                ", versao %d (%dx%d modulos), modulo %.2f px",
                codigo->versao,
                codigo->modulos,
                codigo->modulos,
                (double)codigo->modulo
            );
        }

        printf("\n");

        if (codigo->decodificado) {
            printf("    conteudo (%d bytes): %s\n",
                codigo->tamanho_conteudo,
                codigo->conteudo
            );
        } else {
            printf("    conteudo: nao decodificado\n");
        }
    }
}

/*
 * Executa todas as etapas de processamento sobre um arquivo de imagem.
 *
 * A detecção é tentada primeiro sobre a binarização adaptativa, que lida
 * melhor com iluminação irregular. Se nenhuma região for encontrada, a
 * imagem é binarizada novamente com o limiar global de Otsu, que costuma
 * ser suficiente para imagens sintéticas e capturas de tela.
 */
bool analise_executar(Analise *analise, const char *caminho) {
    analise_liberar(analise);

    SDL_strlcpy(analise->caminho, caminho, sizeof(analise->caminho));

    analise->imagem = carregar_imagem(caminho);

    if (analise->imagem == NULL) {
        return false;
    }

    if (!converter_para_cinza(analise->imagem, &analise->cinza)) {
        analise_liberar(analise);
        return false;
    }

    Uint32 histograma[IMG_NIVEIS];

    calcular_histograma(&analise->cinza, histograma);

    const Uint64 total_pixels =
        (Uint64)analise->cinza.largura * (Uint64)analise->cinza.altura;

    analise->media = calcular_media(histograma, total_pixels);
    analise->limiar_otsu = calcular_limiar_otsu(histograma, total_pixels);
    analise->janela_adaptativa = janela_adaptativa_sugerida(&analise->cinza);

    if (!binarizar_adaptativo(
            &analise->cinza,
            analise->janela_adaptativa,
            DESVIO_ADAPTATIVO,
            &analise->binaria
        )) {

        analise_liberar(analise);
        return false;
    }

    analise->usou_adaptativa = true;

    qr_detectar(&analise->binaria, &analise->resultado);

    if (analise->resultado.quantidade == 0) {
        if (!binarizar_global(
                &analise->cinza,
                analise->limiar_otsu,
                &analise->binaria
            )) {

            analise_liberar(analise);
            return false;
        }

        analise->usou_adaptativa = false;

        qr_detectar(&analise->binaria, &analise->resultado);
    }

    /*
     * Decodificação sobre a imagem em escala de cinza: a biblioteca aplica
     * a sua própria binarização, independente da usada na detecção.
     */
    qr_decodificar(&analise->cinza, &analise->resultado);

    analise->visual_binaria = cinza_para_superficie(&analise->binaria);

    analise->valida = true;

    imprimir_no_terminal(analise);

    return true;
}

/* ------------------------------------------------------------------------- */
/* Consulta do resultado                                                     */
/* ------------------------------------------------------------------------- */

/* Nome do arquivo sem o caminho, para exibição. */
static const char *nome_do_arquivo(const char *caminho) {
    const char *nome = caminho;

    for (const char *p = caminho; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            nome = p + 1;
        }
    }

    return nome;
}

/* Copia um texto truncando-o em `limite` caracteres, com reticências. */
static void copiar_truncado(
    char *destino,
    size_t tamanho,
    const char *origem,
    size_t limite
) {
    const size_t comprimento = SDL_strlen(origem);

    if (comprimento <= limite) {
        SDL_strlcpy(destino, origem, tamanho);
        return;
    }

    if (limite + 4 > tamanho) {
        limite = (tamanho > 4) ? tamanho - 4 : 0;
    }

    memcpy(destino, origem, limite);

    destino[limite] = '\0';

    SDL_strlcat(destino, "...", tamanho);
}

/*
 * Monta as linhas de informação exibidas na janela de leitura. Quando há um
 * código selecionado, uma das linhas descreve esse código em particular.
 */
void analise_descrever(
    const Analise *analise,
    int selecionado,
    AnaliseResumo *resumo
) {
    int quantidade = 0;

    if (!analise->valida) {
        SDL_strlcpy(
            resumo->textos[quantidade++],
            "Nenhuma imagem carregada.",
            ANALISE_MAX_LINHA
        );

        SDL_strlcpy(
            resumo->textos[quantidade++],
            "Selecione um arquivo de imagem para detectar e ler o QR Code.",
            ANALISE_MAX_LINHA
        );

        SDL_snprintf(
            resumo->textos[quantidade++],
            ANALISE_MAX_LINHA,
            "Decodificação: %s",
            qr_decodificador_descricao()
        );

        for (int i = 0; i < quantidade; ++i) {
            resumo->linhas[i] = resumo->textos[i];
        }

        resumo->quantidade = quantidade;

        return;
    }

    char nome[64];

    copiar_truncado(
        nome,
        sizeof(nome),
        nome_do_arquivo(analise->caminho),
        40
    );

    SDL_snprintf(
        resumo->textos[quantidade++],
        ANALISE_MAX_LINHA,
        "Arquivo: %s  (%d x %d px)",
        nome,
        analise->imagem->w,
        analise->imagem->h
    );

    if (analise->usou_adaptativa) {
        SDL_snprintf(
            resumo->textos[quantidade++],
            ANALISE_MAX_LINHA,
            "Binarização: adaptativa, janela %d px, desvio %d   |   "
            "Otsu: %d",
            analise->janela_adaptativa,
            DESVIO_ADAPTATIVO,
            analise->limiar_otsu
        );
    } else {
        SDL_snprintf(
            resumo->textos[quantidade++],
            ANALISE_MAX_LINHA,
            "Binarização: limiar global de Otsu = %d",
            analise->limiar_otsu
        );
    }

    SDL_snprintf(
        resumo->textos[quantidade++],
        ANALISE_MAX_LINHA,
        "Padrões de localização: %d   |   QR Codes: %d   |   lidos: %d",
        analise->resultado.padroes_localizados,
        analise->resultado.quantidade,
        qr_total_decodificados(&analise->resultado)
    );

    if (analise->resultado.quantidade == 0) {
        SDL_strlcpy(
            resumo->textos[quantidade++],
            "Nenhum QR Code localizado nesta imagem.",
            ANALISE_MAX_LINHA
        );
    } else if (selecionado >= 0 &&
               selecionado < analise->resultado.quantidade) {

        const QrCodigo *codigo = &analise->resultado.codigos[selecionado];

        if (codigo->origem == QR_ORIGEM_DETECCAO) {
            SDL_snprintf(
                resumo->textos[quantidade++],
                ANALISE_MAX_LINHA,
                "QR %d: versão %d — %d x %d módulos — módulo de %.1f px",
                selecionado + 1,
                codigo->versao,
                codigo->modulos,
                codigo->modulos,
                (double)codigo->modulo
            );
        } else {
            SDL_snprintf(
                resumo->textos[quantidade++],
                ANALISE_MAX_LINHA,
                "QR %d: região informada pela biblioteca de decodificação",
                selecionado + 1
            );
        }
    }

    SDL_snprintf(
        resumo->textos[quantidade++],
        ANALISE_MAX_LINHA,
        "Decodificação: %s",
        qr_decodificador_descricao()
    );

    for (int i = 0; i < quantidade; ++i) {
        resumo->linhas[i] = resumo->textos[i];
    }

    resumo->quantidade = quantidade;
}

int analise_primeiro_codigo(const Analise *analise) {
    const QrResultado *resultado = &analise->resultado;

    if (resultado->quantidade == 0) {
        return -1;
    }

    for (int i = 0; i < resultado->quantidade; ++i) {
        if (resultado->codigos[i].decodificado) {
            return i;
        }
    }

    return 0;
}
