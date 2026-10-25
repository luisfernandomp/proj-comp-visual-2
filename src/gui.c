#include "gui.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Constantes de layout e cores                                              */
/* ------------------------------------------------------------------------- */

#define FONTE_ARQUIVO     "assets/fonts/DejaVuSans.ttf"
#define FONTE_TAMANHO      15.0f
#define FONTE_TAMANHO_PEQ  12.0f
#define FONTE_TAMANHO_TIT  18.0f

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

/* Espessura (em pixels) do contorno da região do QR Code. */
#define CONTORNO_RAIO_SELECIONADO 1
#define CONTORNO_RAIO_NORMAL      0

static const SDL_Color COR_FUNDO_PRINCIPAL   = {  30,  30,  30, 255 };
static const SDL_Color COR_FUNDO_SECUNDARIA  = {  24,  26,  32, 255 };
static const SDL_Color COR_PAINEL            = {  38,  41,  50, 255 };
static const SDL_Color COR_PAINEL_BORDA      = {  60,  64,  76, 255 };
static const SDL_Color COR_TEXTO             = { 235, 235, 235, 255 };
static const SDL_Color COR_TEXTO_SUAVE       = { 160, 165, 180, 255 };
static const SDL_Color COR_TEXTO_DESTAQUE    = { 130, 210, 150, 255 };
static const SDL_Color COR_BOTAO             = {  58,  63,  78, 255 };
static const SDL_Color COR_BOTAO_HOVER       = {  84,  92, 116, 255 };
static const SDL_Color COR_BOTAO_PRESSIONADO = {  42,  46,  58, 255 };
static const SDL_Color COR_BOTAO_DESABILITADO = { 40, 43, 52, 255 };
static const SDL_Color COR_BOTAO_BORDA       = { 120, 130, 160, 255 };
static const SDL_Color COR_TEXTO_DESABILITADO = { 100, 104, 118, 255 };

/* Marcação da região do QR Code na janela principal. */
static const SDL_Color COR_MARCA             = { 100, 170, 255, 255 };
static const SDL_Color COR_MARCA_SELECIONADA = {  90, 230, 130, 255 };
static const SDL_Color COR_MARCA_FALHA       = { 240, 150,  70, 255 };

/* ------------------------------------------------------------------------- */
/* Utilitários internos                                                      */
/* ------------------------------------------------------------------------- */

static void definir_cor(SDL_Renderer *render, SDL_Color cor) {
    SDL_SetRenderDrawColor(render, cor.r, cor.g, cor.b, cor.a);
}

static SDL_FColor cor_para_fcolor(SDL_Color cor, float alfa) {
    SDL_FColor resultado;

    resultado.r = (float)cor.r / 255.0f;
    resultado.g = (float)cor.g / 255.0f;
    resultado.b = (float)cor.b / 255.0f;
    resultado.a = alfa;

    return resultado;
}

/* Tenta abrir a fonte a partir do diretório do executável e, como
 * alternativa, a partir do diretório de trabalho atual. */
static TTF_Font *abrir_fonte(float tamanho) {
    const char *base = SDL_GetBasePath();
    TTF_Font *fonte = NULL;

    if (base != NULL) {
        char caminho[1024];

        SDL_snprintf(caminho, sizeof(caminho), "%s%s", base, FONTE_ARQUIVO);

        fonte = TTF_OpenFont(caminho, tamanho);
    }

    if (fonte == NULL) {
        fonte = TTF_OpenFont(FONTE_ARQUIVO, tamanho);
    }

    if (fonte == NULL) {
        fprintf(
            stderr,
            "Erro ao carregar a fonte '%s': %s\n",
            FONTE_ARQUIVO,
            SDL_GetError()
        );
    }

    return fonte;
}

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
/* Criação das janelas                                                       */
/* ------------------------------------------------------------------------- */

static bool criar_janela_principal(Gui *gui, const char *titulo) {
    const SDL_DisplayID monitor = SDL_GetPrimaryDisplay();
    const SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, titulo);

    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
        GUI_PRINCIPAL_LARGURA
    );

    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
        GUI_PRINCIPAL_ALTURA
    );

    /* centralizada no monitor principal */
    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_X_NUMBER,
        SDL_WINDOWPOS_CENTERED_DISPLAY(monitor)
    );

    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_Y_NUMBER,
        SDL_WINDOWPOS_CENTERED_DISPLAY(monitor)
    );

    gui->janela_principal = SDL_CreateWindowWithProperties(props);

    SDL_DestroyProperties(props);

    if (gui->janela_principal == NULL) {
        fprintf(
            stderr,
            "Erro ao criar a janela principal: %s\n",
            SDL_GetError()
        );

        return false;
    }

    gui->render_principal = SDL_CreateRenderer(gui->janela_principal, NULL);

    if (gui->render_principal == NULL) {
        fprintf(
            stderr,
            "Erro ao criar o renderizador principal: %s\n",
            SDL_GetError()
        );

        return false;
    }

    return true;
}

static bool criar_janela_secundaria(Gui *gui) {
    const SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetStringProperty(
        props,
        SDL_PROP_WINDOW_CREATE_TITLE_STRING,
        "Leitura do QR Code"
    );

    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
        GUI_SECUNDARIA_LARGURA
    );

    SDL_SetNumberProperty(
        props,
        SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
        GUI_SECUNDARIA_ALTURA
    );

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, 0);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, 0);

    /* filha da janela principal: fica sempre acima dela e é fechada junto */
    SDL_SetPointerProperty(
        props,
        SDL_PROP_WINDOW_CREATE_PARENT_POINTER,
        gui->janela_principal
    );

    gui->janela_secundaria = SDL_CreateWindowWithProperties(props);

    SDL_DestroyProperties(props);

    if (gui->janela_secundaria == NULL) {
        fprintf(
            stderr,
            "Erro ao criar a janela secundária: %s\n",
            SDL_GetError()
        );

        return false;
    }

    /*
     * A posição da SDL refere-se à área cliente; em (0,0) a barra de título
     * ficaria fora da tela e a janela não poderia ser movida. Desloca-se
     * pela espessura da borda para que o quadro encoste no canto (0,0).
     */
    int topo = 0;
    int esquerda = 0;
    int base = 0;
    int direita = 0;

    if (SDL_GetWindowBordersSize(
            gui->janela_secundaria,
            &topo,
            &esquerda,
            &base,
            &direita
        )) {

        SDL_SetWindowPosition(gui->janela_secundaria, esquerda, topo);
        SDL_SyncWindow(gui->janela_secundaria);
    }

    gui->render_secundario = SDL_CreateRenderer(gui->janela_secundaria, NULL);

    if (gui->render_secundario == NULL) {
        fprintf(
            stderr,
            "Erro ao criar o renderizador secundário: %s\n",
            SDL_GetError()
        );

        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/* Botões                                                                    */
/* ------------------------------------------------------------------------- */

static void montar_botoes(Gui *gui) {
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

static bool botao_contem(const GuiBotao *botao, float x, float y) {
    const SDL_FPoint ponto = { x, y };

    return SDL_PointInRectFloat(&ponto, &botao->rect);
}

static void desenhar_botao(Gui *gui, const GuiBotao *botao) {
    SDL_Renderer *render = gui->render_secundario;

    if (!botao->habilitado) {
        definir_cor(render, COR_BOTAO_DESABILITADO);
    } else if (botao->pressionado) {
        definir_cor(render, COR_BOTAO_PRESSIONADO);
    } else if (botao->hover) {
        definir_cor(render, COR_BOTAO_HOVER);
    } else {
        definir_cor(render, COR_BOTAO);
    }

    SDL_RenderFillRect(render, &botao->rect);

    definir_cor(
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

/* ------------------------------------------------------------------------- */
/* Seletor de arquivos                                                       */
/* ------------------------------------------------------------------------- */

static void SDLCALL callback_seletor(
    void *userdata,
    const char *const *filelist,
    int filter
) {
    (void)filter;

    Gui *gui = (Gui *)userdata;

    gui->dialogo_aberto = false;

    if (filelist == NULL) {
        fprintf(
            stderr,
            "Erro no seletor de arquivos: %s\n",
            SDL_GetError()
        );

        return;
    }

    /* Lista vazia: o usuário cancelou. */
    if (filelist[0] == NULL) {
        return;
    }

    SDL_strlcpy(gui->caminho_recebido, filelist[0], GUI_MAX_CAMINHO);

    gui->tem_caminho_recebido = true;
}

void gui_abrir_dialogo(Gui *gui) {
    static const SDL_DialogFileFilter filtros[] = {
        { "Imagens", "png;jpg;jpeg;bmp;gif;tif;tiff;webp" },
        { "Todos os arquivos", "*" }
    };

    if (gui->dialogo_aberto) {
        return;
    }

    gui->dialogo_aberto = true;

    SDL_ShowOpenFileDialog(
        callback_seletor,
        gui,
        gui->janela_principal,
        filtros,
        SDL_arraysize(filtros),
        NULL,
        false
    );
}

bool gui_obter_caminho(Gui *gui, char *destino, size_t tamanho) {
    if (!gui->tem_caminho_recebido) {
        return false;
    }

    SDL_strlcpy(destino, gui->caminho_recebido, tamanho);

    gui->tem_caminho_recebido = false;
    gui->caminho_recebido[0] = '\0';

    return true;
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

bool gui_iniciar(Gui *gui, const char *titulo) {
    memset(gui, 0, sizeof(*gui));

    if (!TTF_Init()) {
        fprintf(stderr, "Erro ao iniciar SDL_ttf: %s\n", SDL_GetError());
        return false;
    }

    gui->fonte = abrir_fonte(FONTE_TAMANHO);

    if (gui->fonte != NULL) {
        gui->fonte_pequena = abrir_fonte(FONTE_TAMANHO_PEQ);
        gui->fonte_titulo = abrir_fonte(FONTE_TAMANHO_TIT);
    }

    if (gui->fonte == NULL ||
        gui->fonte_pequena == NULL ||
        gui->fonte_titulo == NULL) {

        gui_encerrar(gui);

        return false;
    }

    if (!criar_janela_principal(gui, titulo) ||
        !criar_janela_secundaria(gui)) {

        gui_encerrar(gui);

        return false;
    }

    montar_botoes(gui);

    return true;
}

void gui_encerrar(Gui *gui) {
    gui_limpar_imagens(gui);

    if (gui->textura_conteudo) {
        SDL_DestroyTexture(gui->textura_conteudo);
        gui->textura_conteudo = NULL;
    }

    if (gui->render_secundario) {
        SDL_DestroyRenderer(gui->render_secundario);
        gui->render_secundario = NULL;
    }

    if (gui->janela_secundaria) {
        SDL_DestroyWindow(gui->janela_secundaria);
        gui->janela_secundaria = NULL;
    }

    if (gui->render_principal) {
        SDL_DestroyRenderer(gui->render_principal);
        gui->render_principal = NULL;
    }

    if (gui->janela_principal) {
        SDL_DestroyWindow(gui->janela_principal);
        gui->janela_principal = NULL;
    }

    if (gui->fonte_titulo) {
        TTF_CloseFont(gui->fonte_titulo);
        gui->fonte_titulo = NULL;
    }

    if (gui->fonte_pequena) {
        TTF_CloseFont(gui->fonte_pequena);
        gui->fonte_pequena = NULL;
    }

    if (gui->fonte) {
        TTF_CloseFont(gui->fonte);
        gui->fonte = NULL;
    }

    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

void gui_limpar_imagens(Gui *gui) {
    if (gui->textura_original) {
        SDL_DestroyTexture(gui->textura_original);
        gui->textura_original = NULL;
    }

    if (gui->textura_binaria) {
        SDL_DestroyTexture(gui->textura_binaria);
        gui->textura_binaria = NULL;
    }

    gui->imagem_largura = 0;
    gui->imagem_altura = 0;
}

bool gui_definir_imagens(
    Gui *gui,
    SDL_Surface *original,
    SDL_Surface *binaria
) {
    gui_limpar_imagens(gui);

    if (original == NULL) {
        return false;
    }

    gui->textura_original = SDL_CreateTextureFromSurface(
        gui->render_principal,
        original
    );

    if (gui->textura_original == NULL) {
        fprintf(
            stderr,
            "Erro ao criar textura da imagem: %s\n",
            SDL_GetError()
        );

        return false;
    }

    if (binaria != NULL) {
        gui->textura_binaria = SDL_CreateTextureFromSurface(
            gui->render_principal,
            binaria
        );
    }

    /*
     * Amostragem sem interpolação: preserva as bordas dos módulos quando a
     * imagem é ampliada, o que é importante para inspecionar o resultado
     * da binarização.
     */
    SDL_SetTextureScaleMode(gui->textura_original, SDL_SCALEMODE_NEAREST);

    if (gui->textura_binaria != NULL) {
        SDL_SetTextureScaleMode(gui->textura_binaria, SDL_SCALEMODE_NEAREST);
    }

    gui->imagem_largura = original->w;
    gui->imagem_altura = original->h;

    return true;
}

bool gui_definir_resolucao(Gui *gui, int largura, int altura) {
    const SDL_DisplayID monitor = SDL_GetPrimaryDisplay();

    SDL_Rect limites;

    if (!SDL_GetDisplayBounds(monitor, &limites)) {
        fprintf(
            stderr,
            "Erro ao obter a resolucao do monitor: %s\n",
            SDL_GetError()
        );

        return false;
    }

    if (!SDL_SetWindowSize(gui->janela_principal, largura, altura)) {
        fprintf(
            stderr,
            "Erro ao alterar a resolucao da janela: %s\n",
            SDL_GetError()
        );

        return false;
    }

    /*
     * Se a janela não cabe no monitor, encosta o canto superior esquerdo
     * em (0,0). Caso contrário, centraliza.
     */
    if (largura > limites.w || altura > limites.h) {
        SDL_SetWindowPosition(gui->janela_principal, 0, 0);
    } else {
        SDL_SetWindowPosition(
            gui->janela_principal,
            SDL_WINDOWPOS_CENTERED_DISPLAY(monitor),
            SDL_WINDOWPOS_CENTERED_DISPLAY(monitor)
        );
    }

    SDL_SyncWindow(gui->janela_principal);

    return true;
}

GuiAcao gui_processar_evento(Gui *gui, const SDL_Event *evento) {
    const SDL_WindowID id_secundaria = SDL_GetWindowID(gui->janela_secundaria);

    switch (evento->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return GUI_ACAO_SAIR;

    case SDL_EVENT_KEY_DOWN:
        switch (evento->key.key) {
        case SDLK_ESCAPE: return GUI_ACAO_SAIR;
        case SDLK_O:      return GUI_ACAO_ABRIR;
        case SDLK_B:      return GUI_ACAO_ALTERNAR_VISTA;
        case SDLK_M:      return GUI_ACAO_ALTERNAR_MARCACAO;
        case SDLK_TAB:    return GUI_ACAO_PROXIMO_CODIGO;
        case SDLK_C:      return GUI_ACAO_COPIAR;
        case SDLK_S:      return GUI_ACAO_SALVAR;
        case SDLK_R:      return GUI_ACAO_ALTERAR_RESOLUCAO;
        default:          break;
        }
        break;

    /* Arrastar e soltar uma imagem sobre qualquer uma das janelas. */
    case SDL_EVENT_DROP_FILE:
        if (evento->drop.data != NULL) {
            SDL_strlcpy(
                gui->caminho_recebido,
                evento->drop.data,
                GUI_MAX_CAMINHO
            );

            gui->tem_caminho_recebido = true;

            return GUI_ACAO_ARQUIVO_RECEBIDO;
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if (evento->wheel.windowID == id_secundaria) {
            gui->rolagem_conteudo -= evento->wheel.y * 24.0f;

            if (gui->rolagem_conteudo > gui->rolagem_maxima) {
                gui->rolagem_conteudo = gui->rolagem_maxima;
            }

            if (gui->rolagem_conteudo < 0.0f) {
                gui->rolagem_conteudo = 0.0f;
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (evento->motion.windowID == id_secundaria) {
            for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
                gui->botoes[i].hover = botao_contem(
                    &gui->botoes[i],
                    evento->motion.x,
                    evento->motion.y
                );
            }
        }
        break;

    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (evento->window.windowID == id_secundaria) {
            for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
                gui->botoes[i].hover = false;
                gui->botoes[i].pressionado = false;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        /* Clique sobre a imagem: seleciona o QR Code daquela região. */
        if (evento->button.windowID == SDL_GetWindowID(gui->janela_principal) &&
            evento->button.button == SDL_BUTTON_LEFT) {

            gui->clique.x = evento->button.x;
            gui->clique.y = evento->button.y;
            gui->tem_clique = true;

            return GUI_ACAO_CLIQUE_IMAGEM;
        }

        if (evento->button.windowID == id_secundaria &&
            evento->button.button == SDL_BUTTON_LEFT) {

            for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
                gui->botoes[i].pressionado =
                    gui->botoes[i].habilitado &&
                    botao_contem(
                        &gui->botoes[i],
                        evento->button.x,
                        evento->button.y
                    );
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (evento->button.windowID == id_secundaria &&
            evento->button.button == SDL_BUTTON_LEFT) {

            GuiAcao acao = GUI_ACAO_NENHUMA;

            for (int i = 0; i < GUI_NUM_BOTOES; ++i) {
                const bool clicou =
                    gui->botoes[i].pressionado &&
                    botao_contem(
                        &gui->botoes[i],
                        evento->button.x,
                        evento->button.y
                    );

                gui->botoes[i].pressionado = false;

                if (clicou) {
                    acao = gui->botoes[i].acao;
                }
            }

            return acao;
        }
        break;

    default:
        break;
    }

    return GUI_ACAO_NENHUMA;
}

int gui_desenhar_texto(
    SDL_Renderer *render,
    TTF_Font *fonte,
    const char *texto,
    float x,
    float y,
    SDL_Color cor
) {
    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }

    SDL_Surface *superficie = TTF_RenderText_Blended(fonte, texto, 0, cor);

    if (superficie == NULL) {
        fprintf(
            stderr,
            "Erro ao renderizar o texto: %s\n",
            SDL_GetError()
        );

        return 0;
    }

    SDL_Texture *textura = SDL_CreateTextureFromSurface(render, superficie);

    const int largura = superficie->w;

    const SDL_FRect destino = {
        x,
        y,
        (float)superficie->w,
        (float)superficie->h
    };

    SDL_DestroySurface(superficie);

    if (textura == NULL) {
        fprintf(
            stderr,
            "Erro ao criar a textura do texto: %s\n",
            SDL_GetError()
        );

        return 0;
    }

    SDL_RenderTexture(render, textura, NULL, &destino);
    SDL_DestroyTexture(textura);

    return largura;
}

/* Desenha um texto centralizado horizontalmente em `centro_x`. */
static void desenhar_texto_centralizado(
    SDL_Renderer *render,
    TTF_Font *fonte,
    const char *texto,
    float centro_x,
    float y,
    SDL_Color cor
) {
    int largura = 0;
    int altura = 0;

    TTF_GetStringSize(fonte, texto, 0, &largura, &altura);

    gui_desenhar_texto(
        render,
        fonte,
        texto,
        centro_x - (float)largura / 2.0f,
        y,
        cor
    );
}

/* ------------------------------------------------------------------------- */
/* Janela principal: imagem e marcação das regiões                           */
/* ------------------------------------------------------------------------- */

/* Converte um ponto em coordenadas da imagem para coordenadas da janela. */
static SDL_FPoint imagem_para_janela(const Gui *gui, QrPonto ponto) {
    SDL_FPoint resultado = { 0.0f, 0.0f };

    if (gui->imagem_largura <= 0 || gui->imagem_altura <= 0) {
        return resultado;
    }

    const float escala_x = gui->area_imagem.w / (float)gui->imagem_largura;
    const float escala_y = gui->area_imagem.h / (float)gui->imagem_altura;

    resultado.x = gui->area_imagem.x + ponto.x * escala_x;
    resultado.y = gui->area_imagem.y + ponto.y * escala_y;

    return resultado;
}

/* Preenche um quadrilátero com uma cor translúcida (dois triângulos). */
static void preencher_quadrilatero(
    SDL_Renderer *render,
    const SDL_FPoint pontos[4],
    SDL_FColor cor
) {
    static const int indices[6] = { 0, 1, 2, 0, 2, 3 };

    SDL_Vertex vertices[6];

    for (int i = 0; i < 6; ++i) {
        vertices[i].position = pontos[indices[i]];
        vertices[i].color = cor;
        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    SDL_RenderGeometry(render, NULL, vertices, 6, NULL, 0);
}

/*
 * Desenha o contorno fechado do quadrilátero. Como SDL_RenderLines traça
 * linhas de um pixel, o contorno é repetido com pequenos deslocamentos
 * para ganhar espessura.
 */
static void desenhar_contorno(
    SDL_Renderer *render,
    const SDL_FPoint pontos[4],
    int raio
) {
    SDL_FPoint caminho[5];

    for (int i = 0; i < 4; ++i) {
        caminho[i] = pontos[i];
    }

    caminho[4] = pontos[0];

    for (int dy = -raio; dy <= raio; ++dy) {
        for (int dx = -raio; dx <= raio; ++dx) {
            SDL_FPoint deslocado[5];

            for (int i = 0; i < 5; ++i) {
                deslocado[i].x = caminho[i].x + (float)dx;
                deslocado[i].y = caminho[i].y + (float)dy;
            }

            SDL_RenderLines(render, deslocado, 5);
        }
    }
}

/* Quadrado cheio centrado em um ponto, usado para marcar os cantos. */
static void marcar_canto(SDL_Renderer *render, SDL_FPoint ponto, float lado) {
    const SDL_FRect rect = {
        ponto.x - lado / 2.0f,
        ponto.y - lado / 2.0f,
        lado,
        lado
    };

    SDL_RenderFillRect(render, &rect);
}

/* Cruz com moldura, usada para marcar o centro de um padrão de localização. */
static void marcar_padrao(SDL_Renderer *render, SDL_FPoint ponto) {
    const float braco = 6.0f;

    SDL_RenderLine(
        render,
        ponto.x - braco,
        ponto.y,
        ponto.x + braco,
        ponto.y
    );

    SDL_RenderLine(
        render,
        ponto.x,
        ponto.y - braco,
        ponto.x,
        ponto.y + braco
    );

    const SDL_FRect moldura = {
        ponto.x - braco,
        ponto.y - braco,
        braco * 2.0f,
        braco * 2.0f
    };

    SDL_RenderRect(render, &moldura);
}

/* Etiqueta com fundo sólido, para numerar as regiões sobre a imagem. */
static void desenhar_etiqueta(
    Gui *gui,
    const char *texto,
    float x,
    float y,
    SDL_Color fundo
) {
    SDL_Renderer *render = gui->render_principal;

    int largura = 0;
    int altura = 0;

    TTF_GetStringSize(gui->fonte_pequena, texto, 0, &largura, &altura);

    const SDL_FRect caixa = {
        x,
        y,
        (float)largura + 10.0f,
        (float)altura + 6.0f
    };

    definir_cor(render, fundo);
    SDL_RenderFillRect(render, &caixa);

    gui_desenhar_texto(
        render,
        gui->fonte_pequena,
        texto,
        caixa.x + 5.0f,
        caixa.y + 3.0f,
        COR_FUNDO_PRINCIPAL
    );
}

static void desenhar_marcacoes(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_principal;
    const QrResultado *resultado = dados->resultado;

    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < resultado->quantidade; ++i) {
        const QrCodigo *codigo = &resultado->codigos[i];

        SDL_FPoint pontos[4];

        for (int c = 0; c < 4; ++c) {
            pontos[c] = imagem_para_janela(gui, codigo->cantos[c]);
        }

        const bool selecionado = (i == dados->selecionado);

        SDL_Color cor = COR_MARCA;

        if (!codigo->decodificado) {
            cor = COR_MARCA_FALHA;
        } else if (selecionado) {
            cor = COR_MARCA_SELECIONADA;
        }

        /* Preenchimento translúcido destaca o código selecionado. */
        if (selecionado) {
            preencher_quadrilatero(
                render,
                pontos,
                cor_para_fcolor(cor, 0.18f)
            );
        }

        definir_cor(render, cor);

        desenhar_contorno(
            render,
            pontos,
            selecionado ? CONTORNO_RAIO_SELECIONADO : CONTORNO_RAIO_NORMAL
        );

        for (int c = 0; c < 4; ++c) {
            marcar_canto(render, pontos[c], 8.0f);
        }

        /* Padrões de localização encontrados pela detecção própria. */
        for (int p = 0; p < codigo->num_padroes; ++p) {
            marcar_padrao(render, imagem_para_janela(gui, codigo->padroes[p]));
        }

        char rotulo[64];

        if (codigo->decodificado) {
            SDL_snprintf(rotulo, sizeof(rotulo), "QR %d", i + 1);
        } else {
            SDL_snprintf(
                rotulo,
                sizeof(rotulo),
                "QR %d (não decodificado)",
                i + 1
            );
        }

        /* A etiqueta fica acima do contorno, ou abaixo se não houver espaço. */
        float etiqueta_y = pontos[0].y - 22.0f;

        if (etiqueta_y < gui->area_imagem.y) {
            etiqueta_y = pontos[0].y + 4.0f;
        }

        desenhar_etiqueta(gui, rotulo, pontos[0].x, etiqueta_y, cor);
    }

    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
}

void gui_desenhar_principal(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_principal;

    int janela_largura = 0;
    int janela_altura = 0;

    SDL_GetRenderOutputSize(render, &janela_largura, &janela_altura);

    definir_cor(render, COR_FUNDO_PRINCIPAL);
    SDL_RenderClear(render);

    SDL_Texture *textura = gui->textura_original;

    if (dados != NULL &&
        dados->mostrar_binaria &&
        gui->textura_binaria != NULL) {

        textura = gui->textura_binaria;
    }

    if (textura == NULL || gui->imagem_largura <= 0 || gui->imagem_altura <= 0) {
        const float centro_x = (float)janela_largura / 2.0f;
        const float centro_y = (float)janela_altura / 2.0f;

        desenhar_texto_centralizado(
            render,
            gui->fonte_titulo,
            "Nenhuma imagem carregada",
            centro_x,
            centro_y - 40.0f,
            COR_TEXTO
        );

        desenhar_texto_centralizado(
            render,
            gui->fonte,
            "Use o botão \"Abrir imagem\" na janela de leitura (tecla O)",
            centro_x,
            centro_y,
            COR_TEXTO_SUAVE
        );

        desenhar_texto_centralizado(
            render,
            gui->fonte,
            "ou arraste um arquivo de imagem para esta janela.",
            centro_x,
            centro_y + 24.0f,
            COR_TEXTO_SUAVE
        );

        SDL_RenderPresent(render);

        return;
    }

    /* Ajusta a imagem à janela mantendo a proporção e centraliza. */
    const float escala_x = (float)janela_largura / (float)gui->imagem_largura;
    const float escala_y = (float)janela_altura / (float)gui->imagem_altura;
    const float escala = (escala_x < escala_y) ? escala_x : escala_y;

    gui->area_imagem.w = (float)gui->imagem_largura * escala;
    gui->area_imagem.h = (float)gui->imagem_altura * escala;
    gui->area_imagem.x = ((float)janela_largura - gui->area_imagem.w) / 2.0f;
    gui->area_imagem.y = ((float)janela_altura - gui->area_imagem.h) / 2.0f;

    SDL_RenderTexture(render, textura, NULL, &gui->area_imagem);

    if (dados != NULL &&
        dados->mostrar_marcacao &&
        dados->resultado != NULL &&
        dados->resultado->quantidade > 0) {

        desenhar_marcacoes(gui, dados);
    }

    SDL_RenderPresent(render);
}

SDL_Surface *gui_capturar_imagem_marcada(Gui *gui, const GuiDados *dados) {
    if (gui->textura_original == NULL ||
        gui->imagem_largura <= 0 ||
        gui->imagem_altura <= 0) {

        fprintf(stderr, "Erro: nenhuma imagem carregada para salvar.\n");

        return NULL;
    }

    SDL_Texture *textura = gui->textura_original;

    if (dados != NULL &&
        dados->mostrar_binaria &&
        gui->textura_binaria != NULL) {

        textura = gui->textura_binaria;
    }

    /*
     * Desenhar em uma textura do tamanho original evita depender do
     * conteúdo da janela, que já foi apresentado, e grava o resultado na
     * resolução do arquivo de entrada em vez da resolução da janela.
     */
    SDL_Texture *alvo = SDL_CreateTexture(
        gui->render_principal,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        gui->imagem_largura,
        gui->imagem_altura
    );

    if (alvo == NULL) {
        fprintf(
            stderr,
            "Erro ao criar a textura de saida: %s\n",
            SDL_GetError()
        );

        return NULL;
    }

    if (!SDL_SetRenderTarget(gui->render_principal, alvo)) {
        fprintf(
            stderr,
            "Erro ao redirecionar o desenho: %s\n",
            SDL_GetError()
        );

        SDL_DestroyTexture(alvo);

        return NULL;
    }

    const SDL_FRect area_anterior = gui->area_imagem;

    gui->area_imagem.x = 0.0f;
    gui->area_imagem.y = 0.0f;
    gui->area_imagem.w = (float)gui->imagem_largura;
    gui->area_imagem.h = (float)gui->imagem_altura;

    definir_cor(gui->render_principal, COR_FUNDO_PRINCIPAL);
    SDL_RenderClear(gui->render_principal);

    SDL_RenderTexture(gui->render_principal, textura, NULL, &gui->area_imagem);

    if (dados != NULL &&
        dados->mostrar_marcacao &&
        dados->resultado != NULL &&
        dados->resultado->quantidade > 0) {

        desenhar_marcacoes(gui, dados);
    }

    SDL_Surface *captura = SDL_RenderReadPixels(gui->render_principal, NULL);

    SDL_SetRenderTarget(gui->render_principal, NULL);
    SDL_DestroyTexture(alvo);

    gui->area_imagem = area_anterior;

    if (captura == NULL) {
        fprintf(
            stderr,
            "Erro ao capturar a imagem marcada: %s\n",
            SDL_GetError()
        );
    }

    return captura;
}

void gui_reiniciar_rolagem(Gui *gui) {
    gui->rolagem_conteudo = 0.0f;
}

bool gui_ultimo_clique(Gui *gui, QrPonto *destino) {
    if (!gui->tem_clique) {
        return false;
    }

    gui->tem_clique = false;

    if (gui->imagem_largura <= 0 ||
        gui->area_imagem.w <= 0.0f ||
        gui->area_imagem.h <= 0.0f) {

        return false;
    }

    const float x = gui->clique.x;
    const float y = gui->clique.y;

    /* Cliques fora da área ocupada pela imagem são ignorados. */
    if (x < gui->area_imagem.x ||
        x > gui->area_imagem.x + gui->area_imagem.w ||
        y < gui->area_imagem.y ||
        y > gui->area_imagem.y + gui->area_imagem.h) {

        return false;
    }

    destino->x =
        (x - gui->area_imagem.x) *
        (float)gui->imagem_largura / gui->area_imagem.w;

    destino->y =
        (y - gui->area_imagem.y) *
        (float)gui->imagem_altura / gui->area_imagem.h;

    return true;
}

/* ------------------------------------------------------------------------- */
/* Janela secundária: informações, conteúdo e botões                         */
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

    definir_cor(render, COR_PAINEL);
    SDL_RenderFillRect(render, &painel);

    definir_cor(render, COR_PAINEL_BORDA);
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

        definir_cor(render, COR_PAINEL_BORDA);
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

        definir_cor(render, COR_TEXTO_SUAVE);
        SDL_RenderFillRect(render, &barra);
    }
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

void gui_desenhar_secundaria(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_secundario;

    definir_cor(render, COR_FUNDO_SECUNDARIA);
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
