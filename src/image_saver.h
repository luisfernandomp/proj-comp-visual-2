#ifndef IMAGE_SAVER_H
#define IMAGE_SAVER_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Salva uma superfície em PNG, informando no terminal se o arquivo foi
 * criado ou sobrescrito. Retorna false em caso de erro.
 *
 * É usada para gravar a imagem com a marcação da região do QR Code,
 * capturada diretamente da janela principal.
 */
bool salvar_imagem(SDL_Surface *imagem, const char *caminho);

#endif
