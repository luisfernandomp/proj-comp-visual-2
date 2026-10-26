#ifndef GUI_INTERNO_H
#define GUI_INTERNO_H

#include "gui.h"

/*
 * Recursos compartilhados pelos três arquivos que implementam a interface:
 *
 *   gui.c         janelas, fontes, texturas, seletor de arquivos e eventos
 *   gui_imagem.c  janela principal: a imagem e a marcação das regiões
 *   gui_painel.c  janela de leitura: informações, conteúdo e botões
 *
 * Nada declarado aqui faz parte da interface pública do módulo, que está
 * em gui.h e é a única usada pelo restante do programa.
 */

/* Paleta da interface. As cores são definidas em gui.c. */
extern const SDL_Color COR_FUNDO_PRINCIPAL;
extern const SDL_Color COR_FUNDO_SECUNDARIA;
extern const SDL_Color COR_PAINEL;
extern const SDL_Color COR_PAINEL_BORDA;
extern const SDL_Color COR_TEXTO;
extern const SDL_Color COR_TEXTO_SUAVE;
extern const SDL_Color COR_TEXTO_DESTAQUE;
extern const SDL_Color COR_BOTAO;
extern const SDL_Color COR_BOTAO_HOVER;
extern const SDL_Color COR_BOTAO_PRESSIONADO;
extern const SDL_Color COR_BOTAO_DESABILITADO;
extern const SDL_Color COR_BOTAO_BORDA;
extern const SDL_Color COR_TEXTO_DESABILITADO;

/* Cores da marcação da região do QR Code na janela principal. */
extern const SDL_Color COR_MARCA;
extern const SDL_Color COR_MARCA_SELECIONADA;
extern const SDL_Color COR_MARCA_FALHA;

/* Define a cor usada pelas próximas operações de desenho. */
static inline void gui_definir_cor(SDL_Renderer *render, SDL_Color cor) {
    SDL_SetRenderDrawColor(render, cor.r, cor.g, cor.b, cor.a);
}

/* Posiciona os botões da janela de leitura. Chamada por gui_iniciar. */
void gui_montar_botoes(Gui *gui);

#endif
