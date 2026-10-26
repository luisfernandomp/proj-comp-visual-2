#include "gui.h"
#include "gui_interno.h"

#include <stdio.h>

/*
 * Janela de leitura: as linhas de informação da análise, o painel com o
 * conteúdo decodificado do QR Code selecionado e os botões de interação.
 *
 * A janela não guarda estado próprio: tudo o que é desenhado vem do
 * GuiDados montado pela aplicação a cada quadro.
 */

/* Posições e tamanhos dos elementos, em pixels da janela de leitura. */
enum {
    MARGEM            = 24,
    TITULO_Y          = 16,
    INFO_Y            = 52,
    INFO_ESPACO       = 22,
    CONTEUDO_ROTULO_Y = 170,
    CONTEUDO_Y        = 194,
    CONTEUDO_ALTURA   = 144,
    CONTEUDO_RECUO    = 8,
    BOTAO_LARGURA     = 290,
    BOTAO_ALTURA      = 38,
    BOTAO_ESPACO      = 12,
    BOTOES_Y          = 354,
    DICA_Y            = 500,
    MENSAGEM_Y        = 522
};

/* ------------------------------------------------------------------------- */
/* Conteúdo decodificado                                                     */
/* ------------------------------------------------------------------------- */

/* Quantidade de bytes de uma sequência UTF-8 a partir do byte inicial. */
static int tamanho_utf8(unsigned char byte) {
    if ((byte & 0xE0) == 0xC0) {
        return 2;
    }

    if ((byte & 0xF0) == 0xE0) {
        return 3;
    }

    if ((byte & 0xF8) == 0xF0) {
        return 4;
    }

    return 0;
}

/*
 * Prepara o conteúdo decodificado para exibição.
 *
 * O conteúdo de um QR Code é uma sequência arbitrária de bytes: pode conter
 * caracteres de controle ou bytes que não formam UTF-8 válido, e nesse caso
 * o SDL_ttf não consegue renderizar o texto. Caracteres de controle são
 * trocados por espaço (mantendo as quebras de linha) e bytes inválidos por
 * '?', preservando as sequências UTF-8 bem formadas.
 */
static void texto_para_exibicao(
    const char *entrada,
    char *saida,
    size_t tamanho
) {
    size_t destino = 0;

    if (tamanho == 0) {
        return;
    }

    if (entrada == NULL) {
        saida[0] = '\0';
        return;
    }

    for (size_t i = 0; entrada[i] != '\0' && destino + 1 < tamanho; ++i) {
        const unsigned char byte = (unsigned char)entrada[i];

        if (byte == '\n') {
            saida[destino++] = '\n';
            continue;
        }

        if (byte >= 0x20 && byte < 0x7F) {
            saida[destino++] = (char)byte;
            continue;
        }

        if (byte < 0x20 || byte == 0x7F) {
            saida[destino++] = ' ';
            continue;
        }

        const int comprimento = tamanho_utf8(byte);
        bool valido = comprimento > 0;

        for (int j = 1; valido && j < comprimento; ++j) {
            if ((entrada[i + j] & 0xC0) != 0x80) {
                valido = false;
            }
        }

        if (!valido || destino + (size_t)comprimento + 1 >= tamanho) {
            saida[destino++] = '?';
            continue;
        }

        for (int j = 0; j < comprimento; ++j) {
            saida[destino++] = entrada[i + j];
        }

        i += (size_t)comprimento - 1;
    }

    saida[destino] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Botões                                                                    */
/* ------------------------------------------------------------------------- */

void gui_montar_botoes(Gui *gui) {
    static const GuiAcao acoes[GUI_NUM_BOTOES] = {
        GUI_ACAO_ABRIR,
        GUI_ACAO_ALTERNAR_VISTA,
        GUI_ACAO_ALTERNAR_MARCACAO,
        GUI_ACAO_PROXIMO_CODIGO,
        GUI_ACAO_COPIAR,
        GUI_ACAO_SALVAR
    };

    for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
        const int coluna = i % 2;
        const int linha = i / 2;

        GuiBotao *botao = &gui->botoes[i];

        botao->rect.x =
            (float)(MARGEM + coluna * (BOTAO_LARGURA + BOTAO_ESPACO));
        botao->rect.y =
            (float)(BOTOES_Y + linha * (BOTAO_ALTURA + BOTAO_ESPACO));
        botao->rect.w = (float)BOTAO_LARGURA;
        botao->rect.h = (float)BOTAO_ALTURA;

        botao->acao = acoes[i];
        botao->rotulo = "";
        botao->hover = false;
        botao->pressionado = false;
        botao->habilitado = true;
    }
}

static void desenhar_botao(Gui *gui, const GuiBotao *botao) {
    SDL_Renderer *render = gui->render_secundario;

    if (!botao->habilitado) {
        gui_definir_cor(render, COR_BOTAO_DESABILITADO);
    } else if (botao->pressionado) {
        gui_definir_cor(render, COR_BOTAO_PRESSIONADO);
    } else if (botao->hover) {
        gui_definir_cor(render, COR_BOTAO_HOVER);
    } else {
        gui_definir_cor(render, COR_BOTAO);
    }

    SDL_RenderFillRect(render, &botao->rect);

    gui_definir_cor(
        render,
        botao->habilitado ? COR_BOTAO_BORDA : COR_PAINEL_BORDA
    );

    SDL_RenderRect(render, &botao->rect);

    int largura_texto = 0;
    int altura_texto = 0;

    TTF_GetStringSize(
        gui->fonte,
        botao->rotulo,
        0,
        &largura_texto,
        &altura_texto
    );

    gui_desenhar_texto(
        render,
        gui->fonte,
        botao->rotulo,
        botao->rect.x + (botao->rect.w - (float)largura_texto) / 2.0f,
        botao->rect.y + (botao->rect.h - (float)altura_texto) / 2.0f,
        botao->habilitado ? COR_TEXTO : COR_TEXTO_DESABILITADO
    );
}

/* Atualiza rótulos e disponibilidade dos botões conforme o estado atual. */
static void atualizar_botoes(Gui *gui, const GuiDados *dados) {
    const int quantidade =
        (dados->resultado != NULL) ? dados->resultado->quantidade : 0;

    const bool tem_conteudo =
        dados->resultado != NULL &&
        dados->selecionado >= 0 &&
        dados->selecionado < quantidade &&
        dados->resultado->codigos[dados->selecionado].decodificado;

    gui->botoes[0].rotulo = "Abrir imagem (O)";
    gui->botoes[0].habilitado = true;

    gui->botoes[1].rotulo = dados->mostrar_binaria
        ? "Ver imagem original (B)"
        : "Ver imagem binarizada (B)";
    gui->botoes[1].habilitado = dados->tem_imagem;

    gui->botoes[2].rotulo = dados->mostrar_marcacao
        ? "Ocultar marcação (M)"
        : "Mostrar marcação (M)";
    gui->botoes[2].habilitado = dados->tem_imagem && quantidade > 0;

    gui->botoes[3].rotulo = "Próximo QR Code (Tab)";
    gui->botoes[3].habilitado = quantidade > 1;

    gui->botoes[4].rotulo = "Copiar conteúdo (C)";
    gui->botoes[4].habilitado = tem_conteudo;

    gui->botoes[5].rotulo = "Salvar imagem marcada (S)";
    gui->botoes[5].habilitado = dados->tem_imagem;

    for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
        if (!gui->botoes[i].habilitado) {
            gui->botoes[i].pressionado = false;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

/*
 * Desenha o painel com o conteúdo decodificado do QR Code selecionado.
 *
 * O texto é quebrado em linhas pelo próprio SDL_ttf. Quando o resultado é
 * mais alto que o painel, só a parte visível é copiada e o deslocamento é
 * controlado pela roda do mouse.
 */
static void desenhar_conteudo(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_secundario;

    const SDL_FRect painel = {
        (float)MARGEM,
        (float)CONTEUDO_Y,
        (float)(GUI_SECUNDARIA_LARGURA - 2 * MARGEM),
        (float)CONTEUDO_ALTURA
    };

    gui_definir_cor(render, COR_PAINEL);
    SDL_RenderFillRect(render, &painel);

    gui_definir_cor(render, COR_PAINEL_BORDA);
    SDL_RenderRect(render, &painel);

    const QrCodigo *codigo = NULL;

    if (dados->resultado != NULL &&
        dados->selecionado >= 0 &&
        dados->selecionado < dados->resultado->quantidade) {

        codigo = &dados->resultado->codigos[dados->selecionado];
    }

    if (codigo == NULL || !codigo->decodificado) {
        const char *aviso = "Nenhum conteúdo decodificado.";

        if (codigo != NULL) {
            aviso =
                "Região localizada, mas o conteúdo não pôde ser decodificado.";
        } else if (!dados->tem_imagem) {
            aviso = "Selecione uma imagem para iniciar a leitura.";
        }

        gui_desenhar_texto(
            render,
            gui->fonte,
            aviso,
            painel.x + (float)CONTEUDO_RECUO,
            painel.y + (float)CONTEUDO_RECUO,
            COR_TEXTO_SUAVE
        );

        gui->rolagem_maxima = 0.0f;
        gui->rolagem_conteudo = 0.0f;

        return;
    }

    char texto[QR_MAX_CONTEUDO];

    texto_para_exibicao(codigo->conteudo, texto, sizeof(texto));

    if (texto[0] == '\0') {
        SDL_strlcpy(texto, "(conteúdo vazio)", sizeof(texto));
    }

    /*
     * O texto só é renderizado de novo quando muda. Em conteúdos longos,
     * refazer a textura a cada quadro custaria caro sem nenhum ganho.
     */
    if (gui->textura_conteudo == NULL ||
        SDL_strcmp(gui->conteudo_renderizado, texto) != 0) {

        if (gui->textura_conteudo != NULL) {
            SDL_DestroyTexture(gui->textura_conteudo);
            gui->textura_conteudo = NULL;
        }

        SDL_Surface *superficie = TTF_RenderText_Blended_Wrapped(
            gui->fonte,
            texto,
            0,
            COR_TEXTO,
            (int)painel.w - 2 * CONTEUDO_RECUO
        );

        if (superficie == NULL) {
            fprintf(
                stderr,
                "Erro ao renderizar o conteudo: %s\n",
                SDL_GetError()
            );

            return;
        }

        gui->textura_conteudo =
            SDL_CreateTextureFromSurface(render, superficie);

        gui->conteudo_largura = (float)superficie->w;
        gui->conteudo_altura = (float)superficie->h;

        SDL_DestroySurface(superficie);

        SDL_strlcpy(
            gui->conteudo_renderizado,
            texto,
            sizeof(gui->conteudo_renderizado)
        );

        if (gui->textura_conteudo == NULL) {
            return;
        }
    }

    const float largura_texto = gui->conteudo_largura;
    const float altura_texto = gui->conteudo_altura;

    const float area_visivel = painel.h - 2.0f * (float)CONTEUDO_RECUO;

    gui->rolagem_maxima = altura_texto - area_visivel;

    if (gui->rolagem_maxima < 0.0f) {
        gui->rolagem_maxima = 0.0f;
    }

    if (gui->rolagem_conteudo > gui->rolagem_maxima) {
        gui->rolagem_conteudo = gui->rolagem_maxima;
    }

    const float altura_copiada =
        (altura_texto - gui->rolagem_conteudo < area_visivel)
            ? altura_texto - gui->rolagem_conteudo
            : area_visivel;

    const SDL_FRect origem = {
        0.0f,
        gui->rolagem_conteudo,
        largura_texto,
        altura_copiada
    };

    const SDL_FRect destino = {
        painel.x + (float)CONTEUDO_RECUO,
        painel.y + (float)CONTEUDO_RECUO,
        largura_texto,
        altura_copiada
    };

    SDL_RenderTexture(render, gui->textura_conteudo, &origem, &destino);

    /* Indica que há mais texto do que o painel mostra. */
    if (gui->rolagem_maxima > 0.0f) {
        const SDL_FRect trilha = {
            painel.x + painel.w - 6.0f,
            painel.y + 2.0f,
            4.0f,
            painel.h - 4.0f
        };

        gui_definir_cor(render, COR_PAINEL_BORDA);
        SDL_RenderFillRect(render, &trilha);

        const float proporcao = area_visivel / altura_texto;
        const float altura_barra = trilha.h * proporcao;

        const SDL_FRect barra = {
            trilha.x,
            trilha.y +
                (trilha.h - altura_barra) *
                (gui->rolagem_conteudo / gui->rolagem_maxima),
            trilha.w,
            altura_barra
        };

        gui_definir_cor(render, COR_TEXTO_SUAVE);
        SDL_RenderFillRect(render, &barra);
    }
}

void gui_desenhar_secundaria(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_secundario;

    gui_definir_cor(render, COR_FUNDO_SECUNDARIA);
    SDL_RenderClear(render);

    gui_desenhar_texto(
        render,
        gui->fonte_titulo,
        "Detecção e leitura de QR Codes",
        (float)MARGEM,
        (float)TITULO_Y,
        COR_TEXTO
    );

    if (dados == NULL) {
        SDL_RenderPresent(render);
        return;
    }

    for (int i = 0; i < dados->num_linhas && i < GUI_MAX_LINHAS; ++i) {
        gui_desenhar_texto(
            render,
            gui->fonte,
            dados->linhas[i],
            (float)MARGEM,
            (float)(INFO_Y + i * INFO_ESPACO),
            COR_TEXTO
        );
    }

    /* Rótulo do painel de conteúdo, identificando o código selecionado. */
    char rotulo[128];

    const int quantidade =
        (dados->resultado != NULL) ? dados->resultado->quantidade : 0;

    if (quantidade > 0 &&
        dados->selecionado >= 0 &&
        dados->selecionado < quantidade) {

        const QrCodigo *codigo = &dados->resultado->codigos[dados->selecionado];

        if (codigo->decodificado) {
            SDL_snprintf(
                rotulo,
                sizeof(rotulo),
                "Conteúdo do QR %d de %d  —  %d bytes  —  %s",
                dados->selecionado + 1,
                quantidade,
                codigo->tamanho_conteudo,
                codigo->tipo
            );
        } else {
            SDL_snprintf(
                rotulo,
                sizeof(rotulo),
                "QR %d de %d  —  região localizada, sem leitura",
                dados->selecionado + 1,
                quantidade
            );
        }
    } else {
        SDL_strlcpy(rotulo, "Conteúdo decodificado", sizeof(rotulo));
    }

    gui_desenhar_texto(
        render,
        gui->fonte,
        rotulo,
        (float)MARGEM,
        (float)CONTEUDO_ROTULO_Y,
        COR_TEXTO_DESTAQUE
    );

    desenhar_conteudo(gui, dados);

    atualizar_botoes(gui, dados);

    for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
        desenhar_botao(gui, &gui->botoes[i]);
    }

    gui_desenhar_texto(
        render,
        gui->fonte_pequena,
        "R: alternar a resolução da janela da imagem   |   "
        "ESC: sair   |   arraste uma imagem para a janela",
        (float)MARGEM,
        (float)DICA_Y,
        COR_TEXTO_SUAVE
    );

    if (dados->mensagem != NULL && dados->mensagem[0] != '\0') {
        gui_desenhar_texto(
            render,
            gui->fonte_pequena,
            dados->mensagem,
            (float)MARGEM,
            (float)MENSAGEM_Y,
            COR_TEXTO_DESTAQUE
        );
    }

    SDL_RenderPresent(render);
}
