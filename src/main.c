#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "analise.h"
#include "gui.h"
#include "image_saver.h"
#include "qr_tipos.h"

/*
 * Ponto de entrada e integração dos módulos.
 *
 * Este arquivo cuida apenas do estado da aplicação e da ligação entre as
 * partes: recebe as ações geradas pela interface, pede a análise de uma
 * nova imagem quando necessário e entrega à interface o que deve ser
 * desenhado. O processamento da imagem está em analise.c e o desenho nos
 * arquivos do módulo gui.
 *
 * A imagem pode ser informada na linha de comando, escolhida no seletor de
 * arquivos do sistema ou arrastada para a janela.
 */

#define ARQUIVO_SAIDA "qr_resultado.png"

/* Estado completo da aplicação em execução. */
typedef struct {
    Gui     gui;
    Analise analise;

    int  selecionado;         /* índice do QR em destaque, -1 se nenhum */
    bool mostrar_binaria;     /* exibir a imagem binarizada no lugar da original */
    bool mostrar_marcacao;    /* desenhar o destaque das regiões */
    bool resolucao_original;  /* janela no tamanho da imagem ou em 1024x768 */
    bool executando;

    char mensagem[192];       /* status exibido no rodapé da janela de leitura */
} Aplicacao;

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

/* Escreve a mensagem de status exibida na janela de leitura. */
static void definir_mensagem(Aplicacao *app, const char *formato, ...) {
    va_list argumentos;

    va_start(argumentos, formato);

    SDL_vsnprintf(app->mensagem, sizeof(app->mensagem), formato, argumentos);

    va_end(argumentos);
}

/* Código atualmente selecionado, ou NULL se não houver. */
static const QrCodigo *codigo_selecionado(const Aplicacao *app) {
    const QrResultado *resultado = &app->analise.resultado;

    if (app->selecionado < 0 || app->selecionado >= resultado->quantidade) {
        return NULL;
    }

    return &resultado->codigos[app->selecionado];
}

/* Estado que a interface precisa para desenhar o quadro atual. */
static GuiDados montar_dados(const Aplicacao *app, const AnaliseResumo *resumo) {
    GuiDados dados;

    memset(&dados, 0, sizeof(dados));

    dados.resultado = app->analise.valida ? &app->analise.resultado : NULL;
    dados.selecionado = app->selecionado;
    dados.mostrar_binaria = app->mostrar_binaria;
    dados.mostrar_marcacao = app->mostrar_marcacao;
    dados.tem_imagem = app->analise.valida;
    dados.mensagem = app->mensagem;

    if (resumo != NULL) {
        dados.linhas = resumo->linhas;
        dados.num_linhas = resumo->quantidade;
    }

    return dados;
}

/* ------------------------------------------------------------------------- */
/* Ações do usuário                                                          */
/* ------------------------------------------------------------------------- */

/* Analisa uma nova imagem e atualiza a interface com o resultado. */
static void carregar_imagem(Aplicacao *app, const char *caminho) {
    if (!analise_executar(&app->analise, caminho)) {
        gui_limpar_imagens(&app->gui);

        app->selecionado = -1;

        definir_mensagem(app, "Não foi possível abrir essa imagem.");

        return;
    }

    app->selecionado = analise_primeiro_codigo(&app->analise);
    app->mostrar_binaria = false;
    app->resolucao_original = false;

    gui_reiniciar_rolagem(&app->gui);

    gui_definir_imagens(
        &app->gui,
        app->analise.imagem,
        app->analise.visual_binaria
    );

    if (app->analise.resultado.quantidade == 0) {
        definir_mensagem(
            app,
            "Nenhum QR Code encontrado. Tente uma imagem com mais "
            "contraste ou resolução."
        );
    } else {
        definir_mensagem(
            app,
            "%d QR Code(s) localizado(s), %d decodificado(s).",
            app->analise.resultado.quantidade,
            qr_total_decodificados(&app->analise.resultado)
        );
    }
}

static void selecionar_proximo(Aplicacao *app) {
    const int quantidade = app->analise.resultado.quantidade;

    if (quantidade == 0) {
        return;
    }

    app->selecionado = (app->selecionado + 1) % quantidade;

    gui_reiniciar_rolagem(&app->gui);
}

/* Seleciona o código cuja região contém o ponto clicado na imagem. */
static void selecionar_no_clique(Aplicacao *app) {
    QrPonto ponto;

    if (!gui_ultimo_clique(&app->gui, &ponto)) {
        return;
    }

    const QrResultado *resultado = &app->analise.resultado;

    for (int i = 0; i < resultado->quantidade; ++i) {
        if (qr_ponto_no_quadrilatero(resultado->codigos[i].cantos, ponto)) {
            app->selecionado = i;

            gui_reiniciar_rolagem(&app->gui);

            return;
        }
    }
}

static void copiar_conteudo(Aplicacao *app) {
    const QrCodigo *codigo = codigo_selecionado(app);

    if (codigo == NULL || !codigo->decodificado) {
        return;
    }

    if (SDL_SetClipboardText(codigo->conteudo)) {
        definir_mensagem(app, "Conteúdo copiado para a área de transferência.");
    } else {
        definir_mensagem(app, "Não foi possível copiar o conteúdo.");
    }
}

/* Grava a imagem com a marcação, na resolução original do arquivo. */
static void salvar_imagem_marcada(Aplicacao *app) {
    if (!app->analise.valida) {
        return;
    }

    const GuiDados dados = montar_dados(app, NULL);

    SDL_Surface *captura = gui_capturar_imagem_marcada(&app->gui, &dados);

    if (captura == NULL) {
        definir_mensagem(app, "Não foi possível gerar a imagem de saída.");
        return;
    }

    if (salvar_imagem(captura, ARQUIVO_SAIDA)) {
        definir_mensagem(
            app,
            "Imagem salva em %s (%d x %d px).",
            ARQUIVO_SAIDA,
            captura->w,
            captura->h
        );
    } else {
        definir_mensagem(app, "Erro ao salvar a imagem.");
    }

    SDL_DestroySurface(captura);
}

/* Alterna entre a resolução padrão da janela e a resolução da imagem. */
static void alternar_resolucao(Aplicacao *app) {
    if (!app->analise.valida) {
        return;
    }

    const int largura = app->resolucao_original
        ? GUI_PRINCIPAL_LARGURA
        : app->analise.imagem->w;

    const int altura = app->resolucao_original
        ? GUI_PRINCIPAL_ALTURA
        : app->analise.imagem->h;

    if (gui_definir_resolucao(&app->gui, largura, altura)) {
        app->resolucao_original = !app->resolucao_original;
    }
}

static void tratar_acao(Aplicacao *app, GuiAcao acao) {
    switch (acao) {
    case GUI_ACAO_SAIR:
        app->executando = false;
        break;

    case GUI_ACAO_ABRIR:
        gui_abrir_dialogo(&app->gui);
        break;

    case GUI_ACAO_ARQUIVO_RECEBIDO:
        /* O caminho é tratado após o laço de eventos, junto com o que
         * chega do seletor de arquivos. */
        break;

    case GUI_ACAO_ALTERNAR_VISTA:
        if (app->analise.valida) {
            app->mostrar_binaria = !app->mostrar_binaria;
        }
        break;

    case GUI_ACAO_ALTERNAR_MARCACAO:
        app->mostrar_marcacao = !app->mostrar_marcacao;
        break;

    case GUI_ACAO_PROXIMO_CODIGO:
        selecionar_proximo(app);
        break;

    case GUI_ACAO_CLIQUE_IMAGEM:
        selecionar_no_clique(app);
        break;

    case GUI_ACAO_COPIAR:
        copiar_conteudo(app);
        break;

    case GUI_ACAO_SALVAR:
        salvar_imagem_marcada(app);
        break;

    case GUI_ACAO_ALTERAR_RESOLUCAO:
        alternar_resolucao(app);
        break;

    case GUI_ACAO_NENHUMA:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Laço principal                                                            */
/* ------------------------------------------------------------------------- */

static void processar_eventos(Aplicacao *app) {
    SDL_Event evento;

    while (SDL_PollEvent(&evento)) {
        tratar_acao(app, gui_processar_evento(&app->gui, &evento));
    }

    /*
     * O seletor de arquivos responde de forma assíncrona, então o caminho
     * escolhido é recolhido aqui, no mesmo lugar em que chega o caminho de
     * um arquivo arrastado para a janela.
     */
    char caminho[GUI_MAX_CAMINHO];

    if (gui_obter_caminho(&app->gui, caminho, sizeof(caminho))) {
        carregar_imagem(app, caminho);
    }
}

static void desenhar(Aplicacao *app) {
    AnaliseResumo resumo;

    analise_descrever(&app->analise, app->selecionado, &resumo);

    const GuiDados dados = montar_dados(app, &resumo);

    gui_desenhar_principal(&app->gui, &dados);
    gui_desenhar_secundaria(&app->gui, &dados);
}

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    Aplicacao app;

    memset(&app, 0, sizeof(app));

    analise_iniciar(&app.analise);

    app.selecionado = -1;
    app.mostrar_marcacao = true;
    app.executando = true;

    if (!gui_iniciar(&app.gui, "Detecção e leitura de QR Codes")) {
        SDL_Quit();
        return 1;
    }

    /* Imagem informada na linha de comando (opcional). */
    if (argc >= 2) {
        carregar_imagem(&app, argv[1]);
    } else {
        printf(
            "Uso: %s [caminho_da_imagem]\n"
            "Sem argumentos, use o botao \"Abrir imagem\" na janela de "
            "leitura.\n",
            argv[0]
        );
    }

    while (app.executando) {
        processar_eventos(&app);
        desenhar(&app);

        SDL_Delay(16);
    }

    gui_encerrar(&app.gui);
    analise_liberar(&app.analise);

    SDL_Quit();

    return 0;
}
