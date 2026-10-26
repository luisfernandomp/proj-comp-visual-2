#include "gui.h"
#include "gui_interno.h"

#include <stdio.h>
#include <string.h>

/*
 * Infraestrutura da interface: criação das janelas e dos renderizadores,
 * carregamento das fontes, texturas da imagem exibida, seletor de arquivos
 * e tradução dos eventos do SDL em ações da aplicação.
 *
 * O desenho do conteúdo de cada janela fica nos outros dois arquivos do
 * módulo: gui_imagem.c (janela principal) e gui_painel.c (janela de
 * leitura). O que os três compartilham está em gui_interno.h.
 */

#define FONTE_ARQUIVO     "assets/fonts/DejaVuSans.ttf"
#define FONTE_TAMANHO      15.0f
#define FONTE_TAMANHO_PEQ  12.0f
#define FONTE_TAMANHO_TIT  18.0f

/* ------------------------------------------------------------------------- */
/* Paleta da interface (declarada em gui_interno.h)                          */
/* ------------------------------------------------------------------------- */

const SDL_Color COR_FUNDO_PRINCIPAL   = {  30,  30,  30, 255 };
const SDL_Color COR_FUNDO_SECUNDARIA  = {  24,  26,  32, 255 };
const SDL_Color COR_PAINEL            = {  38,  41,  50, 255 };
const SDL_Color COR_PAINEL_BORDA      = {  60,  64,  76, 255 };
const SDL_Color COR_TEXTO             = { 235, 235, 235, 255 };
const SDL_Color COR_TEXTO_SUAVE       = { 160, 165, 180, 255 };
const SDL_Color COR_TEXTO_DESTAQUE    = { 130, 210, 150, 255 };
const SDL_Color COR_BOTAO             = {  58,  63,  78, 255 };
const SDL_Color COR_BOTAO_HOVER       = {  84,  92, 116, 255 };
const SDL_Color COR_BOTAO_PRESSIONADO = {  42,  46,  58, 255 };
const SDL_Color COR_BOTAO_DESABILITADO = { 40, 43, 52, 255 };
const SDL_Color COR_BOTAO_BORDA       = { 120, 130, 160, 255 };
const SDL_Color COR_TEXTO_DESABILITADO = { 100, 104, 118, 255 };
/* Marcação da região do QR Code na janela principal. */
const SDL_Color COR_MARCA             = { 100, 170, 255, 255 };
const SDL_Color COR_MARCA_SELECIONADA = {  90, 230, 130, 255 };
const SDL_Color COR_MARCA_FALHA       = { 240, 150,  70, 255 };

/* ------------------------------------------------------------------------- */
/* Fontes                                                                    */
/* ------------------------------------------------------------------------- */

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
/* Ciclo de vida                                                             */
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

    gui_montar_botoes(gui);

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

/* ------------------------------------------------------------------------- */
/* Imagem exibida                                                            */
/* ------------------------------------------------------------------------- */

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

/* ------------------------------------------------------------------------- */
/* Eventos                                                                   */
/* ------------------------------------------------------------------------- */

static bool botao_contem(const GuiBotao *botao, float x, float y) {
    const SDL_FPoint ponto = { x, y };

    return SDL_PointInRectFloat(&ponto, &botao->rect);
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
/* Desenho de texto                                                          */
/* ------------------------------------------------------------------------- */

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
