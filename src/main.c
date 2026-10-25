#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "image_loader.h"
#include "image_processing.h"
#include "image_saver.h"
#include "qr_decoder.h"
#include "qr_detector.h"
#include "qr_tipos.h"

/*
 * Ponto de entrada e integração dos módulos.
 *
 * Fluxo da aplicação, para cada imagem selecionada pelo usuário:
 *
 *   1. image_loader      carrega o arquivo e converte para RGBA32;
 *   2. image_processing  converte para escala de cinza e binariza;
 *   3. qr_detector       localiza os padrões de localização e a região
 *                        de cada QR Code;
 *   4. qr_decoder        decodifica o conteúdo (biblioteca ZBar) e o
 *                        associa às regiões localizadas;
 *   5. gui               exibe a imagem com a região destacada e o
 *                        conteúdo lido.
 *
 * A imagem pode ser informada na linha de comando, escolhida no seletor de
 * arquivos do sistema ou arrastada para a janela.
 */

#define ARQUIVO_SAIDA "qr_resultado.png"

/*
 * Subtraído da média local na binarização adaptativa. Um valor pequeno e
 * positivo evita que regiões uniformes (fundo claro sem código) virem
 * ruído, mantendo a sensibilidade nas bordas dos módulos.
 */
#define DESVIO_ADAPTATIVO 7

#define MAX_LINHA 160

/* Resultado completo da análise de uma imagem. */
typedef struct {
    char         caminho[GUI_MAX_CAMINHO];
    SDL_Surface *imagem;           /* imagem carregada, em RGBA32 */
    SDL_Surface *visual_binaria;   /* binarização, para exibição */
    ImagemCinza  cinza;
    ImagemCinza  binaria;
    QrResultado  resultado;
    double       media;            /* intensidade média, em tons de cinza */
    int          limiar_otsu;
    int          janela_adaptativa;
    bool         usou_adaptativa;
    bool         valida;
} Analise;

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

static void analise_liberar(Analise *analise) {
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

/* Imprime no terminal o resumo da análise. */
static void imprimir_resultado(const Analise *analise) {
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
static bool analisar_imagem(const char *caminho, Analise *analise) {
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

    imprimir_resultado(analise);

    return true;
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
 * Monta as linhas de informação exibidas na janela secundária e preenche o
 * vetor de ponteiros usado pela GUI. Retorna a quantidade de linhas.
 */
static int montar_linhas(
    const Analise *analise,
    int selecionado,
    char textos[GUI_MAX_LINHAS][MAX_LINHA],
    const char *ponteiros[GUI_MAX_LINHAS]
) {
    int quantidade = 0;

    if (!analise->valida) {
        SDL_strlcpy(
            textos[quantidade++],
            "Nenhuma imagem carregada.",
            MAX_LINHA
        );

        SDL_strlcpy(
            textos[quantidade++],
            "Selecione um arquivo de imagem para detectar e ler o QR Code.",
            MAX_LINHA
        );

        SDL_snprintf(
            textos[quantidade++],
            MAX_LINHA,
            "Decodificação: %s",
            qr_decodificador_descricao()
        );

        for (int i = 0; i < quantidade; ++i) {
            ponteiros[i] = textos[i];
        }

        return quantidade;
    }

    char nome[64];

    copiar_truncado(
        nome,
        sizeof(nome),
        nome_do_arquivo(analise->caminho),
        40
    );

    SDL_snprintf(
        textos[quantidade++],
        MAX_LINHA,
        "Arquivo: %s  (%d x %d px)",
        nome,
        analise->imagem->w,
        analise->imagem->h
    );

    if (analise->usou_adaptativa) {
        SDL_snprintf(
            textos[quantidade++],
            MAX_LINHA,
            "Binarização: adaptativa, janela %d px, desvio %d   |   "
            "Otsu: %d",
            analise->janela_adaptativa,
            DESVIO_ADAPTATIVO,
            analise->limiar_otsu
        );
    } else {
        SDL_snprintf(
            textos[quantidade++],
            MAX_LINHA,
            "Binarização: limiar global de Otsu = %d",
            analise->limiar_otsu
        );
    }

    SDL_snprintf(
        textos[quantidade++],
        MAX_LINHA,
        "Padrões de localização: %d   |   QR Codes: %d   |   lidos: %d",
        analise->resultado.padroes_localizados,
        analise->resultado.quantidade,
        qr_total_decodificados(&analise->resultado)
    );

    if (analise->resultado.quantidade == 0) {
        SDL_strlcpy(
            textos[quantidade++],
            "Nenhum QR Code localizado nesta imagem.",
            MAX_LINHA
        );
    } else if (selecionado >= 0 &&
               selecionado < analise->resultado.quantidade) {

        const QrCodigo *codigo = &analise->resultado.codigos[selecionado];

        if (codigo->origem == QR_ORIGEM_DETECCAO) {
            SDL_snprintf(
                textos[quantidade++],
                MAX_LINHA,
                "QR %d: versão %d — %d x %d módulos — módulo de %.1f px",
                selecionado + 1,
                codigo->versao,
                codigo->modulos,
                codigo->modulos,
                (double)codigo->modulo
            );
        } else {
            SDL_snprintf(
                textos[quantidade++],
                MAX_LINHA,
                "QR %d: região informada pela biblioteca de decodificação",
                selecionado + 1
            );
        }
    }

    SDL_snprintf(
        textos[quantidade++],
        MAX_LINHA,
        "Decodificação: %s",
        qr_decodificador_descricao()
    );

    for (int i = 0; i < quantidade; ++i) {
        ponteiros[i] = textos[i];
    }

    return quantidade;
}

/* Índice do primeiro código decodificado, ou 0 se houver apenas regiões. */
static int selecao_inicial(const QrResultado *resultado) {
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

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(
            stderr,
            "Erro ao inicializar SDL: %s\n",
            SDL_GetError()
        );

        return 1;
    }

    Gui gui;

    if (!gui_iniciar(&gui, "Detecção e leitura de QR Codes")) {
        SDL_Quit();
        return 1;
    }

    Analise analise;

    memset(&analise, 0, sizeof(analise));

    int selecionado = -1;
    bool mostrar_binaria = false;
    bool mostrar_marcacao = true;
    bool resolucao_original = false;

    char mensagem[192] = "";

    /* Imagem informada na linha de comando (opcional). */
    if (argc >= 2) {
        if (analisar_imagem(argv[1], &analise)) {
            selecionado = selecao_inicial(&analise.resultado);

            gui_definir_imagens(
                &gui,
                analise.imagem,
                analise.visual_binaria
            );
        } else {
            SDL_strlcpy(
                mensagem,
                "Não foi possível carregar a imagem informada.",
                sizeof(mensagem)
            );
        }
    } else {
        printf(
            "Uso: %s [caminho_da_imagem]\n"
            "Sem argumentos, use o botao \"Abrir imagem\" na janela de leitura.\n",
            argv[0]
        );
    }

    bool executando = true;

    while (executando) {
        SDL_Event evento;

        while (SDL_PollEvent(&evento)) {
            const GuiAcao acao = gui_processar_evento(&gui, &evento);

            switch (acao) {
            case GUI_ACAO_SAIR:
                executando = false;
                break;

            case GUI_ACAO_ABRIR:
                gui_abrir_dialogo(&gui);
                break;

            case GUI_ACAO_ARQUIVO_RECEBIDO:
                /* O caminho é tratado logo após o laço de eventos, junto
                 * com o que chega do seletor de arquivos. */
                break;

            case GUI_ACAO_ALTERNAR_VISTA:
                if (analise.valida) {
                    mostrar_binaria = !mostrar_binaria;
                }
                break;

            case GUI_ACAO_ALTERNAR_MARCACAO:
                mostrar_marcacao = !mostrar_marcacao;
                break;

            case GUI_ACAO_PROXIMO_CODIGO:
                if (analise.resultado.quantidade > 0) {
                    selecionado =
                        (selecionado + 1) % analise.resultado.quantidade;

                    gui_reiniciar_rolagem(&gui);
                }
                break;

            case GUI_ACAO_CLIQUE_IMAGEM: {
                QrPonto ponto;

                if (!gui_ultimo_clique(&gui, &ponto)) {
                    break;
                }

                for (int i = 0; i < analise.resultado.quantidade; ++i) {
                    if (qr_ponto_no_quadrilatero(
                            analise.resultado.codigos[i].cantos,
                            ponto
                        )) {

                        selecionado = i;

                        gui_reiniciar_rolagem(&gui);

                        break;
                    }
                }

                break;
            }

            case GUI_ACAO_COPIAR:
                if (selecionado >= 0 &&
                    selecionado < analise.resultado.quantidade &&
                    analise.resultado.codigos[selecionado].decodificado) {

                    if (SDL_SetClipboardText(
                            analise.resultado.codigos[selecionado].conteudo
                        )) {

                        SDL_strlcpy(
                            mensagem,
                            "Conteúdo copiado para a área de transferência.",
                            sizeof(mensagem)
                        );
                    } else {
                        SDL_strlcpy(
                            mensagem,
                            "Não foi possível copiar o conteúdo.",
                            sizeof(mensagem)
                        );
                    }
                }
                break;

            case GUI_ACAO_SALVAR: {
                if (!analise.valida) {
                    break;
                }

                GuiDados dados_captura;

                memset(&dados_captura, 0, sizeof(dados_captura));

                dados_captura.resultado = &analise.resultado;
                dados_captura.selecionado = selecionado;
                dados_captura.mostrar_binaria = mostrar_binaria;
                dados_captura.mostrar_marcacao = mostrar_marcacao;
                dados_captura.tem_imagem = true;

                SDL_Surface *captura =
                    gui_capturar_imagem_marcada(&gui, &dados_captura);

                if (captura == NULL) {
                    SDL_strlcpy(
                        mensagem,
                        "Não foi possível gerar a imagem de saída.",
                        sizeof(mensagem)
                    );

                    break;
                }

                if (salvar_imagem(captura, ARQUIVO_SAIDA)) {
                    SDL_snprintf(
                        mensagem,
                        sizeof(mensagem),
                        "Imagem salva em %s (%d x %d px).",
                        ARQUIVO_SAIDA,
                        captura->w,
                        captura->h
                    );
                } else {
                    SDL_strlcpy(
                        mensagem,
                        "Erro ao salvar a imagem.",
                        sizeof(mensagem)
                    );
                }

                SDL_DestroySurface(captura);

                break;
            }

            case GUI_ACAO_ALTERAR_RESOLUCAO:
                if (!analise.valida) {
                    break;
                }

                if (resolucao_original) {
                    if (gui_definir_resolucao(
                            &gui,
                            GUI_PRINCIPAL_LARGURA,
                            GUI_PRINCIPAL_ALTURA
                        )) {

                        resolucao_original = false;
                    }
                } else {
                    if (gui_definir_resolucao(
                            &gui,
                            analise.imagem->w,
                            analise.imagem->h
                        )) {

                        resolucao_original = true;
                    }
                }
                break;

            case GUI_ACAO_NENHUMA:
                break;
            }
        }

        /*
         * Caminho vindo do seletor de arquivos (que responde de forma
         * assíncrona) ou de um arquivo arrastado para a janela.
         */
        char caminho[GUI_MAX_CAMINHO];

        if (gui_obter_caminho(&gui, caminho, sizeof(caminho))) {
            if (analisar_imagem(caminho, &analise)) {
                selecionado = selecao_inicial(&analise.resultado);

                mostrar_binaria = false;
                resolucao_original = false;

                gui_reiniciar_rolagem(&gui);

                gui_definir_imagens(
                    &gui,
                    analise.imagem,
                    analise.visual_binaria
                );

                if (analise.resultado.quantidade == 0) {
                    SDL_strlcpy(
                        mensagem,
                        "Nenhum QR Code encontrado. Tente uma imagem com "
                        "mais contraste ou resolução.",
                        sizeof(mensagem)
                    );
                } else {
                    SDL_snprintf(
                        mensagem,
                        sizeof(mensagem),
                        "%d QR Code(s) localizado(s), %d decodificado(s).",
                        analise.resultado.quantidade,
                        qr_total_decodificados(&analise.resultado)
                    );
                }
            } else {
                gui_limpar_imagens(&gui);

                selecionado = -1;

                SDL_strlcpy(
                    mensagem,
                    "Não foi possível abrir essa imagem.",
                    sizeof(mensagem)
                );
            }
        }

        char textos[GUI_MAX_LINHAS][MAX_LINHA];
        const char *linhas[GUI_MAX_LINHAS];

        const int num_linhas = montar_linhas(
            &analise,
            selecionado,
            textos,
            linhas
        );

        GuiDados dados;

        memset(&dados, 0, sizeof(dados));

        dados.resultado = analise.valida ? &analise.resultado : NULL;
        dados.selecionado = selecionado;
        dados.mostrar_binaria = mostrar_binaria;
        dados.mostrar_marcacao = mostrar_marcacao;
        dados.tem_imagem = analise.valida;
        dados.linhas = linhas;
        dados.num_linhas = num_linhas;
        dados.mensagem = mensagem;

        gui_desenhar_principal(&gui, &dados);
        gui_desenhar_secundaria(&gui, &dados);

        SDL_Delay(16);
    }

    gui_encerrar(&gui);

    analise_liberar(&analise);

    SDL_Quit();

    return 0;
}
