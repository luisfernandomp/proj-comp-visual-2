#ifndef GUI_H
#define GUI_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>

#include "qr_tipos.h"

/*
 * Interface gráfica da aplicação.
 *
 * - Janela principal: exibe a imagem selecionada e desenha sobre ela a
 *   marcação da região de cada QR Code encontrado (contorno, cantos,
 *   padrões de localização e numeração).
 * - Janela secundária: criada como filha da principal, posicionada em
 *   (0,0). Mostra as informações da análise, o conteúdo decodificado do
 *   QR Code selecionado e os botões de interação.
 *
 * A GUI não processa a imagem: apenas desenha o que recebe em GuiDados.
 * Todos os textos são desenhados com SDL_ttf usando a fonte DejaVu Sans
 * distribuída em assets/fonts/.
 */

enum {
    GUI_PRINCIPAL_LARGURA  = 1024,  /* resolução inicial da janela principal */
    GUI_PRINCIPAL_ALTURA   = 768,
    GUI_SECUNDARIA_LARGURA = 640,
    GUI_SECUNDARIA_ALTURA  = 548,
    GUI_MAX_LINHAS         = 8,     /* linhas de análise exibidas no painel */
    GUI_MAX_CAMINHO        = 1024,
    GUI_NUM_BOTOES         = 6
};

/* Ações geradas pela interface a partir dos eventos do usuário. */
typedef enum {
    GUI_ACAO_NENHUMA = 0,
    GUI_ACAO_SAIR,                /* fechar a janela, ESC ou SDL_EVENT_QUIT */
    GUI_ACAO_ABRIR,               /* abrir o seletor de arquivos */
    GUI_ACAO_ARQUIVO_RECEBIDO,    /* imagem escolhida no seletor ou arrastada */
    GUI_ACAO_ALTERNAR_VISTA,      /* imagem original <-> imagem binarizada */
    GUI_ACAO_ALTERNAR_MARCACAO,   /* liga e desliga o destaque da região */
    GUI_ACAO_PROXIMO_CODIGO,      /* seleciona o próximo QR Code da imagem */
    GUI_ACAO_CLIQUE_IMAGEM,       /* clique sobre a imagem (seleção por região) */
    GUI_ACAO_COPIAR,              /* copia o conteúdo para a área de transferência */
    GUI_ACAO_SALVAR,              /* salva a imagem com a marcação */
    GUI_ACAO_ALTERAR_RESOLUCAO    /* alterna entre 1024x768 e a resolução da imagem */
} GuiAcao;

typedef struct {
    SDL_FRect   rect;
    const char *rotulo;
    GuiAcao     acao;
    bool        hover;        /* mouse sobre o botão */
    bool        pressionado;
    bool        habilitado;
} GuiBotao;

/*
 * Estado que a interface precisa desenhar. Preenchido pelo main a cada
 * quadro a partir do resultado da análise.
 */
typedef struct {
    const QrResultado *resultado;
    int                selecionado;        /* índice do QR em destaque, -1 se nenhum */
    bool               mostrar_binaria;
    bool               mostrar_marcacao;
    bool               tem_imagem;
    const char *const *linhas;             /* linhas de informação da análise */
    int                num_linhas;
    const char        *mensagem;           /* mensagem de status ou erro */
} GuiDados;

typedef struct {
    SDL_Window   *janela_principal;
    SDL_Renderer *render_principal;
    SDL_Window   *janela_secundaria;
    SDL_Renderer *render_secundario;

    TTF_Font *fonte;          /* textos e botões */
    TTF_Font *fonte_pequena;  /* rótulos auxiliares */
    TTF_Font *fonte_titulo;

    SDL_Texture *textura_original;  /* imagem carregada */
    SDL_Texture *textura_binaria;   /* resultado da binarização */
    int          imagem_largura;
    int          imagem_altura;

    /* Área em que a imagem foi desenhada na janela principal: usada para
     * converter coordenadas da imagem em coordenadas da janela. */
    SDL_FRect area_imagem;

    GuiBotao botoes[GUI_NUM_BOTOES];

    /* Caminho recebido do seletor de arquivos ou de um arrastar-e-soltar. */
    char caminho_recebido[GUI_MAX_CAMINHO];
    bool tem_caminho_recebido;
    bool dialogo_aberto;

    /* Conteúdo decodificado já renderizado: refeito apenas quando o texto
     * muda, para não gerar uma textura nova a cada quadro. */
    SDL_Texture *textura_conteudo;
    float        conteudo_largura;
    float        conteudo_altura;
    char         conteudo_renderizado[QR_MAX_CONTEUDO];

    float rolagem_conteudo;   /* deslocamento vertical do painel de conteúdo */
    float rolagem_maxima;

    SDL_FPoint clique;        /* último clique na janela principal */
    bool       tem_clique;
} Gui;

/* Cria as duas janelas, os renderizadores e carrega as fontes.
 * Retorna false (com mensagem no stderr) se qualquer etapa falhar. */
bool gui_iniciar(Gui *gui, const char *titulo);

/* Libera todos os recursos criados por gui_iniciar. Seguro chamar mais de uma vez. */
void gui_encerrar(Gui *gui);

/* Converte as superfícies em texturas para exibição na janela principal.
 * `binaria` pode ser NULL. Deve ser chamada sempre que a imagem mudar. */
bool gui_definir_imagens(Gui *gui, SDL_Surface *original, SDL_Surface *binaria);

/* Descarta as texturas atuais (usado quando nenhuma imagem está carregada). */
void gui_limpar_imagens(Gui *gui);

/* Redimensiona a janela principal e a reposiciona no monitor principal. */
bool gui_definir_resolucao(Gui *gui, int largura, int altura);

/* Abre o seletor de arquivos do sistema (assíncrono: a escolha chega
 * depois, como GUI_ACAO_ARQUIVO_RECEBIDO). */
void gui_abrir_dialogo(Gui *gui);

/* Copia para `destino` o caminho recebido do seletor ou do arrastar-e-soltar
 * e limpa o estado interno. Retorna false se não houver caminho pendente. */
bool gui_obter_caminho(Gui *gui, char *destino, size_t tamanho);

/* Traduz um evento SDL em uma ação da interface. */
GuiAcao gui_processar_evento(Gui *gui, const SDL_Event *evento);

/* Desenha a janela principal: imagem e marcação das regiões. */
void gui_desenhar_principal(Gui *gui, const GuiDados *dados);

/* Desenha a janela secundária: informações, conteúdo e botões. */
void gui_desenhar_secundaria(Gui *gui, const GuiDados *dados);

/*
 * Redesenha a imagem e a marcação em uma textura do tamanho original da
 * imagem e devolve o resultado como superfície, pronta para ser salva em
 * disco na resolução do arquivo de entrada (e não na da janela).
 * A superfície devolvida pertence ao chamador.
 */
SDL_Surface *gui_capturar_imagem_marcada(Gui *gui, const GuiDados *dados);

/* Volta o painel de conteúdo ao topo (ao trocar de QR Code ou de imagem). */
void gui_reiniciar_rolagem(Gui *gui);

/*
 * Converte o último clique na janela principal para coordenadas da imagem.
 * Retorna false se não houver clique pendente ou se ele caiu fora da área
 * ocupada pela imagem.
 */
bool gui_ultimo_clique(Gui *gui, QrPonto *destino);

/* Desenha uma linha de texto em (x, y). Retorna a largura desenhada. */
int gui_desenhar_texto(
    SDL_Renderer *render,
    TTF_Font *fonte,
    const char *texto,
    float x,
    float y,
    SDL_Color cor
);

#endif
